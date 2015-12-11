/* The Image Registration Toolkit (IRTK)
 *
 * Copyright 2008-2015 Imperial College London
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#include <irtkPointCorrespondenceDistance.h>

#include <vtkSmartPointer.h>
#include <vtkCharArray.h>
#include <vtkFloatArray.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>

#include <irtkEventDelegate.h>
#include <irtkPolyDataUtils.h>

using namespace fastdelegate;
using namespace irtk::polydata;


// =============================================================================
// Auxiliary functors
// =============================================================================

namespace irtkPointCorrespondenceDistanceUtils {


// -----------------------------------------------------------------------------
/// Evaluate distance error of point correspondences
class EvaluateError
{
  const irtkRegisteredPointSet  *_Target;
  const vector<int>             *_Sample;
  const irtkPointCorrespondence *_Source;
  const irtkRadialErrorFunction *_ErrorFunction;
  double                         _Error;

public:

  EvaluateError() {}

  EvaluateError(const EvaluateError &lhs, split)
  :
    _Target       (lhs._Target),
    _Sample       (lhs._Sample),
    _Source       (lhs._Source),
    _ErrorFunction(lhs._ErrorFunction)
  {
    _Error = .0;
  }

  void join(EvaluateError &rhs)
  {
    _Error += rhs._Error;
  }

  void operator()(const blocked_range<int> &re)
  {
    irtkPoint p1, p2;
    for (int k = re.begin(); k != re.end(); ++k) {
      irtkPointCorrespondence::GetPoint(p1, _Target, _Sample, k);
      if (_Source->GetPoint(k, p2)) {
        _Error += _ErrorFunction->Value(p1.SquaredDistance(p2));
      }
    }
  }

  static double Run(const irtkRegisteredPointSet  *target,
                    const vector<int>             &sample,
                    const irtkPointCorrespondence *source,
                    const irtkRadialErrorFunction *error_func)
  {
    const int n = irtkPointCorrespondence::GetNumberOfPoints(target, &sample);
    if (n == 0) return .0;
    EvaluateError body;
    body._Target         = target;
    body._Sample         = (sample.empty() ? NULL : &sample);
    body._Source         = source;
    body._ErrorFunction  = error_func;
    body._Error          = .0;
    blocked_range<int> range(0, n);
    parallel_reduce(range, body);
    return body._Error / n;
  }
};

// -----------------------------------------------------------------------------
/// Evaluate gradient of distance error of point correspondences
class EvaluateGradient
{
  typedef irtkPointCorrespondenceDistance::GradientType GradientType;

  const irtkRegisteredPointSet  *_Target;
  const vector<int>             *_Sample;
  const irtkPointCorrespondence *_Source;
  const irtkRadialErrorFunction *_ErrorFunction;
  GradientType                  *_Gradient;
  double                         _Norm;

public:

  void operator()(const blocked_range<int> &re) const
  {
    GradientType d;
    irtkPoint    p1, p2;
    for (int k = re.begin(); k != re.end(); ++k) {
      irtkPointCorrespondence::GetPoint(p1, _Target, _Sample, k);
      if (_Source->GetPoint(k, p2)) {
        d = p1 - p2;
        _Gradient[k] = d * (_ErrorFunction->Derivative(p1.SquaredDistance(p2)) * _Norm);
      } else {
        _Gradient[k] = .0;
      }
    }
  }

  static void Run(const irtkRegisteredPointSet  *target,
                  const vector<int>             &sample,
                  const irtkPointCorrespondence *source,
                  const irtkRadialErrorFunction *error_func,
                  GradientType                  *gradient)
  {
    const int n = irtkPointCorrespondence::GetNumberOfPoints(target, &sample);
    if (n == 0) return;
    EvaluateGradient body;
    body._Target         = target;
    body._Sample         = (sample.empty() ? NULL : &sample);
    body._Source         = source;
    body._ErrorFunction  = error_func;
    body._Gradient       = gradient;
    body._Norm           = 2.0 / n;
    blocked_range<int> range(0, n);
    parallel_for(range, body);
  }
};


} // namespace irtkPointCorrespondenceDistanceUtils
using namespace irtkPointCorrespondenceDistanceUtils;

// =============================================================================
// Construction/Destruction
// =============================================================================

// -----------------------------------------------------------------------------
irtkPointCorrespondenceDistance::irtkPointCorrespondenceDistance(const char *name, double weight)
:
  irtkPointSetDistance(name, weight),
  _TargetSampleDistance (.0),
  _SourceSampleDistance (.0),
  _NumberOfTargetSamples( 0),
  _NumberOfSourceSamples( 0),
  _UpdatePeriod         (-1),
  _NumberOfUpdates      ( 0),
  _Correspondence(irtkPointCorrespondence::New(irtkPointCorrespondence::ClosestPoint)),
  _ErrorFunction (irtkRadialErrorFunction::New(irtkRadialErrorFunction::Squared)),
  _EvaluateTargetError(false),
  _EvaluateSourceError(false)
{
  // Forward events of internal components
  _EventDelegate.Bind(MakeDelegate(this, &irtkPointCorrespondenceDistance::ForwardEvent));
}

// -----------------------------------------------------------------------------
irtkPointCorrespondenceDistance
::irtkPointCorrespondenceDistance(const char *name, double weight,
                                  irtkPointCorrespondence *cor,
                                  irtkRadialErrorFunction *fun)
:
  irtkPointSetDistance(name, weight),
  _TargetSampleDistance (.0),
  _SourceSampleDistance (.0),
  _NumberOfTargetSamples( 0),
  _NumberOfSourceSamples( 0),
  _UpdatePeriod         (-1),
  _NumberOfUpdates      ( 0),
  _Correspondence(cor ? cor : irtkPointCorrespondence::New(irtkPointCorrespondence::ClosestPoint)),
  _ErrorFunction (fun ? fun : irtkRadialErrorFunction::New(irtkRadialErrorFunction::Squared)),
  _EvaluateTargetError(false),
  _EvaluateSourceError(false)
{
  // Forward events of internal components
  _EventDelegate.Bind(MakeDelegate(this, &irtkPointCorrespondenceDistance::ForwardEvent));
}

// -----------------------------------------------------------------------------
irtkPointCorrespondenceDistance::irtkPointCorrespondenceDistance(const irtkPointCorrespondenceDistance &other)
:
  irtkPointSetDistance(other,
    other._Target ? irtkPointCorrespondence::GetNumberOfPoints(other._Target, &other._TargetSample) : 0,
    other._Source ? irtkPointCorrespondence::GetNumberOfPoints(other._Source, &other._SourceSample) : 0
  ),
  _TargetSampleDistance (other._TargetSampleDistance),
  _SourceSampleDistance (other._SourceSampleDistance),
  _NumberOfTargetSamples(other._NumberOfTargetSamples),
  _NumberOfSourceSamples(other._NumberOfSourceSamples),
  _TargetSample         (other._TargetSample),
  _SourceSample         (other._SourceSample),
  _UpdatePeriod         (other._UpdatePeriod),
  _NumberOfUpdates      (other._NumberOfUpdates),
  _Correspondence       (other._Correspondence->NewInstance()),
  _ErrorFunction        (other._ErrorFunction->NewInstance()),
  _EvaluateTargetError  (other._EvaluateTargetError),
  _EvaluateSourceError  (other._EvaluateSourceError)
{
  // Forward events of internal components
  _EventDelegate.Bind(MakeDelegate(this, &irtkPointCorrespondenceDistance::ForwardEvent));
}

// -----------------------------------------------------------------------------
irtkPointCorrespondenceDistance &irtkPointCorrespondenceDistance::operator =(const irtkPointCorrespondenceDistance &other)
{
  // Copy base class attributes
  const int m = other._Target ? irtkPointCorrespondence::GetNumberOfPoints(other._Target, &other._TargetSample) : 0;
  const int n = other._Source ? irtkPointCorrespondence::GetNumberOfPoints(other._Source, &other._SourceSample) : 0;
  irtkPointSetDistance::Copy(other, m, n);
  // Copy own attributes
  Delete(_Correspondence);
  Delete(_ErrorFunction);
  _TargetSampleDistance  = other._TargetSampleDistance;
  _SourceSampleDistance  = other._SourceSampleDistance;
  _NumberOfTargetSamples = other._NumberOfTargetSamples;
  _NumberOfSourceSamples = other._NumberOfSourceSamples;
  _TargetSample          = other._TargetSample;
  _SourceSample          = other._SourceSample;
  _UpdatePeriod          = other._UpdatePeriod;
  _NumberOfUpdates       = other._NumberOfUpdates;
  _Correspondence        = other._Correspondence->NewInstance();
  _ErrorFunction         = other._ErrorFunction ->NewInstance();
  _EvaluateTargetError   = other._EvaluateTargetError;
  _EvaluateSourceError   = other._EvaluateSourceError;
  // Forward events of internal components
  _EventDelegate.Bind(MakeDelegate(this, &irtkObservable::Broadcast));
  _Correspondence->AddObserver(_EventDelegate);
  return *this;
}

// -----------------------------------------------------------------------------
irtkPointCorrespondenceDistance::~irtkPointCorrespondenceDistance()
{
  Delete(_Correspondence);
  Delete(_ErrorFunction);
}

// =============================================================================
// Parameters
// =============================================================================

// -----------------------------------------------------------------------------
bool irtkPointCorrespondenceDistance::Set(const char *param, const char *value)
{
  const string name = ParameterNameWithoutPrefix(param);

  // Approximate minimum distance between point samples
  if (name == "Target sample distance" || name == "Target fiducial distance") {
    return FromString(value, _TargetSampleDistance);
  }
  if (name == "Source sample distance" || name == "Source fiducial distance") {
    return FromString(value, _SourceSampleDistance);
  }
  if (name == "Sample distance" || name == "Fiducial distance") {
    if (!FromString(value, _TargetSampleDistance)) return false;
    _SourceSampleDistance = _TargetSampleDistance;
    return true;
  }
  // Maximum number of point samples to draw from each data set
  if (name == "No. of target samples" || name == "Number of target samples") {
    return FromString(value, _NumberOfTargetSamples);
  }
  if (name == "No. of source samples" || name == "Number of source samples") {
    return FromString(value, _NumberOfSourceSamples);
  }
  if (name == "No. of samples" || name == "Number of samples") {
    if (!FromString(value, _NumberOfTargetSamples)) return false;
    _NumberOfSourceSamples = _NumberOfTargetSamples;
    return true;
  }

  // Set correspondence type
  if (name == "Correspondence") {
    irtkPointCorrespondence::TypeId type;
    if (!FromString(value, type)) return false;
    if (_Correspondence->Type() != type) {
      irtkPointCorrespondence *corr = irtkPointCorrespondence::New(type);
      corr->Parameter(_Correspondence->Parameter());
      corr->AddObserver(_EventDelegate);
      this->Correspondence(corr);
    }
    return true;
  }
  // When to update the correspondences
  if (name == "Correspondence update") {
    if (strcmp(value, "Never")       == 0) { _UpdatePeriod =  0; return true; }
    if (strcmp(value, "Always")      == 0) { _UpdatePeriod =  1; return true; }
    if (strcmp(value, "Alternate")   == 0 ||
        strcmp(value, "Alternating") == 0) { _UpdatePeriod = -1; return true; }
    if (strncmp(value, "Skip ", 5) == 0) value += 5;
    return FromString(value, _UpdatePeriod);
  }

  // Set error function
  if (name == "Function") {
    irtkRadialErrorFunction::TypeId type;
    if (!FromString(value, type)) return false;
    if (_ErrorFunction->Type() != type) {
      Delete(_ErrorFunction);
      _ErrorFunction = irtkRadialErrorFunction::New(type);
    }
    return true;
  }
  // Evaluate error in both directions
  if (name == "Symmetric") {
    if (!FromString(value, _EvaluateTargetError)) return false;
    _EvaluateSourceError = _EvaluateTargetError;
    return true;
  }

  // Weight
  if (irtkPointSetDistance::Set(param, value)) return true;

  if (!name.empty()) {
    // Correspondence parameter
    if (_Correspondence->Set(name.c_str(), value)) return true;
    // Error function parameter
    if (_ErrorFunction->Set(name.c_str(), value)) return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
irtkParameterList irtkPointCorrespondenceDistance::Parameter() const
{
  irtkParameterList params = irtkPointSetDistance::Parameter();
  if (!_Name.empty()) {
    if (_TargetSampleDistance == _SourceSampleDistance) {
      Insert(params, _Name + " sample distance",        _TargetSampleDistance);
    } else {
      Insert(params, _Name + " target sample distance", _TargetSampleDistance);
      Insert(params, _Name + " source sample distance", _SourceSampleDistance);
    }
    if (_NumberOfTargetSamples == _NumberOfSourceSamples) {
      Insert(params, _Name + " no. of samples",        _NumberOfTargetSamples);
    } else {
      Insert(params, _Name + " no. of target samples", _NumberOfTargetSamples);
      Insert(params, _Name + " no. of source samples", _NumberOfSourceSamples);
    }
    Insert(params, _Name + " correspondence", _Correspondence->Type());
    string update_period;
    if      (_UpdatePeriod <  0) update_period = "Alternate";
    else if (_UpdatePeriod == 0) update_period = "Never";
    else if (_UpdatePeriod == 1) update_period = "Always";
    else                         update_period = ToString(_UpdatePeriod);
    Insert(params, _Name + " correspondence update", update_period);
    Insert(params, _Name + " function",  _ErrorFunction->Type());
    Insert(params, _Name + " symmetric", _EvaluateTargetError && _EvaluateSourceError);
    Insert(params, _Correspondence->Parameter(), _Name.c_str());
  }
  return params;
}

// =============================================================================
// Initialization/Update
// =============================================================================

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance::SamplePoints()
{
  // Draw samples from data sets for which error is to be evaluated
  if (_Correspondence->Type() == irtkPointCorrespondence::ClosestCell) {
    // TODO:   Decimate meshes instead (would possibly be better anyways...)
    // Update: Remeshing is (optionally) done by the registration filter already.
    _TargetSample.clear();
    _SourceSample.clear();
  } else {
    irtkPointCorrespondenceUtils::SamplePoints(_Target, _TargetSample, _NumberOfTargetSamples, _TargetSampleDistance);
    irtkPointCorrespondenceUtils::SamplePoints(_Source, _SourceSample, _NumberOfSourceSamples, _SourceSampleDistance);
  }
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance::Initialize()
{
  // Check inputs
  if (_Target == NULL) {
    cerr << "irtkPointCorrespondenceDistance::Initialize: Target dataset is NULL" << endl;
    exit(1);
  }
  if (_Source == NULL) {
    cerr << "irtkPointCorrespondenceDistance::Initialize: Source dataset is NULL" << endl;
    exit(1);
  }

  // Draw samples from data sets for which error is to be evaluated
  SamplePoints();

  const int m = irtkPointCorrespondence::GetNumberOfPoints(_Target, &_TargetSample);
  const int n = irtkPointCorrespondence::GetNumberOfPoints(_Source, &_SourceSample);

  if (m == 0) {
    cerr << "irtkPointCorrespondenceDistance::Initialize: No target samples to register" << endl;
    exit(1);
  }
  if (n == 0) {
    cerr << "irtkPointCorrespondenceDistance::Initialize: No source samples to register" << endl;
    exit(1);
  }

  // Initialize base class
  irtkPointSetDistance::Initialize(m, n);

  // Force update of input data sets and point correspondences upon next Update
  _InitialUpdate   = true;
  _NumberOfUpdates = 0;
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance::Reinitialize()
{
  // Draw samples from data sets for which error is to be evaluated
  SamplePoints();

  const int m = irtkPointCorrespondence::GetNumberOfPoints(_Target, &_TargetSample);
  const int n = irtkPointCorrespondence::GetNumberOfPoints(_Source, &_SourceSample);

  if (m == 0) {
    cerr << "irtkPointCorrespondenceDistance::Initialize: No target samples to register" << endl;
    exit(1);
  }
  if (n == 0) {
    cerr << "irtkPointCorrespondenceDistance::Initialize: No source samples to register" << endl;
    exit(1);
  }

  // Reinitialize base class
  irtkPointSetDistance::Reinitialize(m, n);

  // Reinitialize correspondences
  if (!_InitialUpdate) {
    _Target->Update(_Target->SelfUpdate());
    _Source->Update(_Source->SelfUpdate());
    _Correspondence->Initialize();
  }
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance::Update(bool)
{
  // Increment Update counter
  ++_NumberOfUpdates;

  // Perform delayed initialization steps during first call after Initialize
  if (_InitialUpdate) {
    _InitialUpdate = false;

    // Update all input data set(s)
    _Target->Update(_Target->SelfUpdate());
    _Source->Update(_Source->SelfUpdate());

    // Initialize point correspondences
    //
    // Note: This is done during the initial update because only then the
    //       registered data sets are up-to-date and updating these during
    //       the initialization results in one more update than necessary.
    _Correspondence->AddObserver(_EventDelegate);
    _Correspondence->Target(_Target);
    _Correspondence->Source(_Source);
    _Correspondence->TargetSample(_TargetSample.empty() ? NULL : &_TargetSample);
    _Correspondence->SourceSample(_SourceSample.empty() ? NULL : &_SourceSample);
    _Correspondence->FromTargetToSource(DoEvaluateTargetError() || _GradientWrtTarget);
    _Correspondence->FromSourceToTarget(DoEvaluateSourceError() || _GradientWrtSource);
    _Correspondence->Initialize();

  // Perform consecutive update steps
  } else {

    // Update moving input data set(s)
    if (_Target->Transformation()) _Target->Update();
    if (_Source->Transformation()) _Source->Update();

    // a) Never update correspondences
    if (_UpdatePeriod == 0) return;
    // b) Update only once after Upgrade was called
    if (_UpdatePeriod < 0 && _NumberOfUpdates != 1) return;
    // c) Update correspondences periodically
    if (_UpdatePeriod > 0 && (_NumberOfUpdates % _UpdatePeriod) != 0) return;
  }

  // Update correspondences
  _Correspondence->Update();
}

// -----------------------------------------------------------------------------
bool irtkPointCorrespondenceDistance::Upgrade()
{
  // Skip if correspondences are fixed or updated regularly
  if (_UpdatePeriod >= 0) return false;

  // Reset update counter
  _NumberOfUpdates = 0;

  // Reset initial value
  this->ResetInitialValue();

  // Update correspondences
  return _Correspondence->Upgrade();
}

// =============================================================================
// Evaluation
// =============================================================================

// -----------------------------------------------------------------------------
bool irtkPointCorrespondenceDistance::DoEvaluateTargetError() const
{
  if (!_EvaluateTargetError && !_EvaluateSourceError) {
    return (_Target->Transformation() != NULL);
  }
  return _EvaluateTargetError;
}

// -----------------------------------------------------------------------------
bool irtkPointCorrespondenceDistance::DoEvaluateSourceError() const
{
  if (!_EvaluateTargetError && !_EvaluateSourceError) {
    return (_Source->Transformation() != NULL);
  }
  return _EvaluateSourceError;
}

// -----------------------------------------------------------------------------
double irtkPointCorrespondenceDistance::Evaluate()
{
  const bool t = DoEvaluateTargetError(); // target --> source
  const bool s = DoEvaluateSourceError(); // target <-- source

  double error = .0;
  if (t) {
    _Correspondence->DefaultDirection(irtkPointCorrespondence::TargetToSource);
    error += EvaluateError::Run(_Target, _TargetSample, _Correspondence, _ErrorFunction);
  }
  if (s) {
    _Correspondence->DefaultDirection(irtkPointCorrespondence::SourceToTarget);
    error += EvaluateError::Run(_Source, _SourceSample, _Correspondence, _ErrorFunction);
    if (t) error *= .5;
  }
  return error;
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance
::NonParametricGradient(const irtkRegisteredPointSet *wrt_pset,
                        irtkVector3D<double>         *np_gradient)
{
  // Debug assertions
  irtkAssert(wrt_pset == _Target || wrt_pset == _Source,
             "input point set must be either _Target or _Source");
  irtkAssert(np_gradient == _GradientWrtTarget || np_gradient == _GradientWrtSource,
             "output gradient must be either _GradientWrtTarget or _GradientWrtSource");

  // Calculate non-parametric gradient w.r.t. the specified point set
  if (wrt_pset == _Target) {
    _Correspondence->DefaultDirection(irtkPointCorrespondence::TargetToSource);
    EvaluateGradient::Run(_Target, _TargetSample, _Correspondence, _ErrorFunction, np_gradient);
  } else {
    _Correspondence->DefaultDirection(irtkPointCorrespondence::SourceToTarget);
    EvaluateGradient::Run(_Source, _SourceSample, _Correspondence, _ErrorFunction, np_gradient);
  }
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance
::ParametricGradient(const irtkRegisteredPointSet *target,
                     const irtkVector3D<double>   *np_gradient,
                     double                       *gradient,
                     double                        weight)
{
  // Debug assertions
  irtkAssert(target->Transformation() != NULL,
             "input data set must have a transformation set");
  irtkAssert(target == _Target || target == _Source,
             "input data set must be either _Target or _Source");
  irtkAssert(np_gradient == _GradientWrtTarget || np_gradient == _GradientWrtSource,
             "input gradient must be either _GradientWrtTarget or _GradientWrtSource");

  // Get starting positions of non-parametric gradient vectors
  const bool np_wrt_target  = (np_gradient == _GradientWrtTarget);
  const vector<int> &sample = (np_wrt_target ? _TargetSample : _SourceSample);
  const int         npoints = (np_wrt_target
      ? irtkPointCorrespondence::GetNumberOfPoints(_Target, &_TargetSample)
      : irtkPointCorrespondence::GetNumberOfPoints(_Source, &_SourceSample));

  irtkPointSet pos(npoints);
  if ((target == _Target &&  np_wrt_target) ||
      (target == _Source && !np_wrt_target)) {
    if (sample.empty()) {
      for (int k = 0; k < pos.Size(); ++k) {
        target->GetInputPoint(k, pos(k));
      }
    } else {
      for (int k = 0; k < pos.Size(); ++k) {
        target->GetInputPoint(sample[k], pos(k));
      }
    }
  } else {
    _Correspondence->DefaultDirection(np_wrt_target
                                      ? irtkPointCorrespondence::TargetToSource
                                      : irtkPointCorrespondence::SourceToTarget);
    for (int k = 0; k < pos.Size(); ++k) {
      _Correspondence->GetInputPoint(k, pos(k));
    }
    weight = - weight;
  }

  // Add parametric gradient
  const double t0 = target->InputTime();
  const double t  = target->Time();
  const irtkTransformation * const T = target->Transformation();
  T->ParametricGradient(pos, np_gradient, gradient, t, t0, weight);
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance::EvaluateGradient(double *gradient, double, double weight)
{
  // If target and source are transformed by different transformations,
  // the gradient vector contains first the derivative values w.r.t the
  // parameters of the target transformation followed by those computed
  // w.r.t the parameters of the source transformation. Otherwise, if
  // both point sets are transformed by the same transformation, i.e., a
  // velocity based transformation integrated half way in both directions,
  // the derivative values are summed up instead.
  const irtkTransformation * const T1 = _Target->Transformation();
  const irtkTransformation * const T2 = _Source->Transformation();
  const int offset = (T1 && T2 && !HaveSameDOFs(T1, T2) ? T2->NumberOfDOFs() : 0);
  // Compute non-parametric gradient w.r.t. target data set and add corrsponding
  // parametric gradient to either the target tranformation, its negative
  // to the source transformation, or to both of them if both are transformed
  if (_GradientWrtTarget) {
    this->NonParametricGradient(_Target, _GradientWrtTarget);
    if (T1) this->ParametricGradient(_Target, _GradientWrtTarget, gradient,          weight);
    if (T2) this->ParametricGradient(_Source, _GradientWrtTarget, gradient + offset, weight);
  }
  // Compute non-parametric gradient w.r.t. source data set and add corrsponding
  // parametric gradient to either the source tranformation, its negative
  // to the target transformation, or to both of them if both are transformed
  if (_GradientWrtSource) {
    this->NonParametricGradient(_Source, _GradientWrtSource);
    if (T1) this->ParametricGradient(_Target, _GradientWrtSource, gradient,          weight);
    if (T2) this->ParametricGradient(_Source, _GradientWrtSource, gradient + offset, weight);
  }
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance::ForwardEvent(irtkObservable *obj, irtkEvent event, const void *data)
{
  switch (event) {
    // Prepand name of energy term to log messages
    case LogEvent: {
      string msg = (_Name.empty() ? _ParameterPrefix[0] : _Name) + ": ";
      msg += reinterpret_cast<const char *>(data);
      Broadcast(LogEvent, msg.c_str());
    } break;
    // Forward status messages unmodified
    case StatusEvent: {
      Broadcast(StatusEvent, data);
    } break;
    // Ignore other events
    default: break;
  }
}

// =============================================================================
// Debugging
// =============================================================================

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance::WriteDataSets(const char *p, const char *suffix, bool all) const
{
  const int   sz = 1024;
  char        fname[sz];
  string      _prefix = Prefix(p);
  const char *prefix  = _prefix.c_str();

  _Correspondence->WriteDataSets(prefix, suffix, all);

  if (_Target->Transformation() || all) {
    snprintf(fname, sz, "%starget%s%s", prefix, suffix, _Target->DefaultExtension());
    _Correspondence->DefaultDirection(irtkPointCorrespondence::TargetToSource);
    this->WriteDataSet(fname, _Target, _TargetSample, _Correspondence);
  }
  if (_Source->Transformation() || all) {
    snprintf(fname, sz, "%ssource%s%s", prefix, suffix, _Source->DefaultExtension());
    _Correspondence->DefaultDirection(irtkPointCorrespondence::SourceToTarget);
    this->WriteDataSet(fname, _Source, _SourceSample, _Correspondence);
  }
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance::WriteGradient(const char *p, const char *suffix) const
{
  const int   sz = 1024;
  char        fname[sz];
  string      _prefix = Prefix(p);
  const char *prefix  = _prefix.c_str();

  if (_GradientWrtTarget) {
    snprintf(fname, sz, "%sgradient_wrt_target%s.vtp", prefix, suffix);
    irtkPointSetDistance::WriteGradient(fname, _Target, _GradientWrtTarget, &_TargetSample);
  }
  if (_GradientWrtSource) {
    snprintf(fname, sz, "%sgradient_wrt_source%s.vtp", prefix, suffix);
    irtkPointSetDistance::WriteGradient(fname, _Source, _GradientWrtSource, &_SourceSample);
  }
}

// -----------------------------------------------------------------------------
void irtkPointCorrespondenceDistance
::WriteDataSet(const char *fname, const irtkRegisteredPointSet  *target,
                                  const vector<int>             &sample,
                                  const irtkPointCorrespondence *source) const
{
  const vtkIdType n = (sample.empty() ? static_cast<vtkIdType>(target->NumberOfPoints())
                                      : static_cast<vtkIdType>(sample.size()));

  vtkSmartPointer<vtkPointSet>  output;
  vtkSmartPointer<vtkPoints>    points;
  vtkSmartPointer<vtkCellArray> verts;
  vtkPolyData                  *polydata;

  output = target->PointSet()->NewInstance();
  points = vtkSmartPointer<vtkPoints>::New();
  points->SetNumberOfPoints(n);
  output->SetPoints(points);
  polydata = vtkPolyData::SafeDownCast(output);
  if (polydata) {
    verts = vtkSmartPointer<vtkCellArray>::New();
    verts->Allocate(n);
    polydata->SetVerts(verts);
  }

  if (n == target->NumberOfPoints()) {
    output->ShallowCopy(target->PointSet());
  } else {
    vtkSmartPointer<vtkDataArray> copy;
    vtkPointData *pd = target->PointSet()->GetPointData();
    for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
      vtkDataArray *data = pd->GetArray(i);
      copy.TakeReference(data->NewInstance());
      copy->SetName(data->GetName());
      copy->SetNumberOfComponents(data->GetNumberOfComponents());
      copy->SetNumberOfTuples(n);
      for (vtkIdType k = 0; k < n; ++k) {
        copy->SetTuple(k, sample[k], data);
      }
      int j = output->GetPointData()->AddArray(copy);
      int a = pd->IsArrayAnAttribute(i);
      if (a >= 0) {
        output->GetPointData()->SetActiveAttribute(j, a);
      }
    }
  }

  irtkPoint p1, p2;
  if (source && ((source->Target() == target && source->FromTargetToSource()) ||
                 (source->Source() == target && source->FromSourceToTarget()))) {
    vtkSmartPointer<vtkFloatArray> error;
    vtkSmartPointer<vtkFloatArray> disp;
    vtkSmartPointer<vtkCharArray>  outlier;
    error = vtkSmartPointer<vtkFloatArray>::New();
    error->SetName("error");
    error->SetNumberOfComponents(1);
    error->SetNumberOfTuples(n);
    disp = vtkSmartPointer<vtkFloatArray>::New();
    disp->SetName("difference");
    disp->SetNumberOfComponents(3);
    disp->SetNumberOfTuples(n);
    outlier = vtkSmartPointer<vtkCharArray>::New();
    outlier->SetName("outlier");
    outlier->SetNumberOfComponents(1);
    outlier->SetNumberOfTuples(n);
    for (vtkIdType i = 0; i < n; ++i) {
      irtkPointCorrespondence::GetPoint(p1, target, &sample, i);
      points->SetPoint(i, p1._x, p1._y, p1._z);
      if (verts) verts->InsertNextCell(1, &i);
      if (!source->GetPoint(i, p2)) {
        outlier->SetTuple1(i, 1);
        p2 = p1;
      } else {
        outlier->SetTuple1(i, 0);
      }
      error->SetTuple1(i, _ErrorFunction->Value(p2.SquaredDistance(p1)));
      disp ->SetTuple3(i, p2._x - p1._x, p2._y - p1._y, p2._z - p1._z);
    }
    output->GetPointData()->AddArray(error);
    output->GetPointData()->AddArray(disp);
    output->GetPointData()->AddArray(outlier);
  } else {
    for (vtkIdType i = 0; i < n; ++i) {
      irtkPointCorrespondence::GetPoint(p1, target, &sample, i);
      points->SetPoint(i, p1._x, p1._y, p1._z);
      if (verts) verts->InsertNextCell(1, &i);
    }
  }

  WritePointSet(fname, output);
}

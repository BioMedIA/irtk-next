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

#include <irtkNormalizedIntensityCrossCorrelation.h>

#include <irtkVoxelFunction.h>
#include <irtkScalarFunctionToImage.h>
#include <irtkConvolutionFunction.h>

using namespace irtkConvolutionFunction;

// FIXME: Keeping a copy of the intermediate image margins which temporarily
//        have to be modified by the Include function does produce incorrect
//        results. If it were working as expected, the Exclude function only
//        needed to re-evaluate the LCC sum within the specified region using
//        the current intermediate images _A, _B, and _C (and _S and _T).
#define FAST_EXCLUDE 0


// =============================================================================
// Auxiliary functors
// =============================================================================

namespace irtkNormalizedIntensityCrossCorrelationUtil {

// Types
typedef irtkNormalizedIntensityCrossCorrelation::RealImage RealImage;

// -----------------------------------------------------------------------------
irtkNormalizedIntensityCrossCorrelation::Units ParseUnits(const char *value)
{
  // Start at end of string
  const int len = strlen(value);
  const char *p = value + len;
  // Skip trailing whitespaces
  while (p != value && (*(p-1) == ' ' || *(p-1) == '\t')) --p;
  // Skip lowercase letters
  while (p != value && *(p-1) >= 'a' && *(p-1) <= 'z') --p;
  // Check if value ends with unit specification
  if (strcmp(p, "mm") == 0) {
    return irtkNormalizedIntensityCrossCorrelation::UNITS_MM;
  }
  if (strcmp(p, "vox") == 0 || strcmp(p, "voxel") == 0 || strcmp(p, "voxels") == 0)
  {
    return irtkNormalizedIntensityCrossCorrelation::UNITS_Voxel;
  }
  return irtkNormalizedIntensityCrossCorrelation::UNITS_Default;
}

// -----------------------------------------------------------------------------
// Kernel: Gaussian
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
struct SquareIntensity : irtkVoxelFunction
{
  const irtkBaseImage *_Mask;

  SquareIntensity(const irtkBaseImage *mask) : _Mask(mask) {}

  template <class TImage, class T>
  void operator ()(const TImage &, int idx, T *v)
  {
    if (_Mask->IsForeground(idx)) (*v) *= (*v);
  }

  template <class T>
  void operator ()(int i, int j, int k, int, T *v)
  {
    if (_Mask->IsForeground(i, j, k)) (*v) *= (*v);
  }
};

// -----------------------------------------------------------------------------
struct SubtractSquaredIntensity : irtkVoxelFunction
{
  const irtkBaseImage *_Mask;

  SubtractSquaredIntensity(const irtkBaseImage *mask) : _Mask(mask) {}

  template <class TImage, class T>
  void operator ()(const TImage &, int idx, const T *a, T *b)
  {
    if (_Mask->IsForeground(idx)) (*b) -= (*a) * (*a);
  }

  template <class T>
  void operator ()(int i, int j, int k, int, const T *a, T *b)
  {
    if (_Mask->IsForeground(i, j, k)) (*b) -= (*a) * (*a);
  }
};

// -----------------------------------------------------------------------------
struct TakeSquareRoot : irtkVoxelFunction
{
  const irtkBaseImage *_Mask;

  TakeSquareRoot(const irtkBaseImage *mask) : _Mask(mask) {}

  template <class TImage, class T>
  void operator ()(const TImage &, int idx, T *v)
  {
    if (_Mask->IsForeground(idx)) (*v) = sqrt(*v);
  }

  template <class T>
  void operator ()(int i, int j, int k, int, T *v)
  {
    if (_Mask->IsForeground(i, j, k)) (*v) = sqrt(*v);
  }
};

// -----------------------------------------------------------------------------
struct MultiplyIntensities : irtkVoxelFunction
{
  const irtkBaseImage *_MaskA;
  const irtkBaseImage *_MaskB;

  MultiplyIntensities(const irtkBaseImage *maskA, const irtkBaseImage *maskB)
  :
    _MaskA(maskA), _MaskB(maskB)
  {}

  template <class TImage, class T>
  void operator ()(const TImage &, int idx, const T *a, const T *b, T *c)
  {
    if (_MaskA->IsForeground(idx) && _MaskB->IsForeground(idx)) {
      (*c) = (*a) * (*b);
    } else {
      (*c) = -0.01;
    }
  }

  template <class T>
  void operator ()(int i, int j, int k, int, const T *a, const T *b, T *c)
  {
    if (_MaskA->IsForeground(i, j, k) && _MaskB->IsForeground(i, j, k)) {
      (*c) = (*a) * (*b);
    } else {
      (*c) = -0.01;
    }
  }
};

// -----------------------------------------------------------------------------
struct SubtractProduct : irtkVoxelFunction
{
  const irtkBaseImage *_MaskA;
  const irtkBaseImage *_MaskB;

  SubtractProduct(const irtkBaseImage *maskA, const irtkBaseImage *maskB)
  :
    _MaskA(maskA), _MaskB(maskB)
  {}

  template <class TImage, class T>
  void operator ()(const TImage &, int idx, const T *a, const T *b, T *c)
  {
    if (_MaskA->IsForeground(idx) && _MaskB->IsForeground(idx)) {
      (*c) -= (*a) * (*b);
    } else {
      (*c) = numeric_limits<T>::quiet_NaN();
    }
  }

  template <class T>
  void operator ()(int i, int j, int k, int, const T *a, const T *b, T *c)
  {
    if (_MaskA->IsForeground(i, j, k) && _MaskB->IsForeground(i, j, k)) {
      (*c) -= (*a) * (*b);
    } else {
      (*c) = numeric_limits<T>::quiet_NaN();
    }
  }
};

// -----------------------------------------------------------------------------
struct GenerateLNCCImage : public irtkVoxelFunction
{
  template <class TImage, class T>
  void operator()(const TImage &, int idx, const T *a, const T *b, const T *c, T *cc) const
  {
    if (IsNaN(*a)) {
      (*cc) = -0.01; // i.e., background
    } else {
      if (*b >= .01 && *c >= .01) {
        (*cc) = min(1.0, fabs(*a) / ((*b) * (*c)));
      } else if (*b < .01 && *c < .01) {
        (*cc) = 1.0; // i.e., both regions (approx.) constant-valued
      } else {
        (*cc) = .0;  // i.e., one region (approx.) constant-valued
      }
    }
  }
};

// -----------------------------------------------------------------------------
struct EvaluateLNCC : public irtkVoxelReduction
{
  EvaluateLNCC() : _Sum(.0), _Cnt(0) {}
  EvaluateLNCC(const EvaluateLNCC &o) : _Sum(o._Sum), _Cnt(o._Cnt) {}

  void split(const EvaluateLNCC &rhs)
  {
    _Sum = .0;
    _Cnt = 0;
  }

  void join(const EvaluateLNCC &rhs)
  {
    _Sum += rhs._Sum;
    _Cnt += rhs._Cnt;
  }

  template <class TImage, class T>
  void operator()(const TImage &, int, const T *a, const T *b, const T *c)
  {
    if (!IsNaN(*a)) {
      if (*b >= .01 && *c >= .01) {
        _Sum += min(1.0, fabs(*a) / ((*b) * (*c)));
      } else if (*b < .01 && *c < .01) {
        _Sum += 1.0; // i.e., both regions (approx.) constant-valued
      }
      ++_Cnt;
    }
  }

  template <class T>
  void operator()(int, int, int, int, const T *a, const T *b, const T *c)
  {
    if (!IsNaN(*a)) {
      if (*b >= .01 && *c >= .01) {
        _Sum += min(1.0, fabs(*a) / ((*b) * (*c)));
      } else if (*b < .01 && *c < .01) {
        _Sum += 1.0; // i.e., both regions (approx.) constant-valued
      }
      ++_Cnt;
    }
  }

  double Sum  () const { return _Sum; }
  int    Num  () const { return _Cnt; }
  double Value() const { return (_Cnt ? _Sum / _Cnt : 1.0); }

private:
  double _Sum;
  int    _Cnt;
};

// -----------------------------------------------------------------------------
struct EvaluateLNCCGradient : public irtkVoxelFunction
{
  template <class TImage, class T>
  void operator()(const TImage &, int idx, const T *a, const T *b, const T *c, const T *s, const T *t, T *g1, T *g2, T *g3)
  {
    if (!IsNaN(*a) && *b >= .01 && *c >= .01) {
      const T bc   = (*b) * (*c);
      const T bbbc = (*b) * (*b) * bc;
      (*g1) = 1.0 / bc;
      (*g2) = (*a) / bbbc;
      (*g3) = ((*a) * (*s)) / bbbc - (*t) / bc;
      if ((*a) < .0) {
        (*g1) = - (*g1);
        (*g2) = - (*g2);
        (*g3) = - (*g3);
      }
    } else {
      (*g1) = (*g2) = (*g3) = .0;
    }
  }

  template <class TImage, class T1, class T2>
  void operator()(const TImage &, int, const T1 *tgt, const T1 *src, const T2 *g1, const T2 *g2, const T2 *g3, T2 *g)
  {
    (*g) = (*g1) * (*tgt) - (*g2) * (*src) + (*g3);
  }
};

// -----------------------------------------------------------------------------
// Kernel: Box window
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
template <class VoxelType>
struct UpdateBoxWindowLNCC : public irtkVoxelFunction
{
  irtkNormalizedIntensityCrossCorrelation *_This;

  // ---------------------------------------------------------------------------
  UpdateBoxWindowLNCC(irtkNormalizedIntensityCrossCorrelation *_this)
  :
    _This(_this)
  {}

  // ---------------------------------------------------------------------------
  void operator()(int i, int j, int k, int, const VoxelType *tgt, const VoxelType *src,
                  VoxelType *a, VoxelType *b, VoxelType *c, VoxelType *s, VoxelType *t)
  {
    int    cnt  =  0;
    double sumt = .0, sums = .0, sumss = .0, sumts = .0, sumtt = .0;

    if (_This->IsForeground(i, j, k)) {
      const irtkVector3D<int> &radius = _This->NeighborhoodRadius();
      irtkGenericImageIterator<VoxelType> _TargetNeighborhood(_This->Target());
      irtkGenericImageIterator<VoxelType> _SourceNeighborhood(_This->Source());
      _TargetNeighborhood.SetNeighborhood(i, j, k, radius._x, radius._y, radius._z);
      _SourceNeighborhood.SetNeighborhood(i, j, k, radius._x, radius._y, radius._z);
      _TargetNeighborhood.GoToBegin();
      _SourceNeighborhood.GoToBegin();
      while (!_TargetNeighborhood.IsAtEnd()) {
        sums  += _SourceNeighborhood.Value();
        sumt  += _TargetNeighborhood.Value();
        sumss += _SourceNeighborhood.Value() * _SourceNeighborhood.Value();
        sumts += _TargetNeighborhood.Value() * _SourceNeighborhood.Value();
        sumtt += _TargetNeighborhood.Value() * _TargetNeighborhood.Value();
        ++_TargetNeighborhood;
        ++_SourceNeighborhood;
        ++cnt;
      }
    }

    if (cnt) {
      const double ms = sums / cnt;
      const double mt = sumt / cnt;
      *a = voxel_cast<VoxelType>(sumts - ms * sumt - mt * sums + cnt * ms * mt); // <T, S>
      *b = voxel_cast<VoxelType>(sumss -       2.0 * ms * sums + cnt * ms * ms); // <S, S>
      *c = voxel_cast<VoxelType>(sumtt -       2.0 * mt * sumt + cnt * mt * mt); // <T, T>
      *s = voxel_cast<VoxelType>((*src) - ms);
      *t = voxel_cast<VoxelType>((*tgt) - mt);
    } else {
      *a = *b = *c = *s = *t = voxel_cast<VoxelType>(.0);
    }
  }
};

// -----------------------------------------------------------------------------
struct EvaluateBoxWindowLNCC : public irtkVoxelReduction
{
  EvaluateBoxWindowLNCC() : _Sum(.0), _Cnt(0) {}
  EvaluateBoxWindowLNCC(const EvaluateBoxWindowLNCC &o) : _Sum(o._Sum), _Cnt(o._Cnt) {}

  void split(const EvaluateBoxWindowLNCC &rhs)
  {
    _Sum = .0;
    _Cnt = 0;
  }

  void join(const EvaluateBoxWindowLNCC &rhs)
  {
    _Sum += rhs._Sum;
    _Cnt += rhs._Cnt;
  }

  template <class TImage, class T>
  void operator()(const TImage &, int, const T *a, const T *b, const T *c)
  {
    double bc = static_cast<double>(*b) * static_cast<double>(*c);
    double cc = static_cast<double>(*a) * static_cast<double>(*a) / bc;
    if (!IsNaN(cc) && fabs(cc) <= 1.0) {
      _Sum += cc;
      ++_Cnt;
    }
  }

  template <class T>
  void operator()(int, int, int, int, const T *a, const T *b, const T *c)
  {
    double bc = static_cast<double>(*b) * static_cast<double>(*c);
    double cc = static_cast<double>(*a) * static_cast<double>(*a) / bc;
    if (!IsNaN(cc) && fabs(cc) <= 1.0) {
      _Sum += cc;
      ++_Cnt;
    }
  }

  template <class TImage, class T>
  void operator()(const TImage &, int, const T *a, const T *b, const T *c, double *cc)
  {
    double bc = static_cast<double>(*b) * static_cast<double>(*c);
    (*cc) = static_cast<double>(*a) * static_cast<double>(*a) / bc;
    if (!IsNaN(*cc) && fabs(*cc) <= 1.0) {
      _Sum += (*cc);
      ++_Cnt;
    } else {
      (*cc) = .0;
    }
  }

  template <class T>
  void operator()(int, int, int, int, const T *a, const T *b, const T *c, double *cc)
  {
    double bc = static_cast<double>(*b) * static_cast<double>(*c);
    (*cc) = static_cast<double>(*a) * static_cast<double>(*a) / bc;
    if (!IsNaN(*cc) && fabs(*cc) <= 1.0) {
      _Sum += (*cc);
      ++_Cnt;
    } else {
      (*cc) = .0;
    }
  }

  double Sum  () const { return _Sum; }
  int    Num  () const { return _Cnt; }
  double Value() const { return (_Cnt ? _Sum / _Cnt : 1.0); }

private:
  double _Sum;
  int    _Cnt;
};

// -----------------------------------------------------------------------------
struct EvaluateBoxWindowLNCCGradient : public irtkVoxelFunction
{
  template <class TImage, class T>
  void operator()(const TImage &, int, const T *a, const T *b, const T *c, const T *s, const T *t, T *g)
  {
    (*g) = 2.0 * ((*a) / ((*b) * (*c))) * ((*t) - ((*a) / (*b)) * (*s));
    if (IsNaN(*g) || IsInf(*g)) (*g) = .0;
  }
};

// -----------------------------------------------------------------------------
blocked_range3d<int> ExtendedRegion(const blocked_range3d<int> &region,
                                    const irtkImageAttributes  &domain,
                                    const irtkVector3D<int>    &radius)
{
  return blocked_range3d<int>(max(0,         region.pages().begin() - radius._z),
                              min(domain._z, region.pages().end  () + radius._z),
                              max(0,         region.rows ().begin() - radius._y),
                              min(domain._y, region.rows ().end  () + radius._y),
                              max(0,         region.cols ().begin() - radius._x),
                              min(domain._x, region.cols ().end  () + radius._x));
}

// -----------------------------------------------------------------------------
bool operator ==(const blocked_range3d<int> a, const blocked_range3d<int> &b)
{
  return a.pages().begin() == b.pages().begin() && a.pages().end() == b.pages().end() &&
         a.cols ().begin() == b.cols ().begin() && a.cols ().end() == b.cols ().end() &&
         a.rows ().begin() == b.rows ().begin() && a.rows ().end() == b.rows ().end();
}

// -----------------------------------------------------------------------------
void GetMargin(const RealImage            *in,
               const blocked_range3d<int> &region1,
               const blocked_range3d<int> &region2,
               RealImage                  &out)
{
  out.Initialize(region2.cols ().end() - region2.cols ().begin(),
                 region2.rows ().end() - region2.rows ().begin(),
                 region2.pages().end() - region2.pages().begin());
  for (int k1 = region2.pages().begin(), k2 = 0; k1 < region2.pages().end(); ++k1, ++k2) {
  for (int j1 = region2.rows ().begin(), j2 = 0; j1 < region2.rows ().end(); ++j1, ++j2) {
  for (int i1 = region2.cols ().begin(), i2 = 0; i1 < region2.cols ().end(); ++i1, ++i2) {
    if (i1 < region1.cols ().begin() || i1 >= region1.cols ().end() ||
        j1 < region1.rows ().begin() || j1 >= region1.rows ().end() ||
        k1 < region1.pages().begin() || k1 >= region1.pages().end()) {
      out(i2, j2, k2) = in->Get(i1, j1, k1);
    }
  }
  }
  }
}

// -----------------------------------------------------------------------------
void PutMargin(RealImage                  *out,
               const blocked_range3d<int> &region1,
               const blocked_range3d<int> &region2,
               const RealImage            &in)
{
  for (int k1 = region2.pages().begin(), k2 = 0; k1 < region2.pages().end(); ++k1, ++k2) {
  for (int j1 = region2.rows ().begin(), j2 = 0; j1 < region2.rows ().end(); ++j1, ++j2) {
  for (int i1 = region2.cols ().begin(), i2 = 0; i1 < region2.cols ().end(); ++i1, ++i2) {
    if (i1 < region1.cols ().begin() || i1 >= region1.cols ().end() ||
        j1 < region1.rows ().begin() || j1 >= region1.rows ().end() ||
        k1 < region1.pages().begin() || k1 >= region1.pages().end()) {
      out->Put(i1, j1, k1, in(i2, j2, k2));
    }
  }
  }
  }
}


} // namespace irtkNormalizedIntensityCrossCorrelationUtil
using namespace irtkNormalizedIntensityCrossCorrelationUtil;

// =============================================================================
// Construction/Destruction
// =============================================================================

// -----------------------------------------------------------------------------
irtkNormalizedIntensityCrossCorrelation
::irtkNormalizedIntensityCrossCorrelation(const char *name)
:
  irtkImageSimilarity(name),
  _KernelType(GaussianKernel),
  _KernelX(NULL), _KernelY(NULL), _KernelZ(NULL),
  _A(NULL), _B(NULL), _C(NULL), _S(NULL), _T(NULL),
  _NeighborhoodSize(-21.0), // 21 vox FWTM / 5 vox StdDev
  _NeighborhoodRadius(0)
{
  if (version < irtkVersion(3, 1)) this->Weight(-1.0);
}

// -----------------------------------------------------------------------------
irtkNormalizedIntensityCrossCorrelation
::irtkNormalizedIntensityCrossCorrelation(const irtkNormalizedIntensityCrossCorrelation &other)
:
  irtkImageSimilarity(other),
  _KernelType(other._KernelType),
  _KernelX(other._KernelX ? new RealImage(*other._KernelX) : NULL),
  _KernelY(other._KernelY ? new RealImage(*other._KernelY) : NULL),
  _KernelZ(other._KernelZ ? new RealImage(*other._KernelZ) : NULL),
  _A      (other._A       ? new RealImage(*other._A      ) : NULL),
  _B      (other._B       ? new RealImage(*other._B      ) : NULL),
  _C      (other._C       ? new RealImage(*other._C      ) : NULL),
  _S      (other._S       ? new RealImage(*other._S      ) : NULL),
  _T      (other._T       ? new RealImage(*other._T      ) : NULL),
  _NeighborhoodSize  (other._NeighborhoodSize),
  _NeighborhoodRadius(other._NeighborhoodRadius)
{
}

// -----------------------------------------------------------------------------
irtkNormalizedIntensityCrossCorrelation::~irtkNormalizedIntensityCrossCorrelation()
{
  ClearKernel();
  Delete(_A);
  Delete(_B);
  Delete(_C);
  Delete(_S);
  Delete(_T);
}

// =============================================================================
// Parameters
// =============================================================================

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::SetKernelToBoxWindow(double rx, double ry, double rz, Units units)
{
  if (rx < .0) rx = .0;
  if (ry < .0 && rz < .0) ry = rz = rx;
  _KernelType          = BoxWindow;
  _NeighborhoodSize._x = 2.0 * rx;
  _NeighborhoodSize._y = 2.0 * ry;
  _NeighborhoodSize._z = 2.0 * rz;
  if (units == UNITS_Voxel) {
    _NeighborhoodSize._x += 1.0, _NeighborhoodSize._x = -_NeighborhoodSize._x;
    _NeighborhoodSize._y += 1.0, _NeighborhoodSize._y = -_NeighborhoodSize._y;
    _NeighborhoodSize._z += 1.0, _NeighborhoodSize._z = -_NeighborhoodSize._z;
  }
}

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::SetKernelToGaussian(double sx, double sy, double sz, Units units)
{
  if (sx < .0) sx = .0;
  if (sy < .0 && sz < .0) sy = sz = sx;
  _KernelType          = GaussianKernel;
  _NeighborhoodSize._x = 4.29193 * sx; // StdDev -> FWTM
  _NeighborhoodSize._y = 4.29193 * sy;
  _NeighborhoodSize._z = 4.29193 * sz;
  if (units == UNITS_Voxel) {
    _NeighborhoodSize._x = -_NeighborhoodSize._x;
    _NeighborhoodSize._y = -_NeighborhoodSize._y;
    _NeighborhoodSize._z = -_NeighborhoodSize._z;
  }
}

// -----------------------------------------------------------------------------
bool irtkNormalizedIntensityCrossCorrelation::Set(const char *name, const char *value)
{
  if (strncmp(name, "Local window radius", 19) == 0) {

    if (strstr(name + 19, "[gauss]")    != NULL ||
        strstr(name + 19, "[gaussian]") != NULL ||
        strstr(name + 19, "[sigma]")    != NULL ||
        strstr(name + 19, "[stddev]")   != NULL ||
        strstr(name + 19, "[StdDev]")   != NULL) {
      double sx = 0, sy = 0, sz = 0;
      int n = sscanf(value, "%lf %lf %lf", &sx, &sy, &sz);
      if (n == 0) return false;
      if (n == 1) sz = sy = sx;
      Units units = ParseUnits(value);
      if (units != UNITS_Default) {
        if (sx < 0 || sy < 0 || sz < 0) return false;
        if (units == UNITS_Voxel) sx = -sx, sy = -sy, sz = -sz;
      }
      _KernelType          = GaussianKernel;
      _NeighborhoodSize._x = 4.29193 * sx; // StdDev -> FWTM
      _NeighborhoodSize._y = 4.29193 * sy;
      _NeighborhoodSize._z = 4.29193 * sz;
      return true;
    } else if (strstr(name + 19, "[FWHM]") != NULL ||
               strstr(name + 19, "[fwhm]") != NULL ||
               strstr(name + 19, "[FWTM]") != NULL ||
               strstr(name + 19, "[fwtm]") != NULL) {
      return false; // makes only sense for "Local window size"
    } else if (strstr(name + 19, "[voxels]") != NULL) {
      int rx = 0, ry = 0, rz = 0;
      int n = sscanf(value, "%d %d %d", &rx, &ry, &rz);
      if (n == 0) return false;
      if (n == 1) rz = ry = rx;
      Units units = ParseUnits(value);
      if (units == UNITS_MM || rx < 0 || ry < 0 || rz < 0) return false;
      _KernelType          = BoxWindow;
      _NeighborhoodSize._x = -(2.0 * rx + 1.0);
      _NeighborhoodSize._y = -(2.0 * ry + 1.0);
      _NeighborhoodSize._z = -(2.0 * rz + 1.0);
      return true;
    } else if (strstr(name + 19, "[mm]") != NULL) {
      double rx = .0, ry = .0, rz = .0;
      int n = sscanf(value, "%lf %lf %lf", &rx, &ry, &rz);
      if (n == 0) return false;
      if (n == 1) rz = ry = rx;
      Units units = ParseUnits(value);
      if (units == UNITS_Voxel || rx < .0 || ry < .0 || rz < .0) return false;
      _KernelType          = BoxWindow;
      _NeighborhoodSize._x = 2.0 * rx;
      _NeighborhoodSize._y = 2.0 * ry;
      _NeighborhoodSize._z = 2.0 * rz;
      return true;
    } else if (name[19] == '\0' || strstr(name + 19, "[box]") != NULL) {
      double rx = .0, ry = .0, rz = .0;
      int n = sscanf(value, "%lf %lf %lf", &rx, &ry, &rz);
      if (n == 0) return false;
      if (n == 1) rz = ry = rx;
      Units units = ParseUnits(value);
      if (units != UNITS_Default) {
        if (rx < 0 || ry < 0 || rz < 0) return false;
        if (units == UNITS_Voxel) rx = -rx, ry = -ry, rz = -rz;
      }
      _KernelType          = BoxWindow;
      _NeighborhoodSize._x = 2.0 * rx + ((rx < .0) ? -1 : 0);
      _NeighborhoodSize._y = 2.0 * ry + ((ry < .0) ? -1 : 0);
      _NeighborhoodSize._z = 2.0 * rz + ((rz < .0) ? -1 : 0);
      return true;
    }
    return false;

  } else if (strncmp(name, "Local window size", 17) == 0) {

    if (strstr(name + 19, "[gauss]")    != NULL ||
        strstr(name + 19, "[gaussian]") != NULL ||
        strstr(name + 17, "[sigma]")    != NULL ||
        strstr(name + 17, "[stddev]")   != NULL ||
        strstr(name + 17, "[StdDev]")   != NULL) {
      double sx = 0, sy = 0, sz = 0;
      int n = sscanf(value, "%lf %lf %lf", &sx, &sy, &sz);
      if (n == 0) return false;
      if (n == 1) sz = sy = sx;
      Units units = ParseUnits(value);
      if (units != UNITS_Default) {
        if (sx < 0 || sy < 0 || sz < 0) return false;
        if (units == UNITS_Voxel) sx = -sx, sy = -sy, sz = -sz;
      }
      _KernelType          = GaussianKernel;
      _NeighborhoodSize._x = 4.29193 * sx; // StdDev -> FWTM
      _NeighborhoodSize._y = 4.29193 * sy;
      _NeighborhoodSize._z = 4.29193 * sz;
      return true;
    } else if (strstr(name + 17, "[FWHM]") != NULL ||
               strstr(name + 17, "[fwhm]") != NULL) {
      double sx = 0, sy = 0, sz = 0;
      int n = sscanf(value, "%lf %lf %lf", &sx, &sy, &sz);
      if (n == 0) return false;
      if (n == 1) sz = sy = sx;
      Units units = ParseUnits(value);
      if (units != UNITS_Default) {
        if (sx < 0 || sy < 0 || sz < 0) return false;
        if (units == UNITS_Voxel) sx = -sx, sy = -sy, sz = -sz;
      }
      _KernelType          = GaussianKernel;
      _NeighborhoodSize._x = 1.82262 * sx; // FWHM -> FWTM
      _NeighborhoodSize._y = 1.82262 * sy;
      _NeighborhoodSize._z = 1.82262 * sz;
      return true;
    } else if (strstr(name + 17, "[FWTM]") != NULL ||
               strstr(name + 17, "[fwtm]") != NULL) {
      double sx = 0, sy = 0, sz = 0;
      int n = sscanf(value, "%lf %lf %lf", &sx, &sy, &sz);
      if (n == 0) return false;
      if (n == 1) sz = sy = sx;
      Units units = ParseUnits(value);
      if (units != UNITS_Default) {
        if (sx < 0 || sy < 0 || sz < 0) return false;
        if (units == UNITS_Voxel) sx = -sx, sy = -sy, sz = -sz;
      }
      _KernelType          = GaussianKernel;
      _NeighborhoodSize._x = sx;
      _NeighborhoodSize._y = sy;
      _NeighborhoodSize._z = sz;
      return true;
    } else if (strstr(name + 17, "[voxels]") != NULL) {
      int sx = 0, sy = 0, sz = 0;
      int n = sscanf(value, "%d %d %d", &sx, &sy, &sz);
      if (n == 0) return false;
      if (n == 1) sz = sy = sx;
      Units units = ParseUnits(value);
      if (units == UNITS_MM || sx < 0 || sy < 0 || sz < 0) return false;
      _KernelType          = BoxWindow;
      _NeighborhoodSize._x = -sx;
      _NeighborhoodSize._y = -sy;
      _NeighborhoodSize._z = -sz;
      return true;
    } else if (strstr(name + 17, "[mm]") != NULL) {
      double sx = .0, sy = .0, sz = .0;
      int n = sscanf(value, "%lf %lf %lf", &sx, &sy, &sz);
      if (n == 0) return false;
      if (n == 1) sz = sy = sx;
      Units units = ParseUnits(value);
      if (units == UNITS_Voxel || sx < 0 || sy < 0 || sz < 0) return false;
      _KernelType          = BoxWindow;
      _NeighborhoodSize._x = sx;
      _NeighborhoodSize._y = sy;
      _NeighborhoodSize._z = sz;
      return true;
    } else if (name[17] == '\0' || strstr(name + 17, "[box]") != NULL) {
      double sx = .0, sy = .0, sz = .0;
      int n = sscanf(value, "%lf %lf %lf", &sx, &sy, &sz);
      if (n == 0) return false;
      if (n == 1) sz = sy = sx;
      Units units = ParseUnits(value);
      if (units != UNITS_Default) {
        if (sx < 0 || sy < 0 || sz < 0) return false;
        if (units == UNITS_Voxel) sx = -sx, sy = -sy, sz = -sz;
      }
      _KernelType          = BoxWindow;
      _NeighborhoodSize._x = sx;
      _NeighborhoodSize._y = sy;
      _NeighborhoodSize._z = sz;
      return true;
    }
    return false;

  }
  return irtkImageSimilarity::Set(name, value);
}

// -----------------------------------------------------------------------------
irtkParameterList irtkNormalizedIntensityCrossCorrelation::Parameter() const
{
  irtkParameterList params = irtkImageSimilarity::Parameter();
  string name = "Local window size [";
  switch (_KernelType) {
    case BoxWindow:      name += "box";
    case GaussianKernel: name += "FWTM";
    default:             name += "custom";
  }
  name += "]";
  string value = ToString(fabs(_NeighborhoodSize._x)) + " "
               + ToString(fabs(_NeighborhoodSize._y)) + " "
               + ToString(fabs(_NeighborhoodSize._z));
  if (_NeighborhoodSize._x < .0) value += " vox";
  Insert(params, name, value);
  return params;
}

// =============================================================================
// Initialization/Update
// =============================================================================

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::ClearKernel()
{
  Delete(_KernelX);
  Delete(_KernelY);
  Delete(_KernelZ);
}

// -----------------------------------------------------------------------------
irtkNormalizedIntensityCrossCorrelation::RealImage *
irtkNormalizedIntensityCrossCorrelation::CreateGaussianKernel(double sigma)
{
  // Ignore sign of standard deviation parameter (negative --> voxel units)
  sigma = fabs(sigma);

  // Create scalar function which corresponds to a 1D Gaussian function
  irtkScalarGaussian func(sigma, 1, 1, 0, 0, 0);

  // Create filter kernel for 1D Gaussian function
  const int  size   = 2 * static_cast<int>(3.0 * sigma) + 1;
  RealImage *kernel = new RealImage(size, 1, 1);

  // Sample scalar function at discrete kernel positions
  irtkScalarFunctionToImage<RealType> sampler;
  sampler.SetInput (&func);
  sampler.SetOutput(kernel);
  sampler.Run();

  return kernel;
}

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation
::ComputeWeightedAverage(const blocked_range3d<int> &region, RealImage *image)
{
  // Average along x axis
  ConvolveTruncatedForegroundInX<RealType> convX(image, _KernelX->Data(), _KernelX->X());
  ParallelForEachVoxel(region, image, &_Temp, convX);

  // Average along y axis
  ConvolveTruncatedForegroundInY<RealType> convY(image, _KernelY->Data(), _KernelY->X());
  ParallelForEachVoxel(region, &_Temp, image, convY);

  // Average along z axis
  if (_KernelZ) {
    ConvolveTruncatedForegroundInZ<RealType> convZ(image, _KernelZ->Data(), _KernelZ->X());
    ParallelForEachVoxel(region, image, &_Temp, convZ);
    ParallelForEachVoxel(irtkBinaryVoxelFunction::Copy(), region, &_Temp, image);
  }
}

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation
::ComputeStatistics(const blocked_range3d<int> &region,
                    const irtkRegisteredImage *image, RealImage *mean, RealImage *sigma)
{
  using irtkBinaryVoxelFunction::Copy;

  // Extended region including margin needed for convolution
  const blocked_range3d<int> region_plus_margin
      = ExtendedRegion(region, _Domain, _NeighborhoodRadius);

  // Compute local mean
  ParallelForEachVoxel(Copy(), region_plus_margin, image, mean);
  ComputeWeightedAverage(region, mean);

  // Compute local variance, sigma^2 = G*(I^2) - (G*I)^2
  ParallelForEachVoxel(Copy(),                 region_plus_margin, image, sigma);
  ParallelForEachVoxel(SquareIntensity(image), region_plus_margin,        sigma);
  ComputeWeightedAverage(region, sigma);
  ParallelForEachVoxel(SubtractSquaredIntensity(image), region, mean, sigma);

  // Compute local standard deviation
  ParallelForEachVoxel(TakeSquareRoot(image), region, sigma);
}

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::Initialize()
{
  IRTK_START_TIMING();

  // Clear previous kernel
  ClearKernel();

  // Initialize base class
  irtkImageSimilarity::Initialize();

  // Rescale interpolated intensities to [0, 1], bg = -1
  _Target->MinIntensity(0.0);
  _Target->MaxIntensity(1.0);
  _Target->PutBackgroundValueAsDouble(-.01);

  _Source->MinIntensity(0.0);
  _Source->MaxIntensity(1.0);
  _Source->PutBackgroundValueAsDouble(-.01);

  // Initialize intermediate memory
  const irtkImageAttributes &attr = _Target->Attributes();

  if (!_A) _A = new RealImage;
  if (!_B) _B = new RealImage;
  if (!_C) _C = new RealImage;
  if (!_S) _S = new RealImage;
  if (!_T) _T = new RealImage;

  _A->Initialize(attr, 1);
  _B->Initialize(attr, 1);
  _C->Initialize(attr, 1);
  _S->Initialize(attr, 1);
  _T->Initialize(attr, 1);

  if (_KernelType == BoxWindow) {

    // Initialize local neighborhood radius given window size in voxels
    if (_NeighborhoodSize._x < .0) _NeighborhoodRadius._x = round((-_NeighborhoodSize._x - 1) / 2.0);
    if (_NeighborhoodSize._y < .0) _NeighborhoodRadius._y = round((-_NeighborhoodSize._y - 1) / 2.0);
    if (_NeighborhoodSize._z < .0) _NeighborhoodRadius._z = round((-_NeighborhoodSize._z - 1) / 2.0);

    // Initialize local neighborhood radius given window size in mm
    if (_NeighborhoodSize._x > .0) _NeighborhoodRadius._x = round(_NeighborhoodSize._x / (2.0 * attr._dx));
    if (_NeighborhoodSize._y > .0) _NeighborhoodRadius._y = round(_NeighborhoodSize._y / (2.0 * attr._dy));
    if (_NeighborhoodSize._z > .0) _NeighborhoodRadius._z = round(_NeighborhoodSize._z / (2.0 * attr._dz));

  } else {

    _Temp.Initialize(attr, 1);

    // FWTM -> StdDev in voxel units
    double sigmaX = _NeighborhoodSize._x / 4.29193;
    double sigmaY = _NeighborhoodSize._y / 4.29193;
    double sigmaZ = _NeighborhoodSize._z / 4.29193;
    if (sigmaX > .0) sigmaX /= attr._dx;
    if (sigmaY > .0) sigmaY /= attr._dy;
    if (sigmaZ > .0) sigmaZ /= attr._dz;
    if (attr._z == 1) sigmaZ = .0;

    // Initialize local neighborhood kernel
    if (sigmaX != .0) _KernelX = CreateGaussianKernel(sigmaX);
    if (sigmaY != .0) _KernelY = CreateGaussianKernel(sigmaY);
    if (sigmaZ != .0) _KernelZ = CreateGaussianKernel(sigmaZ);
    _NeighborhoodRadius._x = (_KernelX ? (_KernelX->X() - 1) / 2 : 0);
    _NeighborhoodRadius._y = (_KernelY ? (_KernelY->X() - 1) / 2 : 0);
    _NeighborhoodRadius._z = (_KernelZ ? (_KernelZ->X() - 1) / 2 : 0);
  }

  IRTK_DEBUG_TIMING(2, "initialization of LNCC");
}

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::Update(bool gradient)
{
  IRTK_START_TIMING();

  // Registered image domain
  blocked_range3d<int> domain(0, _Domain._z, 0, _Domain._y, 0, _Domain._x);

  // Update images and compute mean and variance of fixed image(s)
  if (_InitialUpdate && _KernelType != BoxWindow) {
    irtkImageSimilarity::Update(gradient);
    if (!Target()->Transformation()) ComputeStatistics(domain, _Target, _T, _C);
    if (!Source()->Transformation()) ComputeStatistics(domain, _Source, _S, _B);
  } else {
    irtkImageSimilarity::Update(gradient);
  }

  // Update inner product images similar to ANTs
  if (_KernelType == BoxWindow) {

    // Compute dot products
    UpdateBoxWindowLNCC<RealType> update(this);
    ParallelForEachVoxel(domain, _Target, _Source, _A, _B, _C, _S, _T, update);
    // Evaluate LNCC value
    EvaluateBoxWindowLNCC cc;
    ParallelForEachVoxel(domain, _A, _B, _C, cc);
    _Sum = cc.Sum(), _N = cc.Num();

  // Update intermediate images similar to NiftyReg
  } else {

    // Compute mean and standard deviation of moving image(s)
    if (Target()->Transformation()) ComputeStatistics(domain, _Target, _T, _C);
    if (Source()->Transformation()) ComputeStatistics(domain, _Source, _S, _B);
    // Compute local correlation of input images, G*(I1 I2) - (G*I1) (G*I2)
    ParallelForEachVoxel(MultiplyIntensities(_Target, _Source), domain, _Target, _Source, _A);
    _A->PutBackgroundValueAsDouble(-0.01);
    ComputeWeightedAverage(domain, _A);
    ParallelForEachVoxel(SubtractProduct(_Target, _Source), domain, _T, _S, _A);
    _A->PutBackgroundValueAsDouble(numeric_limits<double>::quiet_NaN());
    // Evaluate LNCC value
    EvaluateLNCC cc;
    ParallelForEachVoxel(domain, _A, _B, _C, cc);
    _Sum = cc.Sum(), _N = cc.Num();

  }

  IRTK_DEBUG_TIMING(2, "update of LNCC");
}

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::Exclude(const blocked_range3d<int> &region)
{
  if (_KernelType == BoxWindow) {

    // Subtract LNCC values for specified region
    EvaluateBoxWindowLNCC cc;
    ParallelForEachVoxel(region, _A, _B, _C, cc);
    _Sum -= cc.Sum(), _N -= cc.Num();

  } else {

    #if !FAST_EXCLUDE
    // Extended image region including margin needed for convolution
    blocked_range3d<int> region_plus_margin
        = ExtendedRegion(region, _Domain, _NeighborhoodRadius);
    // Compute mean and standard deviation of moving image(s)
    if (Target()->Transformation()) ComputeStatistics(region, _Target, _T, _C);
    if (Source()->Transformation()) ComputeStatistics(region, _Source, _S, _B);
    // Compute local correlation of input images, G*(I1 I2) - (G*I1) (G*I2)
    ParallelForEachVoxel(MultiplyIntensities(_Target, _Source),
                         region_plus_margin, _Target, _Source, _A);
    _A->PutBackgroundValueAsDouble(-0.01);
    ComputeWeightedAverage(region, _A);
    ParallelForEachVoxel(SubtractProduct(_Target, _Source), region, _T, _S, _A);
    _A->PutBackgroundValueAsDouble(numeric_limits<double>::quiet_NaN());
    #endif
    // Subtract LNCC values for specified region
    EvaluateLNCC cc;
    ParallelForEachVoxel(region, _A, _B, _C, cc);
    _Sum -= cc.Sum(), _N -= cc.Num();

  }
}

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::Include(const blocked_range3d<int> &region)
{
  if (_KernelType == BoxWindow) {

    // Compute dot products
    UpdateBoxWindowLNCC<RealType> update(this);
    ParallelForEachVoxel(region, _Target, _Source, _A, _B, _C, _S, _T, update);
    // Add LNCC values for specified region
    EvaluateBoxWindowLNCC cc;
    ParallelForEachVoxel(region, _A, _B, _C, cc);
    _Sum += cc.Sum(), _N += cc.Num();

  } else {

    // Extended image region including margin needed for convolution
    blocked_range3d<int> region_plus_margin
        = ExtendedRegion(region, _Domain, _NeighborhoodRadius);
    // Copy margin of intermediate images
    #if FAST_EXCLUDE
    RealImage marginA, marginB, marginC, marginS, marginT;
    GetMargin(_A, region, region_plus_margin, marginA);
    GetMargin(_B, region, region_plus_margin, marginB);
    GetMargin(_C, region, region_plus_margin, marginC);
    GetMargin(_S, region, region_plus_margin, marginS);
    GetMargin(_T, region, region_plus_margin, marginT);
    #endif
    // Compute mean and standard deviation of moving image(s)
    if (Target()->Transformation()) ComputeStatistics(region, _Target, _T, _C);
    if (Source()->Transformation()) ComputeStatistics(region, _Source, _S, _B);
    // Compute local correlation of input images, G*(I1 I2) - (G*I1) (G*I2)
    ParallelForEachVoxel(MultiplyIntensities(_Target, _Source),
                         region_plus_margin, _Target, _Source, _A);
    _A->PutBackgroundValueAsDouble(-0.01);
    ComputeWeightedAverage(region, _A);
    ParallelForEachVoxel(SubtractProduct(_Target, _Source), region, _T, _S, _A);
    _A->PutBackgroundValueAsDouble(numeric_limits<double>::quiet_NaN());
    // Restore margin of intermediate images
    #if FAST_EXCLUDE
    PutMargin(_A, region, region_plus_margin, marginA);
    PutMargin(_B, region, region_plus_margin, marginB);
    PutMargin(_C, region, region_plus_margin, marginC);
    PutMargin(_S, region, region_plus_margin, marginS);
    PutMargin(_T, region, region_plus_margin, marginT);
    #endif
    // Add LNCC values for specified region
    EvaluateLNCC cc;
    ParallelForEachVoxel(region, _A, _B, _C, cc);
    _Sum += cc.Sum(), _N += cc.Num();

  }
}

// =============================================================================
// Evaluation
// =============================================================================

// -----------------------------------------------------------------------------
double irtkNormalizedIntensityCrossCorrelation::Evaluate()
{
  const double value = (_N > 0 ? _Sum / _N : 1.0);
  if (version < irtkVersion(3, 1)) return value;
  return 1.0 - value;
}

// -----------------------------------------------------------------------------
double irtkNormalizedIntensityCrossCorrelation::RawValue(double value) const
{
  value = irtkImageSimilarity::RawValue(value);
  if (version >= irtkVersion(3, 1)) value = 1.0 - value;
  return value;
}

// -----------------------------------------------------------------------------
bool irtkNormalizedIntensityCrossCorrelation
::NonParametricGradient(const irtkRegisteredImage *image, GradientImageType *gradient)
{
  // Select normalized fixed (T) and moving (S) images
  const irtkRegisteredImage *tgt = _Target, *src = _Source;
  const RealImage *T = _T, *S = _S, *B = _B, *C = _C;
  if (image == _Target) swap(tgt, src), swap(T, S), swap(B, C);

  // Evaluate gradient of LNCC w.r.t S similar to ANTs
  if (_KernelType == BoxWindow) {

    EvaluateBoxWindowLNCCGradient eval;
    ParallelForEachVoxel(_A, B, C, S, T, gradient, eval);

  // Evaluate gradient of LNCC w.r.t S similar to NiftyReg
  } else {

    RealImage *g1 = new RealImage(image->Attributes());
    RealImage *g2 = new RealImage(image->Attributes());
    RealImage *g3 = new RealImage(image->Attributes());

    EvaluateLNCCGradient eval;
    ParallelForEachVoxel(_A, B, C, S, T, g1, g2, g3, eval);
    blocked_range3d<int> domain(0, _Domain._z, 0, _Domain._y, 0, _Domain._x);
    ComputeWeightedAverage(domain, g1);
    ComputeWeightedAverage(domain, g2);
    ComputeWeightedAverage(domain, g3);
    ParallelForEachVoxel(tgt, src, g1, g2, g3, gradient, eval);

    Delete(g1);
    Delete(g2);
    Delete(g3);

  }

  // Negation of LNCC value
  if (version >= irtkVersion(3, 1)) (*gradient) *= -1.0;

  // Apply chain rule to obtain gradient w.r.t y = T(x)
  MultiplyByImageGradient(image, gradient);
  return true;
}

// =============================================================================
// Debugging
// =============================================================================

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::Print(irtkIndent indent) const
{
  irtkImageSimilarity::Print(indent);

  const char *kernel_type;
  switch (_KernelType) {
    case BoxWindow:      kernel_type = "Box window";
    case GaussianKernel: kernel_type = "Gaussian";
    default:             kernel_type = "Custom";
  }
  cout << indent << "Neighborhood kernel:  " << kernel_type << endl;
  cout << indent << "Neighborhood size:    " << fabs(_NeighborhoodSize._x)
                                    << " x " << fabs(_NeighborhoodSize._y)
                                    << " x " << fabs(_NeighborhoodSize._z)
                                    << (_NeighborhoodSize._x < .0 ? " mm" : " vox")
                                    << endl;
  if (_A /* i.e., initialized */) {
    cout << indent << "  --> Kernel size:    " << (2*_NeighborhoodRadius._x+1)
                                      << " x " << (2*_NeighborhoodRadius._y+1)
                                      << " x " << (2*_NeighborhoodRadius._z+1)
                                      << " vox" << endl;
  }
}

// -----------------------------------------------------------------------------
void irtkNormalizedIntensityCrossCorrelation::WriteDataSets(const char *p, const char *suffix, bool all) const
{
  irtkImageSimilarity::WriteDataSets(p, suffix, all);

  const int   sz = 1024;
  char        fname[sz];
  string      _prefix = Prefix(p);
  const char *prefix  = _prefix.c_str();

#ifdef HAS_NIFTI
  if (_Target->Transformation() || all) {
    snprintf(fname, sz, "%starget_mean%s.nii.gz", prefix, suffix);
    _T->Write(fname);
    snprintf(fname, sz, "%starget_sdev%s.nii.gz", prefix, suffix);
    _C->Write(fname);
  }
  if (_Source->Transformation()) {
    snprintf(fname, sz, "%ssource_mean%s.nii.gz", prefix, suffix);
    _S->Write(fname);
    snprintf(fname, sz, "%ssource_sdev%s.nii.gz", prefix, suffix);
    _B->Write(fname);
  }
  if (_KernelType == BoxWindow) {
    snprintf(fname, sz, "%sa%s.nii.gz", prefix, suffix);
    _A->Write(fname);
  } else {
    snprintf(fname, sz, "%svalue%s.nii.gz", prefix, suffix);
    GenerateLNCCImage eval;
    ParallelForEachVoxel(_A, _B, _C, &_Temp, eval);
    _Temp.Write(fname);
  }
#endif
}

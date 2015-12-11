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

#include <irtkTransformation.h>

// =============================================================================
// Construction/Destruction
// =============================================================================

// -----------------------------------------------------------------------------
void irtkAffineTransformation::UpdateShearingTangent()
{
  const double deg2rad = M_PI / 180.0;
  _tansxy = tan(_Param[SXY] * deg2rad);
  _tansxz = tan(_Param[SXZ] * deg2rad);
  _tansyz = tan(_Param[SYZ] * deg2rad);
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::irtkAffineTransformation(int ndofs)
:
  irtkSimilarityTransformation(ndofs)
{
  _Param[SY] = _Param[SZ] = _Param[SX]; // SX set by irtkSimilarityTransformation
  UpdateShearingTangent();
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::irtkAffineTransformation(const irtkRigidTransformation &t, int ndofs)
:
  irtkSimilarityTransformation(t, ndofs)
{
  _Param[SY] = _Param[SZ] = _Param[SX]; // SX set by irtkSimilarityTransformation
  UpdateShearingTangent();
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::irtkAffineTransformation(const irtkSimilarityTransformation &t, int ndofs)
:
  irtkSimilarityTransformation(t, ndofs)
{
  _Param[SY] = _Param[SZ] = _Param[SX]; // SX set by irtkSimilarityTransformation
  UpdateShearingTangent();
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::irtkAffineTransformation(const irtkAffineTransformation &t, int ndofs)
:
  irtkSimilarityTransformation(t, ndofs),
  _tansxy(t._tansxy),
  _tansxz(t._tansxz),
  _tansyz(t._tansyz)
{
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::irtkAffineTransformation()
:
  irtkSimilarityTransformation(12)
{
  _Param[SY] = _Param[SZ] = _Param[SX]; // SX set by irtkSimilarityTransformation
  UpdateShearingTangent();
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::irtkAffineTransformation(const irtkRigidTransformation &t)
:
  irtkSimilarityTransformation(t, 12)
{
  _Param[SY] = _Param[SZ] = _Param[SX]; // SX set by irtkSimilarityTransformation
  UpdateShearingTangent();
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::irtkAffineTransformation(const irtkSimilarityTransformation &t)
:
  irtkSimilarityTransformation(t, 12)
{
  _Param[SY] = _Param[SZ] = _Param[SX]; // SX set by irtkSimilarityTransformation
  UpdateShearingTangent();
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::irtkAffineTransformation(const irtkAffineTransformation &t)
:
  irtkSimilarityTransformation(t, 12),
  _tansxy(t._tansxy),
  _tansxz(t._tansxz),
  _tansyz(t._tansyz)
{
}

// -----------------------------------------------------------------------------
irtkAffineTransformation::~irtkAffineTransformation()
{
}

// =============================================================================
// Approximation
// =============================================================================

// -----------------------------------------------------------------------------
void irtkAffineTransformation
::ApproximateDOFs(const double *x,  const double *y,  const double *z, const double *t,
                  const double *dx, const double *dy, const double *dz, int no)
{
  // Solve linear system to find 12 DoFs affine transformation which minimizes
  // the mean squared error if none of the parameters is passive/disabled
#if defined(HAVE_EIGEN) || defined(HAVE_VNL)
  if (this->NumberOfActiveDOFs() == 12) {
    irtkPointSet target(no), source(no);
    for (int n = 0; n < no; ++n) {
      target(n) = irtkPoint(x[n],         y[n],         z[n]);
      source(n) = irtkPoint(x[n] + dx[n], y[n] + dy[n], z[n] + dz[n]);
    }
    this->PutMatrix(ApproximateAffineMatrix(target, source));
    return;
  }
#endif

  // Initialize transformation using closed form solution for rigid parameters
  // if neither of these parameters is passive/disabled
  if (this->AllowTranslations()) {
    if (this->AllowRotations()) {
      // Use temporary rigid transformation instead of subclass implementation
      // irtkRigidTransformation::ApproximateDOFs such that
      // irtkRigidTransformation::UpdateDOFs is used instead of
      // irtkAffineTransformation::UpdateDOFs as the latter may result in
      // non-zero non-rigid parameters due to numerical errors
      irtkRigidTransformation rigid;
      rigid.ApproximateDOFs(x, y, z, t, dx, dy, dz, no);
      this->CopyFrom(&rigid);
      if (this->NumberOfActiveDOFs() == 6) return;
    } else {
      this->Reset(); // Not done by irtkHomogeneousTransformation::ApproximateDOFs
    }
  }

  // Find (active) transformation parameters which minimize the mean squared error
  irtkHomogeneousTransformation::ApproximateDOFs(x, y, z, t, dx, dy, dz, no);
}

// ===========================================================================
// Parameters (non-DoFs)
// ===========================================================================

// ---------------------------------------------------------------------------
bool irtkAffineTransformation::Set(const char *param, const char *value)
{
  bool active;
  // Status of translation parameters
  if (strcmp(param, "Allow translations") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(TX, active ? _Active : _Passive);
    PutStatus(TY, active ? _Active : _Passive);
    PutStatus(TZ, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow translation in X") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(TX, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow translation in Y") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(TY, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow translation in Z") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(TZ, active ? _Active : _Passive);
    return true;
  // Status of rotation parameters
  } else if (strcmp(param, "Allow rotations") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(RX, active ? _Active : _Passive);
    PutStatus(RY, active ? _Active : _Passive);
    PutStatus(RZ, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow rotation in X") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(RX, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow rotation in Y") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(RY, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow rotation in Z") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(RZ, active ? _Active : _Passive);
    return true;
  // Status of scaling parameters
  } else if (strcmp(param, "Allow scaling") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(SX, active ? _Active : _Passive);
    PutStatus(SY, active ? _Active : _Passive);
    PutStatus(SZ, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow scaling in X") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(SX, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow scaling in Y") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(SY, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow scaling in Z") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(SZ, active ? _Active : _Passive);
    return true;
  // Status of shearing parameters
  } else if (strcmp(param, "Allow shearing") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(SXY, active ? _Active : _Passive);
    PutStatus(SXZ, active ? _Active : _Passive);
    PutStatus(SYZ, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow shearing of XY") == 0 ||
             strcmp(param, "Allow shearing of YX") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(SXY, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow shearing of XZ") == 0 ||
             strcmp(param, "Allow shearing of ZX") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(SXZ, active ? _Active : _Passive);
    return true;
  } else if (strcmp(param, "Allow shearing of YZ") == 0 ||
             strcmp(param, "Allow shearing of ZY") == 0) {
    if (!FromString(value, active)) return false;
    PutStatus(SYZ, active ? _Active : _Passive);
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
irtkParameterList irtkAffineTransformation::Parameter() const
{
  irtkParameterList params;
  // Status of translation parameters
  if (GetStatus(TX) == GetStatus(TY) && GetStatus(TY) == GetStatus(TZ)) {
    Insert(params, "Allow translations",     (GetStatus(TX) == _Active ? "Yes" : "No"));
  } else {
    Insert(params, "Allow translation in X", (GetStatus(TX) == _Active ? "Yes" : "No"));
    Insert(params, "Allow translation in Y", (GetStatus(TY) == _Active ? "Yes" : "No"));
    Insert(params, "Allow translation in Z", (GetStatus(TZ) == _Active ? "Yes" : "No"));
  }
  // Status of rotation parameters
  if (GetStatus(RX) == GetStatus(RY) && GetStatus(RY) == GetStatus(RZ)) {
    Insert(params, "Allow rotations",     (GetStatus(RX) == _Active ? "Yes" : "No"));
  } else {
    Insert(params, "Allow rotation in X", (GetStatus(RX) == _Active ? "Yes" : "No"));
    Insert(params, "Allow rotation in Y", (GetStatus(RY) == _Active ? "Yes" : "No"));
    Insert(params, "Allow rotation in Z", (GetStatus(RZ) == _Active ? "Yes" : "No"));
  }
  // Status of scaling parameters
  if (GetStatus(SX) == GetStatus(SY) && GetStatus(SY) == GetStatus(SZ)) {
    Insert(params, "Allow scaling",      (GetStatus(SX) == _Active ? "Yes" : "No"));
  } else {
    Insert(params, "Allow scaling in X", (GetStatus(SX) == _Active ? "Yes" : "No"));
    Insert(params, "Allow scaling in Y", (GetStatus(SY) == _Active ? "Yes" : "No"));
    Insert(params, "Allow scaling in Z", (GetStatus(SZ) == _Active ? "Yes" : "No"));
  }
  // Status of scaling parameters
  if (GetStatus(SXY) == GetStatus(SXZ) && GetStatus(SXZ) == GetStatus(SYZ)) {
    Insert(params, "Allow shearing",       (GetStatus(SXY) == _Active ? "Yes" : "No"));
  } else {
    Insert(params, "Allow shearing of XY", (GetStatus(SXY) == _Active ? "Yes" : "No"));
    Insert(params, "Allow shearing of XZ", (GetStatus(SXZ) == _Active ? "Yes" : "No"));
    Insert(params, "Allow shearing of YZ", (GetStatus(SYZ) == _Active ? "Yes" : "No"));
  }
  return params;
}

// =============================================================================
// Transformation parameters
// =============================================================================

// -----------------------------------------------------------------------------
irtkMatrix irtkAffineTransformation::DOFs2Matrix(const double *param)
{
  irtkMatrix matrix(4, 4);
  const double deg2rad = M_PI / 180.0;
  AffineParametersToMatrix(param[TX], param[TY], param[TZ],
                           param[RX]  * deg2rad,
                           param[RY]  * deg2rad,
                           param[RZ]  * deg2rad,
                           param[SX]  / 100.0,
                           param[SY]  / 100.0,
                           param[SZ]  / 100.0,
                           param[SXY] * deg2rad,
                           param[SXZ] * deg2rad,
                           param[SYZ] * deg2rad,
                           matrix);
  return matrix;
}

// -----------------------------------------------------------------------------
void irtkAffineTransformation::Matrix2DOFs(const irtkMatrix &matrix, double *param)
{
  // Get affine parameters
  MatrixToAffineParameters(matrix, param[TX],  param[TY],  param[TZ],
                                   param[RX],  param[RY],  param[RZ],
                                   param[SX],  param[SY],  param[SZ],
                                   param[SXY], param[SXZ], param[SYZ]);
  
  // Convert angles to degrees
  const double rad2deg = 180.0 / M_PI;
  param[RX]  *= rad2deg;
  param[RY]  *= rad2deg;
  param[RZ]  *= rad2deg;
  param[SXY] *= rad2deg;
  param[SXZ] *= rad2deg;
  param[SYZ] *= rad2deg;

  // Convert scales to percentages
  param[SX] *= 100;
  param[SY] *= 100;
  param[SZ] *= 100;
}

// ---------------------------------------------------------------------------
void irtkAffineTransformation::UpdateMatrix()
{
  // Convert angles to radians
  const double deg2rad = M_PI / 180.0;
  const double rx  = _Param[RX]  * deg2rad;
  const double ry  = _Param[RY]  * deg2rad;
  const double rz  = _Param[RZ]  * deg2rad;
  const double sxy = _Param[SXY] * deg2rad;
  const double sxz = _Param[SXZ] * deg2rad;
  const double syz = _Param[SYZ] * deg2rad;

  // Update sines, cosines, and tans
  _cosrx  = cos(rx);
  _cosry  = cos(ry);
  _cosrz  = cos(rz);
  _sinrx  = sin(rx);
  _sinry  = sin(ry);
  _sinrz  = sin(rz);
  _tansxy = tan(sxy);
  _tansxz = tan(sxz);
  _tansyz = tan(syz);

  // Update transformation matrix
  AffineParametersToMatrix(_Param[TX], _Param[TY], _Param[TZ], rx, ry, rz,
                           _Param[SX] / 100.0, _Param[SY] / 100.0, _Param[SZ] / 100.0,
                           sxy, sxz, syz, _matrix);
  _inverse = _matrix.Inverse();
}

// ---------------------------------------------------------------------------
void irtkAffineTransformation::UpdateDOFs()
{
  // Get affine parameters
  MatrixToAffineParameters(_matrix, _Param[TX],  _Param[TY],  _Param[TZ],
                                    _Param[RX],  _Param[RY],  _Param[RZ],
                                    _Param[SX],  _Param[SY],  _Param[SZ],
                                    _Param[SXY], _Param[SXZ], _Param[SYZ]);

  // Reset disabled/passive parameters
  if (_Status[TX ] == Passive) _Param[TX ] = .0;
  if (_Status[TY ] == Passive) _Param[TY ] = .0;
  if (_Status[TZ ] == Passive) _Param[TZ ] = .0;
  if (_Status[RX ] == Passive) _Param[RX ] = .0;
  if (_Status[RY ] == Passive) _Param[RY ] = .0;
  if (_Status[RZ ] == Passive) _Param[RZ ] = .0;
  if (_Status[SX ] == Passive) _Param[SX ] = 1.0;
  if (_Status[SY ] == Passive) _Param[SY ] = 1.0;
  if (_Status[SZ ] == Passive) _Param[SZ ] = 1.0;
  if (_Status[SXY] == Passive) _Param[SXY] = .0;
  if (_Status[SYZ] == Passive) _Param[SYZ] = .0;
  if (_Status[SXZ] == Passive) _Param[SXZ] = .0;

  // Update sines, cosines, and tans
  _cosrx  = cos(_Param[RX]);
  _cosry  = cos(_Param[RY]);
  _cosrz  = cos(_Param[RZ]);
  _sinrx  = sin(_Param[RX]);
  _sinry  = sin(_Param[RY]);
  _sinrz  = sin(_Param[RZ]);
  _tansxy = tan(_Param[SXY]);
  _tansxz = tan(_Param[SXZ]);
  _tansyz = tan(_Param[SYZ]);

  // Convert angles to degrees
  const double rad2deg = 180.0 / M_PI;
  _Param[RX]  *= rad2deg;
  _Param[RY]  *= rad2deg;
  _Param[RZ]  *= rad2deg;
  _Param[SXY] *= rad2deg;
  _Param[SXZ] *= rad2deg;
  _Param[SYZ] *= rad2deg;

  // Convert scales to percentages
  _Param[SX] *= 100.0;
  _Param[SY] *= 100.0;
  _Param[SZ] *= 100.0;
}

// -----------------------------------------------------------------------------
bool irtkAffineTransformation::CopyFrom(const irtkTransformation *other)
{
  if (strcmp(other->NameOfClass(), "irtkSimilarityTransformation") == 0) {
    const irtkSimilarityTransformation *sim;
    sim = reinterpret_cast<const irtkSimilarityTransformation *>(other);
    this->Reset();
    if (_Status[TX] == Active) _Param[TX] = sim->GetTranslationX();
    if (_Status[TY] == Active) _Param[TY] = sim->GetTranslationY();
    if (_Status[TZ] == Active) _Param[TZ] = sim->GetTranslationZ();
    if (_Status[RX] == Active) _Param[RX] = sim->GetRotationX();
    if (_Status[RY] == Active) _Param[RY] = sim->GetRotationY();
    if (_Status[RZ] == Active) _Param[RZ] = sim->GetRotationZ();
    if (_Status[SX] == Active) _Param[SX] = sim->GetScale();
    if (_Status[SY] == Active) _Param[SY] = sim->GetScale();
    if (_Status[SZ] == Active) _Param[SZ] = sim->GetScale();
    this->Update(MATRIX);
    return true;
  } else {
    return irtkHomogeneousTransformation::CopyFrom(other);
  }
}

// =============================================================================
// Derivatives
// =============================================================================

// -----------------------------------------------------------------------------
void irtkAffineTransformation::JacobianDOFs(double jac[3], int dof, double x, double y, double z, double, double) const
{
  // T(x) = R W S x + t
  // where
  //        R is rotation matrix
  //        W is shear matrix
  //        S is scale matrix
  //        t is translation vector

  double r[3][3] = {{.0, .0, .0}, {.0, .0, .0}, {.0, .0, .0}};

  // Derivative w.r.t translation or initialization of those rotation matrix
  // elements, which are needed below for the computation of the derivative
  // w.r.t the scale or shear parameter
  switch (dof) {
    case TX:
      jac[0] = 1.0;
      jac[1] = jac[2] = .0;
      return;
    case TY:
      jac[1] = 1.0;
      jac[0] = jac[2] = .0;
      return;
    case TZ:
      jac[2] = 1.0;
      jac[0] = jac[1] = .0;
      return;
    case SX: case SXY: case SXZ:
      r[0][0] = _cosry * _cosrz;
      r[1][0] = _sinrx * _sinry * _cosrz - _cosrx * _sinrz;
      r[2][0] = _cosrx * _sinry * _cosrz + _sinrx * _sinrz;
      if (dof != SX) break;
    case SY: case SYZ: // SX continued, i.e., do not insert a break before
      r[0][1] = _cosry * _sinrz;
      r[1][1] = _sinrx * _sinry * _sinrz + _cosrx * _cosrz;
      r[2][1] = _cosrx * _sinry * _sinrz - _sinrx * _cosrz;
      if (dof == SYZ) break;
    case SZ: // SX|SY continued, i.e., do not insert a break before
      r[0][2] = -_sinry;
      r[1][2] = _sinrx * _cosry;
      r[2][2] = _cosrx * _cosry;
      break;
  }

  // Derivative w.r.t scale or shear parameter
  switch (dof) {
    case SX: {
      jac[0] = r[0][0] * x / 100.0;
      jac[1] = r[1][0] * x / 100.0;
      jac[2] = r[2][0] * x / 100.0;
    } break;
    case SY: {
      jac[0] = (r[0][0] * _tansxy + r[0][1]) * y / 100.0;
      jac[1] = (r[1][0] * _tansxy + r[1][1]) * y / 100.0;
      jac[2] = (r[2][0] * _tansxy + r[2][1]) * y / 100.0;
    } break;
    case SZ: {
      jac[0] = (r[0][0] * _tansxz + r[0][1] * _tansyz + r[0][2]) * z / 100.0;
      jac[1] = (r[1][0] * _tansxz + r[1][1] * _tansyz + r[1][2]) * z / 100.0;
      jac[2] = (r[2][0] * _tansxz + r[2][1] * _tansyz + r[2][2]) * z / 100.0;
    } break;
    case SXY: {
      const double deg2rad = M_PI / 180.0;
      const double dsxy    = (deg2rad / pow(cos(_Param[SXY] * deg2rad), 2.0)) * y * _Param[SY] / 100.0;
      jac[0] = r[0][0] * dsxy;
      jac[1] = r[1][0] * dsxy;
      jac[2] = r[2][0] * dsxy;
    } break;
    case SYZ: {
      const double deg2rad = M_PI / 180.0;
      const double dsyz    = (deg2rad / pow(cos(_Param[SYZ] * deg2rad), 2.0)) * z * _Param[SZ] / 100.0;
      jac[0] = r[0][1] * dsyz;
      jac[1] = r[1][1] * dsyz;
      jac[2] = r[2][1] * dsyz;
    } break;
    case SXZ: {
      const double deg2rad = M_PI / 180.0;
      const double dsxz    = (deg2rad / pow(cos(_Param[SXZ] * deg2rad), 2.0)) * z * _Param[SZ] / 100.0;
      jac[0] = r[0][0] * dsxz;
      jac[1] = r[1][0] * dsxz;
      jac[2] = r[2][0] * dsxz;
    } break;
    // Derivate w.r.t rotation parameter
    default: {
      // apply scaling
      x *= _Param[SX] / 100.0;
      y *= _Param[SY] / 100.0;
      z *= _Param[SZ] / 100.0;
      // apply shearing
      x += _tansxy * y + _tansxz * z;
      y += _tansyz * z;
      // compute derivative w.r.t rigid parameter
      irtkRigidTransformation::JacobianDOFs(jac, dof, x, y, z);
    }
  }
}

// -----------------------------------------------------------------------------
void irtkAffineTransformation::DeriveJacobianWrtDOF(irtkMatrix &dJdp, int dof, double x, double y, double z, double, double) const
{
  dJdp.Initialize(3, 3);

  double r[3][3] = {{.0, .0, .0}, {.0, .0, .0}, {.0, .0, .0}};

  // Derivative w.r.t translation or initialization of those rotation matrix
  // elements, which are needed below for the computation of the derivative
  // w.r.t the scale or shear parameter
  switch (dof) {
    case SX: case SXY: case SXZ:
      r[0][0] = _cosry * _cosrz;
      r[1][0] = _sinrx * _sinry * _cosrz - _cosrx * _sinrz;
      r[2][0] = _cosrx * _sinry * _cosrz + _sinrx * _sinrz;
      if (dof != SX) break;
    case SY: case SYZ: // SX continued, i.e., do not insert a break before
      r[0][1] = _cosry * _sinrz;
      r[1][1] = _sinrx * _sinry * _sinrz + _cosrx * _cosrz;
      r[2][1] = _cosrx * _sinry * _sinrz - _sinrx * _cosrz;
      if (dof == SYZ) break;
    case SZ: // SX|SY continued, i.e., do not insert a break before
      r[0][2] = -_sinry;
      r[1][2] = _sinrx * _cosry;
      r[2][2] = _cosrx * _cosry;
      break;
  }

  // Derivative w.r.t scale or shear parameter
  switch (dof) {
    case SX: {
      dJdp(0, 0) = r[0][0] / 100.0;
      dJdp(1, 0) = r[1][0] / 100.0;
      dJdp(2, 0) = r[2][0] / 100.0;
    } break;
    case SY: {
      dJdp(0, 1) = (r[0][0] * _tansxy + r[0][1]) / 100.0;
      dJdp(1, 1) = (r[1][0] * _tansxy + r[1][1]) / 100.0;
      dJdp(2, 1) = (r[2][0] * _tansxy + r[2][1]) / 100.0;
    } break;
    case SZ: {
      dJdp(0, 2) = (r[0][0] * _tansxz + r[0][1] * _tansyz + r[0][2]) / 100.0;
      dJdp(1, 2) = (r[1][0] * _tansxz + r[1][1] * _tansyz + r[1][2]) / 100.0;
      dJdp(2, 2) = (r[2][0] * _tansxz + r[2][1] * _tansyz + r[2][2]) / 100.0;
    } break;
    case SXY: {
      const double deg2rad = M_PI / 180.0;
      const double dsxy    = (deg2rad / pow(cos(_Param[SXY] * deg2rad), 2.0)) * _Param[SY] / 100.0;
      dJdp(0, 1) = r[0][0] * dsxy;
      dJdp(1, 1) = r[1][0] * dsxy;
      dJdp(2, 1) = r[2][0] * dsxy;
    } break;
    case SYZ: {
      const double deg2rad = M_PI / 180.0;
      const double dsyz    = (deg2rad / pow(cos(_Param[SYZ] * deg2rad), 2.0)) * _Param[SZ] / 100.0;
      dJdp(0, 2) = r[0][1] * dsyz;
      dJdp(1, 2) = r[1][1] * dsyz;
      dJdp(2, 2) = r[2][1] * dsyz;
    } break;
    case SXZ: {
      const double deg2rad = M_PI / 180.0;
      const double dsxz    = (deg2rad / pow(cos(_Param[SXZ] * deg2rad), 2.0)) * _Param[SZ] / 100.0;
      dJdp(0, 2) = r[0][0] * dsxz;
      dJdp(1, 2) = r[1][0] * dsxz;
      dJdp(2, 2) = r[2][0] * dsxz;
    } break;
    // Derivate w.r.t rotation parameter
    default: {
      // compute derivative w.r.t rigid parameter
      irtkRigidTransformation::DeriveJacobianWrtDOF(dJdp, dof, x, y, z);

      //apply W * S (product of shearing and scaling matrices)
      irtkMatrix ws;
      ws.Initialize(3, 3);

      ws(0, 0) = _Param[SX] / 100.0;
      ws(1, 0) = .0;
      ws(2, 0) = .0;
      ws(0, 1) = _tansxy * _Param[SY] / 100.0;
      ws(1, 1) = _Param[SY] / 100.0;
      ws(2, 1) = .0;
      ws(0, 2) = _tansxz * _Param[SZ] / 100.0;
      ws(1, 2) = _tansyz * _Param[SZ] / 100.0;
      ws(2, 2) = _Param[SZ] / 100.0;

      dJdp *= ws;
    }
  }
}

// =============================================================================
// I/O
// =============================================================================

// -----------------------------------------------------------------------------
void irtkAffineTransformation::Print(irtkIndent indent) const
{
  irtkRigidTransformation::Print(indent);

  cout.setf(ios::right);
  cout.setf(ios::fixed);
  streamsize previous_precision = cout.precision(4);

  if (_Status[SX] == _Active || !fequal(_Param[SX], 100.0, 1e-4) ||
      _Status[SY] == _Active || !fequal(_Param[SY], 100.0, 1e-4) ||
      _Status[SZ] == _Active || !fequal(_Param[SZ], 100.0, 1e-4)) {
    cout << indent;
    if (_Status[SX]  == _Active) cout << "sx  = "  << setw(8) << _Param[SX]  << "  ";
    if (_Status[SY]  == _Active) cout << "sy  = "  << setw(8) << _Param[SY]  << "  ";
    if (_Status[SZ]  == _Active) cout << "sz  = "  << setw(8) << _Param[SZ]  << "  ";
    cout << endl;
  }
  if (_Status[SXY] == _Active || !fequal(_Param[SXY], .0, 1e-4) ||
      _Status[SXZ] == _Active || !fequal(_Param[SXZ], .0, 1e-4) ||
      _Status[SYZ] == _Active || !fequal(_Param[SYZ], .0, 1e-4)) {
    cout << indent;
    if (_Status[SXY] == _Active) cout << "sxy = " << setw(8) << _Param[SXY] << "  ";
    if (_Status[SYZ] == _Active) cout << "syz = " << setw(8) << _Param[SYZ] << "  ";
    if (_Status[SXZ] == _Active) cout << "sxz = " << setw(8) << _Param[SXZ] << "  ";
    cout << endl;
  }

  cout.precision(previous_precision);
  cout.unsetf(ios::right);
  cout.unsetf(ios::fixed);
}

// -----------------------------------------------------------------------------
bool irtkAffineTransformation::CanRead(irtkTransformationType format) const
{
  switch (format) {
    case IRTKTRANSFORMATION_RIGID:
    case IRTKTRANSFORMATION_SIMILARITY:
    case IRTKTRANSFORMATION_AFFINE:
      return true;
    default:
      return false;
  }
}

// -----------------------------------------------------------------------------
irtkCofstream &irtkAffineTransformation::Write(irtkCofstream &to) const
{
  unsigned int ndofs = this->NumberOfDOFs();

  // Write rigid parameters only if additional affine components not used
  if ((fabs(_Param[SX] - 100.0) < 0.0001) &&
      (fabs(_Param[SY] - 100.0) < 0.0001) &&
      (fabs(_Param[SZ] - 100.0) < 0.0001) &&
      (fabs(_Param[SXY])        < 0.0001) &&
      (fabs(_Param[SXZ])        < 0.0001) &&
      (fabs(_Param[SYZ])        < 0.0001)) {
    ndofs = 6;
  }

  // Write magic no. for transformations
  unsigned int magic_no = IRTKTRANSFORMATION_MAGIC;
  to.WriteAsUInt(&magic_no, 1);

  // Write transformation type
  unsigned int trans_type = (ndofs == 6) ? IRTKTRANSFORMATION_RIGID
                                         : IRTKTRANSFORMATION_AFFINE;
  to.WriteAsUInt(&trans_type, 1);

  // Write number of rigid parameters
  to.WriteAsUInt(&ndofs, 1);

  // Write rigid transformation parameters
  to.WriteAsDouble(_Param, ndofs);

  return to;
}

// -----------------------------------------------------------------------------
irtkCifstream &irtkAffineTransformation::ReadDOFs(irtkCifstream &from, irtkTransformationType)
{
  // Read number of parameters
  unsigned int ndofs;
  from.ReadAsUInt(&ndofs, 1);
  if (ndofs != 6 && ndofs != 7 && ndofs != 9 && ndofs != 12) {
    cerr << this->NameOfClass() << "::Read: Invalid no. of parameters: " << ndofs << endl;
    exit(1);
  }

  // Read transformation parameters
  from.ReadAsDouble(_Param, ndofs);
  if (ndofs == 6)  _Param[SX]  = _Param[SY]  = _Param[SZ]  = 100;
  if (ndofs == 7)                _Param[SY]  = _Param[SZ]  = _Param[SX];
  if (ndofs != 12) _Param[SXY] = _Param[SXZ] = _Param[SYZ] = 0;

  // Update transformation matrix
  this->Update(MATRIX);

  return from;
}

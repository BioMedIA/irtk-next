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

#ifndef _IRTKCUBICBSPLINEINTERPOLATEIMAGEFUNCTION_HXX
#define _IRTKCUBICBSPLINEINTERPOLATEIMAGEFUNCTION_HXX

#include <irtkCubicBSplineInterpolateImageFunction.h>
#include <irtkInterpolateImageFunction.hxx>
#include <irtkImageToInterpolationCoefficients.h>


// =============================================================================
// Construction/Destruction
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::irtkGenericCubicBSplineInterpolateImageFunction()
:
  _InfiniteCoefficient(NULL)
{
  // Default extrapolation mode is to apply the mirror boundary condition
  // which is also assumed when converting an input image to spline coefficients
  this->Extrapolator(ExtrapolatorType::New(Extrapolation_Mirror), true);
}

// -----------------------------------------------------------------------------
template <class TImage>
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::~irtkGenericCubicBSplineInterpolateImageFunction()
{
  Delete(_InfiniteCoefficient);
}

// -----------------------------------------------------------------------------
template <class TImage>
void irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Initialize(bool coeff)
{
  // Initialize base class
  Superclass::Initialize(coeff);

  // Domain on which cubic B-spline interpolation is defined
  const double margin = 2.0;
  switch (this->NumberOfDimensions()) {
    case 4:
      this->_t1 = fdec(margin);
      this->_t2 = this->Input()->T() - margin - 1;
    case 3:
      this->_z1 = fdec(margin);
      this->_z2 = this->Input()->Z() - margin - 1;
    default:
      this->_y1 = fdec(margin);
      this->_y2 = this->Input()->Y() - margin - 1;
      this->_x1 = fdec(margin);
      this->_x2 = this->Input()->X() - margin - 1;
  }

  // Initialize coefficient image
  if (coeff && this->Input()->GetDataType() == voxel_info<RealType>::type()) {
    _Coefficient.Initialize(this->Input()->Attributes(),
                            reinterpret_cast<RealType *>(
                            const_cast<void *>(this->Input()->GetDataPointer())));
  } else {
    _Coefficient = *(this->Input());
    if (!coeff) {
      Real pole;
      int  unused;
      SplinePoles(3, &pole, unused);
      switch (this->NumberOfDimensions()) {
        case 4:  ConvertToInterpolationCoefficientsT(_Coefficient, &pole, 1);
        case 3:  ConvertToInterpolationCoefficientsZ(_Coefficient, &pole, 1);
        default: ConvertToInterpolationCoefficientsY(_Coefficient, &pole, 1);
                 ConvertToInterpolationCoefficientsX(_Coefficient, &pole, 1);
      }
    }
  }

  // Initialize infinite coefficient image (i.e., extrapolator)
  if (!_InfiniteCoefficient || _InfiniteCoefficient->ExtrapolationMode() != this->ExtrapolationMode()) {
    Delete(_InfiniteCoefficient);
    _InfiniteCoefficient = CoefficientExtrapolator::New(this->ExtrapolationMode(), &_Coefficient);
  }
  if (_InfiniteCoefficient) {
    _InfiniteCoefficient->SetInput(&_Coefficient);
    _InfiniteCoefficient->Initialize();
  }

  // Compute strides for fast iteration over coefficient image
  //_s1 = 1;
  _s2 =  this->Input()->X() - 4;
  _s3 = (this->Input()->Y() - 4) * this->Input()->X();
  _s4 = (this->Input()->Z() - 4) * this->Input()->Y() * this->Input()->X();
}

// =============================================================================
// Domain checks
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
void irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::BoundingInterval(double x, int &i, int &I) const
{
  i = static_cast<int>(floor(x)), I = i + 3;
}

// =============================================================================
// Evaluation
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Get2D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  if (k < 0 || k >= _Coefficient.Z() ||
      l < 0 || l >= _Coefficient.T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);

  --i, --j;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, w;

  int ia, jb;
  for (int b = 0; b <= 3; ++b) {
    jb = j + b;
    if (0 <= jb && jb < _Coefficient.Y()) {
      for (int a = 0; a <= 3; ++a) {
        ia = i + a;
        if (0 <= ia && ia < _Coefficient.X()) {
          w    = wx[a] * wy[b];
          val += w * _Coefficient(ia, jb, k, l);
          nrm += w;
        }
      }
    }
  }

  if (nrm) val /= nrm;
  else     val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPadding2D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  if (k < 0 || k >= _Coefficient.Z() ||
      l < 0 || l >= _Coefficient.T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);

  --i, --j;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, w;

  int ia, jb;
  for (int b = 0; b <= 3; ++b) {
    jb = j + b;
    for (int a = 0; a <= 3; ++a) {
      ia = i + a;
      w  = wx[a] * wy[b];
      if (this->Input()->IsInsideForeground(ia, jb, k, l)) {
        val += w * _Coefficient(ia, jb, k, l);
        fgw += w;
      } else {
        bgw += w;
      }
    }
  }

  if (fgw > bgw) val /= fgw;
  else           val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TCoefficient>
inline typename TCoefficient::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Get2D(const TCoefficient *coeff, double x, double y, double z, double t) const
{
  typedef typename TCoefficient::VoxelType VoxelType;
  typedef typename TCoefficient::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);

  --i, --j;

  RealType val = voxel_cast<RealType>(0);

  int jb;
  for (int b = 0; b <= 3; ++b) {
    jb   = j + b;
    val += wx[0] * wy[b] * coeff->Get(i,   jb, k, l);
    val += wx[1] * wy[b] * coeff->Get(i+1, jb, k, l);
    val += wx[2] * wy[b] * coeff->Get(i+2, jb, k, l);
    val += wx[3] * wy[b] * coeff->Get(i+3, jb, k, l);
  }

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TOtherImage, class TCoefficient>
inline typename TCoefficient::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPadding2D(const TOtherImage *input, const TCoefficient *coeff,
                   double x, double y, double z, double t) const
{
  typedef typename TCoefficient::VoxelType VoxelType;
  typedef typename TCoefficient::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);

  --i, --j;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, w;

  int ia, jb;
  for (int b = 0; b <= 3; ++b) {
    jb = j + b;
    for (int a = 0; a <= 3; ++a) {
      ia = i + a;
      w  = wx[0] * wy[b];
      if (input->IsForeground(ia, jb, k, l)) {
        val += w * coeff->Get(ia, jb, k, l);
        fgw += w;
      } else {
        bgw += w;
      }
    }
  }

  if (fgw > bgw) val /= fgw;
  else           val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Get3D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  if (l < 0 || l >= _Coefficient.T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);

  --i, --j, --k;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wyz, w;

  int ia, jb, kc;
  for (int c = 0; c <= 3; ++c) {
    kc = k + c;
    if (0 <= kc && kc < _Coefficient.Z()) {
      for (int b = 0; b <= 3; ++b) {
        jb = j + b;
        if (0 <= jb && jb < _Coefficient.Y()) {
          wyz = wy[b] * wz[c];
          for (int a = 0; a <= 3; ++a) {
            ia = i + a;
            if (0 <= ia && ia < _Coefficient.X()) {
              w    = wx[a] * wyz;
              val += w * _Coefficient(ia, jb, kc, l);
              nrm += w;
            }
          }
        }
      }
    }
  }

  if (nrm) val /= nrm;
  else     val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPadding3D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  if (l < 0 || l >= _Coefficient.T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);

  --i, --j, --k;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wyz, w;

  int ia, jb, kc;
  for (int c = 0; c <= 3; ++c) {
    kc = k + c;
    for (int b = 0; b <= 3; ++b) {
      jb = j + b;
      wyz = wy[b] * wz[c];
      for (int a = 0; a <= 3; ++a) {
        ia = i + a;
        w  = wx[a] * wyz;
        if (this->Input()->IsInsideForeground(ia, jb, kc, l)) {
          val += w * _Coefficient(ia, jb, kc, l);
          fgw += w;
        } else {
          bgw += w;
        }
      }
    }
  }

  if (fgw > bgw) val /= fgw;
  else           val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TCoefficient>
inline typename TCoefficient::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Get3D(const TCoefficient *coeff, double x, double y, double z, double t) const
{
  typedef typename TCoefficient::VoxelType VoxelType;
  typedef typename TCoefficient::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);

  --i, --j, --k;

  RealType val = voxel_cast<RealType>(0);

  int    jb, kc;
  double wyz;
  for (int c = 0; c <= 3; ++c) {
    kc = k + c;
    for (int b = 0; b <= 3; ++b) {
      jb   = j + b;
      wyz  = wy[b] * wz[c];
      val += wx[0] * wyz * coeff->Get(i,   jb, kc, l);
      val += wx[1] * wyz * coeff->Get(i+1, jb, kc, l);
      val += wx[2] * wyz * coeff->Get(i+2, jb, kc, l);
      val += wx[3] * wyz * coeff->Get(i+3, jb, kc, l);
    }
  }

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TOtherImage, class TCoefficient>
inline typename TCoefficient::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPadding3D(const TOtherImage *input, const TCoefficient *coeff,
                   double x, double y, double z, double t) const
{
  typedef typename TCoefficient::VoxelType VoxelType;
  typedef typename TCoefficient::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);

  --i, --j, --k;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wyz, w;

  int ia, jb, kc;
  for (int c = 0; c <= 3; ++c) {
    kc = k + c;
    for (int b = 0; b <= 3; ++b) {
      jb = j + b;
      wyz = wy[b] * wz[c];
      for (int a = 0; a <= 3; ++a) {
        ia = i + a;
        w  = wx[0] * wyz;
        if (input->IsForeground(ia, jb, kc, l)) {
          val += w * coeff->Get(ia, jb, kc, l);
          fgw += w;
        } else {
          bgw += w;
        }
      }
    }
  }

  if (fgw > bgw) val /= fgw;
  else           val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Get4D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);
  Real wt[4]; Kernel::Weights(t - l, wt);

  --i, --j, --k, --l;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wzt, wyzt, w;

  int ia, jb, kc, ld;
  for (int d = 0; d <= 3; ++d) {
    ld = l + d;
    if (0 <= ld && ld < _Coefficient.T()) {
      for (int c = 0; c <= 3; ++c) {
        kc = k + c;
        if (0 <= kc && kc < _Coefficient.Z()) {
          wzt = wz[c] * wt[d];
          for (int b = 0; b <= 3; ++b) {
            jb = j + b;
            if (0 <= jb && jb < _Coefficient.Y()) {
              wyzt = wy[b] * wzt;
              for (int a = 0; a <= 3; ++a) {
                ia = i + a;
                if (0 <= ia && ia < _Coefficient.X()) {
                  w    = wx[a] * wyzt;
                  val += w * _Coefficient(ia, jb, kc, ld);
                  nrm += w;
                }
              }
            }
          }
        }
      }
    }
  }

  if (nrm) val /= nrm;
  else     val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPadding4D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);
  Real wt[4]; Kernel::Weights(t - l, wt);

  --i, --j, --k, --l;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wzt, wyzt, w;

  int ia, jb, kc, ld;
  for (int d = 0; d <= 3; ++d) {
    ld = l + d;
    for (int c = 0; c <= 3; ++c) {
      kc = k + c;
      wzt = wz[c] * wt[d];
      for (int b = 0; b <= 3; ++b) {
        jb = j + b;
        wyzt = wy[b] * wzt;
        for (int a = 0; a <= 3; ++a) {
          ia = i + a;
          w  = wx[a] * wyzt;
          if (this->Input()->IsInsideForeground(ia, jb, kc, ld)) {
            val += w * _Coefficient(ia, jb, kc, ld);
            fgw += w;
          } else {
            bgw += w;
          }
        }
      }
    }
  }

  if (fgw > bgw) val /= fgw;
  else           val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TCoefficient>
inline typename TCoefficient::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Get4D(const TCoefficient *coeff, double x, double y, double z, double t) const
{
  typedef typename TCoefficient::VoxelType VoxelType;
  typedef typename TCoefficient::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);
  Real wt[4]; Kernel::Weights(t - l, wt);

  --i, --j, --k, --l;

  RealType val = voxel_cast<RealType>(0);

  int    jb, kc, ld;
  double wzt, wyzt;
  for (int d = 0; d <= 3; ++d) {
    ld = l + d;
    for (int c = 0; c <= 3; ++c) {
      kc  = k + c;
      wzt = wz[c] * wt[d];
      for (int b = 0; b <= 3; ++b) {
        jb   = j + b;
        wyzt = wy[b] * wzt;
        val += wx[0] * wyzt * coeff->Get(i,   jb, kc, ld);
        val += wx[1] * wyzt * coeff->Get(i+1, jb, kc, ld);
        val += wx[2] * wyzt * coeff->Get(i+2, jb, kc, ld);
        val += wx[3] * wyzt * coeff->Get(i+3, jb, kc, ld);
      }
    }
  }

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TOtherImage, class TCoefficient>
inline typename TCoefficient::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPadding4D(const TOtherImage *input, const TCoefficient *coeff,
                   double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);
  Real wt[4]; Kernel::Weights(t - l, wt);

  --i, --j, --k, --l;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wzt, wyzt, w;

  int ia, jb, kc, ld;
  for (int d = 0; d <= 3; ++d) {
    ld = l + d;
    for (int c = 0; c <= 3; ++c) {
      kc  = k + c;
      wzt = wz[c] + wt[d];
      for (int b = 0; b <= 3; ++b) {
        jb   = j + b;
        wyzt = wy[b] * wzt;
        for (int a = 0; a <= 3; ++a) {
          ia = i + a;
          w  = wx[0] * wyzt;
          if (input->IsForeground(ia, jb, kc, ld)) {
            val += w * coeff->Get(ia, jb, kc, ld);
            fgw += w;
          } else {
            bgw += w;
          }
        }
      }
    }
  }

  if (fgw > bgw) val /= fgw;
  else           val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Get(double x, double y, double z, double t) const
{
  switch (this->NumberOfDimensions()) {
    case 3:  return Get3D(x, y, z, t);
    case 2:  return Get2D(x, y, z, t);
    default: return Get4D(x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPadding(double x, double y, double z, double t) const
{
  switch (this->NumberOfDimensions()) {
    case 3:  return GetWithPadding3D(x, y, z, t);
    case 2:  return GetWithPadding2D(x, y, z, t);
    default: return GetWithPadding4D(x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TOtherImage>
inline typename TOtherImage::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::Get(const TOtherImage *coeff, double x, double y, double z, double t) const
{
  switch (this->NumberOfDimensions()) {
    case 3:  return Get3D(coeff, x, y, z, t);
    case 2:  return Get2D(coeff, x, y, z, t);
    default: return Get4D(coeff, x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TOtherImage, class TCoefficient>
inline typename TCoefficient::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPadding(const TOtherImage *input, const TCoefficient *coeff,
                 double x, double y, double z, double t) const
{
  switch (this->NumberOfDimensions()) {
    case 3:  return GetWithPadding3D(input, coeff, x, y, z, t);
    case 2:  return GetWithPadding2D(input, coeff, x, y, z, t);
    default: return GetWithPadding4D(input, coeff, x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetInside2D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);

  --i, --j;

  RealType        val   = voxel_cast<RealType>(0);
  const RealType *coeff = _Coefficient.Data(i, j, k, l);

  for (int b = 0; b <= 3; ++b, coeff += _s2) {
    val += wx[0] * wy[b] * (*coeff++);
    val += wx[1] * wy[b] * (*coeff++);
    val += wx[2] * wy[b] * (*coeff++);
    val += wx[3] * wy[b] * (*coeff++);
  }

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetInside3D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);

  --i, --j, --k;

  RealType        val   = voxel_cast<RealType>(0);
  const RealType *coeff = _Coefficient.Data(i, j, k, l);

  Real wyz;
  for (int c = 0; c <= 3; ++c, coeff += _s3) {
    for (int b = 0; b <= 3; ++b, coeff += _s2) {
      wyz  = wy[b] * wz[c];
      val += wx[0] * wyz * (*coeff++);
      val += wx[1] * wyz * (*coeff++);
      val += wx[2] * wyz * (*coeff++);
      val += wx[3] * wyz * (*coeff++);
    }
  }

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetInside4D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  Real wx[4]; Kernel::Weights(x - i, wx);
  Real wy[4]; Kernel::Weights(y - j, wy);
  Real wz[4]; Kernel::Weights(z - k, wz);
  Real wt[4]; Kernel::Weights(t - l, wt);

  --i, --j, --k, --l;

  RealType        val   = voxel_cast<RealType>(0);
  const RealType *coeff = _Coefficient.Data(i, j, k, l);

  Real wzt, wyzt;
  for (int d = 0; d <= 3; ++d, coeff += _s4) {
    for (int c = 0; c <= 3; ++c, coeff += _s3) {
      wzt = wz[c] * wt[d];
      for (int b = 0; b <= 3; ++b, coeff += _s2) {
        wyzt = wy[b] * wzt;
        val += wx[0] * wyzt * (*coeff++);
        val += wx[1] * wyzt * (*coeff++);
        val += wx[2] * wyzt * (*coeff++);
        val += wx[3] * wyzt * (*coeff++);
      }
    }
  }

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetInside(double x, double y, double z, double t) const
{
  // Use faster coefficient iteration than Get(Coefficient(), x, y, z, t)
  switch (this->NumberOfDimensions()) {
    case 3:  return GetInside3D(x, y, z, t);
    case 2:  return GetInside2D(x, y, z, t);
    default: return GetInside4D(x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetOutside(double x, double y, double z, double t) const
{
  if (_InfiniteCoefficient) {
    return Get(_InfiniteCoefficient, x, y, z, t);
  } else {
    return Get(x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPaddingInside(double x, double y, double z, double t) const
{
  switch (this->NumberOfDimensions()) {
    case 3:  return GetWithPadding3D(this->Input(), &_Coefficient, x, y, z, t);
    case 2:  return GetWithPadding2D(this->Input(), &_Coefficient, x, y, z, t);
    default: return GetWithPadding4D(this->Input(), &_Coefficient, x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCubicBSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCubicBSplineInterpolateImageFunction<TImage>
::GetWithPaddingOutside(double x, double y, double z, double t) const
{
  if (this->Extrapolator() && _InfiniteCoefficient) {
    return GetWithPadding(this->Extrapolator(), _InfiniteCoefficient, x, y, z, t);
  } else {
    return GetWithPadding(x, y, z, t);
  }
}


#endif

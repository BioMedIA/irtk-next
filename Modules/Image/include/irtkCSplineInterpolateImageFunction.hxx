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

#ifndef _IRTKCSPLINEINTERPOLATEIMAGEFUNCTION_HXX
#define _IRTKCSPLINEINTERPOLATEIMAGEFUNCTION_HXX

#include <irtkCSplineInterpolateImageFunction.h>
#include <irtkInterpolateImageFunction.hxx>


// =============================================================================
// Cubic spline function
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::Real
irtkGenericCSplineInterpolateImageFunction<TImage>::CSpline(Real x)
{
  const Real xi   = fabs(x);
  const Real xii  = xi  * xi;
  const Real xiii = xii * xi;
  if      (xi < 1) return  xiii - 2 * xii + 1;
  else if (xi < 2) return -xiii + 5 * xii - 8 * xi + 4;
  return 0;
}

// =============================================================================
// Construction/Destruction
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
irtkGenericCSplineInterpolateImageFunction<TImage>
::irtkGenericCSplineInterpolateImageFunction()
{
}

// -----------------------------------------------------------------------------
template <class TImage>
irtkGenericCSplineInterpolateImageFunction<TImage>
::~irtkGenericCSplineInterpolateImageFunction()
{
}

// -----------------------------------------------------------------------------
template <class TImage>
void irtkGenericCSplineInterpolateImageFunction<TImage>::Initialize(bool coeff)
{
  // Initialize base class
  Superclass::Initialize(coeff);

  // Domain on which the C-spline interpolation is defined
  switch (this->NumberOfDimensions()) {
    case 4:
      this->_t1 = 1;
      this->_t2 = this->Input()->T() - 3;
    case 3:
      this->_z1 = 1;
      this->_z2 = this->Input()->Z() - 3;
    default:
      this->_y1 = 1;
      this->_y2 = this->Input()->Y() - 3;
      this->_x1 = 1;
      this->_x2 = this->Input()->X() - 3;
  }
}

// =============================================================================
// Domain checks
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
void irtkGenericCSplineInterpolateImageFunction<TImage>
::BoundingInterval(double x, int &i, int &I) const
{
  i = static_cast<int>(floor(x)) - 1, I = i + 3;
}

// =============================================================================
// Evaluation
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::Get2D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  if (k < 0 || k >= this->Input()->Z() ||
      l < 0 || l >= this->Input()->T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wy, w;

  for (j = j1; j <= j2; ++j) {
    if (0 <= j && j < this->Input()->Y()) {
      wy = CSpline(y - j);
      for (i = i1; i <= i2; ++i) {
        if (0 <= i && i < this->Input()->X()) {
          w    = CSpline(x - i) * wy;
          val += w * static_cast<RealType>(this->Input()->Get(i, j, k, l));
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
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPadding2D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  if (k < 0 || k >= this->Input()->Z() ||
      l < 0 || l >= this->Input()->T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wy, w;

  for (j = j1; j <= j2; ++j) {
    wy = CSpline(y - j);
    for (i = i1; i <= i2; ++i) {
      w = CSpline(x - i) * wy;
      if (this->Input()->IsInsideForeground(i, j, k, l)) {
        val += w * static_cast<RealType>(this->Input()->Get(i, j, k, l));
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
template <class TImage> template <class TOtherImage>
inline typename TOtherImage::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::Get2D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wy, w;

  for (j = j1; j <= j2; ++j) {
    wy = CSpline(y - j);
    for (i = i1; i <= i2; ++i) {
      w    = CSpline(x - i) * wy;
      val += w * static_cast<RealType>(input->Get(i, j, k, l));
      nrm += w;
    }
  }

  if (nrm) val /= nrm;
  else     val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TOtherImage>
inline typename TOtherImage::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPadding2D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wy, w;

  for (j = j1; j <= j2; ++j) {
    wy = CSpline(y - j);
    for (i = i1; i <= i2; ++i) {
      w    = CSpline(x - i) * wy;
      if (input->IsForeground(i, j, k, l)) {
        val += w * static_cast<RealType>(input->Get(i, j, k, l));
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
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::Get3D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  if (l < 0 || l >= this->Input()->T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int k1 = k - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;
  const int k2 = k + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wz, wyz, w;

  for (k = k1; k <= k2; ++k) {
    if (0 <= k && k < this->Input()->Z()) {
      wz = CSpline(z - k);
      for (j = j1; j <= j2; ++j) {
        if (0 <= j && j < this->Input()->Y()) {
          wyz = CSpline(y - j) * wz;
          for (i = i1; i <= i2; ++i) {
            if (0 <= i && i < this->Input()->X()) {
              w    = CSpline(x - i) * wyz;
              val += w * static_cast<RealType>(this->Input()->Get(i, j, k, l));
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
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPadding3D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  if (l < 0 || l >= this->Input()->T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int k1 = k - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;
  const int k2 = k + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wz, wyz, w;

  for (k = k1; k <= k2; ++k) {
    wz = CSpline(z - k);
    for (j = j1; j <= j2; ++j) {
      wyz = CSpline(y - j) * wz;
      for (i = i1; i <= i2; ++i) {
        w = CSpline(x - i) * wyz;
        if (this->Input()->IsInsideForeground(i, j, k, l)) {
          val += w * static_cast<RealType>(this->Input()->Get(i, j, k, l));
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
template <class TImage> template <class TOtherImage>
inline typename TOtherImage::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::Get3D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int k1 = k - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;
  const int k2 = k + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wz, wyz, w;

  for (k = k1; k <= k2; ++k) {
    wz = CSpline(z - k);
    for (j = j1; j <= j2; ++j) {
      wyz = CSpline(y - j) * wz;
      for (i = i1; i < i2; ++i) {
        w    = CSpline(x - i) * wyz;
        val += w * static_cast<RealType>(input->Get(i, j, k, l));
        nrm += w;
      }
    }
  }

  if (nrm) val /= nrm;
  else     val  = voxel_cast<RealType>(this->DefaultValue());

  return voxel_cast<VoxelType>(val);
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TOtherImage>
inline typename TOtherImage::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPadding3D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(round(t));

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int k1 = k - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;
  const int k2 = k + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wz, wyz, w;

  for (k = k1; k <= k2; ++k) {
    wz = CSpline(z - k);
    for (j = j1; j <= j2; ++j) {
      wyz = CSpline(y - j) * wz;
      for (i = i1; i < i2; ++i) {
        w = CSpline(x - i) * wyz;
        if (input->IsForeground(i, j, k, l)) {
          val += w * static_cast<RealType>(input->Get(i, j, k, l));
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
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::Get4D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int k1 = k - 1;
  const int l1 = l - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;
  const int k2 = k + 2;
  const int l2 = l + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wt, wzt, wyzt, w;

  for (l = l1; l <= l2; ++l) {
    if (0 <= l && l < this->Input()->T()) {
      wt = CSpline(t - l);
      for (k = k1; k <= k2; ++k) {
        if (0 <= k && k < this->Input()->Z()) {
          wzt = CSpline(z - k) * wt;
          for (j = j1; j <= j2; ++j) {
            if (0 <= j && j < this->Input()->Y()) {
              wyzt = CSpline(y - j) * wzt;
              for (i = i1; i <= i2; ++i) {
                if (0 <= i && i < this->Input()->X()) {
                  w    = CSpline(x - i) * wyzt;
                  val += w * static_cast<RealType>(this->Input()->Get(i, j, k, l));
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
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPadding4D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int k1 = k - 1;
  const int l1 = l - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;
  const int k2 = k + 2;
  const int l2 = l + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wt, wzt, wyzt, w;

  for (l = l1; l <= l2; ++l) {
    wt = CSpline(t - l);
    for (k = k1; k <= k2; ++k) {
      wzt = CSpline(z - k) * wt;
      for (j = j1; j <= j2; ++j) {
        wyzt = CSpline(y - j) * wzt;
        for (i = i1; i <= i2; ++i) {
          w = CSpline(x - i) * wyzt;
          if (this->Input()->IsInsideForeground(i, j, k, l)) {
            val += w * static_cast<RealType>(this->Input()->Get(i, j, k, l));
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
template <class TImage> template <class TOtherImage>
inline typename TOtherImage::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::Get4D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int k1 = k - 1;
  const int l1 = l - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;
  const int k2 = k + 2;
  const int l2 = l + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wt, wzt, wyzt, w;

  for (l = l1; l <= l2; ++l) {
    wt = CSpline(t - l);
    for (k = k1; k <= k2; ++k) {
      wzt = CSpline(z - k) * wt;
      for (j = j1; j <= j2; ++j) {
        wyzt = CSpline(y - j) * wzt;
        for (i = i1; i <= i2; ++i) {
          w    = CSpline(x - i) * wyzt;
          val += w * static_cast<RealType>(input->Get(i, j, k, l));
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
template <class TImage> template <class TOtherImage>
inline typename TOtherImage::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPadding4D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(floor(x));
  int j = static_cast<int>(floor(y));
  int k = static_cast<int>(floor(z));
  int l = static_cast<int>(floor(t));

  const int i1 = i - 1;
  const int j1 = j - 1;
  const int k1 = k - 1;
  const int l1 = l - 1;
  const int i2 = i + 2;
  const int j2 = j + 2;
  const int k2 = k + 2;
  const int l2 = l + 2;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wt, wzt, wyzt, w;

  for (l = l1; l <= l2; ++l) {
    wt = CSpline(t - l);
    for (k = k1; k <= k2; ++k) {
      wzt = CSpline(z - k) * wt;
      for (j = j1; j <= j2; ++j) {
        wyzt = CSpline(y - j) * wzt;
        for (i = i1; i <= i2; ++i) {
          w = CSpline(x - i) * wyzt;
          if (input->IsForeground(i, j, k, l)) {
            val += w * static_cast<RealType>(input->Get(i, j, k, l));
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
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
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
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
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
irtkGenericCSplineInterpolateImageFunction<TImage>
::Get(const TOtherImage *input, double x, double y, double z, double t) const
{
  switch (this->NumberOfDimensions()) {
    case 3:  return Get3D(input, x, y, z, t);
    case 2:  return Get2D(input, x, y, z, t);
    default: return Get4D(input, x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage> template <class TOtherImage>
inline typename TOtherImage::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPadding(const TOtherImage *input, double x, double y, double z, double t) const
{
  switch (this->NumberOfDimensions()) {
    case 3:  return GetWithPadding3D(input, x, y, z, t);
    case 2:  return GetWithPadding2D(input, x, y, z, t);
    default: return GetWithPadding4D(input, x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetInside(double x, double y, double z, double t) const
{
  return Get(this->Input(), x, y, z, t);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetOutside(double x, double y, double z, double t) const
{
  if (this->Extrapolator()) {
    return Get(this->Extrapolator(), x, y, z, t);
  } else {
    return Get(x, y, z, t);
  }
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPaddingInside(double x, double y, double z, double t) const
{
  return GetWithPadding(this->Input(), x, y, z, t);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericCSplineInterpolateImageFunction<TImage>::VoxelType
irtkGenericCSplineInterpolateImageFunction<TImage>
::GetWithPaddingOutside(double x, double y, double z, double t) const
{
  if (this->Extrapolator()) {
    return GetWithPadding(this->Extrapolator(), x, y, z, t);
  } else {
    return GetWithPadding(x, y, z, t);
  }
}


#endif

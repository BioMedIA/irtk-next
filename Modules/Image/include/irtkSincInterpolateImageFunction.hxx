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

#ifndef _IRTKSINCINTERPOLATEIMAGEFUNCTION_HXX
#define _IRTKSINCINTERPOLATEIMAGEFUNCTION_HXX

#include <irtkSincInterpolateImageFunction.h>
#include <irtkInterpolateImageFunction.hxx>


// =============================================================================
// Construction/Destruction
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
irtkGenericSincInterpolateImageFunction<TImage>
::irtkGenericSincInterpolateImageFunction()
:
  _Epsilon(0.000001)
{
}

// -----------------------------------------------------------------------------
template <class TImage>
void irtkGenericSincInterpolateImageFunction<TImage>::Initialize(bool coeff)
{
  // Initialize base class
  Superclass::Initialize(coeff);

  // Initialize lookup table of Sinc function values
  Kernel::Initialize();

  // Domain on which the Sinc interpolation is defined
  switch (this->NumberOfDimensions()) {
    case 4:
      this->_t1 = Kernel::Radius;
      this->_t2 = this->Input()->T() - Kernel::Radius - 1;
    case 3:
      this->_z1 = Kernel::Radius;
      this->_z2 = this->Input()->Z() - Kernel::Radius - 1;
    default:
      this->_y1 = Kernel::Radius;
      this->_y2 = this->Input()->Y() - Kernel::Radius - 1;
      this->_x1 = Kernel::Radius;
      this->_x2 = this->Input()->X() - Kernel::Radius - 1;
  }
}

// =============================================================================
// Domain checks
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
void irtkGenericSincInterpolateImageFunction<TImage>
::BoundingInterval(double x, int &i, int &I) const
{
  const int c = static_cast<int>(round(x));
  i = c - Kernel::Radius;
  I = c + Kernel::Radius;
}

// =============================================================================
// Evaluation
// =============================================================================

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::Get2D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  if (k < 0 || k >= this->Input()->Z() ||
      l < 0 || l >= this->Input()->T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon)) {
    if (0 <= i && i < this->Input()->X() &&
        0 <= j && j < this->Input()->Y()) {
      return this->Input()->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wy, w;

  for (j = j1; j <= j2; ++j) {
    if (0 <= j && j < this->Input()->Y()) {
      wy = Kernel::Lookup(y - j);
      for (i = i1; i <= i2; ++i) {
        if (0 <= i && i < this->Input()->X()) {
          w    = Kernel::Lookup(x - i) * wy;
          val += w * voxel_cast<RealType>(this->Input()->Get(i, j, k, l));
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::GetWithPadding2D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  if (k < 0 || k >= this->Input()->Z() ||
      l < 0 || l >= this->Input()->T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon)) {
    if (this->Input()->IsInsideForeground(i, j, k, l)) {
      return this->Input()->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wy, w;

  for (j = j1; j <= j2; ++j) {
    wy = Kernel::Lookup(y - j);
    for (i = i1; i <= i2; ++i) {
      w = Kernel::Lookup(x - i) * wy;
      if (this->Input()->IsInsideForeground(i, j, k, l)) {
        val += w * voxel_cast<RealType>(this->Input()->Get(i, j, k, l));
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
irtkGenericSincInterpolateImageFunction<TImage>
::Get2D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon)) {
    return input->Get(i, j, k, l);
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wy, w;

  for (j = j1; j <= j2; ++j) {
    wy = Kernel::Lookup(y - j);
    for (i = i1; i <= i2; ++i) {
      w    = Kernel::Lookup(x - i) * wy;
      val += w * voxel_cast<RealType>(input->Get(i, j, k, l));
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
irtkGenericSincInterpolateImageFunction<TImage>
::GetWithPadding2D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon)) {
    if (input->IsForeground(i, j, k, l)) {
      return input->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wy, w;

  for (j = j1; j <= j2; ++j) {
    wy = Kernel::Lookup(y - j);
    for (i = i1; i <= i2; ++i) {
      w = Kernel::Lookup(x - i) * wy;
      if (input->IsForeground(i, j, k, l)) {
        val += w * voxel_cast<RealType>(input->Get(i, j, k, l));
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::Get3D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  if (l < 0 || l >= this->Input()->T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon) &&
      fequal(z, k, _Epsilon)) {
    if (0 <= i && i < this->Input()->X() &&
        0 <= j && j < this->Input()->Y() &&
        0 <= k && k < this->Input()->Z()) {
      return this->Input()->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int k1 = k - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;
  const int k2 = k + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wz, wyz, w;

  for (k = k1; k <= k2; ++k) {
    if (0 <= k && k < this->Input()->Z()) {
      wz = Kernel::Lookup(z - k);
      for (j = j1; j <= j2; ++j) {
        if (0 <= j && j < this->Input()->Y()) {
          wyz = Kernel::Lookup(y - j) * wz;
          for (i = i1; i <= i2; ++i) {
            if (0 <= i && i < this->Input()->X()) {
              w    = Kernel::Lookup(x - i) * wyz;
              val += w * voxel_cast<RealType>(this->Input()->Get(i, j, k, l));
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::GetWithPadding3D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  if (l < 0 || l >= this->Input()->T()) {
    return voxel_cast<VoxelType>(this->DefaultValue());
  }

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon) &&
      fequal(z, k, _Epsilon)) {
    if (this->Input()->IsInsideForeground(i, j, k, l)) {
      return this->Input()->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int k1 = k - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;
  const int k2 = k + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wz, wyz, w;

  for (k = k1; k <= k2; ++k) {
    wz = Kernel::Lookup(z - k);
    for (j = j1; j <= j2; ++j) {
      wyz = Kernel::Lookup(y - j) * wz;
      for (i = i1; i <= i2; ++i) {
        w  = Kernel::Lookup(x - i) * wyz;
        if (this->Input()->IsInsideForeground(i, j, k, l)) {
          val += w * voxel_cast<RealType>(this->Input()->Get(i, j, k, l));
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
irtkGenericSincInterpolateImageFunction<TImage>
::Get3D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon) &&
      fequal(z, k, _Epsilon)) {
    return input->Get(i, j, k, l);
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int k1 = k - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;
  const int k2 = k + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wz, wyz, w;

  for (k = k1; k <= k2; ++k) {
    wz = Kernel::Lookup(z - k);
    for (j = j1; j <= j2; ++j) {
      wyz = Kernel::Lookup(y - j) * wz;
      for (i = i1; i <= i2; ++i) {
        w    = Kernel::Lookup(x - i) * wyz;
        val += w * voxel_cast<RealType>(input->Get(i, j, k, l));
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
irtkGenericSincInterpolateImageFunction<TImage>
::GetWithPadding3D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon) &&
      fequal(z, k, _Epsilon)) {
    if (input->IsForeground(i, j, k, l)) {
      return input->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int k1 = k - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;
  const int k2 = k + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wz, wyz, w;

  for (k = k1; k <= k2; ++k) {
    wz = Kernel::Lookup(z - k);
    for (j = j1; j <= j2; ++j) {
      wyz = Kernel::Lookup(y - j) * wz;
      for (i = i1; i <= i2; ++i) {
        w = Kernel::Lookup(x - i) * wyz;
        if (input->IsForeground(i, j, k, l)) {
          val += w * voxel_cast<RealType>(input->Get(i, j, k, l));
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::Get4D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon) &&
      fequal(z, k, _Epsilon) &&
      fequal(t, l, _Epsilon)) {
    if (this->Input()->IsInside(i, j, k, l)) {
      return this->Input()->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int k1 = k - Kernel::Radius;
  const int l1 = l - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;
  const int k2 = k + Kernel::Radius;
  const int l2 = l + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wt, wzt, wyzt, w;

  for (l = l1; l <= l2; ++l) {
    if (0 <= l && l < this->Input()->T()) {
      wt = Kernel::Lookup(t - l);
      for (k = k1; k <= k2; ++k) {
        if (0 <= k && k < this->Input()->Z()) {
          wzt = Kernel::Lookup(z - k) * wt;
          for (j = j1; j <= j2; ++j) {
            if (0 <= j && j < this->Input()->Y()) {
              wyzt = Kernel::Lookup(y - j) * wzt;
              for (i = i1; i <= i2; ++i) {
                if (0 <= i && i < this->Input()->X()) {
                  w    = Kernel::Lookup(x - i) * wyzt;
                  val += w * voxel_cast<RealType>(this->Input()->Get(i, j, k, l));
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::GetWithPadding4D(double x, double y, double z, double t) const
{
  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon) &&
      fequal(z, k, _Epsilon) &&
      fequal(t, l, _Epsilon)) {
    if (this->Input()->IsInsideForeground(i, j, k, l)) {
      return this->Input()->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int k1 = k - Kernel::Radius;
  const int l1 = l - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;
  const int k2 = k + Kernel::Radius;
  const int l2 = l + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wt, wzt, wyzt, w;

  for (l = l1; l <= l2; ++l) {
    wt = Kernel::Lookup(t - l);
    for (k = k1; k <= k2; ++k) {
      wzt = Kernel::Lookup(z - k) * wt;
      for (j = j1; j <= j2; ++j) {
        wyzt = Kernel::Lookup(y - j) * wzt;
        for (i = i1; i <= i2; ++i) {
          w = Kernel::Lookup(x - i) * wyzt;
          if (this->Input()->IsInsideForeground(i, j, k, l)) {
            val += w * voxel_cast<RealType>(this->Input()->Get(i, j, k, l));
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
irtkGenericSincInterpolateImageFunction<TImage>
::Get4D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon) &&
      fequal(z, k, _Epsilon) &&
      fequal(t, l, _Epsilon)) {
    return input->Get(i, j, k, l);
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int k1 = k - Kernel::Radius;
  const int l1 = l - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;
  const int k2 = k + Kernel::Radius;
  const int l2 = l + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     nrm = .0, wt, wzt, wyzt, w;

  for (l = l1; l <= l2; ++l) {
    wt = Kernel::Lookup(t - l);
    for (k = k1; k <= k2; ++k) {
      wzt = Kernel::Lookup(z - k) * wt;
      for (j = j1; j <= j2; ++j) {
        wyzt = Kernel::Lookup(y - j) * wzt;
        for (i = i1; i <= i2; ++i) {
          w    = Kernel::Lookup(x - i) * wyzt;
          val += w * voxel_cast<RealType>(input->Get(i, j, k, l));
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
irtkGenericSincInterpolateImageFunction<TImage>
::GetWithPadding4D(const TOtherImage *input, double x, double y, double z, double t) const
{
  typedef typename TOtherImage::VoxelType VoxelType;
  typedef typename TOtherImage::RealType  RealType;

  int i = static_cast<int>(round(x));
  int j = static_cast<int>(round(y));
  int k = static_cast<int>(round(z));
  int l = static_cast<int>(round(t));

  // Return value of nearest neighbor if distance is negligible
  if (fequal(x, i, _Epsilon) &&
      fequal(y, j, _Epsilon) &&
      fequal(z, k, _Epsilon) &&
      fequal(t, l, _Epsilon)) {
    if (input->IsForeground(i, j, k, l)) {
      return input->Get(i, j, k, l);
    } else {
      return voxel_cast<VoxelType>(this->DefaultValue());
    }
  }

  // Truncated Sinc using Hanning window, H(dx/R)*Sinc(dx), R=6 where
  // Sinc(dx) = sin(pi*dx)/(pi*dx), H(dx/R) = 0.5*(1+cos(pi*dx/R))
  const int i1 = i - Kernel::Radius;
  const int j1 = j - Kernel::Radius;
  const int k1 = k - Kernel::Radius;
  const int l1 = l - Kernel::Radius;
  const int i2 = i + Kernel::Radius;
  const int j2 = j + Kernel::Radius;
  const int k2 = k + Kernel::Radius;
  const int l2 = l + Kernel::Radius;

  RealType val = voxel_cast<RealType>(0);
  Real     fgw = .0, bgw = .0, wt, wzt, wyzt, w;

  for (l = l1; l <= l2; ++l) {
    wt = Kernel::Lookup(t - l);
    for (k = k1; k <= k2; ++k) {
      wzt = Kernel::Lookup(z - k) * wt;
      for (j = j1; j <= j2; ++j) {
        wyzt = Kernel::Lookup(y - j) * wzt;
        for (i = i1; i <= i2; ++i) {
          w = Kernel::Lookup(x - i) * wyzt;
          if (input->IsForeground(i, j, k, l)) {
            val += w * voxel_cast<RealType>(input->Get(i, j, k, l));
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
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
irtkGenericSincInterpolateImageFunction<TImage>
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
irtkGenericSincInterpolateImageFunction<TImage>
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::GetInside(double x, double y, double z, double t) const
{
  return Get(this->Input(), x, y, z, t);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
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
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::GetWithPaddingInside(double x, double y, double z, double t) const
{
  return GetWithPadding(this->Input(), x, y, z, t);
}

// -----------------------------------------------------------------------------
template <class TImage>
inline typename irtkGenericSincInterpolateImageFunction<TImage>::VoxelType
irtkGenericSincInterpolateImageFunction<TImage>
::GetWithPaddingOutside(double x, double y, double z, double t) const
{
  if (this->Extrapolator()) {
    return GetWithPadding(this->Extrapolator(), x, y, z, t);
  } else {
    return GetWithPadding(x, y, z, t);
  }
}


#endif

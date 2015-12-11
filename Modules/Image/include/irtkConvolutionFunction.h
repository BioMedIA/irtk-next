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

#include <irtkCommon.h>
#include <irtkBaseImage.h>
#include <irtkVoxelFunction.h>


/// Voxel functions implementing convolution of an image with a discrete kernel
namespace irtkConvolutionFunction {


// =============================================================================
// Boundary conditions for image domain
// =============================================================================

// -----------------------------------------------------------------------------
/// Mirror image at boundary
struct MirrorBoundaryCondition
{
  /// Apply mirror boundary condition to voxel index
  ///
  /// \param[in] i Voxel index along image dimension
  /// \param[in] N Number of voxels in image dimension
  int operator ()(int i, int N) const
  {
    N -= 1;
    int m, n;
    if (i < 0) {
      i = -i;
      n = i / N;
      m = i - n * N;
      if (n % 2) i = N - 1 - m;
      else       i = m;
    } else if (i > N) {
      i = i - N;
      n = i / N;
      m = i - n * N;
      if (n % 2) i = m;
      else       i = N - m;
    }
    return i;
  }

  /// Apply mirror boundary condition to voxel pointer
  template <class T>
  const T *operator ()(int i, int N, const T *p, int stride)
  {
    return p + (this->operator ()(i, N) - i) * stride;
  }
};

// =============================================================================
// Convolve at boundary truncated image with 1D kernel
// =============================================================================

// -----------------------------------------------------------------------------
/// Perform convolution along x with truncation of kernel at boundary
template <class TKernel = double>
struct ConvolveInX : public irtkVoxelFunction
{
  const TKernel *_Kernel;    ///< Convolution kernel
  int            _Radius;    ///< Radius of kernel
  bool           _Normalize; ///< Wether to divide by sum of kernel weights
  int            _Offset;    ///< Vector component offset
  int            _X;         ///< Number of voxels in x

  ConvolveInX(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    _Kernel(kernel), _Radius((size - 1) / 2), _Normalize(norm),
    _Offset(l * image->NumberOfSpatialVoxels()), _X(image->X())
  {}

  template <class T1, class T2>
  void operator ()(int i, int, int, int, const T1 *in, T2 *out) const
  {
    // Convolve l-th vector component
    in += _Offset, out += _Offset;
    // Go to start of input and end of kernel
    i    -= _Radius;
    in   -= _Radius;
    int n = _Radius + _Radius/* + 1 - 1*/;
    // Outside left boundary
    if (i < 0) n += i, in -= i, i = 0;
    // Inside image domain
    double acc = .0, sum = .0;
    while (i < _X && n >= 0) {
      acc += static_cast<double>(_Kernel[n]) * static_cast<double>(*in);
      sum += static_cast<double>(_Kernel[n]);
      ++i, ++in, --n;
    }
    // Output result of convolution
    if (_Normalize && sum != .0) acc /= sum;
    *out = static_cast<T2>(acc);
  }
};

// -----------------------------------------------------------------------------
/// Perform convolution along y with truncation of kernel at boundary
template <class TKernel = double>
struct ConvolveInY : public irtkVoxelFunction
{
  const TKernel *_Kernel;    ///< Convolution kernel
  int            _Radius;    ///< Radius of kernel
  bool           _Normalize; ///< Wether to divide by sum of kernel weights
  int            _Offset;    ///< Vector component offset
  int            _X;         ///< Number of voxels in x
  int            _Y;         ///< Number of voxels in y

  ConvolveInY(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    _Kernel(kernel), _Radius((size - 1) / 2), _Normalize(norm),
    _Offset(l * image->NumberOfSpatialVoxels()), _X(image->X()), _Y(image->Y())
  {}

  template <class T1, class T2>
  void operator ()(int, int j, int, int, const T1 *in, T2 *out) const
  {
    // Convolve l-th vector component
    in += _Offset, out += _Offset;
    // Go to start of input and end of kernel
    j    -= _Radius;
    in   -= _Radius * _X;
    int n = _Radius + _Radius/* + 1 - 1*/;
    // Outside left boundary
    if (j < 0) n += j, in -= j * _X, j = 0;
    // Inside image domain
    double acc = .0, sum = .0;
    while (j < _Y && n >= 0) {
      acc += static_cast<double>(_Kernel[n]) * static_cast<double>(*in);
      sum += static_cast<double>(_Kernel[n]);
      ++j, in += _X, --n;
    }
    // Output result of convolution
    if (_Normalize && sum != .0) acc /= sum;
    *out = static_cast<T2>(acc);
  }
};

// -----------------------------------------------------------------------------
/// Perform convolution along z with truncation of kernel at boundary
template <class TKernel = double>
struct ConvolveInZ : public irtkVoxelFunction
{
  const TKernel *_Kernel;    ///< Convolution kernel
  int            _Radius;    ///< Radius of kernel
  bool           _Normalize; ///< Wether to divide by sum of kernel weights
  int            _Offset;    ///< Vector component offset
  int            _XY;        ///< Number of voxels in slice (nx * ny)
  int            _Z;         ///< Number of voxels in z

  ConvolveInZ(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    _Kernel(kernel), _Radius((size - 1) / 2), _Normalize(norm),
    _Offset(l * image->NumberOfSpatialVoxels()), _XY(image->X() * image->Y()), _Z(image->Z())
  {}

  template <class T1, class T2>
  void operator ()(int, int, int k, int, const T1 *in, T2 *out) const
  {
    // Convolve l-th vector component
    in += _Offset, out += _Offset;
    // Go to start of input and end of kernel
    k    -= _Radius;
    in   -= _Radius * _XY;
    int n = _Radius + _Radius/* + 1 - 1*/;
    // Outside left boundary
    if (k < 0) n += k, in -= k * _XY, k = 0;
    // Inside image domain
    double acc = .0, sum = .0;
    while (k < _Z && n >= 0) {
      acc += static_cast<double>(_Kernel[n]) * static_cast<double>(*in);
      sum += static_cast<double>(_Kernel[n]);
      ++k, in += _XY, --n;
    }
    // Output result of convolution
    if (_Normalize && sum != .0) acc /= sum;
    *out = static_cast<T2>(acc);
  }
};

// -----------------------------------------------------------------------------
/// Perform convolution along t with truncation of kernel at boundary
template <class TKernel = double>
struct ConvolveInT : public irtkVoxelFunction
{
  const TKernel *_Kernel;    ///< Convolution kernel
  int            _Radius;    ///< Radius of kernel
  bool           _Normalize; ///< Wether to divide by sum of kernel weights
  int            _XYZ;       ///< Number of voxels in volume (nx * ny * nz)
  int            _T;         ///< Number of voxels in t

  ConvolveInT(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true)
  :
    _Kernel(kernel), _Radius((size - 1) / 2), _Normalize(norm),
    _XYZ(image->X() * image->Y() * image->Z()), _T(image->T())
  {}

  template <class T1, class T2>
  void operator ()(int, int, int, int l, const T1 *in, T2 *out) const
  {
    double acc = .0, sum = .0;
    // Go to start of input and end of kernel
    l    -= _Radius;
    in   -= _Radius * _XYZ;
    int n = _Radius + _Radius/* + 1 - 1*/;
    // Outside left boundary
    if (l < 0) n += l, in -= l * _XYZ, l = 0;
    // Inside image domain
    while (l < _T && n >= 0) {
      acc += static_cast<double>(_Kernel[n]) * static_cast<double>(*in);
      sum += static_cast<double>(_Kernel[n]);
      ++l, in += _XYZ, --n;
    }
    // Output result of convolution
    if (_Normalize && sum != .0) acc /= sum;
    *out = static_cast<T2>(acc);
  }
};

// =============================================================================
// Convolve at truncated image foreground with 1D kernel
// =============================================================================

// -----------------------------------------------------------------------------
/// Base class of 1D convolution functions which truncate the kernel at the foreground boundary
template <class TKernel = double>
class TruncatedForegroundConvolution1D : public irtkVoxelFunction
{
protected:

  // ---------------------------------------------------------------------------
  /// Constructor
  TruncatedForegroundConvolution1D(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true)
  :
    _Kernel(kernel), _Size(size), _Radius((size - 1) / 2), _Normalize(norm),
    _Background(image->GetBackgroundValueAsDouble())
  {}

  // ---------------------------------------------------------------------------
  /// Apply kernel initially at center voxel
  template <class T>
  bool ConvolveCenterVoxel(const T *in, double &acc, double &sum) const
  {
    if (*in == _Background) {
      acc = _Background;
      sum = .0;
      return false;
    } else {
      acc = static_cast<double>(_Kernel[_Radius]) * static_cast<double>(*in);
      sum = static_cast<double>(_Kernel[_Radius]);
      return true;
    }
  }

  // ---------------------------------------------------------------------------
  /// Apply kernel to left neighbors of given voxel
  template <class T>
  void ConvolveLeftNeighbors(int i, int n, const T *in, int s, double &acc, double &sum) const
  {
    int di = -1;
    for (int k = _Radius + 1; k < _Size; ++k) {
      // Move voxel index/pointer
      i  += di;
      in -= s;
      // Stop if outside image/foreground
      if (i < 0 || i >= n || *in == _Background) break;
      // Multiply with kernel weight
      acc += static_cast<double>(_Kernel[k]) * static_cast<double>(*in);
      sum += static_cast<double>(_Kernel[k]);
    }
  }

  // ---------------------------------------------------------------------------
  /// Apply kernel to right neighbors of given voxel
  template <class T>
  void ConvolveRightNeighbors(int i, int n, const T *in, int s, double &acc, double &sum) const
  {
    int di = +1;
    for (int k = _Radius - 1; k >= 0; --k) {
      // Move voxel index/pointer
      i  += di;
      in += s;
      // Stop if outside image/foreground
      if (i < 0 || i >= n || *in == _Background) break;
      // Multiply with kernel weight
      acc += static_cast<double>(_Kernel[k]) * static_cast<double>(*in);
      sum += static_cast<double>(_Kernel[k]);
    }
  }

  // ---------------------------------------------------------------------------
  template <class T>
  void Put(T *out, double acc, double sum) const
  {
    if (_Normalize && sum != .0) acc /= sum;
    (*out) = static_cast<T>(acc);
  }

  // ---------------------------------------------------------------------------
  // Data members
protected:

  const TKernel *_Kernel;     ///< Convolution kernel
  int            _Size;       ///< Size of kernel = 2 * _Radius + 1
  int            _Radius;     ///< Radius of kernel
  bool           _Normalize;  ///< Whether to normalize by sum of overlapping kernel weights
  double         _Background; ///< Background value (padding)
};


// -----------------------------------------------------------------------------
/// Compute convolution of voxel with kernel in x dimension
template <class TKernel = double>
struct ConvolveTruncatedForegroundInX : public TruncatedForegroundConvolution1D<TKernel>
{
  ConvolveTruncatedForegroundInX(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _Offset(l * image->NumberOfSpatialVoxels()), _X(image->GetX())
  {}

  ConvolveTruncatedForegroundInX(const irtkBaseImage *image, double bg, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _Offset(l * image->NumberOfSpatialVoxels()), _X(image->GetX())
  {
    this->_Background = bg;
  }

  ConvolveTruncatedForegroundInX(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Offset(l * image->NumberOfSpatialVoxels()), _X(image->GetX())
  {}

  template <class T1, class T2>
  void operator ()(int i, int, int, int, const T1 *in, T2 *out) const
  {
    in += _Offset, out += _Offset;
    double acc, sum;
    if (this->ConvolveCenterVoxel(in, acc, sum)) {
      this->ConvolveLeftNeighbors (i, _X, in, 1, acc, sum);
      this->ConvolveRightNeighbors(i, _X, in, 1, acc, sum);
    }
    this->Put(out, acc, sum);
  }

  int _Offset; ///< Vector component offset
  int _X;      ///< Number of voxels in x dimension (nx)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along y dimension
template <class TKernel = double>
struct ConvolveTruncatedForegroundInY : public TruncatedForegroundConvolution1D<TKernel>
{
  ConvolveTruncatedForegroundInY(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _Offset(l * image->NumberOfSpatialVoxels()),
    _X(image->GetX()),
    _Y(image->GetY())
  {}

  ConvolveTruncatedForegroundInY(const irtkBaseImage *image, double bg, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _Offset(l * image->NumberOfSpatialVoxels()),
    _X(image->GetX()),
    _Y(image->GetY())
  {
    this->_Background = bg;
  }

  ConvolveTruncatedForegroundInY(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Offset(l * image->NumberOfSpatialVoxels()),
    _X(image->GetX()),
    _Y(image->GetY())
  {}

  template <class T1, class T2>
  void operator ()(int, int j, int, int, const T1 *in, T2 *out) const
  {
    in += _Offset, out += _Offset;
    double acc, sum;
    if (this->ConvolveCenterVoxel(in, acc, sum)) {
      this->ConvolveLeftNeighbors (j, _Y, in, _X, acc, sum);
      this->ConvolveRightNeighbors(j, _Y, in, _X, acc, sum);
    }
    this->Put(out, acc, sum);
  }

  int _Offset; ///< Vector component offset
  int _X;      ///< Number of voxels in x dimension (nx)
  int _Y;      ///< Number of voxels in y dimension (ny)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along z dimension
template <class TKernel = double>
struct ConvolveTruncatedForegroundInZ : public TruncatedForegroundConvolution1D<TKernel>
{
  ConvolveTruncatedForegroundInZ(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _Offset(l * image->NumberOfSpatialVoxels()),
    _XY(image->GetX() * image->GetY()),
    _Z (image->GetZ())
  {}

  ConvolveTruncatedForegroundInZ(const irtkBaseImage *image, double bg, const TKernel *kernel, int size, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _Offset(l * image->NumberOfSpatialVoxels()),
    _XY(image->GetX() * image->GetY()),
    _Z (image->GetZ())
  {
    this->_Background = bg;
  }

  ConvolveTruncatedForegroundInZ(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, bool norm = true, int l = 0)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Offset(l * image->NumberOfSpatialVoxels()),
    _XY(image->GetX() * image->GetY()),
    _Z (image->GetZ())
  {}

  template <class T1, class T2>
  void operator ()(int, int, int k, int, const T1 *in, T2 *out) const
  {
    in += _Offset, out += _Offset;
    double acc, sum;
    if (this->ConvolveCenterVoxel(in, acc, sum)) {
      this->ConvolveLeftNeighbors (k, _Z, in, _XY, acc, sum);
      this->ConvolveRightNeighbors(k, _Z, in, _XY, acc, sum);
    }
    this->Put(out, acc, sum);
  }

  int _Offset; ///< Vector component offset
  int _XY;     ///< Number of voxels in slice (nx * ny)
  int _Z;      ///< Number of voxels in z dimension (nz)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along t dimension
template <class TKernel = double>
struct ConvolveTruncatedForegroundInT : public TruncatedForegroundConvolution1D<TKernel>
{
  ConvolveTruncatedForegroundInT(const irtkBaseImage *image, const TKernel *kernel, int size, bool norm = true)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _XYZ(image->GetX() * image->GetY() * image->GetZ()),
    _T  (image->GetT())
  {}

  ConvolveTruncatedForegroundInT(const irtkBaseImage *image, double bg, const TKernel *kernel, int size, bool norm = true)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _XYZ(image->GetX() * image->GetY() * image->GetZ()),
    _T  (image->GetT())
  {
    this->_Background = bg;
  }

  ConvolveTruncatedForegroundInT(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, bool norm = true)
  :
    TruncatedForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _XYZ(image->GetX() * image->GetY() * image->GetZ()),
    _T  (image->GetT())
  {}

  template <class T1, class T2>
  void operator ()(int, int, int, int t, const T1 *in, T2 *out) const
  {
    double acc, sum;
    if (this->ConvolveCenterVoxel(in, acc, sum)) {
      this->ConvolveLeftNeighbors (t, _T, in, _XYZ, acc, sum);
      this->ConvolveRightNeighbors(t, _T, in, _XYZ, acc, sum);
    }
    this->Put(out, acc, sum);
  }

  int _XYZ; ///< Number of voxels in frame (nx * ny * nz)
  int _T;   ///< Number of voxels in t dimension (nt)
};

// =============================================================================
// Convolve at boundary mirrored image foreground with 1D kernel
// =============================================================================

// -----------------------------------------------------------------------------
/// Base class of 1D convolution functions which mirror the foreground into background
template <class TKernel = double>
class MirroredForegroundConvolution1D : public irtkVoxelFunction
{
protected:

  // ---------------------------------------------------------------------------
  /// Constructor
  MirroredForegroundConvolution1D(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
  :
    _Kernel(kernel), _Size(size), _Radius((size - 1) / 2), _Norm(norm),
    _Background(image->GetBackgroundValueAsDouble())
  {}

  // ---------------------------------------------------------------------------
  /// Apply kernel initially at center voxel
  template <class T>
  bool ConvolveCenterVoxel(const T *in, double &acc) const
  {
    if (*in == _Background) {
      acc = _Background / _Norm;
      return false;
    } else {
      acc = static_cast<double>(*in) * static_cast<double>(_Kernel[_Radius]);
      return true;
    }
  }

  // ---------------------------------------------------------------------------
  /// Apply kernel to left neighbors of given voxel
  template <class T>
  void ConvolveLeftNeighbors(int i, int n, const T *in, int s, double &acc) const
  {
    int di = -1;
    for (int k = _Radius + 1; k < _Size; ++k) {
      // Move voxel index/pointer
      i  += di;
      in -= s;
      // Reverse if outside image/foreground
      if (i < 0 || i >= n || *in == _Background) {
        di  = -di;
        s   = -s;
        i  += di;
        in -= s;
      }
      // Multiply with kernel weight
      acc += static_cast<double>(*in) * static_cast<double>(_Kernel[k]);
    }
  }

  // ---------------------------------------------------------------------------
  /// Apply kernel to right neighbors of given voxel
  template <class T>
  void ConvolveRightNeighbors(int i, int n, const T *in, int s, double &acc) const
  {
    int di = +1;
    for (int k = _Radius - 1; k >= 0; --k) {
      // Move voxel index/pointer
      i  += di;
      in += s;
      // Reverse if outside image/foreground
      if (i < 0 || i >= n || *in == _Background) {
        di  = -di;
        s   = -s;
        i  += di;
        in +=  s;
      }
      // Multiply with kernel weight
      acc += static_cast<double>(*in) * static_cast<double>(_Kernel[k]);
    }
  }

  // ---------------------------------------------------------------------------
  template <class T>
  void Put(T *out, double acc) const
  {
    (*out) = static_cast<T>(_Norm * acc);
  }

  // ---------------------------------------------------------------------------
  // Data members
private:

  const TKernel *_Kernel;     ///< Convolution kernel
  int            _Size;       ///< Size of kernel = 2 * _Radius + 1
  int            _Radius;     ///< Radius of kernel
  double         _Norm;       ///< Normalization factor
  double         _Background; ///< Background value (padding)
};


// -----------------------------------------------------------------------------
/// Compute convolution of voxel with kernel in x dimension
template <class TKernel = double>
struct ConvolveMirroredForegroundInX : public MirroredForegroundConvolution1D<TKernel>
{
  ConvolveMirroredForegroundInX(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
    : MirroredForegroundConvolution1D<TKernel>(image, kernel, size, norm), _X(image->GetX()) {}

  ConvolveMirroredForegroundInX(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    MirroredForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _X(image->GetX())
  {}

  template <class T1, class T2>
  void operator ()(int i, int, int, int, const T1 *in, T2 *out) const
  {
    double acc;
    if (this->ConvolveCenterVoxel(in, acc)) {
      this->ConvolveLeftNeighbors (i, _X, in, 1, acc);
      this->ConvolveRightNeighbors(i, _X, in, 1, acc);
    }
    this->Put(out, acc);
  }

  int _X; ///< Number of voxels in x dimension (nx)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along y dimension
template <class TKernel = double>
struct ConvolveMirroredForegroundInY : public MirroredForegroundConvolution1D<TKernel>
{
  ConvolveMirroredForegroundInY(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
  :
    MirroredForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _X(image->GetX()),
    _Y(image->GetY())
  {}

  ConvolveMirroredForegroundInY(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    MirroredForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _X(image->GetX()),
    _Y(image->GetY())
  {}

  template <class T1, class T2>
  void operator ()(int, int j, int, int, const T1 *in, T2 *out) const
  {
    double acc;
    if (this->ConvolveCenterVoxel(in, acc)) {
      this->ConvolveLeftNeighbors (j, _Y, in, _X, acc);
      this->ConvolveRightNeighbors(j, _Y, in, _X, acc);
    }
    this->Put(out, acc);
  }

  int _X; ///< Number of voxels in x dimension (nx)
  int _Y; ///< Number of voxels in y dimension (ny)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along z dimension
template <class TKernel = double>
struct ConvolveMirroredForegroundInZ : public MirroredForegroundConvolution1D<TKernel>
{
  ConvolveMirroredForegroundInZ(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
  :
    MirroredForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _XY(image->GetX() * image->GetY()),
    _Z (image->GetZ())
  {}

  ConvolveMirroredForegroundInZ(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    MirroredForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _XY(image->GetX() * image->GetY()),
    _Z (image->GetZ())
  {}

  template <class T1, class T2>
  void operator ()(int, int, int k, int, const T1 *in, T2 *out) const
  {
    double acc;
    if (this->ConvolveCenterVoxel(in, acc)) {
      this->ConvolveLeftNeighbors (k, _Z, in, _XY, acc);
      this->ConvolveRightNeighbors(k, _Z, in, _XY, acc);
    }
    this->Put(out, acc);
  }

  int _XY; ///< Number of voxels in slice (nx * ny)
  int _Z;  ///< Number of voxels in z dimension (nz)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along t dimension
template <class TKernel = double>
struct ConvolveMirroredForegroundInT : public MirroredForegroundConvolution1D<TKernel>
{
  ConvolveMirroredForegroundInT(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
  :
    MirroredForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _XYZ(image->GetX() * image->GetY() * image->GetZ()),
    _T  (image->GetT())
  {}

  ConvolveMirroredForegroundInT(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    MirroredForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _XYZ(image->GetX() * image->GetY() * image->GetZ()),
    _T  (image->GetT())
  {}

  template <class T1, class T2>
  void operator ()(int, int, int, int t, const T1 *in, T2 *out) const
  {
    double acc;
    if (this->ConvolveCenterVoxel(in, acc)) {
      this->ConvolveLeftNeighbors (t, _T, in, _XYZ, acc);
      this->ConvolveRightNeighbors(t, _T, in, _XYZ, acc);
    }
    this->Put(out, acc);
  }

  int _XYZ; ///< Number of voxels in frame (nx * ny * nz)
  int _T;   ///< Number of voxels in t dimension (nt)
};

// =============================================================================
// Convolve at boundary extended image foreground with 1D kernel
// =============================================================================

// -----------------------------------------------------------------------------
/// Base class of 1D convolution functions which extends the foreground into background
template <class TKernel = double>
class ExtendedForegroundConvolution1D : public irtkVoxelFunction
{
protected:

  // ---------------------------------------------------------------------------
  /// Constructor
  ExtendedForegroundConvolution1D(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
  :
    _Kernel(kernel), _Size(size), _Radius((size - 1) / 2), _Norm(norm),
    _Background(image->GetBackgroundValueAsDouble())
  {}

  // ---------------------------------------------------------------------------
  /// Apply kernel initially at center voxel
  template <class T>
  bool ConvolveCenterVoxel(const T *in, double &acc) const
  {
    if (*in == _Background) {
      acc = _Background / _Norm;
      return false;
    } else {
      acc = (*in) * _Kernel[_Radius];
      return true;
    }
  }

  // ---------------------------------------------------------------------------
  /// Apply kernel to left neighbors of given voxel
  template <class T>
  void ConvolveLeftNeighbors(int i, int n, const T *in, int s, double &acc) const
  {
    int di = 1;
    for (int k = _Radius + 1; k < _Size; ++k) {
      i  -= di;
      in -= s;
      if (i < 0 || *in == _Background) {
        i  += di;
        in += s;
        di = s = 0;
      }
      // Multiply with kernel weight
      acc += static_cast<double>(*in) * static_cast<double>(_Kernel[k]);
    }
  }

  // ---------------------------------------------------------------------------
  /// Apply kernel to right neighbors of given voxel
  template <class T>
  void ConvolveRightNeighbors(int i, int n, const T *in, int s, double &acc) const
  {
    int di = 1;
    for (int k = _Radius - 1; k >= 0; --k) {
      i  += di;
      in += s;
      if (n <= i || *in == _Background) {
        i  -= di;
        in -= s;
        di = s = 0;
      }
      // Multiply with kernel weight
      acc += static_cast<double>(*in) * static_cast<double>(_Kernel[k]);
    }
  }

  // ---------------------------------------------------------------------------
  template <class T>
  void Put(T *out, double acc) const
  {
    (*out) = static_cast<T>(_Norm * acc);
  }

  // ---------------------------------------------------------------------------
  // Data members
private:

  const TKernel *_Kernel;     ///< Convolution kernel
  int            _Size;       ///< Size of kernel = 2 * _Radius + 1
  int            _Radius;     ///< Radius of kernel
  double         _Norm;       ///< Normalization factor
  double         _Background; ///< Background value (padding)
};


// -----------------------------------------------------------------------------
/// Compute convolution of voxel with kernel in x dimension
template <class TKernel = double>
struct ConvolveExtendedForegroundInX : public ExtendedForegroundConvolution1D<TKernel>
{
  ConvolveExtendedForegroundInX(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
    : ExtendedForegroundConvolution1D<TKernel>(image, kernel, size, norm), _X(image->GetX()) {}

  ConvolveExtendedForegroundInX(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ExtendedForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _X(image->GetX())
  {}

  template <class T1, class T2>
  void operator ()(int i, int, int, int, const T1 *in, T2 *out) const
  {
    double acc;
    if (this->ConvolveCenterVoxel(in, acc)) {
      this->ConvolveLeftNeighbors (i, _X, in, 1, acc);
      this->ConvolveRightNeighbors(i, _X, in, 1, acc);
    }
    this->Put(out, acc);
  }

  int _X; ///< Number of voxels in x dimension (nx)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along y dimension
template <class TKernel = double>
struct ConvolveExtendedForegroundInY : public ExtendedForegroundConvolution1D<TKernel>
{
  ConvolveExtendedForegroundInY(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
  :
    ExtendedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _X(image->GetX()),
    _Y(image->GetY())
  {}

  ConvolveExtendedForegroundInY(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ExtendedForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _X(image->GetX()),
    _Y(image->GetY())
  {}

  template <class T1, class T2>
  void operator ()(int, int j, int, int, const T1 *in, T2 *out) const
  {
    double acc;
    if (this->ConvolveCenterVoxel(in, acc)) {
      this->ConvolveLeftNeighbors (j, _Y, in, _X, acc);
      this->ConvolveRightNeighbors(j, _Y, in, _X, acc);
    }
    this->Put(out, acc);
  }

  int _X; ///< Number of voxels in x dimension (nx)
  int _Y; ///< Number of voxels in y dimension (ny)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along z dimension
template <class TKernel = double>
struct ConvolveExtendedForegroundInZ : public ExtendedForegroundConvolution1D<TKernel>
{
  ConvolveExtendedForegroundInZ(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
  :
    ExtendedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _XY(image->GetX() * image->GetY()),
    _Z (image->GetZ())
  {}

  ConvolveExtendedForegroundInZ(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ExtendedForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _XY(image->GetX() * image->GetY()),
    _Z (image->GetZ())
  {}

  template <class T1, class T2>
  void operator ()(int, int, int k, int, const T1 *in, T2 *out) const
  {
    double acc;
    if (this->ConvolveCenterVoxel(in, acc)) {
      this->ConvolveLeftNeighbors (k, _Z, in, _XY, acc);
      this->ConvolveRightNeighbors(k, _Z, in, _XY, acc);
    }
    this->Put(out, acc);
  }

  int _XY; ///< Number of voxels in slice (nx * ny)
  int _Z;  ///< Number of voxels in z dimension (nz)
};

// -----------------------------------------------------------------------------
/// Compute convolution for given voxel along t dimension
template <class TKernel = double>
struct ConvolveExtendedForegroundInT : public ExtendedForegroundConvolution1D<TKernel>
{
  ConvolveExtendedForegroundInT(const irtkBaseImage *image, const TKernel *kernel, int size, double norm = 1.0)
  :
    ExtendedForegroundConvolution1D<TKernel>(image, kernel, size, norm),
    _XYZ(image->GetX() * image->GetY() * image->GetZ()),
    _T  (image->GetT())
  {}

  ConvolveExtendedForegroundInT(const irtkBaseImage *image, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ExtendedForegroundConvolution1D<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _XYZ(image->GetX() * image->GetY() * image->GetZ()),
    _T  (image->GetT())
  {}

  template <class T1, class T2>
  void operator ()(int, int, int, int t, const T1 *in, T2 *out) const
  {
    double acc;
    if (this->ConvolveCenterVoxel(in, acc)) {
      this->ConvolveLeftNeighbors (t, _T, in, _XYZ, acc);
      this->ConvolveRightNeighbors(t, _T, in, _XYZ, acc);
    }
    this->Put(out, acc);
  }

  int _XYZ; ///< Number of voxels in frame (nx * ny * nz)
  int _T;   ///< Number of voxels in t dimension (nt)
};

// =============================================================================
// Smooth and downsample mirrored foreground in one step
// =============================================================================

// -----------------------------------------------------------------------------
/// Downsample image using convolution of original voxels with kernel in x dimension
template <class TVoxel, class TKernel = irtkRealPixel>
struct DownsampleConvolvedMirroredForegroundInX : public ConvolveMirroredForegroundInX<TKernel>
{
  DownsampleConvolvedMirroredForegroundInX(const irtkGenericImage<TVoxel> *image, int m, const TKernel *kernel, int size, double norm = 1.0)
  :
    ConvolveMirroredForegroundInX<TKernel>(image, kernel, size, norm),
    _Input(image), _Offset((image->GetX() % m + 1) / 2), _Factor(m)
  {}

  DownsampleConvolvedMirroredForegroundInX(const irtkGenericImage<TVoxel> *image, int m, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ConvolveMirroredForegroundInX<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Input(image), _Offset((image->GetX() % m + 1) / 2), _Factor(m)
  {}

  template <class T>
  void operator ()(int i, int j, int k, int l, T *out) const
  {
    i = _Offset + i * _Factor;
    ConvolveMirroredForegroundInX<TKernel>::operator ()(i, j, k, l, _Input->GetPointerToVoxels(i, j, k, l), out);
  }

  const irtkGenericImage<TVoxel> *_Input;  ///< Image to downsample
  int                             _Offset; ///< Offset of first voxel
  int                             _Factor; ///< Downsampling factor
};

// -----------------------------------------------------------------------------
/// Downsample image using convolution of original voxels with kernel in y dimension
template <class TVoxel, class TKernel = irtkRealPixel>
struct DownsampleConvolvedMirroredForegroundInY : public ConvolveMirroredForegroundInY<TKernel>
{
  DownsampleConvolvedMirroredForegroundInY(const irtkGenericImage<TVoxel> *image, int m, const TKernel *kernel, int size, double norm = 1.0)
  :
    ConvolveMirroredForegroundInY<TKernel>(image, kernel, size, norm),
    _Input(image), _Offset((image->GetY() % m + 1) / 2), _Factor(m)
  {}

  DownsampleConvolvedMirroredForegroundInY(const irtkGenericImage<TVoxel> *image, int m, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ConvolveMirroredForegroundInY<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Input(image), _Offset((image->GetY() % m + 1) / 2), _Factor(m)
  {}

  template <class T>
  void operator ()(int i, int j, int k, int l, T *out) const
  {
    j = _Offset + j * _Factor;
    ConvolveMirroredForegroundInY<TKernel>::operator ()(i, j, k, l, _Input->GetPointerToVoxels(i, j, k, l), out);
  }

  const irtkGenericImage<TVoxel> *_Input;  ///< Image to downsample
  int                             _Offset; ///< Offset of first voxel
  int                             _Factor; ///< Downsampling factor
};

// -----------------------------------------------------------------------------
/// Downsample image using convolution of original voxels with kernel in z dimension
template <class TVoxel, class TKernel = irtkRealPixel>
struct DownsampleConvolvedMirroredForegroundInZ : public ConvolveMirroredForegroundInZ<TKernel>
{
  DownsampleConvolvedMirroredForegroundInZ(const irtkGenericImage<TVoxel> *image, int m, const TKernel *kernel, int size, double norm = 1.0)
  :
    ConvolveMirroredForegroundInZ<TKernel>(image, kernel, size, norm),
    _Input(image), _Offset((image->GetZ() % m + 1) / 2), _Factor(m)
  {}

  DownsampleConvolvedMirroredForegroundInZ(const irtkGenericImage<TVoxel> *image, int m, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ConvolveMirroredForegroundInZ<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Input(image), _Offset((image->GetZ() % m + 1) / 2), _Factor(m)
  {}

  template <class T>
  void operator ()(int i, int j, int k, int l, T *out) const
  {
    k = _Offset + k * _Factor;
    ConvolveMirroredForegroundInZ<TKernel>::operator ()(i, j, k, l, _Input->GetPointerToVoxels(i, j, k, l), out);
  }

  const irtkGenericImage<TVoxel> *_Input;  ///< Image to downsample
  int                             _Offset; ///< Offset of first voxel
  int                             _Factor; ///< Downsampling factor
};

// =============================================================================
// Smooth and downsample extended foreground in one step
// =============================================================================

// -----------------------------------------------------------------------------
/// Downsample image using convolution of original voxels with kernel in x dimension
template <class TVoxel, class TKernel = irtkRealPixel>
struct DownsampleConvolvedExtendedForegroundInX : public ConvolveExtendedForegroundInX<TKernel>
{
  DownsampleConvolvedExtendedForegroundInX(const irtkGenericImage<TVoxel> *image, int m, const TKernel *kernel, int size, double norm = 1.0)
  :
    ConvolveExtendedForegroundInX<TKernel>(image, kernel, size, norm),
    _Input(image), _Offset((image->GetX() % m + 1) / 2), _Factor(m)
  {}

  DownsampleConvolvedExtendedForegroundInX(const irtkGenericImage<TVoxel> *image, int m, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ConvolveExtendedForegroundInX<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Input(image), _Offset((image->GetX() % m + 1) / 2), _Factor(m)
  {}

  template <class T>
  void operator ()(int i, int j, int k, int l, T *out) const
  {
    i = _Offset + i * _Factor;
    ConvolveExtendedForegroundInX<TKernel>::operator ()(i, j, k, l, _Input->GetPointerToVoxels(i, j, k, l), out);
  }

  const irtkGenericImage<TVoxel> *_Input;  ///< Image to downsample
  int                             _Offset; ///< Offset of first voxel
  int                             _Factor; ///< Downsampling factor
};

// -----------------------------------------------------------------------------
/// Downsample image using convolution of original voxels with kernel in y dimension
template <class TVoxel, class TKernel = irtkRealPixel>
struct DownsampleConvolvedExtendedForegroundInY : public ConvolveExtendedForegroundInY<TKernel>
{
  DownsampleConvolvedExtendedForegroundInY(const irtkGenericImage<TVoxel> *image, int m, const TKernel *kernel, int size, double norm = 1.0)
  :
    ConvolveExtendedForegroundInY<TKernel>(image, kernel, size, norm),
    _Input(image), _Offset((image->GetY() % m + 1) / 2), _Factor(m)
  {}

  DownsampleConvolvedExtendedForegroundInY(const irtkGenericImage<TVoxel> *image, int m, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ConvolveExtendedForegroundInY<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Input(image), _Offset((image->GetY() % m + 1) / 2), _Factor(m)
  {}

  template <class T>
  void operator ()(int i, int j, int k, int l, T *out) const
  {
    j = _Offset + j * _Factor;
    ConvolveExtendedForegroundInY<TKernel>::operator ()(i, j, k, l, _Input->GetPointerToVoxels(i, j, k, l), out);
  }

  const irtkGenericImage<TVoxel> *_Input;  ///< Image to downsample
  int                             _Offset; ///< Offset of first voxel
  int                             _Factor; ///< Downsampling factor
};

// -----------------------------------------------------------------------------
/// Downsample image using convolution of original voxels with kernel in z dimension
template <class TVoxel, class TKernel = irtkRealPixel>
struct DownsampleConvolvedExtendedForegroundInZ : public ConvolveExtendedForegroundInZ<TKernel>
{
  DownsampleConvolvedExtendedForegroundInZ(const irtkGenericImage<TVoxel> *image, int m, const TKernel *kernel, int size, double norm = 1.0)
  :
    ConvolveExtendedForegroundInZ<TKernel>(image, kernel, size, norm),
    _Input(image), _Offset((image->GetZ() % m + 1) / 2), _Factor(m)
  {}

  DownsampleConvolvedExtendedForegroundInZ(const irtkGenericImage<TVoxel> *image, int m, const irtkGenericImage<TKernel> *kernel, double norm = 1.0)
  :
    ConvolveExtendedForegroundInZ<TKernel>(image, kernel->GetPointerToVoxels(), kernel->GetNumberOfVoxels(), norm),
    _Input(image), _Offset((image->GetZ() % m) / 2), _Factor(m)
  {}

  template <class T>
  void operator ()(int i, int j, int k, int l, T *out) const
  {
    k = _Offset + k * _Factor;
    ConvolveExtendedForegroundInZ<TKernel>::operator ()(i, j, k, l, _Input->GetPointerToVoxels(i, j, k, l), out);
  }

  const irtkGenericImage<TVoxel> *_Input;  ///< Image to downsample
  int                             _Offset; ///< Offset of first voxel
  int                             _Factor; ///< Downsampling factor
};


} // namespace irtkVoxelConvolution

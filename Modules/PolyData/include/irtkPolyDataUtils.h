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

#ifndef _IRTKPOLYDATAUTILS_H
#define _IRTKPOLYDATAUTILS_H

#include <vtkSmartPointer.h>
#include <vtkPointSet.h>
#include <vtkPolyData.h>


/**
 * @file  irtkPolyDataUtils.h
 * @brief Utility functions for dealing with VTK polydata
 *
 * These functions are in particular used by the registration package.
 */

namespace irtk { namespace polydata {

class irtkEdgeTable;


// =============================================================================
// I/O
// =============================================================================

/// Default extension for given data set
const char *DefaultExtension(vtkDataSet *);

/// Read point set from file
///
/// @param[in]  fname File name.
/// @param[out] ftype File type (VTK_ASCII or VTK_BINARY) of legacy VTK input file.
///
/// @return Point set read from file. The returned point set will have no points
///         when the function failed to read the specified file.
vtkSmartPointer<vtkPointSet> ReadPointSet(const char *fname, int *ftype = NULL);

/// Read polygonal dataset from file
///
/// @param[in]  fname File name.
/// @param[out] ftype File type (VTK_ASCII or VTK_BINARY) of legacy VTK input file.
///
/// @return Polydata read from file. The returned polydata will have no points
///         and cells when the function failed to read the specified file.
vtkSmartPointer<vtkPolyData> ReadPolyData(const char *fname, int *ftype = NULL);

/// Write point set to file
///
/// @param fname    File name. The extension determines the output format.
/// @param pointset Point set to write.
/// @param compress Whether to use compression when writing to VTK XML format (.vtp).
/// @param ascii    Whether to use ASCII format when writing to legacy VTK format (.vtk).
bool WritePointSet(const char *fname, vtkPointSet *pointset, bool compress = true, bool ascii = false);

/// Write polygonal dataset to file
///
/// @param fname    File name. The extension determines the output format.
/// @param polydata Polydata to write.
/// @param compress Whether to use compression when writing to VTK XML format (.vtp).
/// @param ascii    Whether to use ASCII format when writing to legacy VTK format (.vtk).
bool WritePolyData(const char *fname, vtkPolyData *polydata, bool compress = true, bool ascii = false);

// =============================================================================
// Point/cell data
// =============================================================================

/// Map string to vtkDataSetAttributes::AttributeTypes enumeration value
///
/// @param type Case insensitive attribute type name (e.g. "scalars", "NORMALS")
///
/// @return VTK data set attribute type ID (e.g., vtkDataSetAttributes::SCALARS)
///         or -1 if string does not name a known attribute type.
int PolyDataAttributeType(const char *type);

/// Get point data array using case insensitive name
///
/// @param[in]  data Point or cell data attributes (cf. vtkPolyData::GetPointData, vtkPolyData::GetCellData).
/// @param[in]  name Case insenitive name of data array.
/// @param[out] loc  Set to array index if not @c NULL.
///
/// @return Pointer to data array or @c NULL if not found.
vtkDataArray *GetArrayByCaseInsensitiveName(vtkDataSetAttributes *data, const char *name, int *loc = NULL);

/// Copy named data array from one dataset attributes to another
///
/// If an array with the given name exists in the destination dataset attributes,
/// it's data is overridden using the vtkDataSetAttributes::DeepCopy function.
/// Otherwise, a new array is added.
///
/// @param dst  Destination dataset attributes.
/// @param src  Source dataset attributes.
/// @param name Case insensitive name of data array.
///
/// @return Index of array in destination or -1 if source array not found.
int DeepCopyArrayUsingCaseInsensitiveName(vtkDataSetAttributes *dst,
                                          vtkDataSetAttributes *src,
                                          const char *name);

// =============================================================================
// Cell attributes
// =============================================================================

/// Compute area of cell
///
/// @return Area of cell or NaN if cell type is not supported.
double ComputeArea(vtkCell *cell);

/// Compute volume of cell
///
/// @return Volume of cell or NaN if cell type is not supported.
double ComputeVolume(vtkCell *cell);

// =============================================================================
// Surface meshes
// =============================================================================

/// Calculate are of triangle with given edge length
inline double EdgeLengthToTriangleArea(double l)
{
  return 0.25 * sqrt(3.0) * l * l;
}

/// Determine whether a point set is a surface mesh
bool IsSurfaceMesh(vtkDataSet *);

/// Check whether given point set is a triangular mesh
bool IsTriangularMesh(vtkDataSet *);

/// Check whether given point set is a tetrahedral mesh
bool IsTetrahedralMesh(vtkDataSet *);

/// Determine average edge length of point set given a precomputed edge table
double AverageEdgeLength(vtkSmartPointer<vtkPoints>, const irtkEdgeTable &);

/// Determine average edge length of point set
double AverageEdgeLength(vtkSmartPointer<vtkPointSet>);

/// Determine median edge length of point set given a precomputed edge table
double MedianEdgeLength(vtkSmartPointer<vtkPoints>, const irtkEdgeTable &);

/// Determine median edge length of point set
double MedianEdgeLength(vtkSmartPointer<vtkPointSet>);

/// Determine average edge length of point set given a precomputed edge table
///
/// This function only considers edges with a length within in the 5th and 95th
/// percentile of all edge lengths. It thus ignores extrem short/long edges.
double RobustAverageEdgeLength(vtkSmartPointer<vtkPoints>, const irtkEdgeTable &);

/// Determine average edge length of point set
double RobustAverageEdgeLength(vtkSmartPointer<vtkPointSet>);

/// Determine minimum and maximum edge length
///
/// \param[in]  points    Points.
/// \param[in]  edgeTable Edge table.
/// \param[out] min       Minimum edge length.
/// \param[out] max       Maximum edge length.
void GetMinMaxEdgeLength(vtkSmartPointer<vtkPoints>, const irtkEdgeTable &, double &min, double &max);

/// Determine minimum and maximum edge length
///
/// \param[in]  pointset Point set.
/// \param[out] min      Minimum edge length.
/// \param[out] max      Maximum edge length.
void GetMinMaxEdgeLength(vtkSmartPointer<vtkPointSet> pointset, double &min, double &max);

/// Determine minimum edge length
///
/// \param[in] points    Points.
/// \param[in] edgeTable Edge table.
///
/// \return Minimum edge length.
double MinEdgeLength(vtkSmartPointer<vtkPoints>, const irtkEdgeTable &);

/// Determine minimum edge length
///
/// \param[in] pointset Point set.
///
/// \return Minimum edge length.
double MinEdgeLength(vtkSmartPointer<vtkPointSet> pointset);

/// Determine maximum edge length
///
/// \param[in] points    Points.
/// \param[in] edgeTable Edge table.
///
/// \return Maximum edge length.
double MaxEdgeLength(vtkSmartPointer<vtkPoints>, const irtkEdgeTable &);

/// Determine maximum edge length
///
/// \param[in] pointset Point set.
///
/// \return Maximum edge length.
double MaxEdgeLength(vtkSmartPointer<vtkPointSet> pointset);

/// Compute statistics of edge lengths
///
/// \param[in]  points    Points.
/// \param[in]  edgeTable Edge table.
/// \param[out] mean      Average edge length.
/// \param[out] sigma     Standard deviation of edge length.
void EdgeLengthNormalDistribution(vtkSmartPointer<vtkPoints>, const irtkEdgeTable &, double &mean, double &sigma);

/// Compute statistics of edge lengths
///
/// \param[in]  pointset Point set.
/// \param[out] mean     Average edge length.
/// \param[out] sigma    Standard deviation of edge length.
void EdgeLengthNormalDistribution(vtkSmartPointer<vtkPointSet> pointset, double &mean, double &sigma);

/// Get approximate volume enclosed by polygonal mesh
double GetVolume(vtkSmartPointer<vtkPolyData>);

/// Get boundary surface mesh
///
/// @param[in] dataset     Dataset whose boundary surface is extracted.
/// @param[in] passPtIds   Whether to pass point array with IDs of points in @p dataset.
/// @param[in] passCellIds Whether to pass cell array with IDs of cells in @p dataset.
///
/// @return Boundary surface mesh.
vtkSmartPointer<vtkPolyData> DataSetSurface(vtkSmartPointer<vtkDataSet> dataset,
                                            bool passPtIds   = false,
                                            bool passCellIds = false);

/// Get convex hull of point set
///
/// @param[in] pointset Input point set.
/// @param[in] delaunay Whether to generate triangulation using delaunay filter.
///                     This also includes a stratification of input surface
///                     points to avoid numerical errors.
///
/// @return Convex hull of input point set.
vtkSmartPointer<vtkPolyData> ConvexHull(vtkSmartPointer<vtkPointSet> pointset, bool delaunay = false);

/// Triangulate surface mesh
vtkSmartPointer<vtkPolyData> Triangulate(vtkSmartPointer<vtkPolyData>);

/// Tetrahedralize the interior of a piecewise linear complex (PLC)
vtkSmartPointer<vtkPointSet> Tetrahedralize(vtkSmartPointer<vtkPointSet>);


} } // namespace irtk::polydata

#endif

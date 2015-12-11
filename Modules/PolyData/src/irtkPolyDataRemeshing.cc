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

#include <irtkPolyDataRemeshing.h>

#include <irtkCommon.h>
#include <irtkTransformation.h>
#include <irtkEdgeTable.h>
#include <irtkPolyDataSmoothing.h>
#include <irtkDataStatistics.h>
#include <irtkPolyDataUtils.h>

#include <vtkMath.h>
#include <vtkIdList.h>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkTriangle.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkMergePoints.h>
#include <vtkPriorityQueue.h>


namespace irtk { namespace polydata {


// =============================================================================
// Construction/Destruction
// =============================================================================

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::Copy(const irtkPolyDataRemeshing &other)
{
  _Transformation                 = other._Transformation;
  _SkipTriangulation              = other._SkipTriangulation;
  _MinFeatureAngle                = other._MinFeatureAngle;
  _MinFeatureAngleCos             = other._MinFeatureAngleCos;
  _MaxFeatureAngle                = other._MaxFeatureAngle;
  _MaxFeatureAngleCos             = other._MaxFeatureAngleCos;
  _MinEdgeLength                  = other._MinEdgeLength;
  _MinEdgeLengthSquared           = other._MinEdgeLengthSquared;
  _MaxEdgeLength                  = other._MaxEdgeLength;
  _MaxEdgeLengthSquared           = other._MaxEdgeLengthSquared;
  _AdaptiveEdgeLengthArrayName    = other._AdaptiveEdgeLengthArrayName;
  _MeltingOrder                   = other._MeltingOrder;
  _MeltNodes                      = other._MeltNodes;
  _MeltTriangles                  = other._MeltTriangles;
  _NumberOfMeltedNodes            = other._NumberOfMeltedNodes;
  _NumberOfMeltedEdges            = other._NumberOfMeltedEdges;
  _NumberOfMeltedCells            = other._NumberOfMeltedCells;
  _NumberOfInversions             = other._NumberOfInversions;
  _NumberOfBisections             = other._NumberOfBisections;
  _NumberOfTrisections            = other._NumberOfTrisections;
  _NumberOfQuadsections           = other._NumberOfQuadsections;
  // Get output point labels from _Output (copied by irtkPolyDataFilter::Copy)
  if (_Output) {
    _OutputPointLabels  = GetArrayByCaseInsensitiveName(_Output->GetPointData(), "labels");
    _MinEdgeLengthArray = _Output->GetPointData()->GetArray("MinEdgeLength");
    _MaxEdgeLengthArray = _Output->GetPointData()->GetArray("MaxEdgeLength");
  } else {
    _OutputPointLabels  = NULL;
    _MinEdgeLengthArray = NULL;
    _MaxEdgeLengthArray = NULL;
  }
}

// -----------------------------------------------------------------------------
irtkPolyDataRemeshing::irtkPolyDataRemeshing()
:
  _Transformation(NULL),
  _SkipTriangulation(false),
  _MinFeatureAngle(180.0),
  _MinFeatureAngleCos(2.0),
  _MaxFeatureAngle(180.0),
  _MaxFeatureAngleCos(2.0),
  _MinEdgeLength(.0),
  _MinEdgeLengthSquared(.0),
  _MaxEdgeLength(numeric_limits<double>::infinity()),
  _MaxEdgeLengthSquared(numeric_limits<double>::infinity()),
  _MeltingOrder(AREA),
  _MeltNodes(false),
  _MeltTriangles(false),
  _NumberOfMeltedNodes(0),
  _NumberOfMeltedEdges(0),
  _NumberOfMeltedCells(0),
  _NumberOfInversions(0),
  _NumberOfBisections(0),
  _NumberOfTrisections(0),
  _NumberOfQuadsections(0)
{
}

// -----------------------------------------------------------------------------
irtkPolyDataRemeshing::irtkPolyDataRemeshing(const irtkPolyDataRemeshing &other)
:
  irtkPolyDataFilter(other)
{
  Copy(other);
}

// -----------------------------------------------------------------------------
irtkPolyDataRemeshing &irtkPolyDataRemeshing::operator =(const irtkPolyDataRemeshing &other)
{
  irtkPolyDataFilter::operator =(other);
  Copy(other);
  return *this;
}

// -----------------------------------------------------------------------------
irtkPolyDataRemeshing::~irtkPolyDataRemeshing()
{
}

// =============================================================================
// Auxiliary functions
// =============================================================================

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing::GetPoint(vtkIdType ptId, double p[3]) const
{
  _Output->GetPoint(ptId, p);
  if (_Transformation) _Transformation->Transform(p[0], p[1], p[2]);
}

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing::GetNormal(vtkIdType ptId, double n[3]) const
{
  // TODO: Need to compute normal of transformed surface if _Transformation != NULL.
  //       Can be computed from normals of adjacent triangles which in turn
  //       must be computed from the transformed triangle corners (cf. GetPoint)
  _Output->GetPointData()->GetNormals()->GetTuple(ptId, n);
}

// -----------------------------------------------------------------------------
inline double irtkPolyDataRemeshing::ComputeArea(vtkIdType cellId) const
{
  vtkIdType npts, *pts;
  _Output->GetCellPoints(cellId, npts, pts);
  double p1[3], p2[3], p3[3];
  GetPoint(pts[0], p1);
  GetPoint(pts[1], p2);
  GetPoint(pts[2], p3);
  return vtkTriangle::TriangleArea(p1, p2, p3);
}

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing::MiddlePoint(vtkIdType ptId1, vtkIdType ptId2, double p[3]) const
{
  double p2[3];
  _Output->GetPoint(ptId1, p);
  _Output->GetPoint(ptId2, p2);
  p[0] += p2[0], p[1] += p2[1], p[2] += p2[2];
  p[0] *= .5, p[1] *= .5, p[2] *= .5;
}

// -----------------------------------------------------------------------------
inline irtkPoint irtkPolyDataRemeshing::MiddlePoint(vtkIdType ptId1, vtkIdType ptId2) const
{
  double p[3];
  MiddlePoint(ptId1, ptId2, p);
  return p;
}

// -----------------------------------------------------------------------------
inline int irtkPolyDataRemeshing::NodeConnectivity(vtkIdType ptId) const
{
  unsigned short ncells;
  vtkIdType      *cells;
  _Output->GetPointCells(ptId, ncells, cells);
  return ncells;
}

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing::DeleteCell(vtkIdType cellId)
{
  _Output->RemoveCellReference(cellId); // before marking it as deleted!
  _Output->DeleteCell(cellId);
}

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing::DeleteCells(vtkIdList *cellIds)
{
  for (vtkIdType i = 0; i < cellIds->GetNumberOfIds(); ++i) DeleteCell(cellIds->GetId(i));
}

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing::ReplaceCellPoint(vtkIdType cellId, vtkIdType oldPtId, vtkIdType newPtId)
{
  vtkIdType npts, *pts;
  _Output->GetCellPoints(cellId, npts, pts);
  for (vtkIdType j = 0; j < npts; ++j) {
    if (pts[j] == oldPtId) pts[j] = newPtId;
  }
  _Output->RemoveReferenceToCell(oldPtId, cellId); // NOT THREAD SAFE!
  _Output->ResizeCellList(newPtId, 1);
  _Output->AddReferenceToCell(newPtId, cellId);
  // Note: Not necessary as we manipulated the pts array directly!
  //_Output->ReplaceCell(cellId, npts, pts);
}

// -----------------------------------------------------------------------------
inline vtkIdType irtkPolyDataRemeshing::GetCellEdgeNeighbor(vtkIdType cellId, vtkIdType ptId1, vtkIdType ptId2) const
{
  vtkSmartPointer<vtkIdList> cellIds = vtkSmartPointer<vtkIdList>::New();
  _Output->GetCellEdgeNeighbors(cellId, ptId1, ptId2, cellIds);
  vtkIdType neighborCellId = -1;
  for (vtkIdType i = 0; i < cellIds->GetNumberOfIds(); ++i) {
    if (_Output->GetCellType(cellIds->GetId(i)) != VTK_EMPTY_CELL) {
      if (neighborCellId != -1) return -1; // should not happen
      neighborCellId = cellIds->GetId(i);
    }
  }
  return neighborCellId;
}

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing
::GetCellPointNeighbors(vtkIdType cellId, vtkIdType ptId, vtkIdList *ptIds) const
{
  ptIds->Reset();
  unsigned short ncells;
  vtkIdType *cells, *pts, npts;
  _Output->GetPointCells(ptId, ncells, cells);
  for (unsigned short i = 0; i < ncells; ++i) {
    if (cells[i] != cellId) {
      _Output->GetCellPoints(cells[i], npts, pts);
      for (vtkIdType j = 0; j < npts; ++j) {
        if (pts[j] != ptId) ptIds->InsertUniqueId(pts[j]);
      }
    }
  }
}

// -----------------------------------------------------------------------------
inline vtkIdType irtkPolyDataRemeshing
::GetCellEdgeNeighborPoint(vtkIdType cellId, vtkIdType ptId1, vtkIdType ptId2, bool mergeTriples)
{
  unsigned short ncells;
  vtkIdType npts, *pts, *cells;

  // Get points which are adjacent to both edge points
  vtkSmartPointer<vtkIdList> ptIds1 = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> ptIds2 = vtkSmartPointer<vtkIdList>::New();

  GetCellPointNeighbors(cellId, ptId1, ptIds1);
  GetCellPointNeighbors(cellId, ptId2, ptIds2);

  ptIds1->IntersectWith(ptIds2);

  // Intersection should only contain the other point of this cell
  // and the third point belonging to the cell edge neighbor
  if (ptIds1->GetNumberOfIds() != 2) return -1;

  // Get third point defining this cell
  _Output->GetCellPoints(cellId, npts, pts);
  vtkIdType ptId3 = -1;
  for (vtkIdType i = 0; i < npts; ++i) {
    if (pts[i] != ptId1 && pts[i] != ptId2) {
      ptId3 = pts[i];
      break;
    }
  }

  // Get other cell edge neighbor point
  vtkIdType adjPtId = ptIds1->GetId(vtkIdType(ptIds1->GetId(0) == ptId3));

  // Check/adjust connectivity of adjacent point
  switch (NodeConnectivity(adjPtId)) {
    case 1: case 2:
      return -1; // should never happen
    case 3: {
      if (!mergeTriples) return -1;
      // Merge three adjacent triangles into one
      //
      // TODO: Only when the lengths of the edges of the resulting triangle
      //       are within the valid range [_MinEdgeLength, _MaxEdgeLength]?
      //       and if the normals of the three triangles make up an angle
      //       less than the _MinFeatureAngle.
      vtkIdType newPtId = -1;
      _Output->GetPointCells(adjPtId, ncells, cells);
      for (unsigned short i = 0; i < ncells; ++i) {
        _Output->GetCellPoints(cells[i], npts, pts);
        for (vtkIdType j = 0; j < npts; ++j) {
          if (pts[j] != ptId1 && pts[j] != ptId2) {
            newPtId = pts[j];
            break;
          }
        }
      }
      if (newPtId != -1) {
        ReplaceCellPoint(cells[0], adjPtId, newPtId);
        DeleteCell(cells[1]);
        DeleteCell(cells[2]);
        ++_NumberOfMeltedNodes;
      }
      adjPtId = newPtId;
    } break;
  }

  return adjPtId;
}

// -----------------------------------------------------------------------------
irtkPolyDataRemeshing::CellQueue irtkPolyDataRemeshing::QueueCellsByArea() const
{
  CellQueue queue;
  queue.reserve(_Output->GetNumberOfCells());

  CellInfo cur;
  for (cur.cellId = 0; cur.cellId < _Output->GetNumberOfCells(); ++cur.cellId) {
    if (_Output->GetCellType(cur.cellId) == VTK_TRIANGLE) {
      cur.priority = ComputeArea(cur.cellId);
      queue.push_back(cur);
    }
  }

  sort(queue.begin(), queue.end());
  return queue;
}

// -----------------------------------------------------------------------------
irtkPolyDataRemeshing::CellQueue irtkPolyDataRemeshing::QueueCellsByShortestEdge() const
{
  CellInfo  cur;
  vtkIdType npts, *pts;
  double    p1[3], p2[3], p3[3];

  CellQueue queue;
  queue.reserve(_Output->GetNumberOfCells());

  for (cur.cellId = 0; cur.cellId < _Output->GetNumberOfCells(); ++cur.cellId) {
    _Output->GetCellPoints(cur.cellId, npts, pts);
    if (npts == 0) continue;
    GetPoint(pts[0], p1);
    GetPoint(pts[1], p2);
    GetPoint(pts[2], p3);
    cur.priority = min(min(vtkMath::Distance2BetweenPoints(p1, p2),
                           vtkMath::Distance2BetweenPoints(p2, p3)),
                           vtkMath::Distance2BetweenPoints(p1, p3));
    queue.push_back(cur);
  }

  sort(queue.begin(), queue.end());
  return queue;
}

// -----------------------------------------------------------------------------
irtkPolyDataRemeshing::EdgeQueue irtkPolyDataRemeshing::QueueEdgesByLength() const
{
  EdgeInfo cur;
  double   p1[3], p2[3];

  const irtkEdgeTable edgeTable(_Output);

  EdgeQueue queue;
  queue.reserve(edgeTable.NumberOfEdges());

  irtkEdgeIterator it(edgeTable);
  for (it.InitTraversal(); it.GetNextEdge(cur.ptId1, cur.ptId2) != -1;) {
    GetPoint(cur.ptId1, p1);
    GetPoint(cur.ptId2, p2);
    cur.priority = vtkMath::Distance2BetweenPoints(p1, p2);
    queue.push_back(cur);
  }

  sort(queue.begin(), queue.end());
  return queue;
}

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing
::InterpolatePointData(vtkIdType newId, vtkIdType ptId1, vtkIdType ptId2)
{
  vtkPointData *pd = _Output->GetPointData();
  if (pd == NULL) return;
  if (_OutputPointLabels) {
    double label = _OutputPointLabels->GetComponent(ptId1, 0);
    pd->InterpolateEdge(pd, newId, ptId1, ptId2, .5);
    _OutputPointLabels->SetComponent(newId, 0, label);
  } else {
    pd->InterpolateEdge(pd, newId, ptId1, ptId2, .5);
  }
}

// -----------------------------------------------------------------------------
inline void irtkPolyDataRemeshing
::InterpolatePointData(vtkIdType newId, vtkIdList *ptIds, double *weights)
{
  vtkPointData *pd = _Output->GetPointData();
  if (pd == NULL) return;
  if (_OutputPointLabels) {
    double label = _OutputPointLabels->GetComponent(ptIds->GetId(0), 0);
    pd->InterpolatePoint(pd, newId, ptIds, weights);
    _OutputPointLabels->SetComponent(newId, 0, label);
  } else {
    pd->InterpolatePoint(pd, newId, ptIds, weights);
  }
}

// -----------------------------------------------------------------------------
inline double irtkPolyDataRemeshing
::SquaredMinEdgeLength(vtkIdType ptId1, vtkIdType ptId2) const
{
  if (_MinEdgeLengthArray) {
    double l = .5 * (_MinEdgeLengthArray->GetComponent(ptId1, 0) +
                     _MinEdgeLengthArray->GetComponent(ptId2, 0));
    return l * l;
  } else {
    return _MinEdgeLengthSquared;
  }
}

// -----------------------------------------------------------------------------
inline double irtkPolyDataRemeshing
::SquaredMaxEdgeLength(vtkIdType ptId1, vtkIdType ptId2) const
{
  if (_MaxEdgeLengthArray) {
    double l = .5 * (_MaxEdgeLengthArray->GetComponent(ptId1, 0) +
                     _MaxEdgeLengthArray->GetComponent(ptId2, 0));
    return l * l;
  } else {
    return _MaxEdgeLengthSquared;
  }
}

// =============================================================================
// Local remeshing operations
// =============================================================================

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::MeltTriplets()
{
  IRTK_START_TIMING();

  bool replace;
  vtkIdType npts, *pts;
  double    p1[3], p2[3], p3[3];
  vtkSmartPointer<vtkIdList> cellIds = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> ptIds   = vtkSmartPointer<vtkIdList>::New();

  for (vtkIdType ptId = 0; ptId < _Output->GetNumberOfPoints(); ++ptId) {
    _Output->GetPointCells(ptId, cellIds);
    if (cellIds->GetNumberOfIds() == 3) {
      replace = false;
      ptIds->Reset();
      for (vtkIdType i = 0; i < cellIds->GetNumberOfIds(); ++i) {
        _Output->GetCellPoints(cellIds->GetId(i), npts, pts);
        GetPoint(pts[0], p1);
        GetPoint(pts[1], p2);
        GetPoint(pts[2], p3);
        if (vtkMath::Distance2BetweenPoints(p1, p2) < SquaredMinEdgeLength(pts[0], pts[1]) ||
            vtkMath::Distance2BetweenPoints(p2, p3) < SquaredMinEdgeLength(pts[1], pts[2]) ||
            vtkMath::Distance2BetweenPoints(p3, p1) < SquaredMinEdgeLength(pts[2], pts[0])) {
          replace = true;
        }
        for (vtkIdType j = 0; j < npts; ++j) {
          if (pts[j] != ptId) ptIds->InsertUniqueId(pts[j]);
        }
      }
      replace = (replace && ptIds->GetNumberOfIds() == 3);
      if (replace) {
        replace = NodeConnectivity(ptIds->GetId(0)) > 3 &&
                  NodeConnectivity(ptIds->GetId(1)) > 3 &&
                  NodeConnectivity(ptIds->GetId(2)) > 3;
      }
      if (replace) {
        _Output->GetCellPoints(cellIds->GetId(0), npts, pts);
        vtkIdType newPtId = -1;
        for (vtkIdType i = 0; newPtId == -1 && i < ptIds->GetNumberOfIds(); ++i) {
          newPtId = ptIds->GetId(i);
          for (vtkIdType j = 0; j < npts; ++j) {
            if (newPtId == pts[j]) {
              newPtId = -1;
              break;
            }
          }
        }
        ReplaceCellPoint(cellIds->GetId(0), ptId, newPtId);
        DeleteCell(cellIds->GetId(1));
        DeleteCell(cellIds->GetId(2));
        ++_NumberOfMeltedNodes;
      }
    }
  }

  IRTK_DEBUG_TIMING(2, "melting nodes with connectivity 3");
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing
::MeltEdge(vtkIdType cellId, vtkIdType ptId1, vtkIdType ptId2, vtkIdList *cellIds)
{
  // Check/resolve node connectivity of adjacent points
  vtkIdType neighborPtId = GetCellEdgeNeighborPoint(cellId, ptId1, ptId2, true);
  if (neighborPtId == -1) return;

  // Get edge neighbor cell
  vtkIdType neighborCellId = GetCellEdgeNeighbor(cellId, ptId1, ptId2);
  if (neighborCellId == -1) return;

  // Interpolate point data
  InterpolatePointData(ptId1, ptId1, ptId2);

  // Move first point to edge middlepoint
  irtkPoint m = MiddlePoint(ptId1, ptId2);
  _Output->GetPoints()->SetPoint(ptId1, m._x, m._y, m._z);

  // Replace second point in (remaining) adjacent cells by first point
  _Output->GetPointCells(ptId2, cellIds);
  for (vtkIdType i = 0; i < cellIds->GetNumberOfIds(); ++i) {
    ReplaceCellPoint(cellIds->GetId(i), ptId2, ptId1);
  }

  // Mark cell and its edge neighbor as deleted
  DeleteCell(cellId);
  DeleteCell(neighborCellId);

  ++_NumberOfMeltedEdges;
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::MeltTriangle(vtkIdType cellId, vtkIdList *cellIds)
{
  // Get triangle corners
  vtkSmartPointer<vtkIdList> ptIds = vtkSmartPointer<vtkIdList>::New();
  _Output->GetCellPoints(cellId, ptIds);

  // Get points opposite to cell edges and resolve connectivity of 3 if possible
  vtkIdType ptId1 = GetCellEdgeNeighborPoint(cellId, ptIds->GetId(0), ptIds->GetId(1), true);
  if (ptId1 == -1) return;
  vtkIdType ptId2 = GetCellEdgeNeighborPoint(cellId, ptIds->GetId(1), ptIds->GetId(2), true);
  if (ptId2 == -1) return;
  vtkIdType ptId3 = GetCellEdgeNeighborPoint(cellId, ptIds->GetId(2), ptIds->GetId(0), true);
  if (ptId3 == -1) return;

  // Get adjacent triangles
  vtkIdType neighborCellId1 = GetCellEdgeNeighbor(cellId, ptIds->GetId(0), ptIds->GetId(1));
  if (neighborCellId1 == -1) return;
  vtkIdType neighborCellId2 = GetCellEdgeNeighbor(cellId, ptIds->GetId(1), ptIds->GetId(2));
  if (neighborCellId2 == -1) return;
  vtkIdType neighborCellId3 = GetCellEdgeNeighbor(cellId, ptIds->GetId(2), ptIds->GetId(0));
  if (neighborCellId3 == -1) return;

  // Get triangle center point and interpolation weights
  double pcoords[3], c[3], weights[3];
  vtkCell *cell = _Output->GetCell(cellId);
  int subId = cell->GetParametricCenter(pcoords);
  cell->EvaluateLocation(subId, pcoords, c, weights);

  // Interpolate point data
  InterpolatePointData(ptIds->GetId(0), ptIds, weights);

  // Move first point of this triangle to its center
  _Output->GetPoints()->SetPoint(ptIds->GetId(0), c);

  // Link (non-deleted) cells sharing triangle corners to center point
  // instead of any of the other two deleted points
  _Output->GetPointCells(ptIds->GetId(1), cellIds);
  for (vtkIdType i = 0; i < cellIds->GetNumberOfIds(); ++i) {
    ReplaceCellPoint(cellIds->GetId(i), ptIds->GetId(1), ptIds->GetId(0));
  }

  _Output->GetPointCells(ptIds->GetId(2), cellIds);
  for (vtkIdType i = 0; i < cellIds->GetNumberOfIds(); ++i) {
    ReplaceCellPoint(cellIds->GetId(i), ptIds->GetId(2), ptIds->GetId(0));
  }

  // Delete this and adjacent triangles
  DeleteCell(cellId);
  DeleteCell(neighborCellId1);
  DeleteCell(neighborCellId2);
  DeleteCell(neighborCellId3);

  ++_NumberOfMeltedCells;
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::Melting(vtkIdType cellId, vtkIdList *cellIds)
{
  int       melt[3], i, j;
  double    p1[3], p2[3], p3[3], length2[3], n1[3], n2[3], n3[3];
  vtkIdType npts, *pts;

  _Output->GetCellPoints(cellId, npts, pts);
  if (npts == 0) return; // cell marked as deleted (i.e., VTK_EMPTY_CELL)
  irtkAssert(npts == 3, "surface is triangulated");

  // Get (transformed) point coordinates
  GetPoint(pts[0], p1);
  GetPoint(pts[1], p2);
  GetPoint(pts[2], p3);

  length2[0] = vtkMath::Distance2BetweenPoints(p1, p2);
  length2[1] = vtkMath::Distance2BetweenPoints(p2, p3);
  length2[2] = vtkMath::Distance2BetweenPoints(p3, p1);

  // If triangle-melting is allowed
  if (_MeltTriangles) {

    // Determine which edges are too short
    melt[0] = int(length2[0] < SquaredMinEdgeLength(pts[0], pts[1]));
    melt[1] = int(length2[1] < SquaredMinEdgeLength(pts[1], pts[2]));
    melt[2] = int(length2[2] < SquaredMinEdgeLength(pts[2], pts[0]));

    // Do not melt feature edges (otherwise remeshing may smooth surface too much)
    if ((melt[0] || melt[1] || melt[2]) && _MinFeatureAngle < 180.0) {
      GetNormal(pts[0], n1);
      GetNormal(pts[1], n2);
      GetNormal(pts[2], n3);
      if (melt[0]) melt[0] = int(1.0 - vtkMath::Dot(n1, n2) < _MinFeatureAngleCos);
      if (melt[1]) melt[1] = int(1.0 - vtkMath::Dot(n2, n3) < _MinFeatureAngleCos);
      if (melt[2]) melt[2] = int(1.0 - vtkMath::Dot(n3, n1) < _MinFeatureAngleCos);
    }

    // Perform either edge-melting, triangle-melting, or no operation
    switch (melt[0] + melt[1] + melt[2]) {
      case 1: {
        if      (melt[0]) MeltEdge(cellId, pts[0], pts[1], cellIds);
        else if (melt[1]) MeltEdge(cellId, pts[1], pts[2], cellIds);
        else              MeltEdge(cellId, pts[2], pts[0], cellIds);
      } break;
      case 3: {
        MeltTriangle(cellId, cellIds);
      } break;
    }

  // If only individual edges are allowed to be melted
  } else {

    // Determine shortest edge
    for (i = 0, j = 1; j < 3; ++j) {
      if (length2[j] < length2[i]) i = j;
    }
    j = (i + 1) % 3;

    // Melt edge if it is too short and not a feature edge
    if (length2[i] < SquaredMinEdgeLength(pts[i], pts[j])) {
      if (_MinFeatureAngle < 180.0) {
        GetNormal(pts[i], n1);
        GetNormal(pts[j], n2);
        if (1.0 - vtkMath::Dot(n1, n2) < _MinFeatureAngleCos) {
          MeltEdge(cellId, pts[i], pts[j], cellIds);
        }
      } else {
        MeltEdge(cellId, pts[i], pts[j], cellIds);
      }
    }

  }
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing
::Melting(vtkIdType ptId1, vtkIdList *cellIds1, vtkIdType ptId2, vtkIdList *cellIds2)
{
  _Output->GetPointCells(ptId1, cellIds1);
  _Output->GetPointCells(ptId2, cellIds2);
  cellIds1->IntersectWith(cellIds2);

  if (cellIds1->GetNumberOfIds() != 2) {
    // Either unexpected topology or either one of the points is marked as deleted
    return;
  }

  double p1[3], p2[3], n1[3], n2[3];
  GetPoint(ptId1, p1);
  GetPoint(ptId2, p2);

  if (vtkMath::Distance2BetweenPoints(p1, p2) < SquaredMinEdgeLength(ptId1, ptId2)) {
    if (_MinFeatureAngle < 180.0) {
      GetNormal(ptId1, n1);
      GetNormal(ptId2, n2);
      if (1.0 - vtkMath::Dot(n1, n2) < _MinFeatureAngleCos) {
        MeltEdge(cellIds1->GetId(0), ptId1, ptId2, cellIds2);
      }
    } else {
      MeltEdge(cellIds1->GetId(0), ptId1, ptId2, cellIds2);
    }
  }
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing
::Bisect(vtkIdType cellId, vtkIdType ptId1, vtkIdType ptId2, vtkIdType ptId3,
         vtkPoints *points, vtkCellArray *polys)
{
  const irtkPoint midPoint = MiddlePoint(ptId1, ptId2);
  const vtkIdType midPtId  = points->InsertNextPoint(midPoint._x, midPoint._y, midPoint._z);
  vtkIdType       pts[3];

  InterpolatePointData(midPtId, ptId1, ptId2);

  pts[0] = ptId1;
  pts[1] = midPtId;
  pts[2] = ptId3;
  polys->InsertNextCell(3, pts);

  pts[0] = midPtId;
  pts[1] = ptId2;
  pts[2] = ptId3;
  polys->InsertNextCell(3, pts);

  ++_NumberOfBisections;
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing
::Trisect(vtkIdType cellId, vtkIdType ptId1, vtkIdType ptId2, vtkIdType ptId3,
          vtkPoints *points, vtkCellArray *polys)
{
  const irtkPoint midPoint1 = MiddlePoint(ptId1, ptId2);
  const irtkPoint midPoint2 = MiddlePoint(ptId2, ptId3);
  const vtkIdType midPtId1  = points->InsertNextPoint(midPoint1._x, midPoint1._y, midPoint1._z);
  const vtkIdType midPtId2  = points->InsertNextPoint(midPoint2._x, midPoint2._y, midPoint2._z);
  vtkIdType       pts[3];

  InterpolatePointData(midPtId1, ptId1, ptId2);
  InterpolatePointData(midPtId2, ptId2, ptId3);

  pts[0] = ptId1;
  pts[1] = midPtId1;
  pts[2] = ptId3;
  polys->InsertNextCell(3, pts);

  pts[0] = midPtId1;
  pts[1] = ptId2;
  pts[2] = midPtId2;
  polys->InsertNextCell(3, pts);

  pts[0] = midPtId2;
  pts[1] = ptId3;
  pts[2] = midPtId1;
  polys->InsertNextCell(3, pts);

  ++_NumberOfTrisections;
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing
::Quadsect(vtkIdType cellId, vtkIdType ptId1, vtkIdType ptId2, vtkIdType ptId3,
           vtkPoints *points, vtkCellArray *polys)
{
  const irtkPoint midPoint1 = MiddlePoint(ptId1, ptId2);
  const irtkPoint midPoint2 = MiddlePoint(ptId2, ptId3);
  const irtkPoint midPoint3 = MiddlePoint(ptId3, ptId1);
  const vtkIdType midPtId1  = points->InsertNextPoint(midPoint1._x, midPoint1._y, midPoint1._z);
  const vtkIdType midPtId2  = points->InsertNextPoint(midPoint2._x, midPoint2._y, midPoint2._z);
  const vtkIdType midPtId3  = points->InsertNextPoint(midPoint3._x, midPoint3._y, midPoint3._z);
  vtkIdType       pts[3];

  InterpolatePointData(midPtId1, ptId1, ptId2);
  InterpolatePointData(midPtId2, ptId2, ptId3);
  InterpolatePointData(midPtId3, ptId3, ptId1);

  pts[0] = ptId1;
  pts[1] = midPtId1;
  pts[2] = midPtId3;
  polys->InsertNextCell(3, pts);

  pts[0] = midPtId1;
  pts[1] = ptId2;
  pts[2] = midPtId2;
  polys->InsertNextCell(3, pts);

  pts[0] = midPtId1;
  pts[1] = midPtId2;
  pts[2] = midPtId3;
  polys->InsertNextCell(3, pts);

  pts[0] = midPtId2;
  pts[1] = ptId3;
  pts[2] = midPtId3;
  polys->InsertNextCell(3, pts);

  ++_NumberOfQuadsections;
}

// =============================================================================
// Execution
// =============================================================================

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::Initialize()
{
  // Initialize base class
  irtkPolyDataFilter::Initialize();

  // Triangulate input surface
  // or make shallow copy as we may add additional point data arrays
  vtkSmartPointer<vtkPolyData> input;
  if (_SkipTriangulation) {
    input = _Input->NewInstance();
    input->ShallowCopy(_Input);
  }
  else input = Triangulate(_Input);

  // Compute point normals if needed
  _MinFeatureAngle    = max(.0, min(_MinFeatureAngle, 180.0));
  _MaxFeatureAngle    = max(.0, min(_MaxFeatureAngle, 180.0));
  _MinFeatureAngleCos = 1.0 - cos(_MinFeatureAngle * M_PI / 180.0);
  _MaxFeatureAngleCos = 1.0 - cos(_MaxFeatureAngle * M_PI / 180.0);

  if (_MinFeatureAngle < 180.0 || _MaxFeatureAngle < 180.0) {
    vtkSmartPointer<vtkPolyDataNormals> filter = vtkSmartPointer<vtkPolyDataNormals>::New();
    SetVTKInput(filter, input);
    filter->ComputeCellNormalsOff();
    filter->ComputePointNormalsOn();
    filter->AutoOrientNormalsOn();
    filter->SplittingOff();
    filter->Update();
    input = filter->GetOutput();
  }

  // Initialize adaptive edge length range
  _MinEdgeLengthSquared = _MinEdgeLength * _MinEdgeLength;
  _MaxEdgeLengthSquared = _MaxEdgeLength * _MaxEdgeLength;
  InitializeEdgeLengthRange(input);

  // Initialize output
  _Output = vtkSmartPointer<vtkPolyData>::New();
  _Output->DeepCopy(input);
  _Output->GetPointData()->InterpolateAllocate(input->GetPointData(), 0, input->GetNumberOfPoints());
  _Output->GetPointData()->CopyData(input->GetPointData(), 0, input->GetNumberOfPoints());
  _Output->GetCellData()->Initialize(); // TODO: Interpolate cell data during remeshing

  // Set interpolation of discrete labels to nearest neighbor
  int idx;
  _OutputPointLabels = GetArrayByCaseInsensitiveName(_Output->GetPointData(), "labels", &idx);
  if (_OutputPointLabels) _Output->GetPointData()->SetCopyAttribute(idx, 2);

  // Build links
  _Output->BuildLinks();

  // Reset counters
  _NumberOfMeltedEdges  = 0;
  _NumberOfMeltedCells  = 0;
  _NumberOfInversions   = 0;
  _NumberOfBisections   = 0;
  _NumberOfTrisections  = 0;
  _NumberOfQuadsections = 0;
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::InitializeEdgeLengthRange(vtkPolyData *input)
{
  // Remove previous point data arrays
  input->GetPointData()->RemoveArray("MinEdgeLength");
  input->GetPointData()->RemoveArray("MaxEdgeLength");

  // Use global edge length range if no point data array specified
  if (_AdaptiveEdgeLengthArrayName.empty()) {
    _MinEdgeLengthArray = NULL;
    _MaxEdgeLengthArray = NULL;
    return;
  }

  // Parameters of logistic function
  const double k  = 8.0;
  const double x1 = 0.4;
  const double x2 = 0.6;

  // Get point data array
  vtkDataArray* s = input->GetPointData()->GetArray(_AdaptiveEdgeLengthArrayName.c_str());
  if (s == NULL) {
    cerr << "irtkPolyDataRemeshing::InitializeEdgeLengthRange: Missing point data array named "
         << _AdaptiveEdgeLengthArrayName << endl;
    exit(1);
  }
  if (s->GetNumberOfComponents() != 1) {
    cerr << "irtkPolyDataRemeshing::InitializeEdgeLengthRange: Point data array must be scalar" << endl;
    exit(1);
  }

  // Get robust range of scalar values
  const double min    = data::statistic::Percentile::Calculate( 5, s);
  const double max    = data::statistic::Percentile::Calculate(95, s);
  const double scale  = 1.0 / (max - min);
  const double offset = - scale * min;

  // Allocate edge length arrays
  if (!_MinEdgeLengthArray) {
    _MinEdgeLengthArray = vtkSmartPointer<vtkFloatArray>::New();
    _MinEdgeLengthArray->SetName("MinEdgeLength");
    _MinEdgeLengthArray->SetNumberOfComponents(1);
  }
  _MinEdgeLengthArray->SetNumberOfTuples(input->GetNumberOfPoints());

  if (!_MaxEdgeLengthArray) {
    _MaxEdgeLengthArray = vtkSmartPointer<vtkFloatArray>::New();
    _MaxEdgeLengthArray->SetName("MaxEdgeLength");
    _MaxEdgeLengthArray->SetNumberOfComponents(1);
  }
  _MaxEdgeLengthArray->SetNumberOfTuples(input->GetNumberOfPoints());

  // Initialize edge length range
  double x, a, b;
  for (vtkIdType ptId = 0; ptId < input->GetNumberOfPoints(); ++ptId) {
    // Rescale adaptive factor to [0, 1]
    x = clamp(scale * s->GetComponent(ptId, 0) + offset, .0, 1.0);
    // Compute lower logistic function interpolation coefficients
    a = 1.0 / (1.0 + exp(- k * (x - x1))), b = 1.0 - a;
    _MinEdgeLengthArray->SetComponent(ptId, 0, a * _MinEdgeLength + b * _MaxEdgeLength);
    // Compute upper logistic function interpolation coefficients
    a = 1.0 / (1.0 + exp(- k * (x - x2))), b = 1.0 - a;
    _MaxEdgeLengthArray->SetComponent(ptId, 0, a * _MinEdgeLength + b * _MaxEdgeLength);
  }

  // Add edge length point data arrays such that these are
  // interpolated during the local remeshing operations
  input->GetPointData()->AddArray(_MinEdgeLengthArray);
  input->GetPointData()->AddArray(_MaxEdgeLengthArray);

  // Smooth edge length range
  irtkPolyDataSmoothing smoother;
  smoother.Input(input);
  smoother.SmoothArray(_MinEdgeLengthArray->GetName());
  smoother.SmoothArray(_MaxEdgeLengthArray->GetName());
  smoother.NumberOfIterations(10);
  smoother.Run();

  // Replace edge length arrays by smoothed versions
  vtkPointData * const smoothPD = smoother.Output()->GetPointData();
  input->GetPointData()->RemoveArray(_MinEdgeLengthArray->GetName());
  input->GetPointData()->RemoveArray(_MaxEdgeLengthArray->GetName());
  _MinEdgeLengthArray = smoothPD->GetArray(_MinEdgeLengthArray->GetName());
  _MaxEdgeLengthArray = smoothPD->GetArray(_MaxEdgeLengthArray->GetName());
  input->GetPointData()->AddArray(_MinEdgeLengthArray);
  input->GetPointData()->AddArray(_MaxEdgeLengthArray);
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::Execute()
{
  // Melting pass
  Melting();

  // Inversion pass
  Inversion();

  // Subdivision pass
  Subdivision();
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::Melting()
{
  IRTK_START_TIMING();

  // Cell ID list shared by melting operation functions to save reallocation
  vtkSmartPointer<vtkIdList> cellIds = vtkSmartPointer<vtkIdList>::New();

  // Pre-melt triplet of small triangles adjacent sharing one point
  if (_MeltNodes) MeltTriplets();

  // Either iterate edges and only melt individual edges...
  if (false && !_MeltTriangles && _MeltingOrder == SHORTEST_EDGE) {

    // Determine order in which to process edges
    EdgeQueue queue = QueueEdgesByLength();

    // Process edges in determined order
    vtkSmartPointer<vtkIdList> cellIds2 = vtkSmartPointer<vtkIdList>::New();
    for (EdgeIterator cur = queue.begin(); cur != queue.end(); ++cur) {
      Melting(cur->ptId1, cellIds, cur->ptId2, cellIds2);
    }

  // ...or iterate faces and possibly allow also triangles to be melted
  } else {

    // Determine order in which to process cells
    CellQueue queue;
    switch (_MeltingOrder) {
      case INDEX: break;
      case AREA:          queue = QueueCellsByArea();         break;
      case SHORTEST_EDGE: queue = QueueCellsByShortestEdge(); break;
    }

    // Process triangles in determined order
    if (queue.empty()) {
      const vtkIdType ncells = _Output->GetNumberOfCells();
      for (vtkIdType cellId = 0; cellId < ncells; ++cellId) {
        Melting(cellId, cellIds);
      }
    } else {
      for (CellIterator cur = queue.begin(); cur != queue.end(); ++cur) {
        Melting(cur->cellId, cellIds);
      }
    }
  }

  // Post-melt triplet of small triangles adjacent sharing one point
  if (_MeltNodes) MeltTriplets();

  IRTK_DEBUG_TIMING(2, "melting pass");
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::Inversion()
{
  IRTK_START_TIMING();

  int       i, j, k;
  double    p1[3], p2[3], p3[3], length2[3], min2[3], max2[3];
  vtkIdType adjCellId, adjPtId, npts, *pts;

  for (vtkIdType cellId = 0; cellId < _Output->GetNumberOfCells(); ++cellId) {

    _Output->GetCellPoints(cellId, npts, pts);
    if (npts == 0) continue; // cell marked as deleted (i.e., VTK_EMPTY_CELL)
    irtkAssert(npts == 3, "surface is triangulated");

    // Get (transformed) point coordinates
    GetPoint(pts[0], p1);
    GetPoint(pts[1], p2);
    GetPoint(pts[2], p3);

    // Calculate lengths of triangle edges
    length2[0] = vtkMath::Distance2BetweenPoints(p1, p2);
    length2[1] = vtkMath::Distance2BetweenPoints(p2, p3);
    length2[2] = vtkMath::Distance2BetweenPoints(p3, p1);

    min2[0] = SquaredMinEdgeLength(pts[0], pts[1]);
    min2[1] = SquaredMinEdgeLength(pts[1], pts[2]);
    min2[2] = SquaredMinEdgeLength(pts[2], pts[0]);

    max2[0] = SquaredMaxEdgeLength(pts[0], pts[1]);
    max2[1] = SquaredMaxEdgeLength(pts[1], pts[2]);
    max2[2] = SquaredMaxEdgeLength(pts[2], pts[0]);

    // Determine if triangle is elongated
    for (i = 0; i < 3; ++i) {
      if (length2[i] > max2[i]) {
        for (j = 0; j < 3; ++j) {
          if (j != i && (length2[j] < min2[j] || length2[j] > max2[j])) break;
        }
        if (j == 3) break;
      }
    }
    // When long edge found...
    if (i < 3) {                // 1st long edge point index
      j = (i == 2 ? 0 : i + 1); // 2nd long edge point index
      k = (j == 2 ? 0 : j + 1); // 3rd point of this triangle
      // Check connectivity of long edge end points
      if (NodeConnectivity(pts[i]) > 3 && NodeConnectivity(pts[j]) > 3) {
        // Get other vertex of triangle sharing long edge
        adjPtId = GetCellEdgeNeighborPoint(cellId, pts[i], pts[j]);
        if (adjPtId == -1) continue;
        // Check if length of other edges are in range
        GetPoint(pts[i],  p1);
        GetPoint(adjPtId, p3);
        length2[0] = vtkMath::Distance2BetweenPoints(p1, p3);
        if (length2[0] < SquaredMinEdgeLength(pts[i], adjPtId) ||
            length2[0] > SquaredMaxEdgeLength(pts[i], adjPtId)) continue;
        GetPoint(pts[j], p2);
        length2[1] = vtkMath::Distance2BetweenPoints(p2, p3);
        if (length2[1] < SquaredMinEdgeLength(pts[j], adjPtId) ||
            length2[1] > SquaredMaxEdgeLength(pts[j], adjPtId)) continue;
        // Perform inversion operation
        adjCellId = GetCellEdgeNeighbor(cellId, pts[i], pts[j]);
        if (adjCellId == -1) continue;
        ReplaceCellPoint(cellId,    pts[j], adjPtId);
        ReplaceCellPoint(adjCellId, pts[i], pts[k]);
        ++_NumberOfInversions;
      }
    }
  }

  IRTK_DEBUG_TIMING(2, "inversion pass");
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::Subdivision()
{
  IRTK_START_TIMING();

  int       bisect[3];
  vtkIdType npts, *pts;
  double    p1[3], p2[3], p3[3], n1[3], n2[3], n3[3], length2[3], min2[3], max2[3];

  vtkSmartPointer<vtkCellArray> newPolys = vtkSmartPointer<vtkCellArray>::New();
  newPolys->Allocate(_Output->GetNumberOfCells());
  vtkPoints *points = _Output->GetPoints();

  // Get current number of cells (excl. newly added cells)
  const vtkIdType ncells = _Output->GetNumberOfCells();
  for (vtkIdType cellId = 0; cellId < ncells; ++cellId) {

    _Output->GetCellPoints(cellId, npts, pts);
    if (npts == 0) continue; // cell marked as deleted (i.e., VTK_EMPTY_CELL)
    irtkAssert(npts == 3, "surface is triangulated");

    // Get (transformed) point coordinates
    GetPoint(pts[0], p1);
    GetPoint(pts[1], p2);
    GetPoint(pts[2], p3);

    // Compute squared edge lengths
    length2[0] = vtkMath::Distance2BetweenPoints(p1, p2);
    length2[1] = vtkMath::Distance2BetweenPoints(p2, p3);
    length2[2] = vtkMath::Distance2BetweenPoints(p3, p1);

    // Get desired range of squared edge lengths
    min2[0] = SquaredMinEdgeLength(pts[0], pts[1]);
    min2[1] = SquaredMinEdgeLength(pts[1], pts[2]);
    min2[2] = SquaredMinEdgeLength(pts[2], pts[0]);

    max2[0] = SquaredMaxEdgeLength(pts[0], pts[1]);
    max2[1] = SquaredMaxEdgeLength(pts[1], pts[2]);
    max2[2] = SquaredMaxEdgeLength(pts[2], pts[0]);

    // Determine which edges to bisect
    bisect[0] = int(length2[0] > max2[0]);
    bisect[1] = int(length2[1] > max2[1]);
    bisect[2] = int(length2[2] > max2[2]);

    if (_MaxFeatureAngle < 180.0 && (!bisect[0] || !bisect[1] || !bisect[2])) {
      GetNormal(pts[0], n1);
      GetNormal(pts[1], n2);
      GetNormal(pts[2], n3);
      if (!bisect[0] && length2[0] >= 2.0 * min2[0]) {
        bisect[0] = int(1.0 - vtkMath::Dot(n1, n2) > _MaxFeatureAngleCos);
      }
      if (!bisect[1] && length2[1] >= 2.0 * min2[1]) {
        bisect[1] = int(1.0 - vtkMath::Dot(n2, n3) > _MaxFeatureAngleCos);
      }
      if (!bisect[2] && length2[2] >= 2.0 * min2[2]) {
        bisect[2] = int(1.0 - vtkMath::Dot(n3, n1) > _MaxFeatureAngleCos);
      }
    }

    // Perform subdivision
    switch (bisect[0] + bisect[1] + bisect[2]) {
      case 1:
        if      (bisect[0]) Bisect(cellId, pts[0], pts[1], pts[2], points, newPolys);
        else if (bisect[1]) Bisect(cellId, pts[1], pts[2], pts[0], points, newPolys);
        else                Bisect(cellId, pts[2], pts[0], pts[1], points, newPolys);
        break;
      case 2:
        if      (bisect[0] && bisect[1]) Trisect(cellId, pts[0], pts[1], pts[2], points, newPolys);
        else if (bisect[1] && bisect[2]) Trisect(cellId, pts[1], pts[2], pts[0], points, newPolys);
        else                             Trisect(cellId, pts[2], pts[0], pts[1], points, newPolys);
        break;
      case 3:
        Quadsect(cellId, pts[0], pts[1], pts[2], points, newPolys);
        break;
      default:
        newPolys->InsertNextCell(npts, pts);
    }
  }

  newPolys->Squeeze();
  _Output->DeleteCells();
  _Output->SetPolys(newPolys);

  IRTK_DEBUG_TIMING(2, "subdivison pass");
}

// -----------------------------------------------------------------------------
void irtkPolyDataRemeshing::Finalize()
{
  // Finalize base class
  irtkPolyDataFilter::Finalize();

  // If input surface mesh unchanged, set output equal to input
  // Users can check if this->Output() == this->Input() to see if something changed
  if (_NumberOfMeltedNodes + _NumberOfMeltedEdges + _NumberOfMeltedCells +
      _NumberOfInversions  +
      _NumberOfBisections  + _NumberOfTrisections +_NumberOfQuadsections == 0) {
    _Output = _Input;
    return;
  }

  // Release memory pre-allocated for point data as we may have fewer points now
  _Output->GetPointData()->Squeeze();

  // Recompute normals and ensure consistent order of vertices
  vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
  SetVTKInput(normals, _Output);
  normals->SplittingOff();
  normals->AutoOrientNormalsOn();
  normals->ConsistencyOn();
  normals->ComputeCellNormalsOff();
  normals->ComputePointNormalsOn();
  normals->Update();
  _Output = normals->GetOutput();

  // Remove unused/deleted points and merge middle points of bisected edges
  //
  // FIXME: Under certain circumstances, the cleaner Update causes a segfault.
  //        This is avoided when running the vtkPolyDataNormals filter before
  //        instead of after cleaning the output point set.
  vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
  SetVTKInput(cleaner, _Output);
  cleaner->PointMergingOn();
  cleaner->SetAbsoluteTolerance(.0);
  cleaner->ConvertPolysToLinesOn();
  cleaner->ConvertLinesToPointsOn();
  cleaner->ConvertStripsToPolysOn();
  cleaner->Update();
  _Output = cleaner->GetOutput();
  _Output->SetVerts(NULL);
  _Output->SetLines(NULL);
}


} } // namespace irtk::polydata

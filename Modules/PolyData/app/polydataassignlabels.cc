// =============================================================================
// Project: Image Registration Toolkit (IRTK)
// Package: PolyData
// =============================================================================

#ifdef HAS_VTK

#include <queue>

#include <irtkImage.h>
#include <irtkImageFunction.h>
#include <irtkEuclideanDistanceTransform.h>

#include <vtkGenericCell.h>
#include <vtkPolyData.h>
#include <vtkShortArray.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkCellArray.h>
#include <vtkLine.h>
#include <vtkFeatureEdges.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkCellLocator.h>
#include <vtkOBBTree.h>
#include <vtkModifiedBSPTree.h>

#include <vtkPolyDataReader.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkOBJReader.h>
#include <vtkSTLReader.h>
#include <vtkSTLWriter.h>
#include <vtkPLYReader.h>
#include <vtkPLYWriter.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkPolyDataWriter.h>


// =============================================================================
// Help
// =============================================================================

// -----------------------------------------------------------------------------
void PrintHelp(const char *name)
{
  cout << "usage:  " << name << " <input> <output> -labels <segmentation> [options]" << endl;
  cout << "        " << name << " <input> <output> -boundary [-nolabel-points] [-nolabel-cells] [-name <scalars>]" << endl;
  cout << "        " << name << " <segmentation> <input> <output> [options] (deprecated)" << endl;
  cout << endl;
  cout << "Assign labels either to the vertices or the cells of surface mesh.  The value assigned" << endl;
  cout << "to a vertex/cell is the label of the nearest voxel in the given segmentation image." << endl;
  cout << "" << endl;
  cout << "Arguments:" << endl;
  cout << "  input    Input surface mesh." << endl;
  cout << "  output   Output surface mesh with labels as additional scalar data." << endl;
  cout << endl;
  cout << "Options: " << endl;
  cout << "  -labels <file>               Input segmentation image with positive integer labels." << endl;
  cout << "  -name <name>                 Name of output scalar array. (default: Labels)" << endl;
  cout << "  -[no]label-cells             Assign labels to cells of input surface. (default: off)" << endl;
  cout << "  -[no]label-points            Assign labels to points of input surface. (default: on)" << endl;
  cout << "  -writeDilatedLabels <file>   Write image of dilated labels." << endl;
  cout << "  -cortex                      Input surface is cortical CSF/GM boundary." << endl;
  PrintStandardOptions(cout);
}

// =============================================================================
// Types
// =============================================================================

typedef float                       RealType;
typedef short                       LabelType;
typedef irtkGenericImage<RealType>  RealImage;
typedef irtkGenericImage<LabelType> LabelImage;
typedef vtkShortArray               LabelArray;
typedef map<LabelType, long>        CountMap;
typedef CountMap::const_iterator    CountIter;
typedef set<LabelType>              LabelSet;
typedef LabelSet::const_iterator    LabelIter;

// =============================================================================
// Auxiliaries
// =============================================================================

// -----------------------------------------------------------------------------
/// Returns a floating point 0/1 image to show a label.  The image needs to
/// be floating point so that it can later be used in a distance map filter.
void InitializeLabelMask(const LabelImage &labels, RealImage &mask, LabelType label)
{
  const int noOfVoxels = labels.NumberOfVoxels();

  RealType        *ptr2mask   = mask  .Data();
  const LabelType *ptr2labels = labels.Data();

  for (int i = 0; i < noOfVoxels; ++i, ++ptr2mask, ++ptr2labels) {
    *ptr2mask = static_cast<float>(*ptr2labels == label);
  }
}

// -----------------------------------------------------------------------------
LabelImage DilateLabels(const LabelImage &labels)
{
  // Count up different labels so we can identify the number of distinct labels.
  CountMap labelCount;
  const int noOfVoxels = labels.NumberOfVoxels();
  const irtkGreyPixel *ptr2label  = labels.Data();
  for (int i = 0; i < noOfVoxels; ++i, ++ptr2label) {
    if (*ptr2label > 0) ++labelCount[*ptr2label];
  }
  const int noOfLabels = static_cast<int>(labelCount.size());

  if (verbose) {
    cout << "No. of voxels        = " << noOfVoxels << endl;
    if (verbose > 1) {
      cout << "Label Counts " << endl;
      for (CountIter iter = labelCount.begin(); iter != labelCount.end(); ++iter) {
        cout << iter->first << "\t" << iter->second << endl;
      }
    }
    cout << "No. of labels        = " << noOfLabels << endl;
  }

  // Using the distance maps.
  RealImage minDmap(labels.Attributes());
  RealImage curDmap(labels.Attributes());
  RealImage curMask(labels.Attributes());

  // Initialise the minimum distance map.
  minDmap = numeric_limits<irtkRealPixel>::max();

  // Note that the dilated labels are initialised to the given label image.
  // I.e. the original labels are left alone and we seek to assign labels to
  // the zero voxels based on closest labeled voxels.
  LabelImage dilatedLabels = labels;

  // Single distance transform filter for all labels.
  typedef irtkEuclideanDistanceTransform<RealType> DistanceTransform;
  DistanceTransform edt(DistanceTransform::irtkDistanceTransform3D);

  if (verbose) {
    cout << "Finding distance maps ";
    if (verbose > 1) cout << "...\nCurrent label =";
  }
  int niter = 0;
  for (CountIter iter = labelCount.begin(); iter != labelCount.end(); ++iter, ++niter) {
    const LabelType &curLabel = iter->first;
    if (verbose == 1) {
      if (niter > 0 && niter % 65 == 0) cout << "\n               ";
      cout << '.';
      cout.flush();
    } else if (verbose > 1) {
      if (niter > 0 && niter % 20 == 0) cout << "\n               ";
      cout << " " << curLabel;
      cout.flush();
    }

    // There is a new operator used for currLabelMask in the following function call.
    InitializeLabelMask(labels, curMask, curLabel);

    edt.SetInput (&curMask);
    edt.SetOutput(&curDmap);
    edt.Run();

    RealType        *ptr2minDmap      = minDmap.Data();
    RealType        *ptr2dmap         = curDmap.Data();
    const LabelType *ptr2label        = labels.Data();
    LabelType       *ptr2dilatedLabel = dilatedLabels.Data();

    for (int i = 0; i < noOfVoxels; ++i, ++ptr2minDmap, ++ptr2dmap, ++ptr2label, ++ptr2dilatedLabel) {
      if (*ptr2label == 0 && *ptr2dmap < *ptr2minDmap) {
        *ptr2minDmap      = *ptr2dmap;
        *ptr2dilatedLabel = curLabel;
      }
    }
  }
  if (verbose) {
    if (verbose > 1) cout << "\nFinding distance maps ...";
    cout << " done" << endl;
  }
  return dilatedLabels;
}

// -----------------------------------------------------------------------------
void LabelPoints(vtkPolyData *surface, const LabelImage &labels, const char *name = "Labels")
{
  const vtkIdType noOfPoints = surface->GetNumberOfPoints();

  if (noOfPoints == 0) {
    cerr << "Warning: Cannot label points of surface mesh without any points!" << endl;
  }

  irtkGenericNearestNeighborInterpolateImageFunction<LabelImage> nn;
  nn.Input(&labels);
  nn.Initialize();

  vtkSmartPointer<LabelArray> point_labels;
  point_labels = vtkSmartPointer<LabelArray>::New();
  point_labels->SetName(name);
  point_labels->SetNumberOfComponents(1);
  point_labels->SetNumberOfTuples(noOfPoints);

  double p[3], label;
  for (vtkIdType i = 0; i < noOfPoints; ++i) {
    surface->GetPoint(i, p);
    labels.WorldToImage(p[0], p[1], p[2]);
    label = round(nn.Evaluate (p[0], p[1], p[2]));
    point_labels->SetTuple1(i, label);
  }

  surface->GetPointData()->AddArray(point_labels);
}

// -----------------------------------------------------------------------------
void LabelCells(vtkPolyData *surface, const LabelImage &labels, const char *name = "Labels")
{
  const vtkIdType noOfCells = surface->GetNumberOfCells();

  if (noOfCells == 0) {
    cerr << "Warning: Cannot label cells of surface mesh without any cells!" << endl;
  }

  irtkGenericNearestNeighborInterpolateImageFunction<LabelImage> nn;
  nn.Input(&labels);
  nn.Initialize();

  int subId;
  vtkCell *cell;
  double pcoords[3];
  double *weights = new double[surface->GetMaxCellSize()];

  vtkSmartPointer<LabelArray> cell_labels;
  cell_labels = vtkSmartPointer<LabelArray>::New();
  cell_labels->SetName(name);
  cell_labels->SetNumberOfComponents(1);
  cell_labels->SetNumberOfTuples(noOfCells);

  double p[3], label;
  for (vtkIdType i = 0; i < noOfCells; ++i) {
    cell  = surface->GetCell(i);
    subId = cell->GetParametricCenter(pcoords);
    cell->EvaluateLocation(subId, pcoords, p, weights);
    labels.WorldToImage(p[0], p[1], p[2]);
    label = round(nn.Evaluate (p[0], p[1], p[2]));
    cell_labels->SetTuple1(i, round(label));
  }

  surface->GetCellData()->AddArray(cell_labels);
  delete[] weights;
}

// -----------------------------------------------------------------------------
/// Calculate surface normals
vtkSmartPointer<vtkPolyData> ComputeCellNormals(vtkPolyData *surface)
{
  vtkSmartPointer<vtkSmoothPolyDataFilter> smoother;
  smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
  smoother->SetNumberOfIterations(20);
  SetVTKInput(smoother, surface);

  vtkSmartPointer<vtkPolyDataNormals> calc_normals;
  calc_normals = vtkSmartPointer<vtkPolyDataNormals>::New();
  calc_normals->ConsistencyOff();
  calc_normals->AutoOrientNormalsOn();
  calc_normals->SplittingOff();
  calc_normals->ComputePointNormalsOff();
  calc_normals->ComputeCellNormalsOn();
  calc_normals->FlipNormalsOff();
  SetVTKConnection(calc_normals, smoother);
  calc_normals->Update();

  calc_normals->GetOutput()->GetPoints()->DeepCopy(surface->GetPoints());
  return calc_normals->GetOutput();
}

// -----------------------------------------------------------------------------
void LabelCortex(vtkPolyData *surface, const LabelImage &labels, int nsteps = 10, double h = .25, const char *name = "Labels")
{
  const vtkIdType noOfCells = surface->GetNumberOfCells();

  if (noOfCells == 0) {
    cerr << "Warning: Cannot label cells of surface mesh without any cells!" << endl;
    return;
  }

  irtkGenericNearestNeighborInterpolateImageFunction<LabelImage> nn;
  nn.Input(&labels);
  nn.Initialize();

  vtkSmartPointer<LabelArray> cell_labels;
  cell_labels = vtkSmartPointer<LabelArray>::New();
  cell_labels->SetName(name);
  cell_labels->SetNumberOfComponents(1);
  cell_labels->SetNumberOfTuples(noOfCells);

  vtkSmartPointer<vtkPolyData> mesh = ComputeCellNormals(surface);
  vtkDataArray * normals = mesh->GetCellData()->GetNormals();

  irtkMatrix R = labels.Attributes().GetWorldToImageOrientation();
  irtkVector d(3);

  vtkCell *cell;
  int      subId;
  double   pcoords[3], p[3], n[3];
  double  *pweights = new double[mesh->GetMaxCellSize()];
  CountMap hist;
  LabelType label;

  for (vtkIdType cellId = 0; cellId < mesh->GetNumberOfCells(); ++cellId) {
    // Get cell center and normal
    cell  = mesh->GetCell(cellId);
    subId = cell->GetParametricCenter(pcoords);
    cell->EvaluateLocation(subId, pcoords, p, pweights);
    normals->GetTuple(cellId, n);
    // Convert to voxel units and scale direction vector
    labels.WorldToImage(p[0], p[1], p[2]);
    d(0) = h * n[0], d(1) = h * n[1], d(2) = h * n[2];
    d = R * d;
    // Create histogram of labels along ray in normal direction
    hist.clear();
    for (int i = 0; i < nsteps; ++i) {
      label = static_cast<LabelType>(round(nn.Evaluate(p[0], p[1], p[2])));
      if (label > 0) ++hist[label];
      p[0] += d(0), p[1] += d(1), p[2] += d(2);
    }
    // Assign label with highest frequency
    label = 0;
    long max_count = 0;
    for (CountIter i = hist.begin(); i != hist.end(); ++i) {
      if (i->second > max_count) {
        label     = i->first;
        max_count = i->second;
      }
    }
    cell_labels->SetTuple1(cellId, static_cast<double>(label));
  }

  surface->GetCellData()->AddArray(cell_labels);
  delete[] pweights;
}

// -----------------------------------------------------------------------------
/// Consistently label WM/cGM and cGM/CSF surfaces simultaneously
void LabelCortex(vtkPolyData *white_surface, vtkPolyData *pial_surface,
                 const LabelImage &labels, int nsamples = 10,
                 const char *name = "Labels")
{
  if (pial_surface->GetNumberOfCells() == 0 || white_surface->GetNumberOfCells() == 0) {
    cerr << "Warning: Cannot label cells of surface mesh without any cells!" << endl;
    return;
  }

  irtkGenericNearestNeighborInterpolateImageFunction<LabelImage> nn;
  nn.Input(&labels);
  nn.Initialize();

  vtkSmartPointer<LabelArray> white_labels;
  white_labels = vtkSmartPointer<LabelArray>::New();
  white_labels->SetName(name);
  white_labels->SetNumberOfComponents(1);
  white_labels->SetNumberOfTuples(white_surface->GetNumberOfCells());

  vtkSmartPointer<LabelArray> pial_labels;
  pial_labels = vtkSmartPointer<LabelArray>::New();
  pial_labels->SetName(name);
  pial_labels->SetNumberOfComponents(1);
  pial_labels->SetNumberOfTuples(pial_surface->GetNumberOfCells());

  irtkMatrix R = labels.Attributes().GetWorldToImageOrientation();
  irtkVector d(3);

  const int max_cell_size = max(white_surface->GetMaxCellSize(),
                                pial_surface ->GetMaxCellSize());

  vtkCell *cell;
  int      subId;
  double   pcoords[3], p1[3], p2[3], p[3], n[3], t;
  double  *pweights = new double[max_cell_size];
  CountMap hist;
  LabelType label;
  vtkIdType whiteCellId, pialCellId;

  vtkSmartPointer<vtkPolyData> mesh = ComputeCellNormals(white_surface);
  vtkDataArray *normals = mesh->GetCellData()->GetNormals();

  vtkSmartPointer<vtkModifiedBSPTree> locator = vtkSmartPointer<vtkModifiedBSPTree>::New();
  locator->SetDataSet(pial_surface);
  locator->BuildLocator();

  for (whiteCellId = 0; whiteCellId < mesh->GetNumberOfCells(); ++whiteCellId) {
    // Get cell center and normal
    cell  = mesh->GetCell(whiteCellId);
    subId = cell->GetParametricCenter(pcoords);
    cell->EvaluateLocation(subId, pcoords, p1, pweights);
    normals->GetTuple(whiteCellId, n);
    // Find intersection with pial surface
    p2[0] = p1[0] + 5.0 * n[0];
    p2[1] = p1[1] + 5.0 * n[1];
    p2[2] = p1[2] + 5.0 * n[2];
    if (locator->IntersectWithLine(p1, p2, .05, t, p, pcoords, subId, pialCellId) == 0) {
      t = 2.5 / nsamples;
    } else {
      n[0] = p[0] - p1[0];
      n[1] = p[1] - p1[1];
      n[2] = p[2] - p1[2];
      t /= (nsamples - 1);
    }
    // Convert to voxel units and scale direction vector
    labels.WorldToImage(p1[0], p1[1], p1[2]);
    d(0) = t * n[0], d(1) = t * n[1], d(2) = t * n[2];
    d = R * d;
    // Create histogram of labels along ray in normal direction
    hist.clear();
    for (int i = 0; i < nsamples; ++i) {
      label = static_cast<LabelType>(round(nn.Evaluate(p1[0], p1[1], p1[2])));
      if (label > 0) ++hist[label];
      p1[0] += d(0), p1[1] += d(1), p1[2] += d(2);
    }
    // Assign label with highest frequency
    label = 0;
    long max_count = 0;
    for (CountIter i = hist.begin(); i != hist.end(); ++i) {
      if (i->second > max_count) {
        label     = i->first;
        max_count = i->second;
      }
    }
    white_labels->SetTuple1(whiteCellId, static_cast<double>(label));
    if (pialCellId != -1) {
      pial_labels->SetTuple1(pialCellId, static_cast<double>(label));
    }
  }

  mesh    = ComputeCellNormals(pial_surface);
  normals = mesh->GetCellData()->GetNormals();

  locator = vtkSmartPointer<vtkModifiedBSPTree>::New();
  locator->SetDataSet(white_surface);
  locator->BuildLocator();

  for (vtkIdType pialCellId = 0; pialCellId < mesh->GetNumberOfCells(); ++pialCellId) {
    // Get cell center and normal
    cell  = mesh->GetCell(pialCellId);
    subId = cell->GetParametricCenter(pcoords);
    cell->EvaluateLocation(subId, pcoords, p1, pweights);
    normals->GetTuple(pialCellId, n);
    // Find intersection with pial surface
    p2[0] = p1[0] - 10.0 * n[0];
    p2[1] = p1[1] - 10.0 * n[1];
    p2[2] = p1[2] - 10.0 * n[2];
    if (locator->IntersectWithLine(p1, p2, .05, t, p, pcoords, subId, whiteCellId) == 0) {
      t = 5.0 / nsamples;
    } else {
      n[0] = p[0] - p1[0];
      n[1] = p[1] - p1[1];
      n[2] = p[2] - p1[2];
      t /= (nsamples - 1);
    }
    // Convert to voxel units and scale direction vector
    labels.WorldToImage(p1[0], p1[1], p1[2]);
    d(0) = t * n[0], d(1) = t * n[1], d(2) = t * n[2];
    d = R * d;
    // Create histogram of labels along ray in normal direction
    hist.clear();
    label = static_cast<LabelType>(pial_labels->GetTuple1(pialCellId));
    if (label > 0) ++hist[label];
    for (int i = 0; i < nsamples; ++i) {
      label = static_cast<LabelType>(round(nn.Evaluate(p1[0], p1[1], p1[2])));
      if (label > 0) ++hist[label];
      p1[0] += d(0), p1[1] += d(1), p1[2] += d(2);
    }
    // Assign label with highest frequency
    label = 0;
    long max_count = 0;
    for (CountIter i = hist.begin(); i != hist.end(); ++i) {
      if (i->second > max_count) {
        label     = i->first;
        max_count = i->second;
      }
    }
    if (whiteCellId != -1) {
      if (label != static_cast<LabelType>(white_labels->GetTuple1(whiteCellId))) {
        label = 0;
      }
    }
    pial_labels->SetTuple1(pialCellId, static_cast<double>(label));
    if (whiteCellId != -1) {
      white_labels->SetTuple1(whiteCellId, static_cast<double>(label));
    }
  }

  white_surface->GetCellData()->AddArray(white_labels);
  pial_surface ->GetCellData()->AddArray(pial_labels);
  delete[] pweights;
}

// -----------------------------------------------------------------------------
/// Set label of unlabeled cells which are not part of the cut between
/// cortical hemispheres to -1 so they are filled in by FillHoles or SmoothLabels
void MarkHoles(vtkPolyData *surface, const char *scalars_name = "Labels")
{
  vtkSmartPointer<vtkIdList> cellIds = vtkSmartPointer<vtkIdList>::New();
  double pcoords[3], p[3];
  int    subId;
  vtkCell *cell;

  // Get cell labels
  vtkSmartPointer<vtkDataArray> cell_labels;
  cell_labels = surface->GetCellData()->GetArray(scalars_name);
  if (cell_labels == NULL) {
    cerr << "Surface has no " <<  scalars_name << " cell data, re-run with -label-cells" << endl;
    exit(1);
  }

  // Keep pointer to original point scalars
  vtkSmartPointer<vtkDataArray> original_point_scalars;
  original_point_scalars = surface->GetPointData()->GetScalars();

  // Replace scalars by temporary point label mask used by connectivity filter
  vtkSmartPointer<LabelArray> point_mask;
  point_mask = vtkSmartPointer<LabelArray>::New();
  point_mask->SetNumberOfComponents(1);
  point_mask->SetNumberOfTuples(surface->GetNumberOfPoints());
  surface->GetPointData()->SetScalars(point_mask);

  // Initialize connectivity filter
  vtkSmartPointer<vtkPolyDataConnectivityFilter> filter;
  filter = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
  filter->SetScalarRange(0.9, 1.1);
  filter->ScalarConnectivityOn();
  filter->FullScalarConnectivityOn();
  SetVTKInput(filter, surface);

  // Initialize cell locator used to identify corresponding input/output cells
  vtkSmartPointer<vtkCellLocator> locator;
  locator = vtkSmartPointer<vtkCellLocator>::New();

  // Label points as either unlabeled or not
  for (vtkIdType ptId = 0; ptId < surface->GetNumberOfPoints(); ++ptId) {
    surface->GetPointCells(ptId, cellIds);
    point_mask->SetTuple1(ptId, 1);
    for (vtkIdType i = 0; i < cellIds->GetNumberOfIds(); ++i) {
      if (static_cast<LabelType>(cell_labels->GetTuple1(cellIds->GetId(i))) != 0) {
        point_mask->SetTuple1(ptId, 0);
        break;
      }
    }
  }

  // Find largest connected unlabeled region
  filter->SetExtractionModeToLargestRegion();
  filter->Update();
  locator->SetDataSet(filter->GetOutput());
  locator->BuildLocator();

  // Replace label of not extracted unlabeled cells by -1 (these can be filled in)
  double *pweights = new double[surface->GetMaxCellSize()];
  for (vtkIdType cellId = 0; cellId < surface->GetNumberOfCells(); ++cellId) {
    if (static_cast<LabelType>(cell_labels->GetTuple1(cellId)) == 0) {
      cell  = surface->GetCell(cellId);
      subId = cell->GetParametricCenter(pcoords);
      cell->EvaluateLocation(subId, pcoords, p, pweights);
      if (locator->FindCell(p) == -1) {
        cell_labels->SetTuple1(cellId, -1);
      }
    }
  }
  delete[] pweights;

  // Reset original point data scalars
  surface->GetPointData()->SetScalars(original_point_scalars);
}

// -----------------------------------------------------------------------------
void MarkUnvisited(set<vtkIdType> &cellIds, vtkDataArray *regions, vtkDataArray *labels, LabelType label)
{
  for (vtkIdType cellId = 0; cellId < regions->GetNumberOfTuples(); ++cellId) {
    if (static_cast<LabelType>(labels->GetTuple1(cellId)) == label) {
      regions->SetTuple1(cellId, -1);
      cellIds.insert(cellId);
    } else {
      regions->SetTuple1(cellId, -2);
    }
  }
}

// -----------------------------------------------------------------------------
vtkIdType NextSeed(set<vtkIdType> &cellIds, vtkDataArray *regions)
{
  for (set<vtkIdType>::const_iterator cellId = cellIds.begin(); cellId != cellIds.end(); ++cellId) {
    if (static_cast<LabelType>(regions->GetTuple1(*cellId)) == -1) return *cellId;
  }
  return -1;
}

// -----------------------------------------------------------------------------
vtkIdType GrowRegion(vtkPolyData *surface, vtkDataArray *regions, LabelType regionId, vtkIdType cellId)
{
  vtkSmartPointer<vtkIdList> cellPointIds    = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> neighborCellIds = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> idList          = vtkSmartPointer<vtkIdList>::New();
  idList->SetNumberOfIds(2);

  queue<vtkIdType> active;
  active.push(cellId);
  vtkIdType region_size = 0;
  while (!active.empty()) {
    cellId = active.front();
    active.pop();
    if (static_cast<LabelType>(regions->GetTuple1(cellId)) == -1) {
      ++region_size;
      regions->SetTuple1(cellId, regionId);
      surface->GetCellPoints(cellId, cellPointIds);
      for (vtkIdType i = 0; i < cellPointIds->GetNumberOfIds(); ++i) {
        idList->SetId(0, cellPointIds->GetId(i));
        if (i == cellPointIds->GetNumberOfIds() - 1) {
          idList->SetId(1, cellPointIds->GetId(0));
        } else {
          idList->SetId(1, cellPointIds->GetId(i + 1));
        }
        surface->GetCellNeighbors(cellId, idList, neighborCellIds);
        for (vtkIdType j = 0; j < neighborCellIds->GetNumberOfIds(); ++j) {
          const vtkIdType neighborCellId = neighborCellIds->GetId(j);
          if (static_cast<LabelType>(regions->GetTuple1(neighborCellId)) == -1) {
            active.push(neighborCellId);
          }
        }
      }
    }
  }

  return region_size;
}

// -----------------------------------------------------------------------------
void MarkSmallRegions(vtkPolyData *surface, int min_region_size, const char *scalars_name = "Labels")
{
  vtkSmartPointer<vtkDataArray> labels;
  labels = surface->GetCellData()->GetArray(scalars_name);
  if (labels == NULL) {
    cerr << "Surface has no " <<  scalars_name << " cell data, re-run with -label-cells" << endl;
    exit(1);
  }

  LabelSet label_set;
  for (vtkIdType i = 0; i < labels->GetNumberOfTuples(); ++i) {
    label_set.insert(static_cast<LabelType>(labels->GetTuple1(i)));
  }
  label_set.erase(0);
  label_set.erase(-1);

  vtkSmartPointer<LabelArray> regions = vtkSmartPointer<LabelArray>::New();
  regions->SetNumberOfComponents(1);
  regions->SetNumberOfTuples(surface->GetNumberOfCells());

  vtkIdType         cellId;
  set<vtkIdType>    cellIds;
  vector<vtkIdType> regionSz;

  surface->BuildLinks();
  for (LabelIter label = label_set.begin(); label != label_set.end(); ++label) {
    MarkUnvisited(cellIds, regions, labels, *label);
    while ((cellId = NextSeed(cellIds, regions)) != -1) {
      regionSz.push_back(GrowRegion(surface, regions, regionSz.size(), cellId));
    }
    for (size_t i = 0; i < regionSz.size(); ++i) {
      if (regionSz[i] < static_cast<vtkIdType>(min_region_size)) {
        for (cellId = 0; cellId < surface->GetNumberOfCells(); ++cellId) {
          if (static_cast<size_t>(regions->GetTuple1(cellId)) == i) {
            labels->SetTuple1(cellId, -1);
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void FillHoles(vtkPolyData *surface, const char *scalars_name = "Labels")
{
  // Structured needed for "label front propagation"
  vtkSmartPointer<vtkIdList> idList          = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> cellPointIds    = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> neighborCellIds = vtkSmartPointer<vtkIdList>::New();
  vtkIdType neighborCellId;
  vtkIdType cellId;
  LabelType label;

  idList->SetNumberOfIds(2);

  // Get cell labels
  vtkSmartPointer<vtkDataArray> cell_labels;
  cell_labels = surface->GetCellData()->GetArray(scalars_name);
  if (cell_labels == NULL) {
    cerr << "Surface has no " <<  scalars_name << " cell data, re-run with -label-cells" << endl;
    exit(1);
  }

  // Ensure links are build for better efficiency
  surface->BuildLinks();

  // Determine unlabeled boundary cells
  queue<vtkIdType> active;
  for (cellId = 0; cellId < surface->GetNumberOfCells(); ++cellId) {
    label = static_cast<LabelType>(round(cell_labels->GetTuple1(cellId)));
    if (label != -1) continue;
    // Iterate over cell edges
    surface->GetCellPoints(cellId, cellPointIds);
    bool is_boundary_cell = false;
    for (vtkIdType i = 0; i < cellPointIds->GetNumberOfIds(); ++i) {
      // Get cells sharing these edge points
      idList->SetId(0, cellPointIds->GetId(i));
      if (i == cellPointIds->GetNumberOfIds() - 1) {
        idList->SetId(1, cellPointIds->GetId(0));
      } else {
        idList->SetId(1, cellPointIds->GetId(i + 1));
      }
      surface->GetCellNeighbors(cellId, idList, neighborCellIds);
      // Add if any neighboring cell is labeled
      for (vtkIdType j = 0; j < neighborCellIds->GetNumberOfIds(); ++j) {
        neighborCellId = neighborCellIds->GetId(j);
        if (static_cast<LabelType>(cell_labels->GetTuple1(neighborCellId)) != -1) {
          is_boundary_cell = true;
          break;
        }
      }
      if (is_boundary_cell) {
        active.push(cellId);
        break;
      }
    }
  }

  // Propagate labels of neighboring cells
  CountMap hist;
  while (!active.empty()) {
    cellId = active.front();
    active.pop();
    // Get label of current cell
    label = static_cast<LabelType>(cell_labels->GetTuple1(cellId));
    if (label != -1) continue;
    hist.clear();
    // Iterate over cell edges
    surface->GetCellPoints(cellId, cellPointIds);
    for (vtkIdType i = 0; i < cellPointIds->GetNumberOfIds(); ++i) {
      // Get cells sharing these edge points
      idList->SetId(0, cellPointIds->GetId(i));
      if (i == cellPointIds->GetNumberOfIds() - 1) {
        idList->SetId(1, cellPointIds->GetId(0));
      } else {
        idList->SetId(1, cellPointIds->GetId(i + 1));
      }
      surface->GetCellNeighbors(cellId, idList, neighborCellIds);
      // Count labels of neighboring cells and add unlabeled ones to queue
      for (vtkIdType j = 0; j < neighborCellIds->GetNumberOfIds(); ++j) {
        neighborCellId = neighborCellIds->GetId(j);
        label = static_cast<LabelType>(cell_labels->GetTuple1(neighborCellId));
        if (label == -1) active.push(neighborCellId);
        else ++hist[label];
      }
    }
    // Assign label with highest frequency
    label = -1;
    long max_count = 0;
    for (CountIter i = hist.begin(); i != hist.end(); ++i) {
      if (i->second > max_count) {
        label     = i->first;
        max_count = i->second;
      }
    }
    if (label == -1) active.push(cellId);
    else cell_labels->SetTuple1(cellId, label);
  }
}

// -----------------------------------------------------------------------------
void SmoothLabels(vtkPolyData *surface, int niter, const char *scalars_name = "Labels")
{
  if (niter < 1) return;

  // Structured needed for iterating over neighboring cells
  vtkSmartPointer<vtkIdList> idList          = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> cellPointIds    = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> neighborCellIds = vtkSmartPointer<vtkIdList>::New();
  vtkIdType cellId, neighborCellId;
  LabelType label,  neighborLabel;

  idList->SetNumberOfIds(2);

  // Get cell labels
  vtkSmartPointer<vtkDataArray> cell_labels;
  cell_labels = surface->GetCellData()->GetArray(scalars_name);
  if (cell_labels == NULL) {
    cerr << "Surface has no " <<  scalars_name << " cell data, re-run with -label-cells" << endl;
    exit(1);
  }

  // Ensure links are build for better efficiency
  surface->BuildLinks();

  // Replace labels by majority of neighboring cell labels
  CountMap hist;

  vtkSmartPointer<LabelArray> new_labels;
  new_labels = vtkSmartPointer<LabelArray>::New();
  new_labels->SetName(scalars_name);
  new_labels->SetNumberOfComponents(1);
  new_labels->SetNumberOfTuples(surface->GetNumberOfCells());

  for (int iter = 0; iter < niter; ++iter) {
    for (cellId = 0; cellId < surface->GetNumberOfCells(); ++cellId) {
      hist.clear();
      label = static_cast<LabelType>(cell_labels->GetTuple1(cellId));
      if (label != -1) hist[label] = 1;
      // Iterate over cell edges
      surface->GetCellPoints(cellId, cellPointIds);
      for (vtkIdType i = 0; i < cellPointIds->GetNumberOfIds(); ++i) {
        // Get cells sharing these edge points
        idList->SetId(0, cellPointIds->GetId(i));
        if (i == cellPointIds->GetNumberOfIds() - 1) {
          idList->SetId(1, cellPointIds->GetId(0));
        } else {
          idList->SetId(1, cellPointIds->GetId(i + 1));
        }
        surface->GetCellNeighbors(cellId, idList, neighborCellIds);
        // Count labels of neighboring cells and add unlabeled ones to queue
        for (vtkIdType j = 0; j < neighborCellIds->GetNumberOfIds(); ++j) {
          neighborCellId = neighborCellIds->GetId(j);
          neighborLabel  = static_cast<LabelType>(cell_labels->GetTuple1(neighborCellId));
          if (neighborLabel != -1) ++hist[neighborLabel];
        }
      }
      // Assign label with highest frequency
      long max_count = 0;
      for (CountIter i = hist.begin(); i != hist.end(); ++i) {
        if (i->second > max_count) {
          label     = i->first;
          max_count = i->second;
        }
      }
      new_labels->SetTuple1(cellId, static_cast<double>(label));
    }
    cell_labels->DeepCopy(new_labels);
  }
}

// -----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> ExtractLabelBoundaries(vtkPolyData *surface, const char *scalars_name = "Labels")
{
  vtkSmartPointer<vtkPolyData>  mesh;
  vtkSmartPointer<vtkCellArray> edges;
  edges = vtkSmartPointer<vtkCellArray>::New();

  // Clean input surface
  if (verbose) cout << "Cleaning surface mesh ...", cout.flush();
  vtkSmartPointer<vtkCleanPolyData> cleaner;
  cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
  SetVTKInput(cleaner, surface);
  cleaner->PointMergingOff();
  cleaner->ConvertPolysToLinesOn();
  cleaner->ConvertStripsToPolysOn();
  cleaner->ToleranceIsAbsoluteOn();
  cleaner->SetTolerance(.0);
  cleaner->Update();
  mesh = cleaner->GetOutput();
  if (verbose) cout << " done" << endl;

  // Triangulate input surface
  if (verbose) cout << "Triangulating surface mesh ...", cout.flush();
  vtkSmartPointer<vtkTriangleFilter> triangulate;
  triangulate = vtkSmartPointer<vtkTriangleFilter>::New();
  SetVTKInput(triangulate, mesh);
  triangulate->PassVertsOff();
  triangulate->PassLinesOff();
  triangulate->Update();
  mesh = triangulate->GetOutput();
  if (verbose) cout << " done" << endl;

  // Ensure links are build for better efficiency
  mesh->BuildLinks();

  // Get cell labels
  vtkSmartPointer<vtkDataArray> cell_labels;
  cell_labels = mesh->GetCellData()->GetArray(scalars_name);
  if (cell_labels == NULL) {
    cerr << "Surface has no " <<  scalars_name << " cell data, re-run with -label-cells" << endl;
    exit(1);
  }

  // Iterate over cells
  if (verbose) cout << "Determining boundary edges ...", cout.flush();

  LabelType label;
  bool is_boundary_edge;
  vtkSmartPointer<vtkIdList> idList          = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> cellPointIds    = vtkSmartPointer<vtkIdList>::New();
  vtkSmartPointer<vtkIdList> neighborCellIds = vtkSmartPointer<vtkIdList>::New();
  vtkIdType neighborCellId;

  idList->SetNumberOfIds(2);

  set<pair<vtkIdType, vtkIdType> > boundaryEdgeIds;
  for (vtkIdType cellId = 0; cellId < mesh->GetNumberOfCells(); ++cellId) {
    // Get cell label and point IDs
    label = static_cast<LabelType>(round(cell_labels->GetTuple1(cellId)));
    cellPointIds->Reset();
    mesh->GetCellPoints(cellId, cellPointIds);
    // Iterate over cell edges
    for (vtkIdType i = 0; i < cellPointIds->GetNumberOfIds(); ++i) {
      is_boundary_edge = false;
      // Get cells sharing these edge points
      idList->SetId(0, cellPointIds->GetId(i));
      if (i == cellPointIds->GetNumberOfIds() - 1) {
        idList->SetId(1, cellPointIds->GetId(0));
      } else {
        idList->SetId(1, cellPointIds->GetId(i + 1));
      }
      neighborCellIds->Reset();
      mesh->GetCellNeighbors(cellId, idList, neighborCellIds);
      // Compare labels of neighboring cells to this cell's label
      for (vtkIdType j = 0; j < neighborCellIds->GetNumberOfIds(); ++j) {
        neighborCellId = neighborCellIds->GetId(j);
        if (label != static_cast<LabelType>(round(cell_labels->GetTuple1(neighborCellId)))) {
          is_boundary_edge = true;
        }
      }
      // Add edge if it separates differently labeled cells
      if (is_boundary_edge) {
        boundaryEdgeIds.insert(make_pair(idList->GetId(0), idList->GetId(1)));
      }
    }
  }
  set<pair<vtkIdType, vtkIdType> >::iterator i = boundaryEdgeIds.begin();
  while (i != boundaryEdgeIds.end()) {
    idList->SetId(0, i->first);
    idList->SetId(1, i->second);
    edges->InsertNextCell(idList);
    ++i;
  }
  if (verbose) cout << " done: #edges = " << edges->GetNumberOfCells() << endl;

  mesh->SetVerts(NULL);
  mesh->SetLines(edges);
  mesh->SetPolys(NULL);
  mesh->SetStrips(NULL);
  mesh->GetCellData()->Initialize();

  cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
  SetVTKInput(cleaner, mesh);
  cleaner->PointMergingOff();
  cleaner->ConvertPolysToLinesOn();
  cleaner->ConvertStripsToPolysOn();
  cleaner->ToleranceIsAbsoluteOn();
  cleaner->SetTolerance(.0);
  cleaner->Update();
  return cleaner->GetOutput();
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char **argv)
{
  // Positional arguments
  REQUIRES_POSARGS(2);

  const char *input_image_name   = NULL;
  const char *input_surface_name = NULL;
  const char *output_name        = NULL;
  const char *input_white_name   = NULL;
  const char *input_pial_name    = NULL;
  const char *output_white_name  = NULL;
  const char *output_pial_name   = NULL;

  bool csf_gm_boundary = false;
  bool gm_wm_boundary  = false;

  if (NUM_POSARGS == 2) {
    input_surface_name = POSARG(1);
    output_name        = POSARG(2);
  } else if (NUM_POSARGS == 3) {
    input_image_name   = POSARG(1);
    input_surface_name = POSARG(2);
    output_name        = POSARG(3);
  } else if (NUM_POSARGS == 4) {
    input_white_name   = POSARG(1);
    input_pial_name    = POSARG(2);
    output_white_name  = POSARG(3);
    output_pial_name   = POSARG(4);
    csf_gm_boundary = gm_wm_boundary = true;
  } else {
    PrintHelp(EXECNAME);
    exit(1);
  }

  // Optional arguments
  const char *output_label_image_name = NULL;
  const char *output_scalars_name     = "Labels";
  bool        output_boundary_edges   = false;
  bool        label_cells             = false;
  bool        label_points            = false;
  int         min_region_size         = 0;
  bool        fill_holes              = true;
  int         smoothing_iterations    = 0;

  // Parse remaining arguments
  for (ALL_OPTIONS) {
    if      (OPTION("-writeDilatedLabels")) output_label_image_name = ARGUMENT;
    else if (OPTION("-name")) output_scalars_name = ARGUMENT;
    else if (OPTION("-labels")) input_image_name = ARGUMENT;
    else if (OPTION("-min-size")) min_region_size = atoi(ARGUMENT);
    else if (OPTION("-fill"))   fill_holes = true;
    else if (OPTION("-nofill")) fill_holes = false;
    else if (OPTION("-smooth")) smoothing_iterations = atoi(ARGUMENT);
    else if (OPTION("-pial")) {
      csf_gm_boundary = true;
      gm_wm_boundary  = false;
    }
    else if (OPTION("-white")) {
      csf_gm_boundary = false;
      gm_wm_boundary  = true;
    }
    else if (OPTION("-label-cells"))    label_cells  = true;
    else if (OPTION("-nolabel-cells"))  label_cells  = false;
    else if (OPTION("-label-points"))   label_points = true;
    else if (OPTION("-nolabel-points")) label_points = false;
    else if (OPTION("-boundary"))       output_boundary_edges = true;
    else HANDLE_COMMON_OR_UNKNOWN_OPTION();
  }

  // Labeling the cells makes more sense, but for backward compatibility reasons
  // the default is to label the points...
  if (!label_cells && !label_points && !output_boundary_edges) {
    if (csf_gm_boundary || gm_wm_boundary) {
      label_cells = true;
    } else {
      label_points = true;
    }
  }

  // Segmentation image required if we need to label something
  if ((label_cells || label_points) && !input_image_name) {
    cerr << "Input -labels image required" << endl;
    exit(1);
  }

  // Read input surface
  vtkSmartPointer<vtkPolyData> surface, white_surface, pial_surface;
  if (input_surface_name) {
    if (verbose) cout << "Reading surface ...", cout.flush();
    surface = ReadPolyData(input_surface_name);
    if (verbose) cout << " done" << endl;
  }
  if (input_white_name) {
    if (verbose) cout << "Reading  WM/cGM surface ...", cout.flush();
    white_surface = ReadPolyData(input_white_name);
    if (verbose) cout << " done" << endl;
  }
  if (input_pial_name) {
    if (verbose) cout << "Reading cGM/CSF surface ...", cout.flush();
    pial_surface = ReadPolyData(input_pial_name);
    if (verbose) cout << " done" << endl;
  }

  // Read labels from input image
  irtkGreyImage labels;
  if (input_image_name) {
    if (verbose) cout << "Reading labels ...", cout.flush();
    labels.Read(input_image_name);
    if (verbose) cout << " done" << endl;
  }

  // Compute dilated labels
  LabelImage dilatedLabels;
  if (label_points || (label_cells && !csf_gm_boundary && !gm_wm_boundary)) {

    // Check that surface does not go outside fov of label image.
    if (surface) {
      double surfaceBounds[6];
      surface->ComputeBounds();
      surface->GetBounds(surfaceBounds);
      double &xmin = surfaceBounds[0];
      double &xmax = surfaceBounds[1];
      double &ymin = surfaceBounds[2];
      double &ymax = surfaceBounds[3];
      double &zmin = surfaceBounds[4];
      double &zmax = surfaceBounds[5];

      if (verbose) { 
        cout << "Bounds of surface    = ";
        cout << "(" << xmin << ", " << ymin << ", " << zmin << ") and ";
        cout << "(" << xmax << ", " << ymax << ", " << zmax << ")" << endl;
      }

      labels.WorldToImage(xmin, ymin, zmin);
      labels.WorldToImage(xmax, ymax, zmax);

      if (verbose) {
        cout << "In image coordinates = ";
        cout << "(" << xmin << ", " << ymin << ", " << zmin << ") and ";
        cout << "(" << xmax << ", " << ymax << ", " << zmax << ")" << endl;
      }

      if (xmin < -0.5 || xmax > labels.X() - 0.5 ||
          ymin < -0.5 || ymax > labels.Y() - 0.5 ||
          zmin < -0.5 || zmax > labels.Z() - 0.5) {
        cerr << "Surface outside bounds of image" << endl;
        exit(1);
      }
    }

    // Dilate labels such that every voxel is labeled
    dilatedLabels = DilateLabels(labels);
    if (output_label_image_name) dilatedLabels.Write(output_label_image_name);
  }

  // Assign labels to points (vertices)
  if (label_points) {
    if (surface) {
      if (verbose) cout << "Labeling the input vertices ...", cout.flush();
      LabelPoints(surface, dilatedLabels, output_scalars_name);
      if (verbose) cout << " done" << endl;
    }
    if (white_surface) {
      if (verbose) cout << "Labeling vertices of  WM/cGM surface ...", cout.flush();
      LabelPoints(white_surface, dilatedLabels, output_scalars_name);
      if (verbose) cout << " done" << endl;
    }
    if (pial_surface) {
      if (verbose) cout << "Labeling vertices of cGM/CSF surface ...", cout.flush();
      LabelPoints(pial_surface, dilatedLabels, output_scalars_name);
      if (verbose) cout << " done" << endl;
    }
  }

  // Assign labels to cells (faces, triangle, ...)
  if (label_cells) {
    if (csf_gm_boundary && gm_wm_boundary) {
      if (verbose) cout << "Labeling cortical surfaces ...", cout.flush();
      LabelCortex(white_surface, pial_surface, labels, 10, output_scalars_name);
    } else if (gm_wm_boundary) {
      if (verbose) cout << "Labeling WM/cGM surface ...", cout.flush();
      LabelCortex(surface, labels,  5, +.2, output_scalars_name);
    } else if (csf_gm_boundary) {
      if (verbose) cout << "Labeling cGM/CSF surface ...", cout.flush();
      LabelCortex(surface, labels, 10, -.2, output_scalars_name);
    } else {
      if (verbose) cout << "Labeling the input cells ...", cout.flush();
      LabelCells(surface, dilatedLabels, output_scalars_name);
    }
    if (verbose) cout << " done" << endl;
    if (csf_gm_boundary || gm_wm_boundary) {
      if (fill_holes || smoothing_iterations > 0) {
        if (verbose) cout << "Marking holes in parcellation ...", cout.flush();
        if (surface)       MarkHoles(surface,       output_scalars_name);
        if (white_surface) MarkHoles(white_surface, output_scalars_name);
        if (pial_surface)  MarkHoles(pial_surface,  output_scalars_name);
        if (verbose) cout << " done" << endl;
      }
      if ((csf_gm_boundary || gm_wm_boundary) && fill_holes) {
        if (verbose) cout << "Filling holes ...", cout.flush();
        if (surface)       FillHoles(surface, output_scalars_name);
        if (white_surface) FillHoles(white_surface, output_scalars_name);
        if (pial_surface)  FillHoles(pial_surface, output_scalars_name);
        if (verbose) cout << " done" << endl;
      }
      if (min_region_size > 1) {
        if (verbose) cout << "Marking parcels with less than " << min_region_size << " cells ...", cout.flush();
        if (surface)       MarkSmallRegions(surface,       min_region_size, output_scalars_name);
        if (white_surface) MarkSmallRegions(white_surface, min_region_size, output_scalars_name);
        if (pial_surface)  MarkSmallRegions(pial_surface,  min_region_size, output_scalars_name);
        if (verbose) cout << " done" << endl;
      }
    }
    if (smoothing_iterations > 0) {
      if (verbose) cout << "Smoothing/filling parcels (niter=" << smoothing_iterations << ") ...", cout.flush();
      if (surface)       SmoothLabels(surface, smoothing_iterations, output_scalars_name);
      if (white_surface) SmoothLabels(white_surface, smoothing_iterations, output_scalars_name);
      if (pial_surface)  SmoothLabels(pial_surface,  smoothing_iterations, output_scalars_name);
      if (verbose) cout << " done" << endl;
    }
    if ((csf_gm_boundary || gm_wm_boundary) && fill_holes) {
      if (verbose) cout << "Filling remaining holes ...", cout.flush();
      if (surface)       FillHoles(surface, output_scalars_name);
      if (white_surface) FillHoles(white_surface, output_scalars_name);
      if (pial_surface)  FillHoles(pial_surface, output_scalars_name);
      if (verbose) cout << " done" << endl;
    }
  }

  // Write dataset with line segments at boundary between differently labeled regions
  const char *what = "labeled";
  if (output_boundary_edges) {
    what = "boundaries of labeled regions of";
    if (surface) {
      surface = ExtractLabelBoundaries(surface, output_scalars_name);
    }
    if (white_surface) {
      white_surface = ExtractLabelBoundaries(white_surface, output_scalars_name);
    }
    if (pial_surface) {
      pial_surface = ExtractLabelBoundaries(pial_surface, output_scalars_name);
    }
  }

  // Write output surface(s)
  if (white_surface && output_white_name) {
    if (verbose) cout << "Writing " << what << "  WM/cGM surface ...", cout.flush();
    WritePolyData(output_white_name, white_surface);
    if (verbose) cout << " done" << endl;
  }
  if (pial_surface && output_pial_name) {
    if (verbose) cout << "Writing " << what << " cGM/CSF surface ...", cout.flush();
    WritePolyData(output_pial_name, pial_surface);
    if (verbose) cout << " done" << endl;
  }
  if (surface && output_name) {
    if (verbose) cout << "Writing " << what << " surface ...", cout.flush();
    WritePolyData(output_name, surface);
    if (verbose) cout << " done" << endl;
  }

  return 0;
}


#else
#include <iostream>
int main(int, char *argv[])
{
  std::cerr << argv[0] << " needs to be compiled with the VTK library " << std::endl;
  exit(1);
}
#endif


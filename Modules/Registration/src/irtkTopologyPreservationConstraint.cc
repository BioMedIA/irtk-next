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

#include <irtkTopologyPreservationConstraint.h>


// -----------------------------------------------------------------------------
irtkTopologyPreservationConstraint::irtkTopologyPreservationConstraint(const char *name)
:
  irtkTransformationConstraint(name)
{
  _ConstrainPassiveDoFs = true;
}

// -----------------------------------------------------------------------------
double irtkTopologyPreservationConstraint::Evaluate()
{
  const irtkFreeFormTransformation *ffd = FFD();
  if (!ffd) return .0; // No penalty for non-deformable transformation

  double penalty = .0;
  int    nactive = 0;

  double x, y, z, det;
  for (int l = -1; l < ffd->T(); ++l) {
    for (int k = -1; k < ffd->Z(); ++k) {
      for (int j = -1; j < ffd->Y(); ++j) {
        for (int i = -1; i < ffd->X(); ++i) {
          if (_ConstrainPassiveDoFs ||
              (i >= 0 && j >= 0 && k >= 0 && l >= 0 && ffd->IsActive(i, j, k, l))) {
            x = i + 0.5;
            y = j + 0.5;
            z = k + 0.5;
            ffd->LatticeToWorld(x, y, z);
            det = ffd->LocalJacobian(x, y, z, ffd->LatticeToTime(l + 0.5));
            if (det < 0.3) {
              // 10 * det(J)^2 + 1 / (10 * det(J)^2) - 2
              det     *= 10.0 * det;
              penalty += det + 1.0 / det - 2.0;
            }
            ++nactive;
          }
        }
      }
    }
  }

  if (nactive) penalty /= nactive;
  return penalty;
}

// -----------------------------------------------------------------------------
void irtkTopologyPreservationConstraint::EvaluateGradient(double *, double, double)
{
  // TODO: Implement gradient computation (c.f. irtkVolumePreservationConstraint::EvaluateGradient)
  cerr << "irtkTopologyPreservationConstraint::EvaluateGradient: Not implemented yet" << endl;
  exit(1);
}

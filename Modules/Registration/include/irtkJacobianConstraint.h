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

#ifndef _IRTKDIFFEOMORPHICTRANSFORMATIONCONSTRAINT_H
#define _IRTKDIFFEOMORPHICTRANSFORMATIONCONSTRAINT_H

#include <irtkTransformationConstraint.h>


/**
 * Base class of soft transformation constraints penalizing the Jacobian determinant
 */
class irtkJacobianConstraint : public irtkTransformationConstraint
{
  irtkAbstractMacro(irtkJacobianConstraint);

  // ---------------------------------------------------------------------------
  // Attributes

protected:

  double     *_DetJacobian; ///< Determinant of Jacobian at each control point
  irtkMatrix *_AdjJacobian; ///< Adjugate of Jacobian at each control point
  int         _NumberOfCPs; ///< Number of control points

  // ---------------------------------------------------------------------------
  // Construction/destruction

  /// Constructor
  irtkJacobianConstraint(const char * = "");

public:

  /// Destructor
  virtual ~irtkJacobianConstraint();

  // ---------------------------------------------------------------------------
  // Evaluation

  /// Update internal state upon change of input
  virtual void Update(bool = true);

  /// Compute determinant and adjugate of Jacobian of transformation
  ///
  /// This function is called by Update for each (control) point at which the
  /// Jacobian determinant is evaluated. Can be overridden in subclasses to
  /// either compute a different Jacobian or to threshold the determinant value.
  virtual double Jacobian(const irtkFreeFormTransformation *ffd,
                          double x, double y, double z, double t,
                          irtkMatrix &adj) const
  {
    double det;
    ffd->Jacobian(adj, x, y, z, t, t);
    adj.Adjugate(det);
    return det;
  }

  // ---------------------------------------------------------------------------
  // Debugging

  /// Write Jacobian determinant
  virtual void WriteDataSets(const char *, const char *, bool = true) const;

protected:

  /// Write Jacobian determinant values to scalar image
  void WriteJacobian(const char *, const irtkFreeFormTransformation *, const double *) const;

};


#endif

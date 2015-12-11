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

// irtk includes
#include <irtkImage.h>
#include <irtkNoise.h>

// recipe includes
#include <boost/random.hpp>
#include <boost/random/normal_distribution.hpp>

template <class VoxelType> irtkRicianNoise<VoxelType>::irtkRicianNoise(double Amplitude) : irtkNoise<VoxelType>(Amplitude)
{
}

template <class VoxelType> double irtkRicianNoise<VoxelType>::Run(int x, int y, int z, int t)
{
  boost::mt19937 rng;
  rng.seed(this->_Init);

  boost::normal_distribution<> nd(0.0, 1.0);
  boost::variate_generator<boost::mt19937&,
			   boost::normal_distribution<> > var_nor(rng, nd);

#ifdef OLD_RICIAN
  // For backward consistency:
  if (this->_input->Get(x, y, z) <= 3*this->_Amplitude) {
    // Rayleigh distributed noise in background and air
    double s1 = var_nor();
    double s2 = var_nor();
    double M  = sqrt(s1*s1+s2*s2);
    return double(this->_input->Get(x, y, z, y)) + this->_Amplitude * M;
  } else {
    // Gaussian distributed noise in foreground
    return double(this->_input->Get(x, y, z, y)) + this->_Amplitude * var_nor();
  }

#else
  // Add Rician noise by treating the magnitude image as the real part (r) of a new complex variable (r,i),
  // adding Gaussian noise to the separate parts of this complex variable, before taking the magnitude again.
  // Thanks to Ged Ridgway <gerard.ridgway@ucl.ac.uk> for this contribution.
  double r = double(this->_input->Get(x, y, z, t)) + this->_Amplitude * var_nor();
  double i = this->_Amplitude * var_nor();
  return sqrt(r*r + i*i);

#endif
}

template class irtkRicianNoise<irtkBytePixel>;
template class irtkRicianNoise<irtkGreyPixel>;
template class irtkRicianNoise<irtkRealPixel>;


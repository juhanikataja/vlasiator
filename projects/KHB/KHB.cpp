/*
 * This file is part of Vlasiator.
 * Copyright 2010-2016 Finnish Meteorological Institute
 *
 * For details of usage, see the COPYING file and read the "Rules of the Road"
 * at http://www.physics.helsinki.fi/vlasiator/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cstdlib>
#include <iostream>
#include <cmath>

#include "../../common.h"
#include "../../readparameters.h"
#include "../../backgroundfield/backgroundfield.h"
#include "../../object_wrapper.h"

#include "KHB.h"

namespace projects {
   using namespace std;
   KHB::KHB(): Project() { }
   KHB::~KHB() { }
   
   bool KHB::initialize(void) {return Project::initialize();}
   
   void KHB::addParameters() {
      typedef Readparameters RP;
      RP::add("KHB.P", "Constant total pressure (thermal+magnetic), used to determine the temperature profile (Pa)", 0.0);
      RP::add("KHB.rho1", "Number density, this->TOP state (m^-3)", 0.0);
      RP::add("KHB.rho2", "Number density, this->BOTTOM state (m^-3)", 0.0);
      RP::add("KHB.Vx1", "Bulk velocity x component, this->TOP state (m/s)", 0.0);
      RP::add("KHB.Vx2", "Bulk velocity x component, this->BOTTOM state (m/s)", 0.0);
      RP::add("KHB.Vy1", "Bulk velocity y component, this->TOP state (m/s)", 0.0);
      RP::add("KHB.Vy2", "Bulk velocity y component, this->BOTTOM state (m/s)", 0.0);
      RP::add("KHB.Vz1", "Bulk velocity z component, this->TOP state (m/s)", 0.0);
      RP::add("KHB.Vz2", "Bulk velocity z component, this->BOTTOM state (m/s)", 0.0);
      RP::add("KHB.Bx1", "Magnetic field x component, this->TOP state (T)", 0.0);
      RP::add("KHB.Bx2", "Magnetic field x component, this->BOTTOM state (T)", 0.0);
      RP::add("KHB.By1", "Magnetic field y component, this->TOP state (T)", 0.0);
      RP::add("KHB.By2", "Magnetic field y component, this->BOTTOM state (T)", 0.0);
      RP::add("KHB.Bz1", "Magnetic field z component, this->TOP state (T)", 0.0);
      RP::add("KHB.Bz2", "Magnetic field z component, this->BOTTOM state (T)", 0.0);
      RP::add("KHB.lambda", "Initial perturbation wavelength (m)", 0.0);
      RP::add("KHB.amp", "Initial velocity perturbation amplitude (m s^-1)", 0.0);
      RP::add("KHB.offset", "Boundaries offset from 0 (m)", 0.0);
      RP::add("KHB.transitionWidth", "Width of tanh transition for all changing values", 0.0);
   }

   void KHB::getParameters() {
      Project::getParameters();
      typedef Readparameters RP;

      if(getObjectWrapper().particleSpecies.size() > 1) {
         std::cerr << "The selected project does not support multiple particle populations! Aborting in " << __FILE__ << " line " << __LINE__ << std::endl;
         abort();
      }

      RP::get("KHB.P", this->P);
      RP::get("KHB.rho1", this->rho[this->TOP]);
      RP::get("KHB.rho2", this->rho[this->BOTTOM]);
      RP::get("KHB.Vx1", this->Vx[this->TOP]);
      RP::get("KHB.Vx2", this->Vx[this->BOTTOM]);
      RP::get("KHB.Vy1", this->Vy[this->TOP]);
      RP::get("KHB.Vy2", this->Vy[this->BOTTOM]);
      RP::get("KHB.Vz1", this->Vz[this->TOP]);
      RP::get("KHB.Vz2", this->Vz[this->BOTTOM]);
      RP::get("KHB.Bx1", this->Bx[this->TOP]);
      RP::get("KHB.Bx2", this->Bx[this->BOTTOM]);
      RP::get("KHB.By1", this->By[this->TOP]);
      RP::get("KHB.By2", this->By[this->BOTTOM]);
      RP::get("KHB.Bz1", this->Bz[this->TOP]);
      RP::get("KHB.Bz2", this->Bz[this->BOTTOM]);
      RP::get("KHB.lambda", this->lambda);
      RP::get("KHB.amp", this->amp);
      RP::get("KHB.offset", this->offset);
      RP::get("KHB.transitionWidth", this->transitionWidth);
   }
   
   
   Real KHB::profile(creal top, creal bottom, creal x, creal z) const {
      if(top == bottom) {
         return top;
      }
      if(this->offset != 0.0) {
         return 0.5 * ((top-bottom) * (
         tanh((x + this->offset)/this->transitionWidth) -
         tanh((x - this->offset)/this->transitionWidth) -1) + top+bottom);
      } else {
         return 0.5 * ((top-bottom) * tanh(x/this->transitionWidth) + top+bottom);
      }
   }
   
   Real KHB::getDistribValue(creal& x, creal& z, creal& vx, creal& vy, creal& vz, const uint popID) const {
      creal mass = physicalconstants::MASS_PROTON;
      Real rho = profile(this->rho[this->BOTTOM], this->rho[this->TOP], x, z);
      Real Vx = profile(this->Vx[this->BOTTOM], this->Vx[this->TOP], x, z);
      Real Vy = profile(this->Vy[this->BOTTOM], this->Vy[this->TOP], x, z);
      Real Vz = profile(this->Vz[this->BOTTOM], this->Vz[this->TOP], x, z);

      // add an initial velocity perturbation
      if (this->offset != 0.0) {
         Vx += amp * sin(2.0 * M_PI * y / this->lambda) * (exp(-pow((x + this->offset) / this->transitionWidth,2)) + exp(-pow((x - this->offset) / this->transitionWidth,2)));
      } else {
         Vx += amp * sin(2.0 * M_PI * y / this->lambda) * exp(-pow(x / this->transitionWidth,2));
      }

      // calculate the temperature such that the total pressure is constant across the domain
      Real mu0 = physicalconstants::MU_0;
      Real Bx = profile(this->Bx[LEFT], this->Bx[RIGHT], x, y, z);
      Real By = profile(this->By[LEFT], this->By[RIGHT], x, y, z);
      Real Bz = profile(this->Bz[LEFT], this->Bz[RIGHT], x, y, z);
      Real kbT = (this->P - 0.5 * (Bx * Bx + By * By + Bz * Bz) / mu0) / rho;     
 
      return rho * pow(mass / (2.0 * M_PI * kbT), 1.5) *
      exp(- mass * (pow(vx - Vx, 2.0) + pow(vy - Vy, 2.0) + pow(vz - Vz, 2.0)) / (2.0 * kbT));
   }

   Real KHB::calcPhaseSpaceDensity(creal& x, creal& y, creal& z, creal& dx, creal& dy, creal& dz, creal& vx, creal& vy, creal& vz, creal& dvx, creal& dvy, creal& dvz,const uint popID) const {   
      return getDistribValue(x+0.5*dx, z+0.5*dz, vx+0.5*dvx, vy+0.5*dvy, vz+0.5*dvz, popID);
   }   

   void KHB::calcCellParameters(spatial_cell::SpatialCell* cell,creal& t) { }
   
   void KHB::setProjectBField(
      FsGrid< std::array<Real, fsgrids::bfield::N_BFIELD>, FS_STENCIL_WIDTH> & perBGrid,
      FsGrid< std::array<Real, fsgrids::bgbfield::N_BGB>, FS_STENCIL_WIDTH> & BgBGrid,
      FsGrid< fsgrids::technical, FS_STENCIL_WIDTH> & technicalGrid
   ) {
      setBackgroundFieldToZero(BgBGrid);
      
      if(!P::isRestart) {
         auto localSize = perBGrid.getLocalSize().data();
         
         #pragma omp parallel for collapse(3)
         for (FsGridTools::FsIndex_t x = 0; x < localSize[0]; ++x) {
            for (FsGridTools::FsIndex_t y = 0; y < localSize[1]; ++y) {
               for (FsGridTools::FsIndex_t z = 0; z < localSize[2]; ++z) {
                  const std::array<Real, 3> xyz = perBGrid.getPhysicalCoords(x, y, z);
                  std::array<Real, fsgrids::bfield::N_BFIELD>* cell = perBGrid.get(x, y, z);
                  
                  cell->at(fsgrids::bfield::PERBX) = profile(this->Bx[this->BOTTOM], this->Bx[this->TOP], xyz[0]+0.5*perBGrid.DX, xyz[2]+0.5*perBGrid.DZ);
                  cell->at(fsgrids::bfield::PERBY) = profile(this->By[this->BOTTOM], this->By[this->TOP], xyz[0]+0.5*perBGrid.DX, xyz[2]+0.5*perBGrid.DZ);
                  cell->at(fsgrids::bfield::PERBZ) = profile(this->Bz[this->BOTTOM], this->Bz[this->TOP], xyz[0]+0.5*perBGrid.DX, xyz[2]+0.5*perBGrid.DZ);
               }
            }
         }
      }
   }
   
} // namespace projects

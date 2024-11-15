/*
 * This file is part of Vlasiator.
 * Copyright 2010-2024 Finnish Meteorological Institute
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

#include "../object_wrapper.h"
#include "../spatial_cell_wrapper.hpp"
#include "../velocity_blocks.h"
#include "compression_tools.h"
#include <concepts>
#include <stdexcept>
#include <sys/types.h>

/*
Extracts VDF from spatial cell
 */
ASTERIX::UnorderedVDF ASTERIX::extract_pop_vdf_from_spatial_cell(SpatialCell* sc, uint popID) {
   assert(sc && "Invalid Pointer to Spatial Cell !");
   vmesh::VelocityBlockContainer<vmesh::LocalID>& blockContainer = sc->get_velocity_blocks(popID);
   const size_t total_blocks = blockContainer.size();
   const Real* max_v_lims = sc->get_velocity_grid_max_limits(popID);
   const Real* min_v_lims = sc->get_velocity_grid_min_limits(popID);
   const Real* blockParams = sc->get_block_parameters(popID);
   Realf* data = blockContainer.getData();
   assert(max_v_lims && "Invalid Pointre to max_v_limits");
   assert(min_v_lims && "Invalid Pointre to min_v_limits");
   assert(data && "Invalid Pointre block container data");
   auto vcoords = std::vector<std::array<Real, 3>>(blockContainer.size() * WID3, {Real(0), Real(0), Real(0)});
   auto vspace = std::vector<Realf>(blockContainer.size() * WID3, Realf(0));

   // xmin,ymin,zmin,xmax,ymax,zmax;
   std::array<Real, 6> vlims{std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::max(),
                             std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::lowest(),
                             std::numeric_limits<Real>::lowest(), std::numeric_limits<Real>::lowest()};

   std::size_t cnt = 0;
   for (std::size_t n = 0; n < total_blocks; ++n) {
      auto bp = blockParams + n * BlockParams::N_VELOCITY_BLOCK_PARAMS;
      const Realf* vdf_data = &data[n * WID3];
      for (uint k = 0; k < WID; ++k) {
         for (uint j = 0; j < WID; ++j) {
            for (uint i = 0; i < WID; ++i) {
               const Real vx = bp[BlockParams::VXCRD] + (i + 0.5) * bp[BlockParams::DVX];
               const Real vy = bp[BlockParams::VYCRD] + (j + 0.5) * bp[BlockParams::DVY];
               const Real vz = bp[BlockParams::VZCRD] + (k + 0.5) * bp[BlockParams::DVZ];
               vlims[0] = std::min(vlims[0], vx);
               vlims[1] = std::min(vlims[1], vy);
               vlims[2] = std::min(vlims[2], vz);
               vlims[3] = std::max(vlims[3], vx);
               vlims[4] = std::max(vlims[4], vy);
               vlims[5] = std::max(vlims[5], vz);
               Realf vdf_val = vdf_data[cellIndex(i, j, k)];
               vcoords[cnt] = {vx, vy, vz};
               vspace[cnt] = vdf_val;
               cnt++;
            }
         }
      }
   } // over blocks
   return UnorderedVDF{.vdf_vals = vspace, .vdf_coords = vcoords, .v_limits = vlims};
}

// Simply overwrites the VDF of this population for the give spatial cell with a
// new vspace
void ASTERIX::overwrite_pop_spatial_cell_vdf(SpatialCell* sc, uint popID, const std::vector<Realf>& new_vspace) {
   assert(sc && "Invalid Pointer to Spatial Cell !");
   vmesh::VelocityBlockContainer<vmesh::LocalID>& blockContainer = sc->get_velocity_blocks(popID);
   const size_t total_blocks = blockContainer.size();
   const Real* max_v_lims = sc->get_velocity_grid_max_limits(popID);
   const Real* min_v_lims = sc->get_velocity_grid_min_limits(popID);
   const Real* blockParams = sc->get_block_parameters(popID);
   Realf* data = blockContainer.getData();
   assert(max_v_lims && "Invalid Pointre to max_v_limits");
   assert(min_v_lims && "Invalid Pointre to min_v_limits");
   assert(data && "Invalid Pointre block container data");

   std::size_t cnt = 0;
   for (std::size_t n = 0; n < total_blocks; ++n) {
      auto bp = blockParams + n * BlockParams::N_VELOCITY_BLOCK_PARAMS;
      Realf* vdf_data = &data[n * WID3];
      for (uint k = 0; k < WID; ++k) {
         for (uint j = 0; j < WID; ++j) {
            for (uint i = 0; i < WID; ++i) {
               vdf_data[cellIndex(i, j, k)] = new_vspace[cnt];
               cnt++;
            }
         }
      }
   } // over blocks
   return;
}

void ASTERIX::overwrite_pop_spatial_cell_vdf(SpatialCell* sc, uint popID, const OrderedVDF& vdf) {
   assert(sc && "Invalid Pointer to Spatial Cell !");
   vmesh::VelocityBlockContainer<vmesh::LocalID>& blockContainer = sc->get_velocity_blocks(popID);
   const size_t total_blocks = blockContainer.size();
   const Real* blockParams = sc->get_block_parameters(popID);
   Realf* data = blockContainer.getData();
   assert(max_v_lims && "Invalid Pointre to max_v_limits");
   assert(min_v_lims && "Invalid Pointre to min_v_limits");
   assert(data && "Invalid Pointre block container data");

   for (std::size_t n = 0; n < total_blocks; ++n) {
      auto bp = blockParams + n * BlockParams::N_VELOCITY_BLOCK_PARAMS;
      Realf* vdf_data = &data[n * WID3];
      for (uint k = 0; k < WID; ++k) {
         for (uint j = 0; j < WID; ++j) {
            for (uint i = 0; i < WID; ++i) {
               const Real dvx = (blockParams + BlockParams::N_VELOCITY_BLOCK_PARAMS)[BlockParams::DVX];
               const Real dvy = (blockParams + BlockParams::N_VELOCITY_BLOCK_PARAMS)[BlockParams::DVY];
               const Real dvz = (blockParams + BlockParams::N_VELOCITY_BLOCK_PARAMS)[BlockParams::DVZ];
               const std::size_t nx = std::ceil((vdf.v_limits[3] - vdf.v_limits[0]) / dvx);
               const std::size_t ny = std::ceil((vdf.v_limits[4] - vdf.v_limits[1]) / dvy);
               const std::size_t nz = std::ceil((vdf.v_limits[5] - vdf.v_limits[2]) / dvz);
               const Real vx = bp[BlockParams::VXCRD] + (i + 0.5) * bp[BlockParams::DVX];
               const Real vy = bp[BlockParams::VYCRD] + (j + 0.5) * bp[BlockParams::DVY];
               const Real vz = bp[BlockParams::VZCRD] + (k + 0.5) * bp[BlockParams::DVZ];
               const size_t bbox_i = std::min(static_cast<size_t>(std::floor((vx - vdf.v_limits[0]) / dvx)), nx - 1);
               const size_t bbox_j = std::min(static_cast<size_t>(std::floor((vy - vdf.v_limits[1]) / dvy)), ny - 1);
               const size_t bbox_k = std::min(static_cast<size_t>(std::floor((vz - vdf.v_limits[2]) / dvz)), nz - 1);

               vdf_data[cellIndex(i, j, k)] = vdf.at(bbox_i, bbox_j, bbox_k);
            }
         }
      }
   } // over blocks
   return;
}

ASTERIX::VDFUnion
ASTERIX::extract_union_pop_vdfs_from_cids(const std::span<const CellID> cids, uint popID,
                                          const dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid) {

   // Let's find out which of these cellids has the largest VDF
   std::size_t max_cid_block_size = 0;
   std::size_t bytes_of_all_local_vdfs = 0;
   for (const auto& cid : cids) {
      SpatialCell* sc = mpiGrid[cid];
      const vmesh::VelocityBlockContainer<vmesh::LocalID>& blockContainer = sc->get_velocity_blocks(popID);
      const size_t total_size = blockContainer.size();
      max_cid_block_size = std::max(total_size, max_cid_block_size);
      bytes_of_all_local_vdfs += total_size * WID3 * sizeof(Realf);
   }
   std::vector<std::vector<Realf>> vspaces(cids.size());

   // xmin,ymin,zmin,xmax,ymax,zmax;
   std::array<Real, 6> vlims{std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::max(),
                             std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::lowest(),
                             std::numeric_limits<Real>::lowest(), std::numeric_limits<Real>::lowest()};

   VDFUnion vdf_union{};
   for (std::size_t cc = 0; cc < cids.size(); ++cc) {
      const auto& cid = cids[cc];
      SpatialCell* sc = mpiGrid[cid];
      vmesh::VelocityBlockContainer<vmesh::LocalID>& blockContainer = sc->get_velocity_blocks(popID);
      const size_t total_blocks = blockContainer.size();
      Realf* data = blockContainer.getData();
      const Real* blockParams = sc->get_block_parameters(popID);
      for (std::size_t n = 0; n < total_blocks; ++n) {
         const auto bp = blockParams + n * BlockParams::N_VELOCITY_BLOCK_PARAMS;
         const vmesh::GlobalID gid = sc->get_velocity_block_global_id(n, popID);
         const Realf* vdf_data = &data[n * WID3];

         auto [it, block_inserted] = vdf_union.map.try_emplace(gid, vdf_union.vcoords_union.size());
         std::size_t cnt = 0;
         for (uint k = 0; k < WID; ++k) {
            for (uint j = 0; j < WID; ++j) {
               for (uint i = 0; i < WID; ++i) {

                  const VCoords coords = {bp[BlockParams::VXCRD] + (i + 0.5) * bp[BlockParams::DVX],
                                          bp[BlockParams::VYCRD] + (j + 0.5) * bp[BlockParams::DVY],
                                          bp[BlockParams::VZCRD] + (k + 0.5) * bp[BlockParams::DVZ]};

                  vlims[0] = std::min(vlims[0], coords.vx);
                  vlims[1] = std::min(vlims[1], coords.vy);
                  vlims[2] = std::min(vlims[2], coords.vz);
                  vlims[3] = std::max(vlims[3], coords.vx);
                  vlims[4] = std::max(vlims[4], coords.vy);
                  vlims[5] = std::max(vlims[5], coords.vz);
                  const Realf vdf_val = vdf_data[cellIndex(i, j, k)];
                  if (block_inserted) { // which means the block was not there before
                     vdf_union.vcoords_union.push_back({coords.vx, coords.vy, coords.vz});
                     for (std::size_t x = 0; x < cids.size(); ++x) {
                        vspaces[x].push_back((x == cc) ? vdf_val : Realf(0));
                     }
                  } else { // So the block was there
                     vspaces[cc][it->second + cnt] = vdf_val;
                  }
                  cnt++;
               }
            }
         }
      }
   }

   const std::size_t nrows = vspaces.front().size();
   const std::size_t ncols = cids.size();
   // This will be used further down for indexing into the vspace_union
   auto index_2d = [nrows, ncols](std::size_t row, std::size_t col) -> std::size_t { return row * ncols + col; };

   // Resize to fit the union of vspace coords and vspace density
   vdf_union.vspace_union.resize(nrows * ncols, Realf(0));

   for (std::size_t i = 0; i < nrows; ++i) {
      for (std::size_t j = 0; j < ncols; ++j) {
         vdf_union.vspace_union[index_2d(i, j)] = vspaces[j][i];
      }
   }
   vdf_union.size_in_bytes = bytes_of_all_local_vdfs;
   vdf_union.v_limits = vlims;
   return vdf_union;
}

// Extracts VDF in a cartesian C ordered mesh in a minimum BBOX and with a zoom level used for upsampling/downsampling
ASTERIX::OrderedVDF ASTERIX::extract_pop_vdf_from_spatial_cell_ordered_min_bbox_zoomed(SpatialCell* sc, uint popID,
                                                                                       int zoom) {
   assert(sc && "Invalid Pointer to Spatial Cell !");
   if (zoom != 1) {
      throw std::runtime_error("Zoom is not supported yet!");
   }
   vmesh::VelocityBlockContainer<vmesh::LocalID>& blockContainer = sc->get_velocity_blocks(popID);
   const size_t total_blocks = blockContainer.size();
   const Real* blockParams = sc->get_block_parameters(popID);

   // xmin,ymin,zmin,xmax,ymax,zmax;
   std::array<Real, 6> vlims{std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::max(),
                             std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::lowest(),
                             std::numeric_limits<Real>::lowest(), std::numeric_limits<Real>::lowest()};

   // This pass is computing the active vmesh limits
   // Store dvx,dvy,dvz here
   const Real dvx = (blockParams + BlockParams::N_VELOCITY_BLOCK_PARAMS)[BlockParams::DVX];
   const Real dvy = (blockParams + BlockParams::N_VELOCITY_BLOCK_PARAMS)[BlockParams::DVY];
   const Real dvz = (blockParams + BlockParams::N_VELOCITY_BLOCK_PARAMS)[BlockParams::DVZ];
   for (std::size_t n = 0; n < total_blocks; ++n) {
      const auto bp = blockParams + n * BlockParams::N_VELOCITY_BLOCK_PARAMS;
      for (uint k = 0; k < WID; ++k) {
         for (uint j = 0; j < WID; ++j) {
            for (uint i = 0; i < WID; ++i) {
               const Real vx = bp[BlockParams::VXCRD] + (i + 0.5) * bp[BlockParams::DVX];
               const Real vy = bp[BlockParams::VYCRD] + (j + 0.5) * bp[BlockParams::DVY];
               const Real vz = bp[BlockParams::VZCRD] + (k + 0.5) * bp[BlockParams::DVZ];
               vlims[0] = std::min(vlims[0], vx);
               vlims[1] = std::min(vlims[1], vy);
               vlims[2] = std::min(vlims[2], vz);
               vlims[3] = std::max(vlims[3], vx);
               vlims[4] = std::max(vlims[4], vy);
               vlims[5] = std::max(vlims[5], vz);
            }
         }
      }
   } // over blocks

   assert(isPow2(static_cast<size_t>(std::abs(zoom))));
   float ratio = (zoom > 0) ? static_cast<float>(std::abs(zoom)) : 1.0 / static_cast<float>(std::abs(zoom));
   assert(ratio > 0);

   const Real target_dvx = dvx * ratio;
   const Real target_dvy = dvy * ratio;
   const Real target_dvz = dvz * ratio;
   std::size_t nx = std::ceil((vlims[3] - vlims[0]) / target_dvx);
   std::size_t ny = std::ceil((vlims[4] - vlims[1]) / target_dvy);
   std::size_t nz = std::ceil((vlims[5] - vlims[2]) / target_dvz);
   // printf("VDF min box is %zu , %zu %zu \n ", nx, ny, nz);

   Realf* data = blockContainer.getData();
   std::vector<Realf> vspace(nx * ny * nz, Realf(0));
   for (std::size_t n = 0; n < total_blocks; ++n) {
      const auto bp = blockParams + n * BlockParams::N_VELOCITY_BLOCK_PARAMS;
      const Realf* vdf_data = &data[n * WID3];
      for (uint k = 0; k < WID; ++k) {
         for (uint j = 0; j < WID; ++j) {
            for (uint i = 0; i < WID; ++i) {
               const Real vx = bp[BlockParams::VXCRD] + (i + 0.5) * bp[BlockParams::DVX];
               const Real vy = bp[BlockParams::VYCRD] + (j + 0.5) * bp[BlockParams::DVY];
               const Real vz = bp[BlockParams::VZCRD] + (k + 0.5) * bp[BlockParams::DVZ];
               const size_t bbox_i = std::min(static_cast<size_t>(std::floor((vx - vlims[0]) / target_dvx)), nx - 1);
               const size_t bbox_j = std::min(static_cast<size_t>(std::floor((vy - vlims[1]) / target_dvy)), ny - 1);
               const size_t bbox_k = std::min(static_cast<size_t>(std::floor((vz - vlims[2]) / target_dvz)), nz - 1);

               // Averaging
               if (ratio >= 1.0) {
                  const size_t index = bbox_i * (ny * nz) + bbox_j * nz + bbox_k;
                  vspace.at(index) += vdf_data[cellIndex(i, j, k)] / ratio;
               } else {
                  // Same value in all bins
                  int max_off = 1 / ratio;
                  for (int off_z = 0; off_z <= max_off; off_z++) {
                     for (int off_y = 0; off_y <= max_off; off_y++) {
                        for (int off_x = 0; off_x <= max_off; off_x++) {
                           const size_t index = (bbox_i + off_x) * (ny * nz) + (bbox_j + off_y) * nz + (bbox_k + off_z);
                           if (index < vspace.size()) {
                              vspace.at(index) = vdf_data[cellIndex(i, j, k)];
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   } // over blocks
   return ASTERIX::OrderedVDF{.vdf_vals = vspace, .v_limits = vlims, .shape = {nx, ny, nz}};
}

void ASTERIX::overwrite_cellids_vdfs(const std::span<const CellID> cids, uint popID,
                                     dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                                     const std::vector<std::array<Real, 3>>& vcoords,
                                     const std::vector<Realf>& vspace_union,
                                     const std::unordered_map<vmesh::LocalID, std::size_t>& map_exists_id) {
   const std::size_t nrows = vcoords.size();
   const std::size_t ncols = cids.size();
   // This will be used further down for indexing into the vspace_union
   auto index_2d = [nrows, ncols](std::size_t row, std::size_t col) -> std::size_t { return row * ncols + col; };

   for (std::size_t cc = 0; cc < cids.size(); ++cc) {
      const auto& cid = cids[cc];
      SpatialCell* sc = mpiGrid[cid];
      vmesh::VelocityBlockContainer<vmesh::LocalID>& blockContainer = sc->get_velocity_blocks(popID);
      const size_t total_blocks = blockContainer.size();
      Realf* data = blockContainer.getData();
      const Real* blockParams = sc->get_block_parameters(popID);
      for (std::size_t n = 0; n < total_blocks; ++n) {
         const auto bp = blockParams + n * BlockParams::N_VELOCITY_BLOCK_PARAMS;
         const vmesh::GlobalID gid = sc->get_velocity_block_global_id(n, popID);
         const auto it = map_exists_id.find(gid);
         const bool exists = it != map_exists_id.end();
         assert(exists && "Someone has a buuuug!");
         const auto index = it->second;
         Realf* vdf_data = &data[n * WID3];
         size_t cnt = 0;
         for (uint k = 0; k < WID; ++k) {
            for (uint j = 0; j < WID; ++j) {
               for (uint i = 0; i < WID; ++i) {
                  const std::size_t index = it->second;
                  vdf_data[cellIndex(i, j, k)] = vspace_union[index_2d(index + cnt, cc)];
                  cnt++;
               }
            }
         }
      }
   }
   return;
}

void ASTERIX::dump_vdf_to_binary_file(const char* filename, CellID cid,
                                      dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid) {

   SpatialCell* sc = mpiGrid[cid];
   assert(sc && "Invalid Pointer to Spatial Cell !");
   OrderedVDF vdf = extract_pop_vdf_from_spatial_cell_ordered_min_bbox_zoomed(sc, 0, 1);
   vdf.save_to_file(filename);
}

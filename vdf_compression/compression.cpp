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

#include "compression.h"
#include "compression_tools.h"
#include "zfp/array1.hpp"
#include <concepts>
#include <fstream>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <zfp.h>

#include "../object_wrapper.h"
#include "../spatial_cell_wrapper.hpp"
#include "../velocity_blocks.h"

// #define LUMI_FALLBACK
constexpr float ZFP_TOLL = 1e-12;

using namespace ASTERIX;

extern "C" {
Real compress_and_reconstruct_vdf(std::array<Real, 3>* vcoords, Realf* vspace, std::size_t size,
                                  std::array<Real, 3>* inference_vcoords, Realf* new_vspace, std::size_t inference_size,
                                  std::size_t max_epochs, std::size_t fourier_order, size_t* hidden_layers,
                                  size_t n_hidden_layers, Real sparsity, Real tol, Real* weights,
                                  std::size_t weight_size, bool use_input_weights);

std::size_t probe_network_size(std::array<Real, 3>* vcoords, Realf* vspace, std::size_t size,
                               std::array<Real, 3>* inference_vcoords, Realf* new_vspace, std::size_t inference_size,
                               std::size_t max_epochs, std::size_t fourier_order, size_t* hidden_layers,
                               size_t n_hidden_layers, Real sparsity, Real tol);

Real compress_and_reconstruct_vdf_2(std::array<Real, 3>* vcoords, Realf* vspace, std::size_t size,
                                    std::array<Real, 3>* inference_vcoords, Realf* new_vspace,
                                    std::size_t inference_size, std::size_t max_epochs, std::size_t fourier_order,
                                    size_t* hidden_layers, size_t n_hidden_layers, Real sparsity, Real tol,
                                    Real* weights, std::size_t weight_size, bool use_input_weights,
                                    uint32_t downsampling_factor);

Real compress_and_reconstruct_vdf_2_multi(std::size_t nVDFS, std::array<Real, 3>* vcoords, Realf* vspace,
                                          std::size_t size, std::array<Real, 3>* inference_vcoords, Realf* new_vspace,
                                          std::size_t inference_size, std::size_t max_epochs, std::size_t fourier_order,
                                          size_t* hidden_layers, size_t n_hidden_layers, Real sparsity, Real tol,
                                          Real* weights, std::size_t weight_size, bool use_input_weights,
                                          uint32_t downsampling_factor);

std::size_t probe_network_size_2(std::array<Real, 3>* vcoords, Realf* vspace, std::size_t size,
                                 std::array<Real, 3>* inference_vcoords, Realf* new_vspace, std::size_t inference_size,
                                 std::size_t max_epochs, std::size_t fourier_order, size_t* hidden_layers,
                                 size_t n_hidden_layers, Real sparsity, Real tol);

void compress_with_octree_method(Realf* buffer, const size_t Nx, const size_t Ny, const size_t Nz, float oct_tolerance,
                                 float& compression_ratio);
}

auto compress_vdfs_fourier_mlp(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                               size_t number_of_spatial_cells, bool update_weightsu, uint32_t downsampling_factor)
    -> void;

auto compress_vdfs_fourier_mlp_multi(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                                     size_t number_of_spatial_cells, bool update_weights, uint32_t downsampling_factor)
    -> void;

auto compress_vdfs_zfp(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid, size_t number_of_spatial_cells)
    -> void;

auto compress_vdfs_octree(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid, size_t number_of_spatial_cells)
    -> void;

auto compress(float* array, size_t arraySize, size_t& compressedSize) -> std::vector<char>;

auto compress(double* array, size_t arraySize, size_t& compressedSize) -> std::vector<char>;

auto decompressArrayDouble(char* compressedData, size_t compressedSize, size_t arraySize) -> std::vector<double>;

auto decompressArrayFloat(char* compressedData, size_t compressedSize, size_t arraySize) -> std::vector<float>;

// Main driver, look at header file  for documentation
void ASTERIX::compress_vdfs(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                            size_t number_of_spatial_cells, P::ASTERIX_COMPRESSION_METHODS method, bool update_weights,
                            uint32_t downsampling_factor /*=1*/) {

   if (downsampling_factor < 1) {
      throw std::runtime_error("Requested downsampling factor in VDF compression makes no sense!");
   }
   switch (method) {
   case P::ASTERIX_COMPRESSION_METHODS::MLP:
      compress_vdfs_fourier_mlp(mpiGrid, number_of_spatial_cells, update_weights, downsampling_factor);
      break;
   case P::ASTERIX_COMPRESSION_METHODS::MLP_MULTI:
      compress_vdfs_fourier_mlp_multi(mpiGrid, number_of_spatial_cells, update_weights, downsampling_factor);
      break;
   case P::ASTERIX_COMPRESSION_METHODS::ZFP:
      compress_vdfs_zfp(mpiGrid, number_of_spatial_cells);
      break;
   case P::ASTERIX_COMPRESSION_METHODS::OCTREE:
      compress_vdfs_octree(mpiGrid, number_of_spatial_cells);
      break;
   default:
      throw std::runtime_error("This is bad!. Improper Asterix method detected!");
      break;
   };
}

// Detail implementations
/*
   Here we do a 3 step process which compresses and reconstructs the VDFs using
   the Fourier MLP method in Asterix
   First we collect the VDF and vspace coords into buffers.
   Then we call the reconstruction method which compresses reconstructs and
   writes the reconstructed VDF in a separate buffer. Finally we overwrite the
   original vdf with the previous buffer. Then we wait for it to explode! NOTES:
   This is a thread safe operation but will OOM easily. All scaling operations
   should be handled in the compression code. The original VDF leaves Vlasiator
   untouched. The new VDF should be returned in a sane state.

   mpiGrid: Grid with all local spatial cells
   number_of_spatial_cells:
      Used to reduce the global comrpession achieved
   update_weights:
      If the flag is set to true the method will create a feedback loop where
   the weights of the MLP are stored and then re used for the next training
   session. This will? lead to faster convergence down the road. If the flag is
   set to false then every training session starts from randomized weights. I
   think this is what ML people call transfer learning (together with freezing
   and adding extra neuron which we do not do here).
*/
void compress_vdfs_fourier_mlp(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                               size_t number_of_spatial_cells, bool update_weights, uint32_t downsampling_factor) {
   int myRank;
   MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
   int deviceCount = 0;

   float local_compression_achieved = 0.0;
   float global_compression_achieved = 0.0;
   for (uint popID = 0; popID < getObjectWrapper().particleSpecies.size(); ++popID) {
      Real sparse = getObjectWrapper().particleSpecies[popID].sparseMinValue;
      // Vlasiator boilerplate
      const auto& local_cells = getLocalCells();
#pragma omp parallel for reduction(+ : local_compression_achieved)
      for (auto& cid : local_cells) { // loop over spatial cells
         SpatialCell* sc = mpiGrid[cid];
         assert(sc && "Invalid Pointer to Spatial Cell !");

         // (1) Extract and Collect the VDF of this cell
         std::vector<std::array<Real, 3>> vcoords;
         UnorderedVDF vdf = extract_pop_vdf_from_spatial_cell(sc, popID);

         // Min Max normalize Vspace Coords
         auto normalize_vspace_coords = [&]() {
            std::ranges::for_each(vdf.vdf_coords, [&vdf](std::array<Real, 3>& x) {
               x[0] = (x[0] - vdf.v_limits[0]) / (vdf.v_limits[3] - vdf.v_limits[0]);
               x[1] = (x[1] - vdf.v_limits[1]) / (vdf.v_limits[4] - vdf.v_limits[1]);
               x[2] = (x[2] - vdf.v_limits[2]) / (vdf.v_limits[5] - vdf.v_limits[2]);
            });
         };
         normalize_vspace_coords();

         // TODO: fix this
         static_assert(sizeof(Real) == 8 and sizeof(Realf) == 4);

         // (2) Do the compression for this VDF
         // Create spave for the reconstructed VDF
         std::vector<Realf> new_vspace(vdf.vdf_vals.size(), Realf(0));
         bool use_input_weights = update_weights;
         if (sc->fmlp_weights.size() == 0 && use_input_weights) {
            // This is lazilly done. The first time that we have no weights the MLP
            // is overwritten. Subsequent calls use the weights and update them at
            // the end
            size_t sz = probe_network_size(vdf.vdf_coords.data(), vdf.vdf_vals.data(), vdf.vdf_vals.size(),
                                           vdf.vdf_coords.data(), vdf.vdf_vals.data(), vdf.vdf_vals.size(),
                                           P::mlp_max_epochs, P::mlp_fourier_order, P::mlp_arch.data(),
                                           P::mlp_arch.size(), sparse, P::mlp_tollerance);
            sc->fmlp_weights.resize(sz / sizeof(Real));
            use_input_weights = false; // do not use this on the first pass;
         }

         float ratio = compress_and_reconstruct_vdf_2(
             vdf.vdf_coords.data(), vdf.vdf_vals.data(), vdf.vdf_vals.size(), vdf.vdf_coords.data(), new_vspace.data(),
             vdf.vdf_vals.size(), P::mlp_max_epochs, P::mlp_fourier_order, P::mlp_arch.data(), P::mlp_arch.size(),
             sparse, P::mlp_tollerance, nullptr, 0, false, downsampling_factor);
         local_compression_achieved += ratio;

         // (3) Overwrite the VDF of this cell
         overwrite_pop_spatial_cell_vdf(sc, popID, new_vspace);

      } // loop over all spatial cells
   }    // loop over all populations
   MPI_Barrier(MPI_COMM_WORLD);
   MPI_Reduce(&local_compression_achieved, &global_compression_achieved, 1, MPI_FLOAT, MPI_SUM, MASTER_RANK,
              MPI_COMM_WORLD);
   MPI_Barrier(MPI_COMM_WORLD);
   float realized_compression = global_compression_achieved / (float)number_of_spatial_cells;
   if (myRank == MASTER_RANK) {
      logFile << "(INFO): Compression Ratio = " << realized_compression << std::endl;
   }
   return;
}

void compress_vdfs_fourier_mlp_multi(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                                     size_t number_of_spatial_cells, bool update_weights,
                                     uint32_t downsampling_factor) {
   int myRank;
   int mpiProcs;
   MPI_Comm_size(MPI_COMM_WORLD, &mpiProcs);
   MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

   float local_compression_achieved = 0.0;
   float global_compression_achieved = 0.0;
   for (uint popID = 0; popID < getObjectWrapper().particleSpecies.size(); ++popID) {

      Real sparse = getObjectWrapper().particleSpecies[popID].sparseMinValue;
      const std::vector<CellID>& local_cells = getLocalCells();
      std::vector<std::array<Real, 3>> vcoords;
      std::vector<Realf> vspace;

#ifdef LUMI_FALLBACK
      auto retval = extract_union_pop_vdfs_from_cids(local_cells, popID, mpiGrid, vcoords, vspace);
      auto vdf_mem_footprint_bytes = std::get<0>(retval);
      auto vspace_extent = std::get<1>(retval);
      auto map_exists = std::get<2>(retval);
#else
      auto [vdf_mem_footprint_bytes, vspace_extent, map_exists] =
          extract_union_pop_vdfs_from_cids(local_cells, popID, mpiGrid, vcoords, vspace);
#endif

      // Min Max normalize Vspace Coords
      auto normalize_vspace_coords = [&]() {
         std::ranges::for_each(vcoords, [vspace_extent](std::array<Real, 3>& x) {
            x[0] = ((x[0] - vspace_extent[0]) / (vspace_extent[3] - vspace_extent[0]))-Real(0.5);
            x[1] = ((x[1] - vspace_extent[1]) / (vspace_extent[4] - vspace_extent[1]))-Real(0.5);
            x[2] = ((x[2] - vspace_extent[2]) / (vspace_extent[5] - vspace_extent[2]))-Real(0.5);
         });
      };
      normalize_vspace_coords();

      // TODO: fix this
      static_assert(sizeof(Real) == 8 and sizeof(Realf) == 4);

      // (2) Do the compression for this VDF
      // Create space for the reconstructed VDF
      std::vector<Realf> new_vspace(vspace.size(), Realf(0));

      float nn_mem_footprint_bytes = compress_and_reconstruct_vdf_2_multi(
          local_cells.size(), vcoords.data(), vspace.data(), vcoords.size(), vcoords.data(), new_vspace.data(),
          vcoords.size(), P::mlp_max_epochs, P::mlp_fourier_order, P::mlp_arch.data(), P::mlp_arch.size(), sparse,
          P::mlp_tollerance, nullptr, 0, false, downsampling_factor);
      local_compression_achieved += vdf_mem_footprint_bytes / nn_mem_footprint_bytes;

      // (3) Overwrite the VDF of this cell
      overwrite_cellids_vdfs(local_cells, popID, mpiGrid, vcoords, new_vspace, map_exists);

   } // loop over all populations
   MPI_Barrier(MPI_COMM_WORLD);
   MPI_Reduce(&local_compression_achieved, &global_compression_achieved, 1, MPI_FLOAT, MPI_SUM, MASTER_RANK,
              MPI_COMM_WORLD);
   MPI_Barrier(MPI_COMM_WORLD);
   float realized_compression = global_compression_achieved / (float)mpiProcs;
   if (myRank == MASTER_RANK) {
      logFile << "(INFO): Compression Ratio = " << realized_compression << std::endl;
   }
   return;
}

// Just probes the needed size to store the weights of the MLP. Kinda stupid
// interface but this is all we have now.
std::size_t ASTERIX::probe_network_size_in_bytes(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                                                 size_t number_of_spatial_cells) {
   abort();
   std::size_t network_size = 0;
   uint popID = 0;
   const auto& local_cells = getLocalCells();
   auto cid = local_cells.front();
   SpatialCell* sc = mpiGrid[cid];
   assert(sc && "Invalid Pointer to Spatial Cell !");

   // (1) Extract and Collect the VDF of this cell
   UnorderedVDF vdf = extract_pop_vdf_from_spatial_cell(sc, popID);

   // TODO: fix this
   static_assert(sizeof(Real) == 8 and sizeof(Realf) == 4);

   // (2) Probe network size
   std::vector<Realf> new_vspace(vdf.vdf_vals.size(), Realf(0));
   network_size =
       probe_network_size(vdf.vdf_coords.data(), vdf.vdf_vals.data(), vdf.vdf_vals.size(), vdf.vdf_coords.data(),
                          new_vspace.data(), new_vspace.size(), P::mlp_max_epochs, P::mlp_fourier_order,
                          P::mlp_arch.data(), P::mlp_arch.size(), 0.0, P::mlp_tollerance);
   MPI_Barrier(MPI_COMM_WORLD);
   return network_size;
}

// Compresses and reconstucts VDFs using ZFP
void compress_vdfs_zfp(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid, size_t number_of_spatial_cells) {
   int myRank;
   MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
   float local_compression_achieved = 0.0;
   float global_compression_achieved = 0.0;
   for (uint popID = 0; popID < getObjectWrapper().particleSpecies.size(); ++popID) {
      // Vlasiator boilerplate
      const auto& local_cells = getLocalCells();
#pragma omp parallel for reduction(+ : local_compression_achieved)
      for (auto& cid : local_cells) { // loop over spatial cells
         SpatialCell* sc = mpiGrid[cid];
         assert(sc && "Invalid Pointer to Spatial Cell !");

         // (1) Extract and Collect the VDF of this cell
         UnorderedVDF vdf = extract_pop_vdf_from_spatial_cell(sc, popID);

         // (2) Do the compression for this VDF
         // Create spave for the reconstructed VDF
         size_t ss{0};
         std::vector<char> compressedState = compress(vdf.vdf_vals.data(), vdf.vdf_vals.size(), ss);
         std::vector<Realf> new_vdf = decompressArrayFloat(compressedState.data(), ss, vdf.vdf_vals.size());
         float ratio = static_cast<float>(vdf.vdf_vals.size() * sizeof(Realf)) / static_cast<float>(ss);
         local_compression_achieved += ratio;

         // (3) Overwrite the VDF of this cell
         overwrite_pop_spatial_cell_vdf(sc, popID, new_vdf);

      } // loop over all spatial cells
   }    // loop over all populations
   MPI_Barrier(MPI_COMM_WORLD);
   MPI_Reduce(&local_compression_achieved, &global_compression_achieved, 1, MPI_FLOAT, MPI_SUM, MASTER_RANK,
              MPI_COMM_WORLD);
   MPI_Barrier(MPI_COMM_WORLD);
   float realized_compression = global_compression_achieved / (float)number_of_spatial_cells;
   if (myRank == MASTER_RANK) {
      logFile << "(INFO): Compression Ratio = " << realized_compression << std::endl;
   }
   return;
}

// Compresses and reconstucts VDFs using ZFP
void compress_vdfs_octree(dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                          size_t number_of_spatial_cells) {
   int myRank;
   MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
   float local_compression_achieved = 0.0;
   float global_compression_achieved = 0.0;
   for (uint popID = 0; popID < getObjectWrapper().particleSpecies.size(); ++popID) {
      // Vlasiator boilerplate
      const auto& local_cells = getLocalCells();
#pragma omp parallel for reduction(+ : local_compression_achieved)
      for (auto& cid : local_cells) { // loop over spatial cells
         SpatialCell* sc = mpiGrid[cid];
         assert(sc && "Invalid Pointer to Spatial Cell !");

         // (1) Extract and Collect the VDF of this cell
         OrderedVDF vdf = extract_pop_vdf_from_spatial_cell_ordered_min_bbox_zoomed(sc, popID, 1);

         // (2) Do the compression for this VDF
         float ratio = 0.0;
         compress_with_octree_method(vdf.vdf_vals.data(), vdf.shape[0], vdf.shape[1], vdf.shape[2],
                                     P::octree_tollerance, ratio);

         local_compression_achieved += ratio;

         // (3) Overwrite the VDF of this cell
         overwrite_pop_spatial_cell_vdf(sc, popID, vdf);

      } // loop over all spatial cells
   }    // loop over all populations
   MPI_Barrier(MPI_COMM_WORLD);
   MPI_Reduce(&local_compression_achieved, &global_compression_achieved, 1, MPI_FLOAT, MPI_SUM, MASTER_RANK,
              MPI_COMM_WORLD);
   MPI_Barrier(MPI_COMM_WORLD);
   float realized_compression = global_compression_achieved / (float)number_of_spatial_cells;
   if (myRank == MASTER_RANK) {
      logFile << "(INFO): Compression Ratio = " << realized_compression << std::endl;
   }
   return;
}

std::vector<char> compress(float* array, size_t arraySize, size_t& compressedSize) {
   // Allocate memory for compressed data

   zfp_stream* zfp = zfp_stream_open(NULL);
   zfp_field* field = zfp_field_1d(array, zfp_type_float, arraySize);
   size_t maxSize = zfp_stream_maximum_size(zfp, field);
   std::vector<char> compressedData(maxSize);

   // Initialize ZFP compression
   zfp_stream_set_accuracy(zfp, ZFP_TOLL);
   bitstream* stream = stream_open(compressedData.data(), compressedSize);
   zfp_stream_set_bit_stream(zfp, stream);
   zfp_stream_rewind(zfp);

   // Compress the array
   compressedSize = zfp_compress(zfp, field);
   compressedData.erase(compressedData.begin() + compressedSize, compressedData.end());
   zfp_field_free(field);
   zfp_stream_close(zfp);
   stream_close(stream);
   return compressedData;
}

// Function to decompress a compressed array of floats using ZFP
std::vector<float> decompressArrayFloat(char* compressedData, size_t compressedSize, size_t arraySize) {
   // Allocate memory for decompresseFloatd data
   std::vector<float> decompressedArray(arraySize);

   // Initialize ZFP decompression
   zfp_stream* zfp = zfp_stream_open(NULL);
   zfp_stream_set_accuracy(zfp, ZFP_TOLL);
   bitstream* stream_decompress = stream_open(compressedData, compressedSize);
   zfp_stream_set_bit_stream(zfp, stream_decompress);
   zfp_stream_rewind(zfp);

   // Decompress the array
   zfp_field* field_decompress = zfp_field_1d(decompressedArray.data(), zfp_type_float, decompressedArray.size());
   size_t retval = zfp_decompress(zfp, field_decompress);
   (void)retval;
   zfp_field_free(field_decompress);
   zfp_stream_close(zfp);
   stream_close(stream_decompress);

   return decompressedArray;
}

// Function to compress a 1D array of doubles using ZFP
std::vector<char> compress(double* array, size_t arraySize, size_t& compressedSize) {
   zfp_stream* zfp = zfp_stream_open(NULL);
   zfp_field* field = zfp_field_1d(array, zfp_type_double, arraySize);
   size_t maxSize = zfp_stream_maximum_size(zfp, field);
   std::vector<char> compressedData(maxSize);

   zfp_stream_set_accuracy(zfp, ZFP_TOLL);
   bitstream* stream = stream_open(compressedData.data(), compressedSize);
   zfp_stream_set_bit_stream(zfp, stream);
   zfp_stream_rewind(zfp);

   compressedSize = zfp_compress(zfp, field);
   compressedData.erase(compressedData.begin() + compressedSize, compressedData.end());
   zfp_field_free(field);
   zfp_stream_close(zfp);
   stream_close(stream);
   return compressedData;
}

// Function to decompress a compressed array of doubles using ZFP
std::vector<double> decompressArrayDouble(char* compressedData, size_t compressedSize, size_t arraySize) {
   // Allocate memory for decompressed data
   std::vector<double> decompressedArray(arraySize);

   zfp_stream* zfp = zfp_stream_open(NULL);
   zfp_stream_set_accuracy(zfp, ZFP_TOLL);
   bitstream* stream_decompress = stream_open(compressedData, compressedSize);
   zfp_stream_set_bit_stream(zfp, stream_decompress);
   zfp_stream_rewind(zfp);

   zfp_field* field_decompress = zfp_field_1d(decompressedArray.data(), zfp_type_double, decompressedArray.size());
   size_t retval = zfp_decompress(zfp, field_decompress);
   (void)retval;
   zfp_field_free(field_decompress);
   zfp_stream_close(zfp);
   stream_close(stream_decompress);
   return decompressedArray;
}

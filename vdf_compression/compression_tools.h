#pragma once
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

// These tools  are fwd declared here and implemented at the end of the file for
// better clarity. They are not for external usage and as such they do not go
// into the header file

#include "../definitions.h"
#include "../logger.h"
#include "../mpiconversion.h"
#include "../object_wrapper.h"
#include "../spatial_cell_wrapper.hpp"
#include "../velocity_blocks.h"
#include "stdlib.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <span>
#include <unordered_map>

namespace ASTERIX {
struct VCoords {
   Real vx, vy, vz;
};

struct OrderedVDF {
   std::vector<Realf> vdf_vals;
   std::array<Real, 6> v_limits;     // vx_min,vy_min,vz_min,vx_max,vy_max,vz_max
   std::array<std::size_t, 3> shape; // x,y,z
   std::size_t index(std::size_t i, std::size_t j, std::size_t k) const noexcept {
      return i * (shape[1] * shape[2]) + j * shape[0] + k;
   }

   Realf& at(std::size_t i, std::size_t j, std::size_t k) noexcept { return vdf_vals.at(index(i, j, k)); }

   const Realf& at(std::size_t i, std::size_t j, std::size_t k) const noexcept { return vdf_vals.at(index(i, j, k)); }

   bool save_to_file(const char* filename) const noexcept {
      std::ofstream file(filename, std::ios::out | std::ios::binary);
      if (!file) {
         std::cerr << "Could not open file for writting! Exiting!" << std::endl;
         return false;
      }
      file.write((char*)shape.data(), 3 * sizeof(size_t));
      if (!file) {
         std::cerr << "Error writing shape data to file!" << std::endl;
         return false;
      }

      file.write((char*)vdf_vals.data(), vdf_vals.size() * sizeof(Realf));
      if (!file) {
         std::cerr << "Error writing vdf_vals data to file!" << std::endl;
         return false;
      }
      return true;
   }
};

struct UnorderedVDF {
   std::vector<Realf> vdf_vals;
   std::vector<std::array<Real, 3>> vdf_coords;
   std::array<Real, 6> v_limits{std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::max(),
                                std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::lowest(),
                                std::numeric_limits<Real>::lowest(), std::numeric_limits<Real>::lowest()};

   bool save_to_file(const char* filename) const noexcept {
      std::ofstream file(filename, std::ios::out | std::ios::binary);
      if (!file) {
         std::cerr << "Could not open file for writting! Exiting!" << std::endl;
         return false;
      }

      file.write((char*)vdf_vals.size(), sizeof(size_t));
      if (!file) {
         std::cerr << "Error writing size data to file!" << std::endl;
         return false;
      }

      file.write((char*)v_limits.data(), 6 * sizeof(Real));
      if (!file) {
         std::cerr << "Error writing size data to file!" << std::endl;
         return false;
      }

      file.write((char*)vdf_coords.data(), vdf_coords.size() * 3 * sizeof(Real));
      if (!file) {
         std::cerr << "Error writing vdf_coords data to file!" << std::endl;
         return false;
      }

      file.write((char*)vdf_vals.data(), vdf_vals.size() * sizeof(Realf));
      if (!file) {
         std::cerr << "Error writing vdf_vals data to file!" << std::endl;
         return false;
      }
      return true;
   }
};

struct VDFUnion {
   std::vector<std::array<Real, 3>> vcoords_union;
   std::vector<Realf> vspace_union;
   std::unordered_map<vmesh::LocalID, std::size_t> map;
   std::size_t size_in_bytes;
   std::array<Real, 6> v_limits{std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::max(),
                                std::numeric_limits<Real>::max(),    std::numeric_limits<Real>::lowest(),
                                std::numeric_limits<Real>::lowest(), std::numeric_limits<Real>::lowest()};
                                
   bool save_to_file(const char* filename) const noexcept {
      std::ofstream file(filename, std::ios::out | std::ios::binary);
      if (!file) {
         std::cerr << "Could not open file for writting! Exiting!" << std::endl;
         return false;
      }

      file.write((char*)vspace_union.size(), sizeof(size_t));
      if (!file) {
         std::cerr << "Error writing size data to file!" << std::endl;
         return false;
      }

      file.write((char*)v_limits.data(), 6 * sizeof(Real));
      if (!file) {
         std::cerr << "Error writing size data to file!" << std::endl;
         return false;
      }

      file.write((char*)vcoords_union.data(), vcoords_union.size() * 3 * sizeof(Real));
      if (!file) {
         std::cerr << "Error writing vdf_coords data to file!" << std::endl;
         return false;
      }

      file.write((char*)vspace_union.data(), vspace_union.size() * sizeof(Realf));
      if (!file) {
         std::cerr << "Error writing vdf_vals data to file!" << std::endl;
         return false;
      }
      return true;
   }
};

auto extract_pop_vdf_from_spatial_cell(SpatialCell* sc, uint popID) -> UnorderedVDF;

auto extract_union_pop_vdfs_from_cids(const std::span<const CellID> cids, uint popID,
                                      const dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid) -> VDFUnion;

auto extract_pop_vdf_from_spatial_cell_ordered_min_bbox_zoomed(SpatialCell* sc, uint popID, int zoom) -> OrderedVDF;

constexpr auto isPow2(std::unsigned_integral auto val) -> bool { return (val & (val - 1)) == 0; };

auto overwrite_pop_spatial_cell_vdf(SpatialCell* sc, uint popID, const std::vector<Realf>& new_vspace) -> void;

auto overwrite_pop_spatial_cell_vdf(SpatialCell* sc, uint popID, const OrderedVDF& vdf) -> void;

auto overwrite_cellids_vdfs(const std::span<const CellID> cids, uint popID,
                            dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid,
                            const std::vector<std::array<Real, 3>>& vcoords, const std::vector<Realf>& vspace_union,
                            const std::unordered_map<vmesh::LocalID, std::size_t>& map_exists_id) -> void;

auto dump_vdf_to_binary_file(const char* filename, CellID cid) -> void;

auto dump_vdf_to_binary_file(const char* filename, CellID cid,
                             dccrg::Dccrg<SpatialCell, dccrg::Cartesian_Geometry>& mpiGrid) -> void;

// https://en.wikipedia.org/wiki/Entropy_(information_theory)
template <typename T>
requires(std::is_same_v<T, float> || std::is_same_v<T, double>) auto shannon_entropy(const std::vector<T>& data) -> T {
   const std::size_t sz = data.size();
   if (sz == 0) {
      return 0.0;
   }

   using key_t = std::conditional_t<std::is_same_v<T, float>, uint32_t, uint64_t>;
   std::unordered_map<key_t, int> frequency;
   for (std::size_t i = 0; i < sz; ++i) {
      frequency[*(reinterpret_cast<const key_t*>(&data[i]))]++;
   }
   T entropy = 0.0;
   for (const auto& [byte, count] : frequency) {
      T pk = static_cast<T>(count) / sz;
      entropy -= pk * std::log2(pk);
   }
   return entropy;
}

template <typename T>
requires(std::is_same_v<T, float> ||
         std::is_same_v<T, double>) auto theoritical_lossless_compression_ratio(const std::vector<T>& data,
                                                                                std::size_t bits) -> T {
   T entorpy = shannon_entropy(data);
   return static_cast<T>(bits) / entorpy;
}

template <typename NetworkType>
requires(std::is_same_v<NetworkType, float> ||
         std::is_same_v<NetworkType, double>) auto calculate_total_size_bytes(const std::vector<std::size_t>& neurons)
    -> std::size_t {
   const std::size_t weight_size = sizeof(NetworkType);
   const std::size_t bias_size = sizeof(NetworkType);
   std::size_t total_weights = 0;
   std::size_t total_biases = 0;
   for (size_t i = 0; i < neurons.size() - 1; ++i) {
      total_weights += neurons[i] * neurons[i + 1];
      total_biases += neurons[i + 1];
   }
   return (total_weights + total_biases) * weight_size;
}

template <typename NetworkType>
requires(std::is_same_v<NetworkType, float> || std::is_same_v<NetworkType, double>) auto calculate_hidden_neurons(
    std::size_t N_input, std::size_t N_output, std::size_t num_hidden_layers, std::size_t target_size)
    -> std::vector<std::size_t> {
   std::vector<std::size_t> neurons(num_hidden_layers + 2); // 2 input and output
   neurons[0] = N_input;
   neurons[num_hidden_layers + 1] = N_output;

   // We guess this heyuristically
   std::size_t initial_hidden_size = 1;
   for (std::size_t i = 1; i <= num_hidden_layers; ++i) {
      neurons[i] = initial_hidden_size;
   }
   std::size_t current_size = calculate_total_size_bytes<NetworkType>(neurons);

   while (current_size < target_size) {
      for (std::size_t i = 1; i <= num_hidden_layers; ++i) {
         neurons[i]++;
      }
      current_size = calculate_total_size_bytes<NetworkType>(neurons);
   }

   while (current_size > target_size) {
      for (std::size_t i = 1; i <= num_hidden_layers; ++i) {
         if (neurons[i] > 1) {
            neurons[i]--;
         }
      }
      current_size = calculate_total_size_bytes<NetworkType>(neurons);
   }
   return neurons;
}

} // namespace ASTERIX

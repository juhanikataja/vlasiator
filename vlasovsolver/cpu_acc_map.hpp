/*
This file is part of Vlasiator.

Copyright 2012 Finnish Meteorological Institute

*/

#ifndef CPU_ACC_MAP_H
#define CPU_ACC_MAP_H

#include "algorithm"
#include "cmath"
#include "utility"

#include "common.h"
#include "spatial_cell.hpp"

const int STENCIL_WIDTH=2; //at most equalt to WID

using namespace std;
using namespace spatial_cell;

enum InterpolationType { CONSTANT,LINEAR,PARABOLIC };



/*!
MC slope limiter
*/

template<typename T> inline T slope_limiter(const T& l,const T& m, const T& r) {
  T sign;
  T a=r-m;
  T b=m-l; 
  if (a*b < 0.0)
    return 0.0;
  T minval=min(2.0*fabs(a),2.0*fabs(b));
  minval=min(minval,0.5*fabs(a+b));
  
  if(a<0)
    return -minval;
  else
    return minval;
}

// indices in padded z block
#define i_pblock(i,j,k) ( ((k) + 2) * WID2 + (j) * WID + (i) )
// indices in normal block
#define i_block(i,j,k) ( (k) * WID2 + (j) * WID + (i) )

/*
  copy data for x mappings, no rotation here
*/

inline void copy_block_data_z(Velocity_Block *block,Real *values){
    Velocity_Block *nbrBlock;  
    // Construct values
    // Copy averages from -z neighbour if it exists (if not, array is initialized to zero)
    nbrBlock = block->neighbors[velocity_neighbor::XCC_YCC_ZM1];
    if ( nbrBlock != NULL) {
      for (int k=-STENCIL_WIDTH; k<0; ++k) 
	for (uint j=0; j<WID; ++j) 
	  for (uint i=0; i<WID; ++i) {
	    values[i_pblock(i,j,k)] = nbrBlock->fx[i_block(i,j,k+WID)];
	  }
    }   
    // Copy volume averages of this block:
    for (uint k=0; k<WID; ++k) 
      for (uint j=0; j<WID; ++j) 
	for (uint i=0; i<WID; ++i) {
	  values[i_pblock(i,j,k)] = block->fx[i_block(i,j,k)];
	}    
    // Copy averages from +z neighbour if it exists (if not, array is initialized to zero)
    nbrBlock = block->neighbors[velocity_neighbor::XCC_YCC_ZP1];
    if ( nbrBlock != NULL) {
      for (uint k=WID; k<WID+STENCIL_WIDTH; ++k) 
	for (uint j=0; j<WID; ++j) 
	  for (uint i=0; i<WID; ++i) {
	    values[i_pblock(i,j,k)] = nbrBlock->fx[i_block(i,j,k-WID)];
	  }
    }
}

/*
  copy data for x mappings, note how we have rotated data
  i -> k
  j -> j
  k -> i

This is so that the mapping computation can remain the same pretty much, and data is in a nice order for vectorization
*/  
inline void copy_block_data_x(Velocity_Block *block,Real *values){
    Velocity_Block *nbrBlock;  
    // Construct values
    // Copy averages from -z neighbour if it exists (if not, array is initialized to zero)
    nbrBlock = block->neighbors[velocity_neighbor::XM1_YCC_ZCC];
    if ( nbrBlock != NULL) {
      for (uint j=0; j<WID; ++j) 
	for (int k=-STENCIL_WIDTH; k<0; ++k) 
	  for (uint i=0; i<WID; ++i) {
	    values[i_pblock(i,j,k)] = nbrBlock->fx[i_block(k+WID,j,i)];
	  }
    }   
    // Copy volume averages of this block:
    for (uint j=0; j<WID; ++j) 
      for (uint k=0; k<WID; ++k) 
	for (uint i=0; i<WID; ++i) {
	  values[i_pblock(i,j,k)] = block->fx[i_block(k,j,i)];
      }    
    // Copy averages from +z neighbour if it exists (if not, array is initialized to zero)
    nbrBlock = block->neighbors[velocity_neighbor::XP1_YCC_ZCC];
    if ( nbrBlock != NULL) {
      for (uint j=0; j<WID; ++j) 
	for (uint k=WID; k<WID+STENCIL_WIDTH; ++k) 
	  for (uint i=0; i<WID; ++i) {
	    values[i_pblock(i,j,k)] = nbrBlock->fx[i_block(k-WID,j,i)];
	  }
    }
}



/*
  copy data for y mappings, note how we have rotated data
  i -> i
  j -> k
  k -> j

This is so that the mapping computation can remain the same pretty much, and data is in a nice order for vectorization
*/  
inline void copy_block_data_y(Velocity_Block *block,Real *values){
    Velocity_Block *nbrBlock;  
    // Construct values
    // Copy averages from -z neighbour if it exists (if not, array is initialized to zero)
    nbrBlock = block->neighbors[velocity_neighbor::XCC_YM1_ZCC];
    if ( nbrBlock != NULL) {
      for (uint j=0; j<WID; ++j) 
	for (int k=-STENCIL_WIDTH; k<0; ++k) 
	  for (uint i=0; i<WID; ++i) {
	    values[i_pblock(i,j,k)] = nbrBlock->fx[i_block(i,k+WID,j)];
	  }
    }   
    // Copy volume averages of this block:
    for (uint j=0; j<WID; ++j) 
      for (uint k=0; k<WID; ++k) 
	for (uint i=0; i<WID; ++i) {
	  values[i_pblock(i,j,k)] = block->fx[i_block(i,k,j)];
      }    
    // Copy averages from +z neighbour if it exists (if not, array is initialized to zero)
    nbrBlock = block->neighbors[velocity_neighbor::XCC_YP1_ZCC];
    if ( nbrBlock != NULL) {
      for (uint j=0; j<WID; ++j) 
	for (uint k=WID; k<WID+STENCIL_WIDTH; ++k) 
	  for (uint i=0; i<WID; ++i) {
	    values[i_pblock(i,j,k)] = nbrBlock->fx[i_block(i,k-WID,j)];
	  }
    }
}



bool map_1d(SpatialCell* spatial_cell,   Real intersection, Real intersection_di, Real intersection_dj,Real intersection_dk,
	   uint dimension ) {
  /*values used with an stencil in 1 dimension, initialized to 0 */  
  Real values[WID*WID*(WID+2*STENCIL_WIDTH)]={};
  // Make a copy of the blocklist, the blocklist will change during this algorithm
  uint*  blocks=new uint[spatial_cell->number_of_blocks];
  const uint nblocks=spatial_cell->number_of_blocks;
  /* 
     Move densities from data to fx and clear data, to prepare for mapping
     Also copy blocklist since the velocity block list in spatial cell changes when we add values
  */
  for (unsigned int block_i = 0; block_i < nblocks; block_i++) {
    const unsigned int block = spatial_cell->velocity_block_list[block_i];
    blocks[block_i] = block; 
    Velocity_Block * __restrict__ block_ptr = spatial_cell->at(block);
    Real * __restrict__ fx = block_ptr->fx;
    Real * __restrict__ data = block_ptr->data;

    for (unsigned int cell = 0; cell < VELOCITY_BLOCK_LENGTH; cell++) {
      fx[cell] = data[cell];
      data[cell] = 0.0;
    }
  }
  if(dimension>2)
    return false; //not possible
  
  Real dr;
  Real is_temp;
  uint block_indices_to_id[3];
  uint cell_indices_to_id[3];

  switch (dimension){
  case 0:
    /* i and k coordinates have been swapped*/
    /*set cell size in dimension direction*/
    dr=SpatialCell::cell_dvx; 
    /*swap intersection i and k coordinates*/
    is_temp=intersection_di;
    intersection_di=intersection_dk;
    intersection_dk=is_temp;
    /*set values in array that is used to transfer blockindices to id using a dot product*/
    block_indices_to_id[0]=SpatialCell::vx_length * SpatialCell::vy_length;
    block_indices_to_id[1]=SpatialCell::vx_length;
    block_indices_to_id[2]=1;
    /*set values in array that is used to transfer blockindices to id using a dot product*/
    cell_indices_to_id[0]=WID2;
    cell_indices_to_id[1]=WID;
    cell_indices_to_id[2]=1;
    break;
  case 1:
    /* j and k coordinates have been swapped*/
    /*set cell size in dimension direction*/
    dr=SpatialCell::cell_dvy;
    /*swap intersection j and k coordinates*/
    is_temp=intersection_dj;
    intersection_dj=intersection_dk;
    intersection_dk=is_temp;
    /*set values in array that is used to transfer blockindices to id using a dot product*/
    block_indices_to_id[0]=1;
    block_indices_to_id[1]=SpatialCell::vx_length * SpatialCell::vy_length;
    block_indices_to_id[2]=SpatialCell::vx_length;
    /*set values in array that is used to transfer blockindices to id using a dot product*/
    cell_indices_to_id[0]=1;
    cell_indices_to_id[1]=WID2;
    cell_indices_to_id[2]=WID;
    break;
  case 2:
    /*set cell size in dimension direction*/
    dr=SpatialCell::cell_dvz;
    /*set values in array that is used to transfer blockindices to id using a dot product*/
    block_indices_to_id[0]=1;
    block_indices_to_id[1]=SpatialCell::vx_length;
    block_indices_to_id[2]=SpatialCell::vx_length * SpatialCell::vy_length;
    /*set values in array that is used to transfer blockindices to id using a dot product*/
    cell_indices_to_id[0]=1;
    cell_indices_to_id[1]=WID;
    cell_indices_to_id[2]=WID2;
    break;
  }
  const Real i_dr=1.0/dr;
  
  for (unsigned int block_i = 0; block_i < nblocks; block_i++) {
    Velocity_Block *block=spatial_cell->at(blocks[block_i]);
    velocity_block_indices_t block_indices=SpatialCell::get_velocity_block_indices(blocks[block_i]);
    uint temp;
    switch (dimension){
    case 0:
      /* i and k coordinates have been swapped*/
      copy_block_data_x(block,values); 
      temp=block_indices[2];
      block_indices[2]=block_indices[0];
      block_indices[0]=temp;
      break;
    case 1:
      /*in values j and k coordinates have been swapped*/
      copy_block_data_y(block,values); 
      temp=block_indices[2];
      block_indices[2]=block_indices[1];
      block_indices[1]=temp;
      break;
    case 2:
      copy_block_data_z(block,values);
      break;
    }
    
    /*i,j,k are now relative to the order in which we copied data to the values array. 
      After this point in the k,j,i loops there should be no branches based on dimensions
     */
    for (uint k=0; k<WID; ++k){ 
      //todo, could also compute z index and compute the start velocity of block
      const Real v_l=(WID*block_indices[2]+k)*dr;
      const Real v_c=v_l+0.5*dr;
      const Real v_r=v_l+dr;
      for (uint j = 0; j < WID; ++j){
	for (uint i = 0; i < WID; ++i){
	  const Real intersection_min=intersection +
	    (block_indices[0]*WID+i)*intersection_di + 
	    (block_indices[1]*WID+j)*intersection_dj;
	  //f is mean value
	  const Real f = values[i_pblock(i,j,k)];
	  //A is slope of linear approximation
	  const Real A = slope_limiter(values[i_pblock(i,j,k-1)],
				       f,values[i_pblock(i,j,k+1)])*i_dr;
	  //left(l) and right(r) k values (global index) in the target
	  //lagrangian grid, the intersecting cells
	  const uint lagrangian_gk_l=(v_l-intersection_min)/intersection_dk; 
	  const uint lagrangian_gk_r=(v_r-intersection_min)/intersection_dk;

	  //the velocity between the two target cells. If both v_r and
	  //v_l are in same cell then lagrangian_v will be smaller
	  //than v_l, set it then to v_l
	  const Real lagrangian_v = max(lagrangian_gk_r * intersection_dk + intersection_min, v_l);
	  
	  //target mass is value in center of intersecting length,
	  //times length (missing x,y, but they would be cancelled
	  //anyway when we divide to get density
	  const Real target_mass_l = (f + A * (0.5*(lagrangian_v+v_l)-v_c))*(lagrangian_v - v_l);
	  const Real target_mass_r = f*dr-target_mass_l; //the rest	  
	  

	  //the blocks of the two lagrangian cells
	  const uint target_block_l = 
	    block_indices[0]*block_indices_to_id[0]+
	    block_indices[1]*block_indices_to_id[1]+
	    (lagrangian_gk_l/WID)*block_indices_to_id[2];
	  const uint target_block_r= 
	    block_indices[0]*block_indices_to_id[0]+
	    block_indices[1]*block_indices_to_id[1]+
	    (lagrangian_gk_r/WID)*block_indices_to_id[2];
	  
	  //cell index in the block of the l,r target cells
	  const uint target_cell_l = 
	    i*cell_indices_to_id[0] + 
	    j*cell_indices_to_id[1] +
	    (lagrangian_gk_l%WID)*cell_indices_to_id[2];

	  const uint target_cell_r = 
	    i*cell_indices_to_id[0] + 
	    j*cell_indices_to_id[1] +
	    (lagrangian_gk_r%WID)*cell_indices_to_id[2];
	  
	  spatial_cell->increment_value(target_block_l,target_cell_l,target_mass_l*i_dr);
	  spatial_cell->increment_value(target_block_r,target_cell_r,target_mass_r*i_dr);
	  
	}
      }
    }
  }
  delete [] blocks;
  return true;
}


#endif   

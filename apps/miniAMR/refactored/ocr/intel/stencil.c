// ************************************************************************
//
// miniAMR: stencil computations with boundary exchange and AMR.
//
// Copyright (2014) Sandia Corporation. Under the terms of Contract
// DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
// retains certain rights in this software.
//
// Portions Copyright (2016) Intel Corporation.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
// Questions? Contact Courtenay T. Vaughan (ctvaugh@sandia.gov)
//                    Richard F. Barrett (rfbarre@sandia.gov)
//
// ************************************************************************

//#include <mpi.h>

#include "block.h"
#include "control.h"
#include "proto.h"

#ifdef NANNY_FUNC_NAMES
#line __LINE__ "stencil"
#endif

// This routine does the stencil calculations.
void stencil_calc(u32 depc, ocrEdtDep_t depv[], int var) {

   blockClone_Deps_t * myDeps  = (blockClone_Deps_t *) depv;
   Control_t         * control = myDeps->control_Dep.ptr;
   Block_t           * block   = myDeps->block_Dep.ptr;
   double (* cp) /*[control->num_vars]*/ [control->x_block_size+2] [control->y_block_size+2] [control->z_block_size+2] =
       (double(*)/*[control->num_vars]*/ [control->x_block_size+2] [control->y_block_size+2] [control->z_block_size+2]) (block->cells);
   int i, j, k;
   double sb, sm, sf, work[control->x_block_size+2][control->y_block_size+2][control->z_block_size+2];

   if (control->stencil == 7) {
      for (i = 1; i <= control->x_block_size; i++) {
         for (j = 1; j <= control->y_block_size; j++) {
            for (k = 1; k <= control->z_block_size; k++) {
               work[i][j][k] = (cp[var][i-1][j  ][k  ] +
                                cp[var][i  ][j-1][k  ] +
                                cp[var][i  ][j  ][k-1] +
                                cp[var][i  ][j  ][k  ] +
                                cp[var][i  ][j  ][k+1] +
                                cp[var][i  ][j+1][k  ] +
                                cp[var][i+1][j  ][k  ])/7.0;
            }
         }
      }
      for (i = 1; i <= control->x_block_size; i++) {
         for (j = 1; j <= control->y_block_size; j++) {
            for (k = 1; k <= control->z_block_size; k++) {
               cp[var][i][j][k] = work[i][j][k];
            }
         }
      }
   } else {
      for (i = 1; i <= control->x_block_size; i++) {
         for (j = 1; j <= control->y_block_size; j++) {
            for (k = 1; k <= control->z_block_size; k++) {
               sb = cp[var][i-1][j-1][k-1] +
                    cp[var][i-1][j-1][k  ] +
                    cp[var][i-1][j-1][k+1] +
                    cp[var][i-1][j  ][k-1] +
                    cp[var][i-1][j  ][k  ] +
                    cp[var][i-1][j  ][k+1] +
                    cp[var][i-1][j+1][k-1] +
                    cp[var][i-1][j+1][k  ] +
                    cp[var][i-1][j+1][k+1];
               sm = cp[var][i  ][j-1][k-1] +
                    cp[var][i  ][j-1][k  ] +
                    cp[var][i  ][j-1][k+1] +
                    cp[var][i  ][j  ][k-1] +
                    cp[var][i  ][j  ][k  ] +
                    cp[var][i  ][j  ][k+1] +
                    cp[var][i  ][j+1][k-1] +
                    cp[var][i  ][j+1][k  ] +
                    cp[var][i  ][j+1][k+1];
               sf = cp[var][i+1][j-1][k-1] +
                    cp[var][i+1][j-1][k  ] +
                    cp[var][i+1][j-1][k+1] +
                    cp[var][i+1][j  ][k-1] +
                    cp[var][i+1][j  ][k  ] +
                    cp[var][i+1][j  ][k+1] +
                    cp[var][i+1][j+1][k-1] +
                    cp[var][i+1][j+1][k  ] +
                    cp[var][i+1][j+1][k+1];
               work[i][j][k] = (sb + sm + sf)/27.0;
            }
         }
      }
      for (i = 1; i <= control->x_block_size; i++) {
         for (j = 1; j <= control->y_block_size; j++) {
            for (k = 1; k <= control->z_block_size; k++) {
               cp[var][i][j][k] = work[i][j][k];
            }
         }
      }
   }
}

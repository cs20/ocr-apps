/*
 *  Copyright (c) 2014, The Regents of the University of California, through
 *  Lawrence Berkeley National Laboratory and UChicago Argonne, LLC.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/orother materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *  THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  Portions contributed by Battelle Memorial Institute developed under Contract
 *  Number DE-SC0008717 with the U.S. Department of Energy for the operation of
 *  the Pacific Northwest National Laboratory: Copyright 2015, Battelle Memorial
 *  Institute.  Contributed under the terms of the original license.
 *
 *  Authors
 *   v.CnC: Ellen Porter (ellen.porter@pnnl.gov)
 *          Luke Rodriguez (luke.rodriguez@pnnl.gov)
 *          Pacific Northwest National Lab
 *   HPGMG: Samuel Williams (SWWilliams@lbl.gov)
 *          Lawrence Berkeley National Lab
 *
 */

#include "hpgmg.h"

/**
 * Step function definition for "interpolate"
 */
void hpgmg_interpolate(cncTag_t cycle, cncTag_t level, cncTag_t i, cncTag_t j,
    cncTag_t k, double *u_in_previous_level_0, double *u_in_previous_level_1,
    double *u_in_previous_segment, hpgmgCtx *ctx) {

  int num_cells = ctx->num_cells_per_level[level];
  int num_blocks = ctx->num_blocks_per_level[level];
  int cells = num_cells+2*GHOSTS;
  int sizeof_block = block_size(cells, cells, cells);

  double *u_out = cncItemAlloc(sizeof_block);

  if(i < num_blocks && j < num_blocks && k < num_blocks) {

    initialize_block(u_out, cells, cells, cells);

    if(!ctx->restrict_blocks[level]) {
      interpolate_cells(u_out, u_in_previous_level_0, u_in_previous_segment,
          num_cells);
    } else {
      interpolate_blocks(u_out, ctx->num_cells_per_level[level+1], i, j, k,
          u_in_previous_level_1, u_in_previous_segment);
    }
  }

  //
  // OUTPUTS
  //
  cncPut_u(u_out, 1, cycle, 1, level, i, j, k, ctx);

}

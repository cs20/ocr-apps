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

#include <mpi.h>

#include "block.h"
#include "comm.h"
#include "proto.h"

// This routine uses the communication structure for communicating ghost
// values to communicate the new processors of blocks that will be moving
// in the mesh during load balancing.  This coupled with blocks communicating
// to neighboring blocks on processor allows reconstruction of the structure
// used to communicate ghost values.
void comm_proc(Globals_t * const glbl)
{
   OBTAIN_ACCESS_TO_blocks
   typedef struct {
      Frame_Header_t myFrame;
      int i, j, k, n, offset, dir, which, face, face_case, err, type;
      MPI_Status status;
      struct {
         int *send_int;
         int *recv_int;
         Block_t *bp;
      } pointers;
      Frame_Header_t calleeFrame;
   } Frame__comm_proc_t;

#define i                 (lcl->i)
#define j                 (lcl->j)
#define k                 (lcl->k)
#define n                 (lcl->n)
#define offset            (lcl->offset)
#define dir               (lcl->dir)
#define which             (lcl->which)
#define face              (lcl->face)
#define face_case         (lcl->face_case)
#define err               (lcl->err)
#define type              (lcl->type)
#define send_int          (lcl->pointers.send_int)
#define recv_int          (lcl->pointers.recv_int)
#define bp                (lcl->pointers.bp)
#define status            (lcl->status)

   SUSPENDABLE_FUNCTION_PROLOGUE(Frame__comm_proc_t)
   TRACE

   send_int = (int *) send_buff;
   recv_int = (int *) recv_buff;

   for (dir = 0; dir < 3; dir++) {
      type = 10 + dir;
      for (i = 0; i < num_comm_partners[dir]; i++)
         gasket__mpi_Irecv(glbl, &recv_int[comm_index[dir][i]], comm_num[dir][i],
                   MPI_INTEGER, comm_partner[dir][i], type, MPI_COMM_WORLD, -9999,
                   &request[i], __FILE__, __LINE__);

      for (i = 0; i < num_comm_partners[dir]; i++) {
         if (nonblocking)
            offset = comm_index[dir][i];
         else
            offset = 0;
         for (n = 0; n < comm_num[dir][i]; n++)
            send_int[offset+n] =
                        blocks[comm_block[dir][comm_index[dir][i]+n]].new_proc;
         if (nonblocking) {
            CALL_SUSPENDABLE_CALLEE(1)
            gasket__mpi_Isend(glbl, &send_int[comm_index[dir][i]], comm_num[dir][i],
                      MPI_INTEGER, comm_partner[dir][i], type, MPI_COMM_WORLD, -9999,
                      &s_req[i], __FILE__, __LINE__);
            DEBRIEF_SUSPENDABLE_FUNCTION(;)
         } else {
            CALL_SUSPENDABLE_CALLEE(0)
            gasket__mpi_Send(glbl, &send_int[0], comm_num[dir][i], MPI_INTEGER,
                     comm_partner[dir][i], type, MPI_COMM_WORLD, -9999, __FILE__, __LINE__);
            DEBRIEF_SUSPENDABLE_FUNCTION(;)
         }
      }

      for (i = 0; i < num_comm_partners[dir]; i++) {
#ifdef BUILD_REFERENCE_VERSION   // Original reference version did this:
         err = MPI_Waitany(num_comm_partners[dir], request, &which, &status);
#else   // OCR doesn't (presently) support the concept of an EDT triggering when ANY of a list of dependences is satisfied.  Rework
        // to wait specifically for each IRecv.  Also provide stuff OCR version needs to be able to receive the result buffer.
         CALL_SUSPENDABLE_CALLEE(1)
         err = gasket__mpi_Wait__for_Irecv (glbl, &recv_int[comm_index[dir][i]], comm_num[dir][i], MPI_INTEGER, comm_partner[dir][i], MPI_COMM_WORLD, -9999, &request[i], &status, __FILE__, __LINE__);
         DEBRIEF_SUSPENDABLE_FUNCTION(;)
         which = i;
#endif
         for (n = 0; n < comm_num[dir][which]; n++) {
            face = dir*2+(comm_face_case[dir][comm_index[dir][which]+n] >= 10);
            bp = &blocks[comm_block[dir][comm_index[dir][which]+n]];
            j = k = 0;
            face_case = comm_face_case[dir][comm_index[dir][which]+n]%10;
            if (face_case >= 6) {
               j = ((face_case+2)/2)%2;
               k = face_case%2;
            }
            bp->nei[face][j][k] = -1 - recv_int[comm_index[dir][which]+n];
         }
      }

      if (nonblocking)
         for (i = 0; i < num_comm_partners[dir]; i++) {
#ifdef BUILD_REFERENCE_VERSION   // Original reference version did this:
            err = MPI_Waitany(num_comm_partners[dir], s_req, &which, &status);
#else   // OCR doesn't (presently) support the concept of an EDT triggering when ANY of a list of dependences is satisfied.  Rework
        // to wait specifically for each ISend.
            err = gasket__mpi_Wait__for_Isend(glbl, &s_req[i], &status, __FILE__, __LINE__);
#endif
         }
   }
   SUSPENDABLE_FUNCTION_NORMAL_RETURN_SEQUENCE(;)
   SUSPENDABLE_FUNCTION_EPILOGUE
#undef  i
#undef  j
#undef  k
#undef  n
#undef  offset
#undef  dir
#undef  which
#undef  face
#undef  face_case
#undef  err
#undef  type
#undef  send_int
#undef  recv_int
#undef  bp
#undef  status
}

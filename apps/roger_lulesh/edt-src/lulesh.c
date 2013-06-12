/*

                 Copyright (c) 2010.
      Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
                  LLNL-CODE-461231
                All rights reserved.

This file is part of LULESH, Version 1.0.0
Please also read this link -- http://www.opensource.org/licenses/index.php

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the disclaimer below.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the disclaimer (as noted below)
     in the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the LLNS/LLNL nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC,
THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Additional BSD Notice

1. This notice is required to be provided under our contract with the U.S.
   Department of Energy (DOE). This work was produced at Lawrence Livermore
   National Laboratory under Contract No. DE-AC52-07NA27344 with the DOE.

2. Neither the United States Government nor Lawrence Livermore National
   Security, LLC nor any of their employees, makes any warranty, express
   or implied, or assumes any liability or responsibility for the accuracy,
   completeness, or usefulness of any information, apparatus, product, or
   process disclosed, or represents that its use would not infringe
   privately-owned rights.

3. Also, reference herein to any specific commercial products, process, or
   services by trade name, trademark, manufacturer or otherwise does not
   necessarily constitute or imply its endorsement, recommendation, or
   favoring by the United States Government or Lawrence Livermore National
   Security, LLC. The views and opinions of authors expressed herein do not
   necessarily state or reflect those of the United States Government or
   Lawrence Livermore National Security, LLC, and shall not be used for
   advertising or product endorsement purposes.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#if !defined(FSIM) && !defined(OCR)
#include <string.h>
#endif

#include "RAG.h"
#include "AMO.h"
#include "MEM.h"
#include "FP.h"

#if defined(FSIM)
static struct DomainObject_t domainObject = { .guid.data = (uint64_t)NULL, .base = NULL, .offset = 0, .limit = 0 };
#elif defined(OCR)
static struct DomainObject_t domainObject = { .guid      = (uint64_t)NULL, .base = NULL, .offset = 0, .limit = 0 };
#endif // FSIM or OCR

static SHARED struct Domain_t *domain = NULL;

static INLINE
Real_t *Allocate_Real_t( size_t hcSize ) {
  return  (Real_t *)SPAD_MALLOC(hcSize,sizeof(Real_t));
} // Allocate_Real_t()

static INLINE
void     Release_Real_t( Real_t *ptr ) {
  if(ptr != NULL) SPAD_FREE(ptr);
} // Release_Real_t()

/* RAG -- prototypes for the edt functions */
#include "RAG_edt.h"
/* RAG -- moved serial/scalar functions to here */
#include "kernels.h"

/* This first implementation allows for runnable code */
/* and is not meant to be optimal. Final implementation */
/* should separate declaration and allocation phases */
/* so that allocation can be scheduled in a cache conscious */
/* manner. */

   /**************/
   /* Allocation */
   /**************/

void
domain_AllocateNodalPersistent(size_t hcSize) {
TRACE1("Allocate EP entry");
#ifdef FSIM
  xe_printf("rag: domain              %16.16lx\n",domain);
  xe_printf("rag: domain->m_x         %16.16lx\n",domain->m_x);
  xe_printf("rag: domain->m_y         %16.16lx\n",domain->m_y);
  xe_printf("rag: domain->m_z         %16.16lx\n",domain->m_z);
  xe_printf("rag: domain->m_xd        %16.16lx\n",domain->m_xd);
  xe_printf("rag: domain->m_yd        %16.16lx\n",domain->m_yd);
  xe_printf("rag: domain->m_zd        %16.16lx\n",domain->m_zd);
  xe_printf("rag: domain->m_xdd       %16.16lx\n",domain->m_xdd);
  xe_printf("rag: domain->m_ydd       %16.16lx\n",domain->m_ydd);
  xe_printf("rag: domain->m_zdd       %16.16lx\n",domain->m_zdd);
  xe_printf("rag: domain->m_fx        %16.16lx\n",domain->m_fx);
  xe_printf("rag: domain->m_fy        %16.16lx\n",domain->m_fy);
  xe_printf("rag: domain->m_fz        %16.16lx\n",domain->m_fz);
  xe_printf("rag: domain->m_nodalMass %16.16lx\n",domain->m_nodalMass);
#endif // FSIM
TRACE1("Allocate NP m_(x|y|z)");
   if(domain->m_x != NULL)DRAM_FREE(domain->m_x); domain->m_x = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_y != NULL)DRAM_FREE(domain->m_y); domain->m_y = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_z != NULL)DRAM_FREE(domain->m_z); domain->m_z = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
TRACE1("Allocate NP m_(x|y|z)d");
   if(domain->m_xd != NULL)DRAM_FREE(domain->m_xd); domain->m_xd = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
   if(domain->m_yd != NULL)DRAM_FREE(domain->m_yd); domain->m_yd = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
   if(domain->m_zd != NULL)DRAM_FREE(domain->m_zd); domain->m_zd = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
TRACE1("Allocate NP m_(x|y|z)dd");
   if(domain->m_xdd != NULL)DRAM_FREE(domain->m_xdd); domain->m_xdd = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
   if(domain->m_ydd != NULL)DRAM_FREE(domain->m_ydd); domain->m_ydd = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
   if(domain->m_zdd != NULL)DRAM_FREE(domain->m_zdd); domain->m_zdd = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
TRACE1("Allocate NP m_f(x|y|z)");
   if(domain->m_fx != NULL)DRAM_FREE(domain->m_fx); domain->m_fx = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_fy != NULL)DRAM_FREE(domain->m_fy); domain->m_fy = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_fz != NULL)DRAM_FREE(domain->m_fz); domain->m_fz = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
TRACE1("Allocate NP m_nodalMass(x|y|z)");
   if(domain->m_nodalMass != NULL)DRAM_FREE(domain->m_nodalMass); domain->m_nodalMass = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
TRACE1("Allocate NP zero m_(x|y|z)d, m_(x|y|z)dd and m_nodalMass(x|y|z)");
#ifdef FSIM
  xe_printf("rag: domain %16.16lx\n",domain);
  xe_printf("rag: domain->m_xd %16.16lx\n",domain->m_xd);
  xe_printf("rag: domain->m_xy %16.16lx\n",domain->m_yd);
  xe_printf("rag: domain->m_xz %16.16lx\n",domain->m_zd);
  xe_printf("rag: domain->m_xd %16.16lx\n",domain->m_xdd);
  xe_printf("rag: domain->m_xy %16.16lx\n",domain->m_ydd);
  xe_printf("rag: domain->m_xz %16.16lx\n",domain->m_zdd);
  xe_printf("rag: domain->m_nodalMassv %16.16lx\n",domain->m_nodalMass);
  xe_printf("rag: loop (0;<%16.16lx;1)\n",hcSize);
#endif
  FINISH
    EDT_PAR_FOR_0xNx1(i,hcSize,domain_AllocateNodalPersistent_edt_1,domain);
  END_FINISH
#ifdef FSIM
  xe_printf("rag: domain              %16.16lx\n",domain);
  xe_printf("rag: domain->m_x         %16.16lx\n",domain->m_x);
  xe_printf("rag: domain->m_y         %16.16lx\n",domain->m_y);
  xe_printf("rag: domain->m_z         %16.16lx\n",domain->m_z);
  xe_printf("rag: domain->m_xd        %16.16lx\n",domain->m_xd);
  xe_printf("rag: domain->m_yd        %16.16lx\n",domain->m_yd);
  xe_printf("rag: domain->m_zd        %16.16lx\n",domain->m_zd);
  xe_printf("rag: domain->m_xdd       %16.16lx\n",domain->m_xdd);
  xe_printf("rag: domain->m_ydd       %16.16lx\n",domain->m_ydd);
  xe_printf("rag: domain->m_zdd       %16.16lx\n",domain->m_zdd);
  xe_printf("rag: domain->m_fx        %16.16lx\n",domain->m_fx);
  xe_printf("rag: domain->m_fy        %16.16lx\n",domain->m_fy);
  xe_printf("rag: domain->m_fz        %16.16lx\n",domain->m_fz);
  xe_printf("rag: domain->m_nodalMass %16.16lx\n",domain->m_nodalMass);
#endif // FSIM
TRACE1("Allocate NP returns");
}

void
domain_AllocateElemPersistent(size_t hcSize) {
TRACE1("Allocate EP entry");
#if defined(FSIM)
  xe_printf("rag: domain                %16.16lx\n",domain);
  xe_printf("rag: domain->m_matElemlist %16.16lx\n",domain->m_matElemlist);
  xe_printf("rag: domain->m_nodelist    %16.16lx\n",domain->m_nodelist);
  xe_printf("rag: domain->m_lxim        %16.16lx\n",domain->m_lxim);
  xe_printf("rag: domain->m_lxip        %16.16lx\n",domain->m_lxip);
  xe_printf("rag: domain->m_letam       %16.16lx\n",domain->m_letam);
  xe_printf("rag: domain->m_letap       %16.16lx\n",domain->m_letap);
  xe_printf("rag: domain->m_lzetam      %16.16lx\n",domain->m_lzetam);
  xe_printf("rag: domain->m_lzetap      %16.16lx\n",domain->m_lzetap);
  xe_printf("rag: domain->m_elemBC      %16.16lx\n",domain->m_elemBC);
  xe_printf("rag: domain->m_e           %16.16lx\n",domain->m_e);
  xe_printf("rag: domain->m_p           %16.16lx\n",domain->m_p);
  xe_printf("rag: domain->m_q           %16.16lx\n",domain->m_q);
  xe_printf("rag: domain->m_ql          %16.16lx\n",domain->m_ql);
  xe_printf("rag: domain->m_qq          %16.16lx\n",domain->m_qq);
  xe_printf("rag: domain->m_v           %16.16lx\n",domain->m_v);
  xe_printf("rag: domain->m_volo        %16.16lx\n",domain->m_volo);
  xe_printf("rag: domain->m_delv        %16.16lx\n",domain->m_delv);
  xe_printf("rag: domain->m_vdov        %16.16lx\n",domain->m_vdov);
  xe_printf("rag: domain->m_arealg      %16.16lx\n",domain->m_arealg);
  xe_printf("rag: domain->m_ss          %16.16lx\n",domain->m_ss);
  xe_printf("rag: domain->m_elemMass    %16.16lx\n",domain->m_elemMass);
#endif // FSIM
TRACE1("Allocate EP m_matElemlist and m_nodelist");
   if(domain->m_matElemlist != NULL)DRAM_FREE(domain->m_matElemlist); domain->m_matElemlist = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
   if(domain->m_nodelist != NULL)DRAM_FREE(domain->m_nodelist); domain->m_nodelist= (SHARED Index_t *)DRAM_MALLOC(hcSize,EIGHT*sizeof(Index_t)) ;
TRACE1("Allocate EP m_lixm,m_lixp,m_letam,m_letap,m_lzetam,m_lzetap");
   if(domain->m_lxim != NULL)DRAM_FREE(domain->m_lxim); domain->m_lxim = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
   if(domain->m_lxip != NULL)DRAM_FREE(domain->m_lxip); domain->m_lxip = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
   if(domain->m_letam != NULL)DRAM_FREE(domain->m_letam); domain->m_letam = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
   if(domain->m_letap != NULL)DRAM_FREE(domain->m_letap); domain->m_letap = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
   if(domain->m_lzetam != NULL)DRAM_FREE(domain->m_lzetam); domain->m_lzetam = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
   if(domain->m_lzetap != NULL)DRAM_FREE(domain->m_lzetap); domain->m_lzetap = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
TRACE1("Allocate EP m_elemBC");
   if(domain->m_elemBC != NULL)DRAM_FREE(domain->m_elemBC); domain->m_elemBC = (SHARED Int_t *)DRAM_MALLOC(hcSize,sizeof(Int_t)) ;
TRACE1("Allocate EP m_e,m_p,m_q,m_ql,m_qq");
   if(domain->m_e != NULL)DRAM_FREE(domain->m_e); domain->m_e = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
   if(domain->m_p != NULL)DRAM_FREE(domain->m_p); domain->m_p = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
   if(domain->m_q != NULL)DRAM_FREE(domain->m_q); domain->m_q = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_ql != NULL)DRAM_FREE(domain->m_ql); domain->m_ql = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_qq != NULL)DRAM_FREE(domain->m_qq); domain->m_qq = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
TRACE1("Allocate EP m_v,m_volo,m_delv,m_vdov");
   if(domain->m_v != NULL)DRAM_FREE(domain->m_v); domain->m_v = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t));
   if(domain->m_volo != NULL)DRAM_FREE(domain->m_volo); domain->m_volo = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_delv != NULL)DRAM_FREE(domain->m_delv); domain->m_delv = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_vdov != NULL)DRAM_FREE(domain->m_vdov); domain->m_vdov = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
TRACE1("Allocate EP m_arealg,m_ss,m_elemMass");
   if(domain->m_arealg != NULL)DRAM_FREE(domain->m_arealg); domain->m_arealg = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_ss != NULL)DRAM_FREE(domain->m_ss); domain->m_ss = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
   if(domain->m_elemMass != NULL)DRAM_FREE(domain->m_elemMass); domain->m_elemMass = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
TRACE1("Allocate EP zero m_(e|p|v)");
#if defined(FSIM)
  xe_printf("rag: domain %16.16lx\n",domain);
  xe_printf("rag: domain->m_e %16.16lx\n",domain->m_e);
  xe_printf("rag: domain->m_p %16.16lx\n",domain->m_p);
  xe_printf("rag: domain->m_v %16.16lx\n",domain->m_v);
  xe_printf("rag: loop (0;<%16.16lx;1)\n",hcSize);
#endif // FSIM
   FINISH 
     EDT_PAR_FOR_0xNx1(i,hcSize,domain_AllocateElemPersistent_edt_1,domain);
   END_FINISH
#if defined(FSIM)
  xe_printf("rag: domain                %16.16lx\n",domain);
  xe_printf("rag: domain->m_matElemlist %16.16lx\n",domain->m_matElemlist);
  xe_printf("rag: domain->m_nodelist    %16.16lx\n",domain->m_nodelist);
  xe_printf("rag: domain->m_lxim        %16.16lx\n",domain->m_lxim);
  xe_printf("rag: domain->m_lxip        %16.16lx\n",domain->m_lxip);
  xe_printf("rag: domain->m_letam       %16.16lx\n",domain->m_letam);
  xe_printf("rag: domain->m_letap       %16.16lx\n",domain->m_letap);
  xe_printf("rag: domain->m_lzetam      %16.16lx\n",domain->m_lzetam);
  xe_printf("rag: domain->m_lzetap      %16.16lx\n",domain->m_lzetap);
  xe_printf("rag: domain->m_elemBC      %16.16lx\n",domain->m_elemBC);
  xe_printf("rag: domain->m_e           %16.16lx\n",domain->m_e);
  xe_printf("rag: domain->m_p           %16.16lx\n",domain->m_p);
  xe_printf("rag: domain->m_q           %16.16lx\n",domain->m_q);
  xe_printf("rag: domain->m_ql          %16.16lx\n",domain->m_ql);
  xe_printf("rag: domain->m_qq          %16.16lx\n",domain->m_qq);
  xe_printf("rag: domain->m_v           %16.16lx\n",domain->m_v);
  xe_printf("rag: domain->m_volo        %16.16lx\n",domain->m_volo);
  xe_printf("rag: domain->m_delv        %16.16lx\n",domain->m_delv);
  xe_printf("rag: domain->m_vdov        %16.16lx\n",domain->m_vdov);
  xe_printf("rag: domain->m_arealg      %16.16lx\n",domain->m_arealg);
  xe_printf("rag: domain->m_ss          %16.16lx\n",domain->m_ss);
  xe_printf("rag: domain->m_elemMass    %16.16lx\n",domain->m_elemMass);
#endif // FSIM
TRACE1("Allocate EP returns");
}

   /* Temporaries should not be initialized in bulk but */
   /* this is a runnable placeholder for now */
void
domain_AllocateElemTemporary(size_t hcSize) {
TRACE1("Allocate ET entry");
#if defined(FSIM)
  xe_printf("rag: domain              %16.16lx\n",domain);
  xe_printf("rag: domain->m_dxx       %16.16lx\n",domain->m_dxx);
  xe_printf("rag: domain->m_dyy       %16.16lx\n",domain->m_dyy);
  xe_printf("rag: domain->m_dzz       %16.16lx\n",domain->m_dzz);
  xe_printf("rag: domain->m_delv_xi   %16.16lx\n",domain->m_delv_xi);
  xe_printf("rag: domain->m_delv_eta  %16.16lx\n",domain->m_delv_eta);
  xe_printf("rag: domain->m_delv_zeta %16.16lx\n",domain->m_delv_zeta);
  xe_printf("rag: domain->m_delx_xi   %16.16lx\n",domain->m_delx_xi);
  xe_printf("rag: domain->m_delx_eta  %16.16lx\n",domain->m_delx_eta);
  xe_printf("rag: domain->m_delx_zeta %16.16lx\n",domain->m_delx_zeta);
  xe_printf("rag: domain->m_vnew      %16.16lx\n",domain->m_vnew);
#endif // FSIM
TRACE1("Allocate ET m_d(xx|yy|zz)");
  if(domain->m_dxx != NULL)DRAM_FREE(domain->m_dxx); domain->m_dxx = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
  if(domain->m_dyy != NULL)DRAM_FREE(domain->m_dyy); domain->m_dyy = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
  if(domain->m_dzz != NULL)DRAM_FREE(domain->m_dzz); domain->m_dzz = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
TRACE1("Allocate ET m_delv_zi,m_delv_eta,m_delv_zeta");
  if(domain->m_delv_xi != NULL)DRAM_FREE(domain->m_delv_xi); domain->m_delv_xi = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
  if(domain->m_delv_eta != NULL)DRAM_FREE(domain->m_delv_eta); domain->m_delv_eta = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
  if(domain->m_delv_zeta != NULL)DRAM_FREE(domain->m_delv_zeta); domain->m_delv_zeta = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
TRACE1("Allocate ET m_delx_zi,m_delx_eta,m_delx_zeta");
  if(domain->m_delx_xi != NULL)DRAM_FREE(domain->m_delx_xi); domain->m_delx_xi = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
  if(domain->m_delx_eta != NULL)DRAM_FREE(domain->m_delx_eta); domain->m_delx_eta = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
  if(domain->m_delx_zeta != NULL)DRAM_FREE(domain->m_delx_zeta); domain->m_delx_zeta = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
TRACE1("Allocate ET m_vnew");
  if(domain->m_vnew != NULL)DRAM_FREE(domain->m_vnew); domain->m_vnew = (SHARED Real_t *)DRAM_MALLOC(hcSize,sizeof(Real_t)) ;
#if defined(FSIM)
  xe_printf("rag: domain              %16.16lx\n",domain);
  xe_printf("rag: domain->m_dxx       %16.16lx\n",domain->m_dxx);
  xe_printf("rag: domain->m_dyy       %16.16lx\n",domain->m_dyy);
  xe_printf("rag: domain->m_dzz       %16.16lx\n",domain->m_dzz);
  xe_printf("rag: domain->m_delv_xi   %16.16lx\n",domain->m_delv_xi);
  xe_printf("rag: domain->m_delv_eta  %16.16lx\n",domain->m_delv_eta);
  xe_printf("rag: domain->m_delv_zeta %16.16lx\n",domain->m_delv_zeta);
  xe_printf("rag: domain->m_delx_xi   %16.16lx\n",domain->m_delx_xi);
  xe_printf("rag: domain->m_delx_eta  %16.16lx\n",domain->m_delx_eta);
  xe_printf("rag: domain->m_delx_zeta %16.16lx\n",domain->m_delx_zeta);
  xe_printf("rag: domain->m_vnew      %16.16lx\n",domain->m_vnew);
#endif // FSIM
TRACE1("Allocate ET returns");
}

void
domain_AllocateNodesets(size_t hcSize) {
TRACE1("Allocate NS entry");
#if defined(FSIM)
  xe_printf("rag: domain              %16.16lx\n",domain);
  xe_printf("rag: domain->m_symmX     %16.16lx\n",domain->m_symmX);
  xe_printf("rag: domain->m_symmY     %16.16lx\n",domain->m_symmY);
  xe_printf("rag: domain->m_symmZ     %16.16lx\n",domain->m_symmZ);
#endif // FSIM
TRACE1("Allocate NS m_symm(X|Y|Z)");
   if(domain->m_symmX != NULL)DRAM_FREE(domain->m_symmX); domain->m_symmX = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
   if(domain->m_symmY != NULL)DRAM_FREE(domain->m_symmY); domain->m_symmY = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
   if(domain->m_symmZ != NULL)DRAM_FREE(domain->m_symmZ); domain->m_symmZ = (SHARED Index_t *)DRAM_MALLOC(hcSize,sizeof(Index_t)) ;
#if defined(FSIM)
  xe_printf("rag: domain              %16.16lx\n",domain);
  xe_printf("rag: domain->m_symmX     %16.16lx\n",domain->m_symmX);
  xe_printf("rag: domain->m_symmY     %16.16lx\n",domain->m_symmY);
  xe_printf("rag: domain->m_symmZ     %16.16lx\n",domain->m_symmZ);
#endif // FSIM
TRACE1("Allocate NS returns");
}
#if !defined(FSIM) && !defined(OCR)
   /****************/
   /* Deallocation */
   /****************/

void
domain_DeallocateNodalPersistent(void) {
TRACE1("Deallocate NP m_(x|y|z)");
   if(domain->m_x != NULL)DRAM_FREE(domain->m_x); domain->m_x = NULL;
   if(domain->m_y != NULL)DRAM_FREE(domain->m_y); domain->m_y = NULL;
   if(domain->m_z != NULL)DRAM_FREE(domain->m_z); domain->m_z = NULL;
TRACE1("Deallocate NP m_(x|y|z)d");
   if(domain->m_xd != NULL)DRAM_FREE(domain->m_xd); domain->m_xd = NULL;
   if(domain->m_yd != NULL)DRAM_FREE(domain->m_yd); domain->m_yd = NULL;
   if(domain->m_zd != NULL)DRAM_FREE(domain->m_zd); domain->m_zd = NULL;
TRACE1("Deallocate NP m_(x|y|z)dd");
   if(domain->m_xdd != NULL)DRAM_FREE(domain->m_xdd); domain->m_xdd = NULL;
   if(domain->m_ydd != NULL)DRAM_FREE(domain->m_ydd); domain->m_ydd = NULL;
   if(domain->m_zdd != NULL)DRAM_FREE(domain->m_zdd); domain->m_zdd = NULL;
TRACE1("Deallocate NP m_f(x|y|z)");
   if(domain->m_fx != NULL)DRAM_FREE(domain->m_fx); domain->m_fx = NULL;
   if(domain->m_fy != NULL)DRAM_FREE(domain->m_fy); domain->m_fy = NULL;
   if(domain->m_fz != NULL)DRAM_FREE(domain->m_fz); domain->m_fz = NULL;
TRACE1("Deallocate NP m_nodalMass(x|y|z)");
   if(domain->m_nodalMass != NULL)DRAM_FREE(domain->m_nodalMass); domain->m_nodalMass = NULL;
}

void
domain_DeallocateElemPersistent(void) {
TRACE1("Deallocate EP m_matElemlist and m_nodelist");
   if(domain->m_matElemlist != NULL)DRAM_FREE(domain->m_matElemlist); domain->m_matElemlist = NULL;
   if(domain->m_nodelist != NULL)DRAM_FREE(domain->m_nodelist); domain->m_nodelist= NULL;
TRACE1("Deallocate EP m_lixm,m_lixp,m_letam,m_letap,m_lzetam,m_lzetap");
   if(domain->m_lxim != NULL)DRAM_FREE(domain->m_lxim); domain->m_lxim = NULL;
   if(domain->m_lxip != NULL)DRAM_FREE(domain->m_lxip); domain->m_lxip = NULL;
   if(domain->m_letam != NULL)DRAM_FREE(domain->m_letam); domain->m_letam = NULL;
   if(domain->m_letap != NULL)DRAM_FREE(domain->m_letap); domain->m_letap = NULL;
   if(domain->m_lzetam != NULL)DRAM_FREE(domain->m_lzetam); domain->m_lzetam = NULL;
   if(domain->m_lzetap != NULL)DRAM_FREE(domain->m_lzetap); domain->m_lzetap = NULL;
TRACE1("Deallocate EP m_elemBC");
   if(domain->m_elemBC != NULL)DRAM_FREE(domain->m_elemBC); domain->m_elemBC = NULL;
TRACE1("Deallocate EP m_e,m_p,m_q,m_ql,m_qq");
   if(domain->m_e != NULL)DRAM_FREE(domain->m_e); domain->m_e = NULL;
   if(domain->m_p != NULL)DRAM_FREE(domain->m_p); domain->m_p = NULL;
   if(domain->m_q != NULL)DRAM_FREE(domain->m_q); domain->m_q = NULL;
   if(domain->m_ql != NULL)DRAM_FREE(domain->m_ql); domain->m_ql = NULL;
   if(domain->m_qq != NULL)DRAM_FREE(domain->m_qq); domain->m_qq = NULL;
TRACE1("Deallocate EP m_v,m_volo,m_delv,m_vdov");
   if(domain->m_v != NULL)DRAM_FREE(domain->m_v); domain->m_v = NULL;
   if(domain->m_volo != NULL)DRAM_FREE(domain->m_volo); domain->m_volo = NULL;
   if(domain->m_delv != NULL)DRAM_FREE(domain->m_delv); domain->m_delv = NULL;
   if(domain->m_vdov != NULL)DRAM_FREE(domain->m_vdov); domain->m_vdov = NULL;
TRACE1("Deallocate EP m_arealg,m_ss,m_elemMass");
   if(domain->m_arealg != NULL)DRAM_FREE(domain->m_arealg); domain->m_arealg = NULL;
   if(domain->m_ss != NULL)DRAM_FREE(domain->m_ss); domain->m_ss = NULL;
   if(domain->m_elemMass != NULL)DRAM_FREE(domain->m_elemMass); domain->m_elemMass = NULL;
}

   /* Temporaries should not be initialized in bulk but */
   /* this is a runnable placeholder for now */
void
domain_DeallocateElemTemporary(void) {
TRACE1("Deallocate ET m_d(xx|yy|zz)");
  if(domain->m_dxx != NULL)DRAM_FREE(domain->m_dxx); domain->m_dxx = NULL;
  if(domain->m_dyy != NULL)DRAM_FREE(domain->m_dyy); domain->m_dyy = NULL;
  if(domain->m_dzz != NULL)DRAM_FREE(domain->m_dzz); domain->m_dzz = NULL;
TRACE1("Deallocate ET m_delv_zi,m_delv_eta,m_delv_zeta");
  if(domain->m_delv_xi != NULL)DRAM_FREE(domain->m_delv_xi); domain->m_delv_xi = NULL;
  if(domain->m_delv_eta != NULL)DRAM_FREE(domain->m_delv_eta); domain->m_delv_eta = NULL;
  if(domain->m_delv_zeta != NULL)DRAM_FREE(domain->m_delv_zeta); domain->m_delv_zeta = NULL;
TRACE1("Deallocate ET m_delx_zi,m_delx_eta,m_delx_zeta");
  if(domain->m_delx_xi != NULL)DRAM_FREE(domain->m_delx_xi); domain->m_delx_xi = NULL;
  if(domain->m_delx_eta != NULL)DRAM_FREE(domain->m_delx_eta); domain->m_delx_eta = NULL;
  if(domain->m_delx_zeta != NULL)DRAM_FREE(domain->m_delx_zeta); domain->m_delx_zeta = NULL;
TRACE1("Deallocate ET m_vnew");
  if(domain->m_vnew != NULL)DRAM_FREE(domain->m_vnew); domain->m_vnew = NULL;
}

void
domain_DeallocateNodesets(void) {
TRACE1("Deallocate NS m_symm(X|Y|Z)");
   if(domain->m_symmX != NULL)DRAM_FREE(domain->m_symmX); domain->m_symmX = NULL;
   if(domain->m_symmY != NULL)DRAM_FREE(domain->m_symmY); domain->m_symmY = NULL;
   if(domain->m_symmZ != NULL)DRAM_FREE(domain->m_symmZ); domain->m_symmZ = NULL;
}
#endif //NOT FSIM and NOT OCR

static INLINE
void InitStressTermsForElems(Index_t numElem, 
                             Real_t *sigxx, Real_t *sigyy, Real_t *sigzz) {
   // pull in the stresses appropriate to the hydro integration
   FINISH 
     EDT_PAR_FOR_0xNx1(i,numElem,InitStressTermsForElems_edt_1,domain,sigxx,sigyy,sigzz);
   END_FINISH
} // InitStressTermsForElems()

static INLINE
void IntegrateStressForElems( Index_t numElem,
                              Real_t *sigxx, Real_t *sigyy, Real_t *sigzz,
                              Real_t *determ) {
  // loop over all elements
  FINISH
    EDT_PAR_FOR_0xNx1(i,numElem,IntegrateStressForElems_edt_1,domain,sigxx,sigyy,sigzz,determ);
  END_FINISH
} // IntegrateStressForElems()

static INLINE
void CalcFBHourglassForceForElems(Real_t *determ,
            Real_t *x8n,      Real_t *y8n,      Real_t *z8n,
            Real_t *dvdx,     Real_t *dvdy,     Real_t *dvdz,
            Real_t hourg) {
  /*************************************************
   *
   *     FUNCTION: Calculates the Flanagan-Belytschko anti-hourglass
   *               force.
   *
   *************************************************/
  Index_t numElem = domain->m_numElem ;
// compute the hourglass modes
  FINISH
    EDT_PAR_FOR_0xNx1(i2,numElem,CalcFBHourglassForceForElems_edt_1,domain,hourg,determ,x8n,y8n,z8n,dvdx,dvdy,dvdz)
  END_FINISH
} // CalcFBHourglassForceForElems

static INLINE
void CalcHourglassControlForElems(Real_t determ[], Real_t hgcoef) {
TRACE6("CalcHourglassControlForElems entry");
  Index_t numElem = domain->m_numElem ;
  Index_t numElem8 = numElem * EIGHT ;

TRACE6("Allocate dvdx,dvdy,dvdz,x8n,y8n,z8n");
  Real_t *dvdx = Allocate_Real_t(numElem8) ;
  Real_t *dvdy = Allocate_Real_t(numElem8) ;
  Real_t *dvdz = Allocate_Real_t(numElem8) ;
  Real_t *x8n  = Allocate_Real_t(numElem8) ;
  Real_t *y8n  = Allocate_Real_t(numElem8) ;
  Real_t *z8n  = Allocate_Real_t(numElem8) ;

TRACE6("/* start loop over elements */");
  FINISH
    EDT_PAR_FOR_0xNx1(i,numElem,CalcHourglassControlForElems_edt_1,domain,determ,x8n,y8n,z8n,dvdx,dvdy,dvdz)
  END_FINISH

  if ( hgcoef > cast_Real_t(0.) ) {
TRACE6("Call CalcFBHourglassForceForElems()");
    CalcFBHourglassForceForElems(determ,x8n,y8n,z8n,dvdx,dvdy,dvdz,hgcoef) ;
  }

TRACE6("Release  z8n,y8n,x8n,dvdz,dvdy,dvdx");
  Release_Real_t(z8n) ;
  Release_Real_t(y8n) ;
  Release_Real_t(x8n) ;
  Release_Real_t(dvdz) ;
  Release_Real_t(dvdy) ;
  Release_Real_t(dvdx) ;

TRACE6("CalcHourglassControlForElems return");
  return ;
} // CalcHourglassControlForElems

static INLINE
void CalcVolumeForceForElems() {
TRACE5("CalcVolueForceForElems() entry");
  Index_t numElem = domain->m_numElem ;

  if (numElem != 0) {
    Real_t  hgcoef = domain->m_hgcoef ;

TRACE5("Allocate sigxx,sigyy,sigzz,determ");
    Real_t *sigxx  = Allocate_Real_t(numElem) ;
    Real_t *sigyy  = Allocate_Real_t(numElem) ;
    Real_t *sigzz  = Allocate_Real_t(numElem) ;
    Real_t *determ = Allocate_Real_t(numElem) ;

TRACE5("/* Sum contributions to total stress tensor */");
    InitStressTermsForElems( numElem , sigxx , sigyy , sigzz );

TRACE5("// call elemlib stress integration loop to produce nodal forces from");
TRACE5("// material stresses.");
    IntegrateStressForElems( numElem , sigxx , sigyy , sigzz , determ) ;

TRACE5("// check for negative element volume");
    FINISH
      EDT_PAR_FOR_0xNx1(i,numElem,CalcVolumeForceForElems_edt_1,determ)
    END_FINISH

TRACE5("// Hourglass Control for Elems");
    CalcHourglassControlForElems(determ, hgcoef) ;

TRACE5("Release  determ,sigzz,sigyy,sigxx");
    Release_Real_t(determ) ;
    Release_Real_t(sigzz) ;
    Release_Real_t(sigyy) ;
    Release_Real_t(sigxx) ;
  } // if numElem

TRACE5("CalcVolueForceForElems() entry");
} // CalcVolumeForceForElems()

static INLINE void CalcForceForNodes() {
  Index_t numNode = domain->m_numNode ;
  FINISH 
    EDT_PAR_FOR_0xNx1(i,numNode,CalcForceForNodes_edt_1,domain)
  END_FINISH

TRACE4("/* Calcforce calls partial, force, hourq */");

  CalcVolumeForceForElems() ;

TRACE4("/* Calculate Nodal Forces at domain boundaries */");
TRACE4("/* problem->commSBN->Transfer(CommSBN::forces); */");

} // CalcForceForNodes()

static INLINE
void CalcAccelerationForNodes() {
  Index_t numNode = domain->m_numNode ;
  FINISH 
    EDT_PAR_FOR_0xNx1(i,numNode,CalcAccelerationForNodes_edt_1,domain)
  END_FINISH
} // CalcAccelerationForNodes()

static INLINE
void ApplyAccelerationBoundaryConditionsForNodes() {
  Index_t numNodeBC = (domain->m_sizeX+1)*(domain->m_sizeX+1) ;
  FINISH
    EDT_PAR_FOR_0xNx1(i,numNodeBC,ApplyAccelerationBoundaryConditionsForNodes_edt_1,domain);
  END_FINISH
} // ApplyAccelerationBoundaryConditionsForNodes()

static INLINE
void CalcVelocityForNodes(const Real_t dt, const Real_t u_cut) {
  Index_t numNode = domain->m_numNode ;
  FINISH
    EDT_PAR_FOR_0xNx1(i,numNode,CalcVelocityForNodes_edt_1,domain,dt,u_cut)
  END_FINISH
} // CalcVelocityForNodes()

static INLINE
void CalcPositionForNodes(const Real_t dt) {
  Index_t numNode = domain->m_numNode ;
//DEBUG fprintf(stdout,"CPFN:dt= %e\n",dt);
  FINISH
    EDT_PAR_FOR_0xNx1(i,numNode,CalcPositionForNodes_edt_1,domain,dt)
  END_FINISH
} // CalcPositionForNodes()

static INLINE
void LagrangeNodal() {
  HAB_CONST Real_t delt = domain->m_deltatime ;
  Real_t u_cut = domain->m_u_cut ;

TRACE2("/* time of boundary condition evaluation is beginning of step for force and");
TRACE2(" * acceleration boundary conditions. */");

TRACE2(" /* Call CalcForceForNodes() */");

  CalcForceForNodes();

TRACE2(" /* Call AccelerationForNodes() */");

  CalcAccelerationForNodes();

TRACE2(" /* Call ApplyAccelerationBoundaryConditionsForNodes() */");

  ApplyAccelerationBoundaryConditionsForNodes();

TRACE2(" /* Call CalcVelocityForNOdes() */");

  CalcVelocityForNodes( delt, u_cut ) ;

TRACE2(" /* Call CalcPositionForNodes() */");

  CalcPositionForNodes( delt );

  return;
} // LagrangeNodal()

static INLINE
void CalcKinematicsForElems( Index_t numElem, Real_t dt ) {
  FINISH
    // loop over all elements
    EDT_PAR_FOR_0xNx1(i,numElem,CalcKinematicsForElems_edt_1,domain,dt);
  END_FINISH
} // CalcKinematicsForElems()

static INLINE
void CalcLagrangeElements(Real_t deltatime) {
   Index_t numElem = domain->m_numElem ;
   if (numElem > 0) {
      CalcKinematicsForElems(numElem, deltatime) ;

      // element loop to do some stuff not included in the elemlib function.
      FINISH
        EDT_PAR_FOR_0xNx1(i,numElem,CalcLagrangeElements_edt_1,domain);
      END_FINISH
   } // if numElem
} // CalcLagrangeElements()

static INLINE
void CalcMonotonicQGradientsForElems() {
  Index_t numElem = domain->m_numElem ;
  FINISH
    EDT_PAR_FOR_0xNx1(i,numElem,CalcMonotonicQGradientsForElems_edt_1,domain)
  END_FINISH
} // CalcMonotonicQGradientsForElems()

static INLINE
void CalcMonotonicQRegionForElems(
                          Real_t qlc_monoq,            // parameters
                          Real_t qqc_monoq,
                          Real_t monoq_limiter_mult,
                          Real_t monoq_max_slope,
                          Real_t ptiny,
                          Index_t elength ) {          // the elementset length
  FINISH
    EDT_PAR_FOR_0xNx1(ielem,elength,CalcMonotonicQRegionForElems_edt_1,domain,qlc_monoq,qqc_monoq,monoq_limiter_mult,monoq_max_slope,ptiny);
  END_FINISH
} // CalcMonotonicQRegionForElems()

static INLINE
void CalcMonotonicQForElems() {  
   //
   // initialize parameters
   // 
   HAB_CONST Real_t ptiny    = cast_Real_t(1.e-36) ;
   Real_t monoq_max_slope    = domain->m_monoq_max_slope ;
   Real_t monoq_limiter_mult = domain->m_monoq_limiter_mult ;

   //
   // calculate the monotonic q for pure regions
   //
   Index_t elength = domain->m_numElem ;
   if (elength > 0) {
      Real_t qlc_monoq = domain->m_qlc_monoq;
      Real_t qqc_monoq = domain->m_qqc_monoq;
      CalcMonotonicQRegionForElems(// parameters
                           qlc_monoq,
                           qqc_monoq,
                           monoq_limiter_mult,
                           monoq_max_slope,
                           ptiny,

                           // the elemset length
                           elength );
   } // if elength
} // CalcMonotonicQForElems()

static SHARED uint64_t index_AMO;
static SHARED uint64_t *pIndex_AMO = &index_AMO;

static INLINE
void CalcQForElems() {
   Real_t qstop = domain->m_qstop ;
   Index_t numElem = domain->m_numElem ;

   //
   // MONOTONIC Q option
   //

   /* Calculate velocity gradients */
   CalcMonotonicQGradientsForElems() ;

   /* Transfer veloctiy gradients in the first order elements */
   /* problem->commElements->Transfer(CommElements::monoQ) ; */
   CalcMonotonicQForElems() ;
   /* Don't allow excessive artificial viscosity */
   if (numElem != 0) {
#ifdef    UPC
      bupc_atomicU64_set_strict(pIndex_AMO,(uint64_t)0); 
#else  // NOT UPC
      *pIndex_AMO = 0; 
#endif // UPC
      FINISH
        EDT_PAR_FOR_0xNx1(i,numElem,CalcQForElems_edt_1,domain,qstop,pIndex_AMO);
      END_FINISH

#ifdef    UPC
      if ( bupc_atomicU64_read_strict(pIndex_AMO) > 0 ) {
         EXIT(QStopError) ;
      } // if pIndex_AMO
#else  // NOT UPC
      if( *pIndex_AMO > 0 ) {
         EXIT(QStopError) ;
      } // if pIndex_AMO
#endif // UPC
   } // if numElem
} // CalcQForElems()

#if 0  // RAG -- inlined CalcPressureForElems_edt_1 into CalcEnergyForElems()
static INLINE
void CalcPressureForElems(Real_t* p_new, Real_t* bvc,
                          Real_t* pbvc, Real_t* e_old,
                          Real_t* compression, Real_t *vnewc,
                          Real_t pmin,
                          Real_t p_cut, Real_t eosvmax,
                          Index_t length) {
  FINISH
    EDT_PAR_FOR_0xNx1(i,length,CalcPressureForElems_edt_1,p_new,bvc,pbvc,e_old,compression,vnewc,pmin,p_cut,eosvmax)
  END_FINISH
} // CalcPressureForElems()
#endif

static INLINE
void CalcEnergyForElems(Real_t* p_new, Real_t* e_new, Real_t* q_new,
                        Real_t* bvc, Real_t* pbvc,
                        Real_t* p_old, Real_t* e_old, Real_t* q_old,
                        Real_t* compression, Real_t* compHalfStep,
                        Real_t* vnewc, Real_t* work, Real_t* delvc, Real_t pmin,
                        Real_t p_cut, Real_t  e_cut, Real_t q_cut, Real_t emin,
                        Real_t* qq, Real_t* ql,
                        Real_t rho0,
                        Real_t eosvmax,
                        Index_t length) {
  Real_t *pHalfStep = Allocate_Real_t(length) ;
  FINISH // RAG STRIDE ONE
    EDT_PAR_FOR_0xNx1(i,length,CalcEnergyForElems_edt_1,  e_new,e_old,delvc,p_old,q_old,work,
                                                          emin);
    EDT_PAR_FOR_0xNx1(i,length,CalcPressureForElems_edt_1,pHalfStep,bvc,pbvc,e_new,compHalfStep,vnewc,
                                                          pmin,p_cut,eosvmax);
    EDT_PAR_FOR_0xNx1(i,length,CalcEnergyForElems_edt_2,  compHalfStep,q_new,qq,ql,pbvc,e_new,bvc,pHalfStep,delvc,p_old,q_old,work,
                                                          rho0,e_cut,emin);
    EDT_PAR_FOR_0xNx1(i,length,CalcPressureForElems_edt_1,p_new,bvc,pbvc,e_new,compression,vnewc,
                                                          pmin,p_cut,eosvmax)
    EDT_PAR_FOR_0xNx1(i,length,CalcEnergyForElems_edt_3,  delvc,pbvc,e_new,vnewc,bvc,p_new,ql,qq,p_old,q_old,pHalfStep,q_new,
                                                          rho0,e_cut,emin);
    EDT_PAR_FOR_0xNx1(i,length,CalcPressureForElems_edt_1,p_new,bvc,pbvc,e_new,compression,vnewc,
                                                          pmin,p_cut,eosvmax);
    EDT_PAR_FOR_0xNx1(i,length,CalcEnergyForElems_edt_4,  delvc,pbvc,e_new,vnewc,bvc,p_new,ql,qq,q_new,
                                                          rho0,q_cut);
  END_FINISH // RAG STRIDE ONE
  Release_Real_t(pHalfStep) ;
  return ;
} // CalcEnergyForElems()

#if 0
static INLINE
void CalcSoundSpeedForElems(Real_t *vnewc, Real_t rho0, Real_t *enewc,
                            Real_t *pnewc, Real_t *pbvc,
                            Real_t *bvc,   Index_t nz) {
  FINISH // RAG SCATTERS
    EDT_PAR_FOR_0xNx1(i,nz,CalcSoundSpeedForElems_edt_1,domain,vnewc,enewc,pnewc,pbvc,bvc,rho0)
  END_FINISH // RAG SCATTERs
} // CalcSoundSpeedForElems()
#endif

static INLINE
void EvalEOSForElems(Real_t *vnewc, Index_t length) {
  Real_t  e_cut = domain->m_e_cut;
  Real_t  p_cut = domain->m_p_cut;
  Real_t  q_cut = domain->m_q_cut;

  Real_t eosvmax = domain->m_eosvmax ;
  Real_t eosvmin = domain->m_eosvmin ;
  Real_t pmin    = domain->m_pmin ;
  Real_t emin    = domain->m_emin ;
  Real_t rho0    = domain->m_refdens ;

  Real_t *e_old = Allocate_Real_t(length) ;
  Real_t *delvc = Allocate_Real_t(length) ;
  Real_t *p_old = Allocate_Real_t(length) ;
  Real_t *q_old = Allocate_Real_t(length) ;
  Real_t *compression = Allocate_Real_t(length) ;
  Real_t *compHalfStep = Allocate_Real_t(length) ;
  Real_t *qq = Allocate_Real_t(length) ;
  Real_t *ql = Allocate_Real_t(length) ;
  Real_t *work = Allocate_Real_t(length) ;
  Real_t *p_new = Allocate_Real_t(length) ;
  Real_t *e_new = Allocate_Real_t(length) ;
  Real_t *q_new = Allocate_Real_t(length) ;
  Real_t *bvc = Allocate_Real_t(length) ;
  Real_t *pbvc = Allocate_Real_t(length) ;

TRACE1("/* compress data, minimal set */");

  FINISH // RAG GATHERS
#if 1
    EDT_PAR_FOR_0xNx1(i,length,EvalEOSForElems_edt_1,domain,delvc,e_old,p_old,q_old,qq,ql);
#else
    PAR_FOR_0xNx1(i,length,domain,delvc,e_old,p_old,q_old,qq,ql)
      Index_t zidx = domain->m_matElemlist[i] ;
      e_old[i] = domain->m_e[zidx] ;
      delvc[i] = domain->m_delv[zidx] ;
      p_old[i] = domain->m_p[zidx] ;
      q_old[i] = domain->m_q[zidx] ;
      qq[i]    = domain->m_qq[zidx] ;
      ql[i]    = domain->m_ql[zidx] ;
    END_PAR_FOR(i)
#endif
  END_FINISH // RAG GATHERS

TRACE1("/* Check for v > eosvmax or v < eosvmin */");

  FINISH // RAG STRIDE ONE
#if 1
    EDT_PAR_FOR_0xNx1(i,length,EvalEOSForElems_edt_2,compression,vnewc,delvc,compHalfStep,work,p_old,eosvmin,eosvmax);
#else
    PAR_FOR_0xNx1(i,length,compression,vnewc,delvc,compHalfStep,work,p_old,eosvmin,eosvmax)
      Real_t vchalf ;
      compression[i] = cast_Real_t(1.) / vnewc[i] - cast_Real_t(1.);
      vchalf = vnewc[i] - delvc[i] * cast_Real_t(.5);
      compHalfStep[i] = cast_Real_t(1.) / vchalf - cast_Real_t(1.);
      work[i] = cast_Real_t(0.) ; 
//  END_PAR_FOR(i)

      if ( eosvmin != cast_Real_t(0.) ) {
//    PAR_FOR_0xNx1(i,length,compression,vnewc,delvc,compHalfStep,work,p_old,eosvmin,eosvmax)
        if (vnewc[i] <= eosvmin) { /* impossible due to calling func? */
          compHalfStep[i] = compression[i] ;
        } // if vnewc
//    END_PAR_FOR(i)
      } // if eosvmin

      if ( eosvmax != cast_Real_t(0.) ) {
//    PAR_FOR_0xNx1(i,length,compression,vnewc,delvc,compHalfStep,work,p_old,eosvmin,eosvmax)
        if (vnewc[i] >= eosvmax) { /* impossible due to calling func? */
          p_old[i]        = cast_Real_t(0.) ;
          compression[i]  = cast_Real_t(0.) ;
          compHalfStep[i] = cast_Real_t(0.) ;
        } // if vnewc
//    END_PAR_FOR(i)
      } // if eosvmax

    END_PAR_FOR(i)
#endif
  END_FINISH // RAG STRIDE ONE

  CalcEnergyForElems(p_new, e_new, q_new, bvc, pbvc,
                     p_old, e_old, q_old, compression, compHalfStep,
                     vnewc, work,  delvc, pmin,
                     p_cut, e_cut, q_cut, emin,
                     qq, ql, rho0, eosvmax, length);


  FINISH     // RAG SCATTERS
#if 1
    EDT_PAR_FOR_0xNx1(i,length,EvalEOSForElems_edt_3,domain,p_new,e_new,q_new);
#else
    PAR_FOR_0xNx1(i,length,domain,p_new,e_new,q_new)
      Index_t zidx = domain->m_matElemlist[i] ;
      domain->m_p[zidx] = p_new[i] ;
      domain->m_e[zidx] = e_new[i] ;
      domain->m_q[zidx] = q_new[i] ;
    END_PAR_FOR(i)
#endif
  END_FINISH // RAG SCATTERS

#if 1
  FINISH     // RAG SCATTERS
    EDT_PAR_FOR_0xNx1(i,length,CalcSoundSpeedForElems_edt_1,domain,vnewc,e_new,p_new,pbvc,bvc,rho0)
  END_FINISH // RAG SCATTERs
#else
  CalcSoundSpeedForElems(vnewc, rho0, e_new, p_new,
                         pbvc, bvc, length) ;
#endif

  Release_Real_t(pbvc) ;
  Release_Real_t(bvc) ;
  Release_Real_t(q_new) ;
  Release_Real_t(e_new) ;
  Release_Real_t(p_new) ;
  Release_Real_t(work) ;
  Release_Real_t(ql) ;
  Release_Real_t(qq) ;
  Release_Real_t(compHalfStep) ;
  Release_Real_t(compression) ;
  Release_Real_t(q_old) ;
  Release_Real_t(p_old) ;
  Release_Real_t(delvc) ;
  Release_Real_t(e_old) ;
} // EvalEOSForElems()

static INLINE
void ApplyMaterialPropertiesForElems() {
  Index_t length = domain->m_numElem ;

  if (length != 0) {
TRACE1("/* Expose all of the variables needed for material evaluation */");
    Real_t eosvmin = domain->m_eosvmin ;
    Real_t eosvmax = domain->m_eosvmax ;
    Real_t *vnewc  = Allocate_Real_t(length) ;

// RAG GATHERS

    FINISH
      PAR_FOR_0xNx1(i,length,domain,vnewc)
        Index_t zn = domain->m_matElemlist[i] ;
        vnewc[i] = domain->m_vnew[zn] ;
      END_PAR_FOR(i)
    END_FINISH 

// RAG STIDE ONES

    FINISH

      if (eosvmin != cast_Real_t(0.)) {
        PAR_FOR_0xNx1(i,length,vnewc,eosvmin)
          if (vnewc[i] < eosvmin) {
            vnewc[i] = eosvmin ;
          } // if vnewc
        END_PAR_FOR(i)
      } // if eosvmin

      if (eosvmax != cast_Real_t(0.)) {
        PAR_FOR_0xNx1(i,length,vnewc,eosvmax)
          if (vnewc[i] > eosvmax) {
            vnewc[i] = eosvmax ;
          } // if vnewc
        END_PAR_FOR(i)
      } // if eosvmax

    END_FINISH 

// RAG GATHER ERROR CHECK

    FINISH
      PAR_FOR_0xNx1(i,length,domain,vnewc,eosvmin,eosvmax)
        Index_t zn = domain->m_matElemlist[i] ;
        Real_t  vc = domain->m_v[zn] ;
        if (eosvmin != cast_Real_t(0.)) {
          if (vc < eosvmin) { 
            vc = eosvmin ;
          } // if domain->m_v
        } // if eosvmin
        if (eosvmax != cast_Real_t(0.)) {
          if (vc > eosvmax) {
            vc = eosvmax ;
          } // if domain->m_v
        } // if eosvmax
        if (vc <= 0.) {
          EXIT(VolumeError) ;
        } // if domain->m_v
      END_PAR_FOR(i)
    END_FINISH 

    EvalEOSForElems(vnewc, length);

    Release_Real_t(vnewc) ;

  } // if length

} // ApplyMaterialPropertiesForElems()

static INLINE
void UpdateVolumesForElems() {
  Index_t numElem = domain->m_numElem;
  if (numElem != 0) {
    Real_t v_cut = domain->m_v_cut;
    FINISH
      PAR_FOR_0xNx1(i,numElem,domain,v_cut)
        Real_t tmpV ;
        tmpV = domain->m_vnew[i] ;

        if ( FABS(tmpV - cast_Real_t(1.0)) < v_cut ) {
          tmpV = cast_Real_t(1.0) ;
        } // tmpV
        domain->m_v[i] = tmpV ;
      END_PAR_FOR(i)
    END_FINISH 
  } // if numElem
  return ;
} // UpdateVolumesForElems()

static INLINE
void LagrangeElements() {
  HAB_CONST Real_t deltatime = domain->m_deltatime ;

TRACE2("/* Call CalcLagrangeElements() */");

  CalcLagrangeElements(deltatime) ;

TRACE2("/* Call CalcQForElems() -- Calculate Q.  (Monotonic q option requires communication) */");

  CalcQForElems() ;

TRACE2("/* Call ApplyMaterialPropertiesForElems() */");

  ApplyMaterialPropertiesForElems() ;

TRACE2("/* Call UpdateVolumesForElems() */");

  UpdateVolumesForElems() ;

} // LagrangeElements()

static INLINE
void CalcCourantConstraintForElems() {
  Real_t  *pDtCourant    =  (Real_t *)SPAD_MALLOC(ONE,sizeof(Real_t)) ;
  Index_t *pCourant_elem = (Index_t *)SPAD_MALLOC(ONE,sizeof(Index_t));
  *pDtCourant = cast_Real_t(1.0e+20) ;
  *pCourant_elem = -1;

  FINISH
    Real_t      qqc = domain->m_qqc ;
    Real_t  qqc2 = cast_Real_t(64.0) * qqc * qqc ;
    Index_t length = domain->m_numElem ;
    PAR_FOR_0xNx1(i,length,domain,qqc2,pDtCourant,pCourant_elem,pidamin_lock)
      Index_t indx = domain->m_matElemlist[i] ;

      Real_t dtf = domain->m_ss[indx] * domain->m_ss[indx] ;

      if ( domain->m_vdov[indx] < cast_Real_t(0.) ) {

        dtf = dtf
            + qqc2 * domain->m_arealg[indx] * domain->m_arealg[indx]
            * domain->m_vdov[indx] * domain->m_vdov[indx] ;
      } // if domain->m_vdov

      dtf = SQRT(dtf) ;

      dtf = domain->m_arealg[indx] / dtf ;

      /* determine minimum timestep with its corresponding elem */
      if (domain->m_vdov[indx] != cast_Real_t(0.)) {
        if ( dtf < *pDtCourant ) {
AMO__lock_uint64_t(pidamin_lock);          // LOCK
          *pDtCourant    = dtf ;
          *pCourant_elem = indx ;
AMO__unlock_uint64_t(pidamin_lock);        // UNLOCK
        } // if *pDtCourant
      } // if domain->m_vdov

    END_PAR_FOR(i)
  END_FINISH

  /* Don't try to register a time constraint if none of the elements
   * were active */
  if ( *pCourant_elem != -1) {
//DEBUG fprintf(stdout,"dtcourant %e\n",domain->m_dtcourant);
     domain->m_dtcourant = *pDtCourant ;
  } // if *pCourant_elem

  SPAD_FREE(pCourant_elem);
  SPAD_FREE(pDtCourant);
  return ;
} // CalcCourantConstraintForElems()

static INLINE
void CalcHydroConstraintForElems() {
  Real_t  *pDtHydro    =  (Real_t *) SPAD_MALLOC(ONE,sizeof(Real_t)) ;
  Index_t *pHydro_elem = (Index_t *) SPAD_MALLOC(ONE,sizeof(Index_t));
  *pDtHydro = cast_Real_t(1.0e+20) ;
  *pHydro_elem = -1 ;

  FINISH
    Real_t dvovmax = domain->m_dvovmax ;
    Index_t length = domain->m_numElem ;
    PAR_FOR_0xNx1(i,length,domain,dvovmax,pidamin_lock,pDtHydro,pHydro_elem)
      Index_t indx = domain->m_matElemlist[i] ;

      if (domain->m_vdov[indx] != cast_Real_t(0.)) {
        Real_t dtdvov = dvovmax / (FABS(domain->m_vdov[indx])+cast_Real_t(1.e-20)) ;
        if ( *pDtHydro > dtdvov ) {
AMO__lock_uint64_t(pidamin_lock);          // LOCK
          *pDtHydro    = dtdvov ;
          *pHydro_elem = indx ;
AMO__unlock_uint64_t(pidamin_lock);        // UNLOCK
        } // if *pDtHydro
      } // if domain->m_vdov

    END_PAR_FOR(i)
  END_FINISH

  if (*pHydro_elem != -1) {
     domain->m_dthydro = *pDtHydro ;
//DEBUG fprintf(stdout,"dthydro %e\n",domain->m_dthydro);
  } // if *pHydro_elem

  SPAD_FREE(pHydro_elem);
  SPAD_FREE(pDtHydro);
  return ;
} // CalcHydroConstraintForElems()

static INLINE
void CalcTimeConstraintsForElems() {
TRACE3("CalcTimeConstrantsForElems() entry");

TRACE3("/* evaluate time constraint */");
  CalcCourantConstraintForElems() ;

TRACE3("/* check hydro constraint */");
  CalcHydroConstraintForElems() ;
TRACE3("CalcTimeConstrantsForElems() return");
} // CalcTimeConstraintsForElems()

static INLINE
void LagrangeLeapFrog() {
TRACE1("LagrangeLeapFrog() entry");

TRACE1("/* calculate nodal forces, accelerations, velocities, positions, with */");
TRACE1(" * applied boundary conditions and slide surface considerations       */");

  LagrangeNodal();

TRACE1("/* calculate element quantities (i.e. velocity gradient & q), and update */");
TRACE1(" * material states                                                       */");

  LagrangeElements();

TRACE1("/* calculate time constraints for elems */");

  CalcTimeConstraintsForElems();

TRACE1("LagrangeLeapFrog() return");
} // LagangeLeapFrog()

#if    defined(OCR)
ocrGuid_t mainEdt(u32 paramc, u64 *params, void *paramv[], u32 depc, ocrEdtDep_t depv[]);
int main(int argc, char ** argv) {
TRACE0("main entry");
  ocrEdt_t edtList[1] = { mainEdt };
  ocrGuid_t mainEdtGuid;
TRACE0("call ocrInit()");
  ocrInit((int *)&argc,argv,1,edtList);
TRACE0("call ocrEdtCreate()");
   /* (ocrGuid_t *)guid,ocrEdt_ funcPtr,u32 paramc, u64 *params, void *paramv[], u16 properties, u32 depc, ocrGuid_t *depv, ocrGuid_t *outputEvent */
  ocrEdtCreate(&mainEdtGuid, mainEdt, 0, NULL, NULL, 0, 0, NULL, NULL);
TRACE0("call ocrEdtSchedule()");
  ocrEdtSchedule(mainEdtGuid);
TRACE0("call ocrEdtCleanup()");
  ocrCleanup();
TRACE0("main return");
  return 0;
}

ocrGuid_t mainEdt(u32 paramc, u64 *params, void *paramv[], u32 depc, ocrEdtDep_t depv[]) {
#elif defined(FSIM)
int mainEdt() {
TRACE0("mainEdt entry");
#else // DEFAULT, cilk, h-c, c99, and upc
int main(int argc, char *argv[]) {
#endif // OCR or FSIM
#if     defined(FSIM)
// tiny problem size 
  Index_t edgeElems =  5 ;
// ran to completion quickly with 5, many cycles for 10, 15, 30 and 45
#else   // FSIM
  Index_t edgeElems = 45 ; // standard problem size
#endif // FSIM
  Index_t edgeNodes = edgeElems+1 ;
  Index_t domElems ;

TRACE0("/* allocate domain data structure */");

#if defined(FSIM) || defined(OCR)
  DOMAIN_CREATE(&domainObject,edgeElems,edgeNodes);
#endif // FSIM or OCR

  domain = (SHARED struct Domain_t *)DRAM_MALLOC(ONE,sizeof(struct Domain_t));
#if defined(FSIM)
  xe_printf("rag: domain %16.16lx\n",(uint64_t)domain);
#endif
#if defined(FSIM) || defined(OCR) || defined(UPC)
  for( size_t i = 0; i < ONE*sizeof(struct Domain_t) ; ++i ) {
     char *ptr = (char *)domain;
     ptr[i] = (char)0;
  } // for i
#else // DEFAULT all the others
  memset(domain,0,sizeof(ONE*sizeof(struct Domain_t)));
#endif // FSIM or OCR

TRACE0("/* get run options to measure various metrics */");

  /****************************/
  /*   Initialize Sedov Mesh  */
  /****************************/

TRACE0("/* construct a uniform box for this processor */");

  domain->m_sizeX   = edgeElems ;
  domain->m_sizeY   = edgeElems ;
  domain->m_sizeZ   = edgeElems ;
  domain->m_numElem = edgeElems*edgeElems*edgeElems ;
  domain->m_numNode = edgeNodes*edgeNodes*edgeNodes ;

  domElems = domain->m_numElem ;

TRACE0("/* allocate field memory */");

  domain_AllocateElemPersistent(domain->m_numElem) ;
  domain_AllocateElemTemporary (domain->m_numElem) ;

  domain_AllocateNodalPersistent(domain->m_numNode) ;
  domain_AllocateNodesets(edgeNodes*edgeNodes) ;

TRACE0("/* initialize nodal coordinates */");


  FINISH
    Real_t sf = cast_Real_t(1.125)/cast_Real_t(edgeElems);
    Index_t dimN = edgeNodes, dimNdimN = edgeNodes*edgeNodes;

    for(Index_t pln = 0 ; pln < edgeNodes ; ++pln ) {
      Real_t tz = cast_Real_t(pln)*sf;
      Index_t pln_nidx = pln*dimNdimN;
      for(Index_t row = 0 ; row < edgeNodes ; ++row ) {
        Real_t ty = cast_Real_t(row)*sf;
        Index_t pln_row_nidx = pln_nidx + row*dimN;
        PAR_FOR_0xNx1(col,edgeNodes,pln,row,tz,ty,pln_row_nidx,domain,sf,dimN,dimNdimN)
          Real_t tx = cast_Real_t(col)*sf;
          Index_t nidx = pln_row_nidx+col;
          domain->m_x[nidx] = tx;
//DEBUG if(nidx==1)fprintf(stdout,"m_x[1] = %e\n",domain->m_x[1]);
          domain->m_y[nidx] = ty;
//DEBUG if(nidx==1)fprintf(stdout,"m_y[1] = %e\n",domain->m_y[1]);
          domain->m_z[nidx] = tz;
//DEBUG if(nidx==1)fprintf(stdout,"m_z[1] = %e\n",domain->m_z[1]);
        END_PAR_FOR(col)
      } // for row
    } // for pln
  END_FINISH

TRACE0("/* embed hexehedral elements in nodal point lattice */");

  FINISH
    Index_t dimE = edgeElems, dimEdimE = edgeElems*edgeElems;
    Index_t dimN = edgeNodes, dimNdimN = edgeNodes*edgeNodes;

    for(Index_t pln = 0 ; pln < edgeElems ; ++pln ) {
      Index_t pln_nidx = pln*dimNdimN;
      Index_t pln_zidx = pln*dimEdimE;
      for(Index_t row = 0 ; row < edgeElems ; ++row ) {
        Index_t pln_row_nidx = pln_nidx + row*dimN;
        Index_t pln_row_zidx = pln_zidx + row*dimE;
        PAR_FOR_0xNx1(col,edgeElems,pln,row,pln_row_nidx,pln_row_zidx,domain,dimE,dimEdimE,dimN,dimNdimN)
          Index_t nidx = pln_row_nidx+col;
          Index_t zidx = pln_row_zidx+col;
          SHARED Index_t *localNode = (SHARED Index_t *)&domain->m_nodelist[EIGHT*zidx] ;
          localNode[0] = nidx                       ;
          localNode[1] = nidx                   + 1 ;
          localNode[2] = nidx            + dimN + 1 ;
          localNode[3] = nidx            + dimN     ;
          localNode[4] = nidx + dimNdimN            ;
          localNode[5] = nidx + dimNdimN        + 1 ;
          localNode[6] = nidx + dimNdimN + dimN + 1 ;
          localNode[7] = nidx + dimNdimN + dimN     ;
        END_PAR_FOR(col)
      } // for row
    } // for pln
  END_FINISH

TRACE0("/* Create a material IndexSet (entire domain same material for now) */");

  FINISH
    PAR_FOR_0xNx1(i,domElems,domain)
      domain->m_matElemlist[i] = i ;
    END_PAR_FOR(i)
  END_FINISH
   
TRACE0("/* initialize material parameters */");

  domain->m_dtfixed            = cast_Real_t(-1.0e-7) ;
  domain->m_deltatime          = cast_Real_t(1.0e-7) ;
  domain->m_deltatimemultlb    = cast_Real_t(1.1) ;
  domain->m_deltatimemultub    = cast_Real_t(1.2) ;
  domain->m_stoptime           = cast_Real_t(1.0e-2) ;
  domain->m_dtcourant          = cast_Real_t(1.0e+20) ;
  domain->m_dthydro            = cast_Real_t(1.0e+20) ;
  domain->m_dtmax              = cast_Real_t(1.0e-2) ;
  domain->m_time               = cast_Real_t(0.) ;
  domain->m_cycle              = 0 ;

  domain->m_e_cut              = cast_Real_t(1.0e-7) ;
  domain->m_p_cut              = cast_Real_t(1.0e-7) ;
  domain->m_q_cut              = cast_Real_t(1.0e-7) ;
  domain->m_u_cut              = cast_Real_t(1.0e-7) ;
  domain->m_v_cut              = cast_Real_t(1.0e-10) ;

  domain->m_hgcoef             = cast_Real_t(3.0) ;

  domain->m_qstop              = cast_Real_t(1.0e+12) ;
  domain->m_monoq_max_slope    = cast_Real_t(1.0) ;
  domain->m_monoq_limiter_mult = cast_Real_t(2.0) ;
  domain->m_qlc_monoq          = cast_Real_t(0.5) ;
  domain->m_qqc_monoq          = cast_Real_t(2.0)/cast_Real_t(3.0) ;
  domain->m_qqc                = cast_Real_t(2.0) ;

  domain->m_pmin               = cast_Real_t(0.) ;
  domain->m_emin               = cast_Real_t(-1.0e+15) ;

  domain->m_dvovmax            = cast_Real_t(0.1) ;

  domain->m_eosvmax            = cast_Real_t(1.0e+9) ;
  domain->m_eosvmin            = cast_Real_t(1.0e-9) ;

  domain->m_refdens            = cast_Real_t(1.0) ;

  FINISH
    PAR_FOR_0xNx1(i,domElems,domain)
#if  defined(HAB_C)
      Real_t *x_local = (Real_t *)malloc(EIGHT*sizeof(Real_t)) ;
      Real_t *y_local = (Real_t *)malloc(EIGHT*sizeof(Real_t)) ;
      Real_t *z_local = (Real_t *)malloc(EIGHT*sizeof(Real_t)) ;
#else // NOT HAB_C
      Real_t x_local[EIGHT], y_local[EIGHT], z_local[EIGHT] ;
#endif //    HAB_C
      SHARED Index_t *elemToNode = (SHARED Index_t *)&domain->m_nodelist[EIGHT*i] ;
      for( Index_t lnode=0 ; lnode < EIGHT ; ++lnode ) {
        Index_t gnode = elemToNode[lnode];
        x_local[lnode] = domain->m_x[gnode];
        y_local[lnode] = domain->m_y[gnode];
        z_local[lnode] = domain->m_z[gnode];
      } // for lnode

      // volume calculations
      Real_t volume = CalcElemVolume(x_local, y_local, z_local );
      domain->m_volo[i] = volume ;
      domain->m_elemMass[i] = volume ;

// RAG ///////////////////////////////////////////////////////// RAG //
// RAG  Atomic Memory Floating-point Addition Scatter operation  RAG //
// RAG ///////////////////////////////////////////////////////// RAG //

      for( Index_t j=0 ; j<EIGHT ; ++j ) {
        Index_t idx = elemToNode[j] ;
        Real_t value = volume / cast_Real_t(8.0);
        AMO__sync_addition_double(&domain->m_nodalMass[idx], value);
      } // for j

#if  defined(HAB_C)
      free(z_local) ;
      free(y_local) ;
      free(x_local) ;
#endif //    HAB_C
    END_PAR_FOR(i)
  END_FINISH

TRACE0("/* deposit energy */");

  domain->m_e[0] = cast_Real_t(3.948746e+7) ;

TRACE0("/* set up symmetry nodesets */");
//DEBUG fprintf(stdout,"e(0)=%e\n",domain->m_e[0]);

  FINISH
    Index_t dimN = edgeNodes, dimNdimN = dimN*dimN;
    
    for ( Index_t i = 0; i < edgeNodes ; ++i ) {
      Index_t planeInc = i*dimNdimN ;
      Index_t rowInc   = i*dimN ;
      PAR_FOR_0xNx1(j,edgeNodes,i,planeInc,rowInc,domain,dimN,dimNdimN)
        Index_t nidx = rowInc + j;
        domain->m_symmX[nidx] = planeInc + j*dimN ;
        domain->m_symmY[nidx] = planeInc + j ;
        domain->m_symmZ[nidx] = rowInc   + j ;
      END_PAR_FOR(j)
    } // for i
  END_FINISH

TRACE0("/* set up elemement connectivity information */");
//DEBUG fprintf(stdout,"e(0)=%e\n",domain->m_e[0]);

  FINISH
    domain->m_lxim[0] = 0 ;
    PAR_FOR_0xNx1(i,domElems-1,domain,domElems)
          domain->m_lxim[i+1] = i ;
          domain->m_lxip[i  ] = i+1 ;
    END_PAR_FOR(i)
    domain->m_lxip[domElems-1] = domElems-1 ;
  END_FINISH

  FINISH
    PAR_FOR_0xNx1(i,edgeElems,domain,domElems,edgeElems)
      domain->m_letam[i] = i ; 
      domain->m_letap[domElems-edgeElems+i] = domElems-edgeElems+i ;
    END_PAR_FOR(i)
  END_FINISH

  FINISH
    PAR_FOR_0xNx1(i,(domElems-edgeElems),domain,edgeElems)
      domain->m_letam[i+edgeElems] = i ;
      domain->m_letap[i          ] = i+edgeElems ;
    END_PAR_FOR(i)
  END_FINISH

  FINISH
    PAR_FOR_0xNx1(i,(edgeElems*edgeElems),domain,domElems,edgeElems)
      domain->m_lzetam[i] = i ;
      domain->m_lzetap[domElems-edgeElems*edgeElems+i] = domElems-edgeElems*edgeElems+i ;
    END_PAR_FOR(i)
  END_FINISH

  FINISH
    Index_t dimE = edgeElems, dimEdimE = dimE*dimE;
    PAR_FOR_0xNx1(i,(domElems-dimEdimE),domain,dimEdimE,domElems)
      domain->m_lzetam[i+dimEdimE] = i ;
      domain->m_lzetap[i]          = i+dimEdimE ;
    END_PAR_FOR(i)
  END_FINISH

TRACE0("/* set up boundary condition information */");
//DEBUG fprintf(stdout,"e(0)=%e\n",domain->m_e[0]);

  FINISH
    PAR_FOR_0xNx1(i,domElems,domain,domElems)
      domain->m_elemBC[i] = 0 ;  /* clear BCs by default */
    END_PAR_FOR(i)
  END_FINISH

TRACE0("/* faces on \"external\" boundaries will be */");
TRACE0("/* symmetry plane or free surface BCs     */");
//DEBUG fprintf(stdout,"e(0)=%e\n",domain->m_e[0]);

  FINISH
    Index_t dimE = edgeElems, dimEdimE = dimE*dimE;
    for ( Index_t i = 0 ; i < edgeElems ; ++i ) {
      Index_t planeInc = i*dimEdimE ;
      Index_t rowInc   = i*dimE ;
      PAR_FOR_0xNx1(j,edgeElems,i,domain,planeInc,rowInc,domElems,dimE,dimEdimE)
        domain->m_elemBC[planeInc+j*dimE           ] |= XI_M_SYMM ;
        domain->m_elemBC[planeInc+j*dimE+1*dimE-1  ] |= XI_P_FREE ;
        domain->m_elemBC[planeInc+j                ] |= ETA_M_SYMM ;
        domain->m_elemBC[planeInc+j+dimEdimE-dimE  ] |= ETA_P_FREE ;
        domain->m_elemBC[rowInc+j                  ] |= ZETA_M_SYMM ;
        domain->m_elemBC[rowInc+j+domElems-dimEdimE] |= ZETA_P_FREE ;
      END_PAR_FOR(j)
    } // for i
  END_FINISH

TRACE0("/* TIMESTEP TO SOLUTION */");

  while(domain->m_time < domain->m_stoptime ) {

TRACE0("/* TimeIncrement() */");

    TimeIncrement(domain) ;

TRACE0("/* LagrangeLeapFrog() */");

    LagrangeLeapFrog() ;

#if       LULESH_SHOW_PROGRESS
#ifdef      FSIM
    xe_printf("time = %16.16lx, dt=%16.16lx, e(0)=%16.16lx\n",
          *(uint64_t *)&(domain->m_time),
          *(uint64_t *)&(domain->m_deltatime),
          *(uint64_t *)&(domain->m_e[0])) ;
#else    // NOT FSIM
#ifdef        HEX
    printf("time = %16.16lx, dt=%16.16lx, e(0)=%16.16lx\n",
          *(uint64_t *)&(domain->m_time),
          *(uint64_t *)&(domain->m_deltatime),
          *(uint64_t *)&(domain->m_e[0])) ;
#else      // NOT HEX
    printf("time = %e, dt=%e, e(0)=%e\n",
          ((double)domain->m_time),
          ((double)domain->m_deltatime),
          ((double)domain->m_e[0])) ;
#endif     // HEX
    fflush(stdout);
#endif   // FSIM
#endif // LULESH_SHOW_PROGRESS

  } // while time

#ifdef    FSIM
  xe_printf("   Final Origin Energy = %16.16lx \n", *(SHARED uint64_t *)&domain->m_e[0]) ;
#else //  NOT FSIM
#ifdef      HEX
  printf("   Final Origin Energy = %16.16lx \n", *(SHARED uint64_t *)&domain->m_e[0]) ;
#else    // NOT HEX
  printf("   Final Origin Energy = %12.6e \n", (double)domain->m_e[0]) ;
#endif //   HEX
  fflush(stdout);
#endif // FSIM

#if defined(FSIM) || defined(OCR)
  DOMAIN_DESTROY(&domainObject);
#else
TRACE0("/* Deallocate field memory */");
  domain_DeallocateNodesets() ;
  domain_DeallocateNodalPersistent() ;

  domain_DeallocateElemTemporary () ;
  domain_DeallocateElemPersistent() ;

  DRAM_FREE(domain);
#endif // FSIM or OCR

TRACE0("mainEdt exit");
  EXIT(0);
  return 0; // IMPOSSIBLE
} // main()

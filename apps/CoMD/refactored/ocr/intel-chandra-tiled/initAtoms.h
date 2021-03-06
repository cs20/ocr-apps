/// \file
/// Initialize the atom configuration.

#ifndef __INIT_ATOMS_H
#define __INIT_ATOMS_H

#include "mytype.h"

struct SimFlatSt;
struct LinkCellSt;

/// Atom data
typedef struct AtomsSt
{
   // atom-specific data
   int nLocal;    //!< total number of atoms on this processor
   int nGlobal;   //!< total number of atoms in simulation

   int* gid;      //!< A globally unique id for each atom
   int* iSpecies; //!< the species index of the atom

   real3*  r;     //!< positions
   real3*  p;     //!< momenta of atoms
   real3*  f;     //!< forces
   real_t* U;     //!< potential energy per atom

   ocrDBK_t DBK_gid;
   ocrDBK_t DBK_iSpecies;

   ocrDBK_t DBK_r, DBK_p, DBK_f;
   ocrDBK_t DBK_U;
} Atoms;

typedef struct
{
    real_t temperature;
    int nGlobal;
} adjustTemperatureEdtParamv_t;

typedef struct
{
    real_t delta;
} randomDisplacementsEdtParamv_t;

/// Allocates memory to store atom data.
void initAtoms(Atoms* atoms, struct LinkCellSt* boxes);
void destroyAtoms(struct AtomsSt* atoms);

void createFccLattice(int nx, int ny, int nz, real_t lat, struct SimFlatSt* s);

void setMomentumAndComputeVcm(struct SimFlatSt* s, real_t temperature);

ocrGuid_t adjustTemperatureEdt( EDT_ARGS );
ocrGuid_t randomDisplacementsEdt( EDT_ARGS );

ocrGuid_t adjustVcmAndComputeKeEdt( EDT_ARGS );
#endif

#include "comd.h"

#include "ljForce.h"

#include <stdlib.h>
//#include <assert.h>
#include "string.h"
#include <math.h>

#include "constants.h"
#include "mytype.h"
#include "parallel.h"
#include "linkCells.h"
#include "memUtils.h"
#include "CoMDTypes.h"

#include "force.h"

/*
#define POT_SHIFT 1.0

int force(int i, int iter, struct box *b);
int KinEnergy(SimFlat* s, int i, int iter, struct box *b);
void getTuple1(struct box *b, int iBox, int* ixp, int* iyp, int* izp);
int getBoxFromTuple1(struct box *b, int ix, int iy, int iz);
int getNeighborBoxes1(struct box *b, int iBox, int* nbrBoxes);

*/

void comd_forceStep (cncTag_t i, cncTag_t iter, BItem b1, comdCtx *ctx) {

    struct box *b = b1;
    if (i == 0)
    printf("CnC: 0 forceStep %lu, %lu\n", i, iter);

    int nbrBoxes[27];

    int nNbrBoxes = getNeighborBoxes1(b, i, nbrBoxes);

    for (int iOff=0; iOff< b->nAtoms; iOff++) {
        zeroReal3(b->atoms.f[iOff]);
        b->atoms.U[iOff] = 0.;
    }

    b->ePot = 0.0;
    b->eKin = 0.0;

//    printf("CnC: 1 forceStep %lu, %lu\n", i, iter);

    cncPrescribe_computeForcefromNeighborsStep(i, nbrBoxes[0], nbrBoxes[1],nbrBoxes[2],nbrBoxes[3],nbrBoxes[4],nbrBoxes[5],nbrBoxes[6],nbrBoxes[7],nbrBoxes[8],nbrBoxes[9],nbrBoxes[10],nbrBoxes[11],nbrBoxes[12],nbrBoxes[14],nbrBoxes[15],nbrBoxes[16],nbrBoxes[17],nbrBoxes[18],nbrBoxes[19],nbrBoxes[20],nbrBoxes[21],nbrBoxes[22],nbrBoxes[23],nbrBoxes[24],nbrBoxes[25],nbrBoxes[26], iter, ctx);

if (ctx->doeam)
      cncPrescribe_computeForcefromNeighborsStep1(i, nbrBoxes[0], nbrBoxes[1],nbrBoxes[2],nbrBoxes[3],nbrBoxes[4],nbrBoxes[5],nbrBoxes[6],nbrBoxes[7],nbrBoxes[8],nbrBoxes[9],nbrBoxes[10],nbrBoxes[11],nbrBoxes[12],nbrBoxes[14],nbrBoxes[15],nbrBoxes[16],nbrBoxes[17],nbrBoxes[18],nbrBoxes[19],nbrBoxes[20],nbrBoxes[21],nbrBoxes[22],nbrBoxes[23],nbrBoxes[24],nbrBoxes[25],nbrBoxes[26], iter, ctx);


//    printf("CnC: 2 forceStep %lu, %lu\n", i, iter);
}

void getTuple1(struct box *b, int iBox, int* ixp, int* iyp, int* izp) {
   int ix, iy, iz;
   const int* gridSize = b->gridSize; // alias

   // If a local box -- Manu: we can do away with this condition as there are no halo boxes
   if( iBox < b->nLocalBoxes)
   {
      ix = iBox % gridSize[0];
      iBox /= gridSize[0];
      iy = iBox % gridSize[1];
      iz = iBox / gridSize[1];
   }
   *ixp = ix;
   *iyp = iy;
   *izp = iz;
}

int getBoxFromTuple1(struct box *b, int ix, int iy, int iz) {
    int iBox = 0;
    const int* gridSize = b->gridSize; // alias

    iBox = ix + gridSize[0] * iy + gridSize[0] * gridSize[1] * iz;
    assert(iBox >= 0);
    if (iBox > b->nLocalBoxes) {
      printf("%d, %d, %d:: %d\n", ix, iy, iz, iBox);
      printf("%d, %d, %d\n",gridSize[0], gridSize[1], gridSize[2]);
    }
    assert(iBox < b->nLocalBoxes);

    return iBox;
}



int getNeighborBoxes1(struct box *b, int iBox, int* nbrBoxes) {
   int ix, iy, iz;
   const int* gridSize = b->gridSize;

   getTuple1(b, iBox, &ix, &iy, &iz);

   int count = 0;
   int ii,jj,kk;
   for (int i=ix-1; i<=ix+1; i++)
      for (int j=iy-1; j<=iy+1; j++)
         for (int k=iz-1; k<=iz+1; k++) {  // Manu:: modified this loop to take care of periodic boundaries (without halo cells)
             ii = i; jj = j; kk = k;

             if (i==-1) ii = gridSize[0]-1;
             if (i == gridSize[0]) ii = 0;

             if (j==-1) jj = gridSize[1]-1;
             if (j == gridSize[1]) jj = 0;

             if (k==-1) kk = gridSize[2]-1;
             if (k == gridSize[2]) kk = 0;

             nbrBoxes[count++] = getBoxFromTuple1(b,ii,jj,kk);
         }

   return count;
}

int getBoxFromCoord(struct box* boxes, real_t rr[3]) {
   const real_t* localMin = boxes->localMin; // alias
   const real_t* localMax = boxes->localMax; // alias
   const int*    gridSize = boxes->gridSize; // alias
   int ix = (int)(floor((rr[0] - localMin[0])*boxes->invBoxSize[0]));
   int iy = (int)(floor((rr[1] - localMin[1])*boxes->invBoxSize[1]));
   int iz = (int)(floor((rr[2] - localMin[2])*boxes->invBoxSize[2]));


   // For each axis, if we are inside the local domain, make sure we get
   // a local link cell.  Otherwise, make sure we get a halo link cell.
   if (rr[0] < localMax[0])
   {
      if (ix == gridSize[0]) ix = gridSize[0] - 1;
   }
   else
      ix = 0; // assign to halo cell
   if (rr[1] < localMax[1])
   {
      if (iy == gridSize[1]) iy = gridSize[1] - 1;
   }
   else
      iy = 0;
   if (rr[2] < localMax[2])
   {
      if (iz == gridSize[2]) iz = gridSize[2] - 1;
   }
   else
      iz = 0;

   return getBoxFromTuple1(boxes, ix, iy, iz);
}

// Manu:: Uncomment the below functions once we have qsort implemented.
//        May be we don't need these functions as there is no impact on output after commenting these out!!
/*
void sortAtomsInCell1(struct box *b) {
   int nAtoms = b->nAtoms;


   AMsg tmp[nAtoms];

   int begin = 0;
   int end = nAtoms;
   for (int ii=begin, iTmp=0; ii<end; ++ii, ++iTmp)
   {
      tmp[iTmp].gid  = b->atoms.gid[ii];
      tmp[iTmp].type = b->atoms.iSpecies[ii];
      tmp[iTmp].rx =   b->atoms.r[ii][0];
      tmp[iTmp].ry =   b->atoms.r[ii][1];
      tmp[iTmp].rz =   b->atoms.r[ii][2];
      tmp[iTmp].px =   b->atoms.p[ii][0];
      tmp[iTmp].py =   b->atoms.p[ii][1];
      tmp[iTmp].pz =   b->atoms.p[ii][2];
   }
   qsort(&tmp, nAtoms, sizeof(AMsg), sortAtomsById1);
   for (int ii=begin, iTmp=0; ii<end; ++ii, ++iTmp)
   {
      b->atoms.gid[ii]   = tmp[iTmp].gid;
      b->atoms.iSpecies[ii] = tmp[iTmp].type;
      b->atoms.r[ii][0]  = tmp[iTmp].rx;
      b->atoms.r[ii][1]  = tmp[iTmp].ry;
      b->atoms.r[ii][2]  = tmp[iTmp].rz;
      b->atoms.p[ii][0]  = tmp[iTmp].px;
      b->atoms.p[ii][1]  = tmp[iTmp].py;
      b->atoms.p[ii][2]  = tmp[iTmp].pz;
   }

}

///  A function suitable for passing to qsort to sort atoms by gid.
///  Because every atom in the simulation is supposed to have a unique
///  id, this function checks that the atoms have different gids.  If
///  that assertion ever fails it is a sign that something has gone
///  wrong elsewhere in the code.
int sortAtomsById1(const void* a, const void* b) {
   int aId = ((AMsg*) a)->gid;
   int bId = ((AMsg*) b)->gid;
   assert(aId != bId);

   if (aId < bId)
      return -1;
   return 1;
}
*/

void interpolateNew(InterpolationObjectNew* table, real_t r, real_t* f, real_t* df) {
   const real_t* tt = table->values; // alias

   if ( r < table->x0 ) r = table->x0;

   r = (r-table->x0)*(table->invDx) ;
   int ii = (int)floor(r);
   if (ii > table->n)
   {
      ii = table->n;
      r = table->n / table->invDx;
   }
   // reset r to fractional distance
   r = r - floor(r);

   real_t g1 = tt[ii+1] - tt[ii-1];
   real_t g2 = tt[ii+2] - tt[ii];

   *f = tt[ii] + 0.5*r*(g1 + r*(tt[ii+1] + tt[ii-1] - 2.0*tt[ii]) );

   *df = 0.5*(g1 + r*(g2-g1))*table->invDx;
}


/// \file
/// Compute forces for the Embedded Atom Model (EAM).
///
/// The Embedded Atom Model (EAM) is a widely used model of atomic
/// interactions in simple metals.
///
/// http://en.wikipedia.org/wiki/Embedded_atom_model
///
/// In the EAM, the total potential energy is written as a sum of a pair
/// potential and the embedding energy, F:
///
/// \f[
///   U = \sum_{ij} \varphi(r_{ij}) + \sum_i F({\bar\rho_i})
/// \f]
///
/// The pair potential \f$\varphi_{ij}\f$ is a two-body inter-atomic
/// potential, similar to the Lennard-Jones potential, and
/// \f$F(\bar\rho)\f$ is interpreted as the energy required to embed an
/// atom in an electron field with density \f$\bar\rho\f$.  The local
/// electon density at site i is calulated by summing the "effective
/// electron density" due to all neighbors of atom i:
///
/// \f[
/// \bar\rho_i = \sum_j \rho_j(r_{ij})
/// \f]
///
/// The force on atom i, \f${\bf F}_i\f$ is given by
///
/// \f{eqnarray*}{
///   {\bf F}_i & = & -\nabla_i \sum_{jk} U(r_{jk})\\
///       & = & - \sum_j\left\{
///                  \varphi'(r_{ij}) +
///                  [F'(\bar\rho_i) + F'(\bar\rho_j)]\rho'(r_{ij})
///                \right\} \hat{r}_{ij}
/// \f}
///
/// where primes indicate the derivative of a function with respect to
/// its argument and \f$\hat{r}_{ij}\f$ is a unit vector in the
/// direction from atom i to atom j.
///
/// The form of this force expression has two significant consequences.
/// First, unlike with a simple pair potential, it is not possible to
/// compute the potential energy and the forces on the atoms in a single
/// loop over the pairs.  The terms involving \f$ F'(\bar\rho) \f$
/// cannot be calculated until \f$ \bar\rho \f$ is known, but
/// calculating \f$ \bar\rho \f$ requires a loop over the pairs.  Hence
/// the EAM force routine contains three loops.
///
///   -# Loop over all pairs, compute the two-body
///   interaction and the electron density at each atom
///   -# Loop over all atoms, compute the embedding energy and its
///   derivative for each atom
///   -# Loop over all pairs, compute the embedding
///   energy contribution to the force and add to the two-body force
///
/// The second loop over pairs doubles the data motion requirement
/// relative to a simple pair potential.
///
/// The second consequence of the force expression is that computing the
/// forces on all atoms requires additional communication beyond the
/// coordinates of all remote atoms within the cutoff distance.  This is
/// again because of the terms involving \f$ F'(\bar\rho_j) \f$.  If
/// atom j is a remote atom, the local task cannot compute \f$
/// \bar\rho_j \f$.  (Such a calculation would require all the neighbors
/// of atom j, some of which can be up to 2 times the cutoff distance
/// away from a local atom---outside the typical halo exchange range.)
///
/// To obtain the needed remote density we introduce a second halo
/// exchange after loop number 2 to communicate \f$ F'(\bar\rho) \f$ for
/// remote atoms.  This provides the data we need to complete the third
/// loop, but at the cost of introducing a communication operation in
/// the middle of the force routine.
///
/// At least two alternate methods can be used to deal with the remote
/// density problem.  One possibility is to extend the halo exchange
/// radius for the atom exchange to twice the potential cutoff distance.
/// This is likely undesirable due to large increase in communication
/// volume.  The other possibility is to accumulate partial force terms
/// on the tasks where they can be computed.  In this method, tasks will
/// compute force contributions for remote atoms, then communicate the
/// partial forces at the end of the halo exchange.  This method has the
/// advantage that the communication is deffered until after the force
/// loops, but the disadvantage that three times as much data needs to
/// be set (three components of the force vector instead of a single
/// scalar \f$ F'(\bar\rho) \f$.


#include "eam.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "constants.h"
#include "linkCells.h"
#include "CoMDTypes.h"
#include "performanceTimers.h"
#include "haloExchange.h"

#define MAX(A,B) ((A) > (B) ? (A) : (B))


// EAM functionality
static int eamForce(SimFlat* s);
ocrGuid_t eamForce_edt( EDT_ARGS );
ocrGuid_t eamForce1_edt( EDT_ARGS );
ocrGuid_t eamForce2_edt( EDT_ARGS );

static void eamPrint(BasePotential* pot);
static void eamDestroy(BasePotential** pot);
static void eamBcastPotential(EamPotential* pot);


// Table interpolation functionality
static void initInterpolationObject( InterpolationObject* table,
   int n, real_t x0, real_t dx, real_t* data);
static void destroyInterpolationObject(InterpolationObject** table);
static void interpolate(InterpolationObject* table, real_t r, real_t* f, real_t* df);
static void bcastInterpolationObject(InterpolationObject** table);


// Read potential tables from files.
static void eamReadSetfl(EamPotential* pot, const char* dir, const char* potName);
static void eamReadFuncfl(EamPotential* pot, const char* dir, const char* potName);
static void fileNotFound(const char* callSite, const char* filename);
static void notAlloyReady(const char* callSite);
static void typeNotSupported(const char* callSite, const char* type);


/// Allocate and initialize the EAM potential data structure.
///
/// \param [in] dir   The directory in which potential table files are found.
/// \param [in] file  The name of the potential table file.
/// \param [in] type  The file format of the potential file (setfl or funcfl).
ocrDBK_t initEamPot(BasePotential** bpot, const char* dir, const char* file, const char* type)
{
   DEBUG_PRINTF(("%s\n", __func__));
   ocrDBK_t DBK_pot;
   ocrDbCreate( &DBK_pot, (void**) bpot, sizeof(EamPotential), 0, NULL_HINT, NO_ALLOC );
   EamPotential* pot = (EamPotential*)(*bpot);

   pot->force_edt = eamForce_edt;
   ocrEdtTemplateCreate( &pot->forceTML,   eamForce_edt, 1, 3 );
   ocrEdtTemplateCreate( &pot->force1TML, eamForce1_edt, 1, 14 );
   ocrEdtTemplateCreate( &pot->force2TML, eamForce2_edt, 1, 11 );
   pot->print = eamPrint;
   pot->destroy = eamDestroy;

   pot->phi = &pot->phi_INST;
   pot->rho = &pot->rho_INST;
   pot->f   = &pot->f_INST;

   // Initialization of the next three items requires information about
   // the parallel decomposition and link cells that isn't available
   // with the potential is initialized.  Hence, we defer their
   // initialization until the first time we call the force routine.
   pot->dfEmbed = NULL;
   pot->rhobar  = NULL;
   pot->forceExchange = NULL;

   pot->DBK_dfEmbed = NULL_GUID;
   pot->DBK_rhobar = NULL_GUID;

   //if (getMyRank() == 0)
   {
      if (strcmp(type,"funcfl") == 0)
         eamReadFuncfl(pot, dir, file); //Supported now
      //else if (strcmp(type, "setfl" ) == 0)
      //   eamReadSetfl(pot, dir, file); //Not supported
      else
         typeNotSupported("initEamPot", type);
   }

   //eamBcastPotential(pot);

   return DBK_pot;
}

/// Calculate potential energy and forces for the EAM potential.
///
/// Three steps are required:
///
///   -# Loop over all atoms and their neighbors, compute the two-body
///   interaction and the electron density at each atom
///   -# Loop over all atoms, compute the embedding energy and its
///   derivative for each atom
///   -# Loop over all atoms and their neighbors, compute the embedding
///   energy contribution to the force and add to the two-body force
///
int eamForce1(SimFlat* s, u64 itimestep)
{
   EamPotential* pot = (EamPotential*) s->pot;
   ocrAssert(pot);

   real_t rCut2 = pot->cutoff*pot->cutoff;

   // zero forces / energy / rho /rhoprime
   real_t etot = 0.0;
   memset(s->atoms->f,  0, s->boxes->nTotalBoxes*MAXATOMS*sizeof(real3));
   memset(s->atoms->U,  0, s->boxes->nTotalBoxes*MAXATOMS*sizeof(real_t));
   memset(pot->dfEmbed, 0, s->boxes->nTotalBoxes*MAXATOMS*sizeof(real_t));
   memset(pot->rhobar,  0, s->boxes->nTotalBoxes*MAXATOMS*sizeof(real_t));

   int nbrBoxes[27];
   // loop over local boxes
   for (int iBox=0; iBox<s->boxes->nLocalBoxes; iBox++)
   {
      int nIBox = s->boxes->nAtoms[iBox];
      int nNbrBoxes = getNeighborBoxes(s->boxes, iBox, nbrBoxes);
      // loop over neighbor boxes of iBox (some may be halo boxes)
      for (int jTmp=0; jTmp<nNbrBoxes; jTmp++)
      {
         int jBox = nbrBoxes[jTmp];
         if (jBox < iBox ) continue;

         int nJBox = s->boxes->nAtoms[jBox];
         // loop over atoms in iBox
         for (int iOff=MAXATOMS*iBox,ii=0; ii<nIBox; ii++,iOff++)
         {
            // loop over atoms in jBox
            for (int jOff=MAXATOMS*jBox,ij=0; ij<nJBox; ij++,jOff++)
            {
               if ( (iBox==jBox) &&(ij <= ii) ) continue;

               double r2 = 0.0;
               real3 dr;
               for (int k=0; k<3; k++)
               {
                  dr[k]=s->atoms->r[iOff][k]-s->atoms->r[jOff][k];
                  r2+=dr[k]*dr[k];
               }
               if(r2>rCut2) continue;

               double r = sqrt(r2);

               real_t phiTmp, dPhi, rhoTmp, dRho;
               interpolate(pot->phi, r, &phiTmp, &dPhi);
               interpolate(pot->rho, r, &rhoTmp, &dRho);

               for (int k=0; k<3; k++)
               {
                  s->atoms->f[iOff][k] -= dPhi*dr[k]/r;
                  s->atoms->f[jOff][k] += dPhi*dr[k]/r;
               }

               // update energy terms
               // calculate energy contribution based on whether
               // the neighbor box is local or remote
               if (jBox < s->boxes->nLocalBoxes)
                  etot += phiTmp;
               else
                  etot += 0.5*phiTmp;

               s->atoms->U[iOff] += 0.5*phiTmp;
               s->atoms->U[jOff] += 0.5*phiTmp;

               // accumulate rhobar for each atom
               pot->rhobar[iOff] += rhoTmp;
               pot->rhobar[jOff] += rhoTmp;

            } // loop over atoms in jBox
         } // loop over atoms in iBox
      } // loop over neighbor boxes
   } // loop over local boxes

   // Compute Embedding Energy
   // loop over all local boxes
   for (int iBox=0; iBox<s->boxes->nLocalBoxes; iBox++)
   {
      int iOff;
      int nIBox =  s->boxes->nAtoms[iBox];

      // loop over atoms in iBox
      for (int iOff=MAXATOMS*iBox,ii=0; ii<nIBox; ii++,iOff++)
      {
         real_t fEmbed, dfEmbed;
         interpolate(pot->f, pot->rhobar[iOff], &fEmbed, &dfEmbed);
         pot->dfEmbed[iOff] = dfEmbed; // save derivative for halo exchange
         etot += fEmbed;
         s->atoms->U[iOff] += fEmbed;
      }
   }

   s->ePotential = (real_t) etot;

   return 0;
}

int eamForce2(SimFlat* s)
{
   EamPotential* pot = (EamPotential*) s->pot;
   ocrAssert(pot);

   real_t rCut2 = pot->cutoff*pot->cutoff;

   int nbrBoxes[27];
   // third pass
   // loop over local boxes
   for (int iBox=0; iBox<s->boxes->nLocalBoxes; iBox++)
   {
      int nIBox =  s->boxes->nAtoms[iBox];
      int nNbrBoxes = getNeighborBoxes(s->boxes, iBox, nbrBoxes);
      // loop over neighbor boxes of iBox (some may be halo boxes)
      for (int jTmp=0; jTmp<nNbrBoxes; jTmp++)
      {
         int jBox = nbrBoxes[jTmp];
         if(jBox < iBox) continue;

         int nJBox = s->boxes->nAtoms[jBox];
         // loop over atoms in iBox
         for (int iOff=MAXATOMS*iBox,ii=0; ii<nIBox; ii++,iOff++)
         {
            // loop over atoms in jBox
            for (int jOff=MAXATOMS*jBox,ij=0; ij<nJBox; ij++,jOff++)
            {
               if ((iBox==jBox) && (ij <= ii))  continue;

               double r2 = 0.0;
               real3 dr;
               for (int k=0; k<3; k++)
               {
                  dr[k]=s->atoms->r[iOff][k]-s->atoms->r[jOff][k];
                  r2+=dr[k]*dr[k];
               }
               if(r2>=rCut2) continue;

               real_t r = sqrt(r2);

               real_t rhoTmp, dRho;
               interpolate(pot->rho, r, &rhoTmp, &dRho);

               for (int k=0; k<3; k++)
               {
                  s->atoms->f[iOff][k] -= (pot->dfEmbed[iOff]+pot->dfEmbed[jOff])*dRho*dr[k]/r;
                  s->atoms->f[jOff][k] += (pot->dfEmbed[iOff]+pot->dfEmbed[jOff])*dRho*dr[k]/r;
               }

            } // loop over atoms in jBox
         } // loop over atoms in iBox
      } // loop over neighbor boxes
   } // loop over local boxes

   return 0;
}

void eamPrint(BasePotential* pot)
{
   EamPotential *eamPot = (EamPotential*) pot;
   ocrPrintf( "  Potential type  : EAM\n");
   ocrPrintf( "  Species name    : %s\n", eamPot->name);
   ocrPrintf( "  Atomic number   : %d\n", eamPot->atomicNo);
   ocrPrintf( "  Mass            : "FMT1" amu\n", eamPot->mass/amuToInternalMass); // print in amu
   ocrPrintf( "  Lattice type    : %s\n", eamPot->latticeType);
   ocrPrintf( "  Lattice spacing : "FMT1" Angstroms\n", eamPot->lat);
   ocrPrintf( "  Cutoff          : "FMT1" Angstroms\n", eamPot->cutoff);
}

void eamDestroy(BasePotential** pPot)
{
   if ( ! pPot ) return;
   EamPotential* epot = *(EamPotential**)pPot;
   if ( ! epot ) return;

    epot->phi = &epot->phi_INST;
    epot->rho = &epot->rho_INST;
    epot->f   = &epot->f_INST;
   destroyInterpolationObject(&(epot->phi));
   destroyInterpolationObject(&(epot->rho));
   destroyInterpolationObject(&(epot->f));

   //destroyHaloExchange(&(pot->forceExchange));
   return;
}

/// Builds a structure to store interpolation data for a tabular
/// function.  Interpolation must be supported on the range
/// \f$[x_0, x_n]\f$, where \f$x_n = n*dx\f$.
///
/// \see interpolate
/// \see bcastInterpolationObject
/// \see destroyInterpolationObject
///
/// \param [in] n    number of values in the table.
/// \param [in] x0   minimum ordinate value of the table.
/// \param [in] dx   spacing of the ordinate values.
/// \param [in] data abscissa values.  An array of size n.
void initInterpolationObject( InterpolationObject* table,
   int n, real_t x0, real_t dx, real_t* data)
{
   ocrAssert(table);

   ocrDbCreate( &table->DBK_values, (void**) &table->values, (n+3)*sizeof(real_t), 0, NULL_HINT, NO_ALLOC );
   ocrAssert(table->values);

   table->values++;
   table->n = n;
   table->invDx = 1.0/dx;
   table->x0 = x0;

   for (int ii=0; ii<n; ++ii)
      table->values[ii] = data[ii];

   table->values[-1] = table->values[0];
   table->values[n+1] = table->values[n] = table->values[n-1];

   ocrDbRelease( table->DBK_values );
}

void destroyInterpolationObject(InterpolationObject** a)
{
   ocrDbDestroy((*a)->DBK_values);
   *a = NULL;

   return;
}

/// Interpolate a table to determine f(r) and its derivative f'(r).
///
/// The forces on the particle are much more sensitive to the derivative
/// of the potential than on the potential itself.  It is therefore
/// absolutely essential that the interpolated derivatives are smooth
/// and continuous.  This function uses simple quadratic interpolation
/// to find f(r).  Since quadric interpolants don't have smooth
/// derivatives, f'(r) is computed using a 4 point finite difference
/// stencil.
///
/// Interpolation is used heavily by the EAM force routine so this
/// function is a potential performance hot spot.  Feel free to
/// reimplement this function (and initInterpolationObject if necessay)
/// with any higher performing implementation of interpolation, as long
/// as the alternate implmentation that has the required smoothness
/// properties.  Cubic splines are one common alternate choice.
///
/// \param [in] table Interpolation table.
/// \param [in] r Point where function value is needed.
/// \param [out] f The interpolated value of f(r).
/// \param [out] df The interpolated value of df(r)/dr.
void interpolate(InterpolationObject* table, real_t r, real_t* f, real_t* df)
{
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

/// Reads potential data from a funcfl file and populates
/// corresponding members and InterpolationObjects in an EamPotential.
///
/// funcfl is a file format for tabulated potential functions used by
/// the original EAM code DYNAMO.  A funcfl file contains an EAM
/// potential for a single element.
///
/// The contents of a funcfl file are:
///
/// | Line Num | Description
/// | :------: | :----------
/// | 1        | comments
/// | 2        | elem amass latConstant latType
/// | 3        | nrho   drho   nr   dr    rcutoff
/// | 4        | embedding function values F(rhobar) starting at rhobar=0
/// |    ...   | (nrho values. Multiple values per line allowed.)
/// | x'       | electrostatic interation Z(r) starting at r=0
/// |    ...   | (nr values. Multiple values per line allowed.)
/// | y'       | electron density values rho(r) starting at r=0
/// |    ...   | (nr values. Multiple values per line allowed.)
///
/// Where:
///    -  elem          :   atomic number for this element
///    -  amass         :   atomic mass for this element in AMU
///    -  latConstant   :   lattice constant for this elemnent in Angstroms
///    -  lattticeType  :   lattice type for this element (e.g. FCC)
///    -  nrho          :   number of values for the embedding function, F(rhobar)
///    -  drho          :   table spacing for rhobar
///    -  nr            :   number of values for Z(r) and rho(r)
///    -  dr            :   table spacing for r in Angstroms
///    -  rcutoff       :   potential cut-off distance in Angstroms
///
/// funcfl format stores the "electrostatic interation" Z(r).  This needs to
/// be converted to the pair potential phi(r).
/// using the formula
/// \f[phi = Z(r) * Z(r) / r\f]
/// NB: phi is not defined for r = 0
///
/// Z(r) is in atomic units (i.e., sqrt[Hartree * bohr]) so it is
/// necesary to convert to eV.
///
/// F(rhobar) is in eV.
///
void eamReadFuncfl(EamPotential* pot, const char* dir, const char* potName)
{
  //HARD-CODE THIS FUNCTION
   char tmp[4096];

   // line 1
   char name[3];
   sscanf("Cu functions (universal 4) - JB Adams et al J. Mater. Res., 4(1), 102 (1989)", "%s", name);
   strcpy(pot->name, name);

   // line 2
   int nAtomic;
   double mass, lat;
   char latticeType[8];
   sscanf("   29     63.550         3.6150    FCC", "%d %le %le %s", &nAtomic, &mass, &lat, latticeType);
   pot->atomicNo = nAtomic;
   pot->lat = lat;
   pot->mass = mass*amuToInternalMass; // file has mass in AMU.
   strcpy(pot->latticeType, latticeType);

   // line 3
   int nRho, nR;
   double dRho, dR, cutoff;
   sscanf("  500  5.0100200400801306e-04  500  1.0000000000000009e-02  4.9499999999999886e+00", "%d %le %d %le %le", &nRho, &dRho, &nR, &dR, &cutoff);
   pot->cutoff = cutoff;
   real_t x0 = 0.0; // tables start at zero.

   // allocate read buffer
   int bufSize = MAX(nRho, nR);

   ocrDBK_t DBK_buf;
   real_t* buf;
   ocrDbCreate( &DBK_buf, (void**) &buf, bufSize * sizeof(real_t), 0, NULL_HINT, NO_ALLOC );
   real_t* bufOrig = buf;

   // read embedding energy
                sscanf("  0.                     -3.1589719908208558e-01 -5.2405175291223927e-01 -6.9885553834123115e-01 -8.5420409172727574e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4]);
        buf+=5; sscanf(" -9.9627285782417374e-01 -1.1284756487892835e+00 -1.2529454148045645e+00 -1.3711252149943363e+00 -1.4840478277127076e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.5924840805403662e+00 -1.6954285804552853e+00 -1.7942937845174001e+00 -1.8901318213864968e+00 -1.9832501645057476e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.0739063371790252e+00 -2.1623187777983759e+00 -2.2486748239473968e+00 -2.3331367007241965e+00 -2.4158460932269890e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.4969276936450484e+00 -2.5764919917168783e+00 -2.6546374977553171e+00 -2.7314525337618534e+00 -2.8070166913917660e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.8814020298474361e+00 -2.9546740684873072e+00 -3.0268926157701515e+00 -3.0981124665488835e+00 -3.1683839923973949e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -3.2377536446699082e+00 -3.3062643854300688e+00 -3.3739560586949437e+00 -3.4408657118035535e+00 -3.5070278749074930e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -3.5724748051301987e+00 -3.6372367006631805e+00 -3.7013418893374563e+00 -3.7648169952241943e+00 -3.8276870863304424e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -3.8899758061050136e+00 -3.9517054906525857e+00 -4.0128972737054482e+00 -4.0735711808432313e+00 -4.1324020819066334e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -4.1903921077370399e+00 -4.2479051536742531e+00 -4.3049572584920952e+00 -4.3615637243846948e+00 -4.4177391662932166e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -4.4734975570518145e+00 -4.5288522686579995e+00 -4.5838161101054880e+00 -4.6384013621277234e+00 -4.6926198091528875e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -4.7464827687459206e+00 -4.8000011187718314e+00 -4.8531853225280486e+00 -4.9060454519053280e+00 -4.9585912090057604e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -5.0108319460781274e+00 -5.0627766840981678e+00 -5.1144341300432075e+00 -5.1658126929883963e+00 -5.2169204991179470e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -5.2677654057399081e+00 -5.3183550143895957e+00 -5.3686966830900360e+00 -5.4187975378393958e+00 -5.4686644833797118e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -5.5183042133078573e+00 -5.5677232195759814e+00 -5.6169278014231168e+00 -5.6659240737851633e+00 -5.7147179752168427e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -5.7633152753617765e+00 -5.8117215820040258e+00 -5.8599423477251662e+00 -5.9079828761981332e+00 -5.9558483281427925e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -6.0035437269616239e+00 -6.0510739641062798e+00 -6.0984438040355258e+00 -6.1456578891786364e+00 -6.1927207443901580e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -6.2396367812953599e+00 -6.2864103024253382e+00 -6.3330455051271599e+00 -6.3795464852936732e+00 -6.4259172409138614e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -6.4721616754587501e+00 -6.5182836011053098e+00 -6.5642867418169999e+00 -6.6101747362826870e+00 -6.6559511407215268e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -6.7016194315664848e+00 -6.7471830080289408e+00 -6.7926321787266488e+00 -6.8376565597087335e+00 -6.8825828851093718e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -6.9274142262937062e+00 -6.9721535889762833e+00 -7.0168039151812138e+00 -7.0613680851242009e+00 -7.1058489190182854e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -7.1502491788078260e+00 -7.1945715698338404e+00 -7.2388187424386103e+00 -7.2829932935017325e+00 -7.3270977679243288e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -7.3711346600521210e+00 -7.4151064150498769e+00 -7.4590154302223652e+00 -7.5028640562861142e+00 -7.5466545985988489e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -7.5903893183397599e+00 -7.6340704336532212e+00 -7.6777001207475166e+00 -7.7212805149568453e+00 -7.7648137117691078e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -7.8083017678126225e+00 -7.8517467018166371e+00 -7.8951504955327323e+00 -7.9385150946301337e+00 -7.9818424095582259e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -8.0251343163853335e+00 -8.0683926576019758e+00 -8.1116192429086027e+00 -8.1548158499697934e+00 -8.1979842251487867e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -8.2411260842174556e+00 -8.2842431130446244e+00 -8.3273369682627845e+00 -8.3704092779143480e+00 -8.4134616420805628e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -8.4564956334858721e+00 -8.4995127980902225e+00 -8.5425146556591471e+00 -8.5855027003218538e+00 -8.6284784011075430e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -8.6714432024708685e+00 -8.7143985247980709e+00 -8.7573457649043576e+00 -8.8002862965079203e+00 -8.8432214707042931e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -8.8861526164113798e+00 -8.9290810408153902e+00 -8.9720080297988716e+00 -9.0149348483554377e+00 -9.0578627409975070e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -9.1007929321486927e+00 -9.1437266265307358e+00 -9.1866650095359432e+00 -9.2296092475897353e+00 -9.2725604885091570e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -9.3155198618426311e+00 -9.3584884792123262e+00 -9.4014674346366292e+00 -9.4444578048540961e+00 -9.4874606496305205e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -9.5304770120671378e+00 -9.5735079188909253e+00 -9.6165543807549625e+00 -9.6596173924971254e+00 -9.7026979334374914e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -9.7457969676344760e+00 -9.7889154441094774e+00 -9.8320542972711564e+00 -9.8749024758928954e+00 -9.9176676449848173e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -9.9604518241702067e+00 -1.0003255855809869e+01 -1.0046080568707225e+01 -1.0088926778514065e+01 -1.0131795287896693e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.0174686886752681e+01 -1.0217602352416293e+01 -1.0260542449857894e+01 -1.0303507931886486e+01 -1.0346499539340471e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.0389518001277565e+01 -1.0432564035159942e+01 -1.0475638347037830e+01 -1.0518741631724708e+01 -1.0561874572971817e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.0605037843637035e+01 -1.0648232105854731e+01 -1.0691458011195323e+01 -1.0734716200826654e+01 -1.0778007305671338e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.0821331946557279e+01 -1.0864690734371720e+01 -1.0908084270204256e+01 -1.0951513145493493e+01 -1.0994977942169044e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.1038479232788461e+01 -1.1082017580672300e+01 -1.1125593540039802e+01 -1.1169207656138724e+01 -1.1212860465371193e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.1256552495422056e+01 -1.1300284265381379e+01 -1.1344056285864497e+01 -1.1387869059131503e+01 -1.1431723079201220e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.1475618831971758e+01 -1.1519556795323240e+01 -1.1563537439236882e+01 -1.1607561225897086e+01 -1.1651628609799957e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.1695740037856638e+01 -1.1739895949496542e+01 -1.1784096776765693e+01 -1.1828342944444898e+01 -1.1872634870038326e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.1916972964140655e+01 -1.1961357630185660e+01 -1.2005789264757027e+01 -1.2050268257623998e+01 -1.2094794991829531e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.2139369843773920e+01 -1.2183993183309894e+01 -1.2228665373815033e+01 -1.2273386772283743e+01 -1.2318157729378811e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.2362978589648264e+01 -1.2407849691329261e+01 -1.2452771366701370e+01 -1.2497743942026659e+01 -1.2542767737650308e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.2587843068072686e+01 -1.2632970242029501e+01 -1.2678149562554552e+01 -1.2723381327055449e+01 -1.2768665827383359e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.2814003349896723e+01 -1.2859394175532202e+01 -1.2904838579871580e+01 -1.2950336833201561e+01 -1.2995889200586475e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.3041495941923358e+01 -1.3087157312007946e+01 -1.3132873560597147e+01 -1.3178644932465090e+01 -1.3224471667467185e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.3270354000597138e+01 -1.3316292162042373e+01 -1.3362286377246335e+01 -1.3408336866957768e+01 -1.3454443847292225e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.3500607529781746e+01 -1.3546828121432384e+01 -1.3593105824775080e+01 -1.3639440837916936e+01 -1.3685833354596070e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.3732283564231011e+01 -1.3778791651965662e+01 -1.3825357798727850e+01 -1.3871982181271051e+01 -1.3918664972226225e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.3965406340150423e+01 -1.4012206449566122e+01 -1.4059065461010562e+01 -1.4105983531084178e+01 -1.4152960812497383e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.4199997454109450e+01 -1.4247093600900882e+01 -1.4294249394311976e+01 -1.4341464971842868e+01 -1.4388740467389482e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.4436076011212833e+01 -1.4483471729977452e+01 -1.4530927746798284e+01 -1.4578444181270356e+01 -1.4626021149517328e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.4673658764226502e+01 -1.4721357134688901e+01 -1.4769116366835874e+01 -1.4816936563275590e+01 -1.4864817823335784e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.4912760243093658e+01 -1.4960763915414873e+01 -1.5008828929991182e+01 -1.5056955373373171e+01 -1.5105143329006637e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.5153392877265333e+01 -1.5201704095489163e+01 -1.5250077058012721e+01 -1.5298511836202465e+01 -1.5347008498487980e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.5395567110395177e+01 -1.5444187734579373e+01 -1.5492870430854509e+01 -1.5541615256228738e+01 -1.5590422264928975e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.5639291508440465e+01 -1.5688223035530768e+01 -1.5737216892280117e+01 -1.5786273122116540e+01 -1.5835391765839859e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.5884572861650554e+01 -1.5933816445184789e+01 -1.5983122549535665e+01 -1.6032491205288238e+01 -1.6081922440538847e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.6131416280932740e+01 -1.6180972749683292e+01 -1.6230591867602584e+01 -1.6280273653129598e+01 -1.6330018122352271e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.6379825289037512e+01 -1.6429695164657005e+01 -1.6479627758410516e+01 -1.6529623077253063e+01 -1.6579681125920047e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.6629801906948160e+01 -1.6679985420709272e+01 -1.6730231665424185e+01 -1.6780540637196850e+01 -1.6830912330026536e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.6881346735842499e+01 -1.6931843844523655e+01 -1.6982403643917451e+01 -1.7033026119869078e+01 -1.7083711256241486e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.7134459034938232e+01 -1.7185269435924170e+01 -1.7236142437252170e+01 -1.7287078015076759e+01 -1.7338076143685498e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.7389136795514560e+01 -1.7440259941169757e+01 -1.7491445549452351e+01 -1.7542693587371559e+01 -1.7594004020176158e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.7645376811364258e+01 -1.7696811922712527e+01 -1.7748309314288690e+01 -1.7799868944476657e+01 -1.7851490769996190e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.7903174745917113e+01 -1.7954920825684781e+01 -1.8006728961136105e+01 -1.8058599102520361e+01 -1.8110531198513968e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.8162525196246406e+01 -1.8214581041310112e+01 -1.8266698677789350e+01 -1.8318878048276588e+01 -1.8371119093861239e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.8423421754189917e+01 -1.8475785967356501e+01 -1.8528211670354381e+01 -1.8580698798442995e+01 -1.8633247285599964e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.8685857064432412e+01 -1.8738528066186518e+01 -1.8791260220768891e+01 -1.8844053456762026e+01 -1.8896907701446821e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.8949822880805868e+01 -1.9002798919552447e+01 -1.9055835741138708e+01 -1.9108933267775342e+01 -1.9162091420446473e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.9215310118921138e+01 -1.9268589281777849e+01 -1.9321928826411295e+01 -1.9375328669053715e+01 -1.9428788724780020e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.9482308907539277e+01 -1.9535889130155283e+01 -1.9589529304345319e+01 -1.9643229340734365e+01 -1.9696989148876355e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -1.9750808637254977e+01 -1.9804687713312433e+01 -1.9858626283450008e+01 -1.9912624253055810e+01 -1.9966681526506250e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.0020798007185476e+01 -2.0074973597498911e+01 -2.0129208198888136e+01 -2.0183501711838630e+01 -2.0237854035899204e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.0292265069692348e+01 -2.0346734710925716e+01 -2.0401262856409858e+01 -2.0455849402064473e+01 -2.0510494242935920e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.0565197273209719e+01 -2.0619958386219423e+01 -2.0674777474463212e+01 -2.0729654429611969e+01 -2.0784589142526556e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.0839581503263844e+01 -2.0894631401093307e+01 -2.0949738724508393e+01 -2.1004903361235051e+01 -2.1060125198247988e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.1115404121775100e+01 -2.1170740017318053e+01 -2.1226132769657397e+01 -2.1281582262894972e+01 -2.1337088380382852e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.1392651004661843e+01 -2.1448270018015251e+01 -2.1503945301662043e+01 -2.1559676736299366e+01 -2.1615464201984196e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.1671307578140954e+01 -2.1727206743568217e+01 -2.1783161576455427e+01 -2.1839171954393919e+01 -2.1895237754379082e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.1951358852829799e+01 -2.2007535125591289e+01 -2.2063766447950343e+01 -2.2120052694642595e+01 -2.2176393739860259e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.2232789457272474e+01 -2.2289239719961643e+01 -2.2345744400729018e+01 -2.2402303371517178e+01 -2.2458916504038370e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.2515583669430725e+01 -2.2572304738325670e+01 -2.2629079581086330e+01 -2.2685908067306855e+01 -2.2742790066346515e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.2799725447076526e+01 -2.2856714077931429e+01 -2.2913755826927172e+01 -2.2970850561669295e+01 -2.3027998149359064e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.3085198456800299e+01 -2.3142451350411534e+01 -2.3199756696229770e+01 -2.3257114359926845e+01 -2.3314524206806482e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.3371986101824632e+01 -2.3429499909581864e+01 -2.3487065494349963e+01 -2.3544682720213359e+01 -2.3602351450489550e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.3660071548650194e+01 -2.3717842877714475e+01 -2.3775665300345281e+01 -2.3833538678952209e+01 -2.3891462875653588e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.3949437752298763e+01 -2.4007463170460369e+01 -2.4065538991453877e+01 -2.4123665076338057e+01 -2.4181841285923610e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.4240067480782272e+01 -2.4298343521253173e+01 -2.4356669267446705e+01 -2.4415044579252253e+01 -2.4473469316351384e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.4531943338216706e+01 -2.4590466504118467e+01 -2.4649038673143195e+01 -2.4707659704179378e+01 -2.4766329455946106e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.4825047786983760e+01 -2.4883814555664912e+01 -2.4942629620207981e+01 -2.5001492838670174e+01 -2.5060404068966136e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.5119363168864538e+01 -2.5178369996001948e+01 -2.5237424407882145e+01 -2.5296526261886129e+01 -2.5355675415276437e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.5414871725205558e+01 -2.5474115048716612e+01 -2.5533405242759045e+01 -2.5592742164177480e+01 -2.5652125669732186e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.5711555616102487e+01 -2.5771031859886762e+01 -2.5830554257610174e+01 -2.5890122665731269e+01 -2.5949736940650155e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.6009396938703958e+01 -2.6069102516181829e+01 -2.6128853529326307e+01 -2.6188649834339685e+01 -2.6248491287389243e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.6308377744605878e+01 -2.6368309062100934e+01 -2.6428285095962565e+01 -2.6488305702088610e+01 -2.6548370736885317e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf(" -2.6608480056235521e+01 -2.6668633516188947e+01 -2.6728830972768947e+01 -2.6789072282060033e+01 -2.6849357300140582e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );

   buf = bufOrig;
   initInterpolationObject(pot->f, nRho, x0, dRho, buf);

   // read Z(r) and convert to phi(r)
                sscanf("  1.0000000000000000e+01  1.0801250630455797e+01  1.0617301586939504e+01  1.0436833263885262e+01  1.0259774441010791e+01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.0086055417347552e+01  9.9156079783837754e+00  9.7483653639170598e+00  9.5842622365983630e+00  9.4232346511569745e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.2652200242883396e+00  9.1101571051919450e+00  8.9579859467472716e+00  8.8086478773091699e+00  8.6620854731154395e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.5182425312888768e+00  8.3770640434238430e+00  8.2384961697429731e+00  8.1024862138128810e+00  7.9689825978078090e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.8379348383064951e+00  7.7092935226140753e+00  7.5830102855955488e+00  7.4590377870116811e+00  7.3373296893445286e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.2178406361032330e+00  7.1005262306008490e+00  6.9853430151894997e+00  6.8722484509459889e+00  6.7612008977976643e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.6521595950793255e+00  6.5450846425111706e+00  6.4399369815891703e+00  6.3366783773799114e+00  6.2352714007088537e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.1356794107365431e+00  6.0378665379119241e+00  5.9417976672963562e+00  5.8474384222482740e+00  5.7547551484639143e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.6637148983634233e+00  5.5742854158160355e+00  5.4864351211977294e+00  5.4001330967728620e+00  5.3153490723937580e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.2320534115112594e+00  5.1502170974889339e+00  5.0698117202151138e+00  4.9908094630057178e+00  4.9131830897922271e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.8369059325884791e+00  4.7619518792290876e+00  4.6882953613761629e+00  4.6159113427851821e+00  4.5447753078280471e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.4748632502644341e+00  4.4061516622586225e+00  4.3386175236344116e+00  4.2722382913651700e+00  4.2069918892916007e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.1428566980642358e+00  4.0798115453044659e+00  4.0178356959802954e+00  3.9569088429914245e+00  3.8970110979596200e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.8381229822198435e+00  3.7802254180071628e+00  3.7232997198366320e+00  3.6673275860703995e+00  3.6122910906684780e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.5581726751199767e+00  3.5049551405497539e+00  3.4526216399971048e+00  3.4011556708634600e+00  3.3505410675235936e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.3007619940993322e+00  3.2518029373895416e+00  3.2036486999552665e+00  3.1562843933552358e+00  3.1096954315288059e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.0638675243237259e+00  3.0187866711643210e+00  2.9744391548584872e+00  2.9308115355394193e+00  2.8878906447394712e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.8456635795935767e+00  2.8041176971686212e+00  2.7632406089169876e+00  2.7230201752508236e+00  2.6834445002347138e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.6445019263941703e+00  2.6061810296373835e+00  2.5684706142875200e+00  2.5313597082236612e+00  2.4948375581276281e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.4588936248343458e+00  2.4235175787838017e+00  2.3886992955719677e+00  2.3544288515990814e+00  2.3206965198123726e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.2874927655419270e+00  2.2548082424273730e+00  2.2226337884330718e+00  2.1909604219504502e+00  2.1597793379851566e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.1290819044273803e+00  2.0988596584032564e+00  2.0691043027062364e+00  2.0398077023055379e+00  2.0109618809312195e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.9825590177335428e+00  1.9545914440145751e+00  1.9270516400317490e+00  1.8999322318704799e+00  1.8732259883849522e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.8469258182056052e+00  1.8210247668116040e+00  1.7955160136671395e+00  1.7703928694199718e+00  1.7456487731607453e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.7212772897420763e+00  1.6972721071559391e+00  1.6736270339679251e+00  1.6503359968072218e+00  1.6273930379113750e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.6047923127241077e+00  1.5825280875452847e+00  1.5605947372321296e+00  1.5389867429502644e+00  1.5176986899731588e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4967252655299603e+00  1.4760612566992890e+00  1.4557015483494027e+00  1.4356411211222593e+00  1.4158750494618957e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.3963984996850130e+00  1.3772067280935900e+00  1.3582950791282968e+00  1.3396589835618542e+00  1.3212939567312461e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.3031955968085143e+00  1.2853595831085869e+00  1.2677816744339125e+00  1.2504577074545153e+00  1.2333835951233212e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.2165553251254906e+00  1.1999689583611897e+00  1.1836206274610817e+00  1.1675065353339207e+00  1.1516229537450258e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.1359662219258198e+00  1.1205327452130547e+00  1.1053189937170771e+00  1.0903215010191687e+00  1.0755368628964561e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.0609617360743258e+00  1.0465928370056616e+00  1.0324269406761744e+00  1.0184608794353309e+00  1.0046915418523596e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.9111587159659820e-01  9.7773086634202144e-01  9.6453357669496853e-01  9.5152110514480626e-01  9.3869060503712376e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.2603927956864140e-01  9.1356438080370239e-01  9.0126320871155485e-01  8.8913311022427521e-01  8.7717147831462938e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.6537575109353426e-01  8.5374341092675010e-01  8.4227198357020328e-01  8.3095903732382581e-01  8.1980218220310874e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.0879906912829824e-01  7.9794738913080465e-01  7.8724487257618136e-01  7.7668928840378371e-01  7.6627844338220541e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.5601018138057441e-01  7.4588238265517859e-01  7.3589296315095609e-01  7.2603987381787150e-01  7.1632109994155613e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.0673466048788924e-01  6.9727860746149162e-01  6.8795102527759866e-01  6.7875003014695068e-01  6.6967376947370028e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.6072042126578623e-01  6.5188819355765304e-01  6.4317532384495735e-01  6.3458007853101606e-01  6.2610075238490026e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.1773566801053903e-01  6.0948317532703555e-01  6.0134165105970538e-01  5.9330949824153834e-01  5.8538514572508404e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.7756704770434197e-01  5.6985368324655994e-01  5.6224355583360364e-01  5.5473519291279416e-01  5.4732714545698968e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.4001798753364838e-01  5.3280631588273764e-01  5.2569074950327632e-01  5.1866992924830413e-01  5.1174251742814292e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.0490719742172274e-01  4.9816267329578068e-01  4.9150766943189694e-01  4.8494093016099704e-01  4.7846121940521869e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.7206732032718257e-01  4.6575803498626733e-01  4.5953218400168616e-01  4.5338860622255339e-01  4.4732615840449164e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.4134371489266400e-01  4.3544016731128998e-01  4.2961442425926322e-01  4.2386541101190822e-01  4.1819206922867025e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.1259335666658892e-01  4.0706824689955035e-01  4.0161572904300691e-01  3.9623480748423034e-01  3.9092450161784775e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.8568384558664448e-01  3.8051188802739233e-01  3.7540769182181144e-01  3.7037033385227858e-01  3.6539890476236891e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.6049250872219041e-01  3.5565026319808801e-01  3.5087129872705347e-01  3.4615475869537526e-01  3.4149979912160333e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.3690558844382679e-01  3.3237130731094133e-01  3.2789614837797743e-01  3.2347931610542879e-01  3.1912002656234151e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.1481750723327195e-01  3.1057099682888989e-01  3.0637974510021770e-01  3.0224301265637266e-01  2.9816007078585649e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.9413020128113843e-01  2.9015269626666473e-01  2.8622685803004089e-01  2.8235199885637741e-01  2.7852744086590420e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.7475251585446614e-01  2.7102656513704559e-01  2.6734893939429405e-01  2.6371899852185088e-01  2.6013611148246873e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.5659965616091007e-01  2.5310901922152595e-01  2.4966359596849230e-01  2.4626279020852770e-01  2.4290601411627044e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.3959268810202516e-01  2.3632224068197960e-01  2.3309410835077227e-01  2.2990773545640408e-01  2.2676257407740064e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.2365808390219311e-01  2.2059373211072231e-01  2.1756899325815038e-01  2.1458334916067301e-01  2.1163628878335583e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.0872730813005891e-01  2.0585591013519533e-01  2.0302160455757168e-01  2.0022390787598532e-01  1.9746234318676770e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.9473644010305513e-01  1.9204573465593366e-01  1.8938976919722261e-01  1.8676809230404867e-01  1.8418025868501697e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.8162582908807767e-01  1.7910437020998504e-01  1.7661545460728956e-01  1.7415866060892160e-01  1.7173357223026997e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.6933977908869480e-01  1.6697687632057967e-01  1.6464446449973735e-01  1.6234214955720816e-01  1.6006954270248031e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.5782626034600167e-01  1.5561192402300605e-01  1.5342616031865575e-01  1.5126860079441595e-01  1.4913888191569047e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4703664498065105e-01  1.4496153605028095e-01  1.4291320587954814e-01  1.4089130984976528e-01  1.3889550790202332e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.3692546447178433e-01  1.3498084842450275e-01  1.3306133299233025e-01  1.3116659571184286e-01  1.2929631836281086e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.2745018690792786e-01  1.2562789143357200e-01  1.2382912609148811e-01  1.2205358904140517e-01  1.2030098239464682e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.1857101215854371e-01  1.1686338818183373e-01  1.1517782410088362e-01  1.1351403728677267e-01  1.1187174879324324e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.1025068330544618e-01  1.0865056908951143e-01  1.0707113794291789e-01  1.0551212514563968e-01  1.0397326941204543e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.0245431284357487e-01  1.0095500088214582e-01  9.9475082264262937e-02  9.8014308975870268e-02  9.6572436207889467e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.5149222312433945e-02  9.3744428759716225e-02  9.2357820095600562e-02  9.0989163899807046e-02  8.9638230744780500e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.8304794155128263e-02  8.6988630567732095e-02  8.5689519292436067e-02  8.4407242473327759e-02  8.3141585050604316e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.1892334723027815e-02  8.0659281910912206e-02  7.9442219719687568e-02  7.8240943904000826e-02  7.7055252832346710e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.5884947452225848e-02  7.4729831255814450e-02  7.3589710246154016e-02  7.2464392903814900e-02  7.1353690154065674e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.0257415334517681e-02  6.9175384163253639e-02  6.8107414707390124e-02  6.7053327352138758e-02  6.6012944770277748e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.4986091892085707e-02  6.3972595875702698e-02  6.2972286077917161e-02  6.1984994025379159e-02  6.1010553386208866e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.0048799942037157e-02  5.9099571560412123e-02  5.8162708167624810e-02  5.7238051721903105e-02  5.6325446186999528e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.5424737506136967e-02  5.4535773576326108e-02  5.3658404223060785e-02  5.2792481175329975e-02  5.1937858041017471e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.1094390282629742e-02  5.0261935193351093e-02  4.9440351873451860e-02  4.8629501207008818e-02  4.7829245838954204e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.7039450152438045e-02  4.6259980246510013e-02  4.5490703914087494e-02  4.4731490620259606e-02  4.3982211480854794e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.3242739241325268e-02  4.2512948255901239e-02  4.1792714467042247e-02  4.1081915385161816e-02  4.0380430068628126e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.9688139104032016e-02  3.9004924586723888e-02  3.8330670101623943e-02  3.7665260704261794e-02  3.7008582902100740e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.6360524636098734e-02  3.5720975262514720e-02  3.5089825534961649e-02  3.4466967586696873e-02  3.3852294913154113e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.3245702354694373e-02  3.2647086079598653e-02  3.2056343567280043e-02  3.1473373591718756e-02  3.0898076205114089e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.0330352721755771e-02  2.9770105702100036e-02  2.9217238937061962e-02  2.8671657432515429e-02  2.8133267393987360e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.7601976211552692e-02  2.7077692444942292e-02  2.6560325808820506e-02  2.6049787158272331e-02  2.5545988474477754e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.5048842850559749e-02  2.4558264477628988e-02  2.4074168631003978e-02  2.3596471656606832e-02  2.3125090957536454e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.2659944980823798e-02  2.2200953204335683e-02  2.1748036123873327e-02  2.1301115240412782e-02  2.0860113047532769e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.0424953018977066e-02  1.9995559596398205e-02  1.9571858177249157e-02  1.9153775102828896e-02  1.8741237646474174e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.8334174001923720e-02  1.7932513271801676e-02  1.7536185456265119e-02  1.7145121441799471e-02  1.6759252990136364e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.6378512727332817e-02  1.6002834132978205e-02  1.5632151529535232e-02  1.5266400071822006e-02  1.4905515736628239e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4549435312452008e-02  1.4198096389378967e-02  1.3851437349077567e-02  1.3509397354926733e-02  1.3171916342264223e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.2838935008766206e-02  1.2510394804928215e-02  1.2186237924689647e-02  1.1866407296154180e-02  1.1550846572444651e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.1239500122655843e-02  1.0932313022929629e-02  1.0629231047647014e-02  1.0330200660712663e-02  1.0035169006965661e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.7440839036889715e-03  9.4568938322237006e-03  9.1735479296908284e-03  8.8939959808188584e-03  8.6181884098633921e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.3460762726380588e-03  8.0776112486364848e-03  7.8127456332576228e-03  7.5514323301215658e-03  7.2936248434884998e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.0392772707632556e-03  6.7883442950981143e-03  6.5407811780883174e-03  6.2965437525517309e-03  6.0555884154033235e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.8178721206175732e-03  5.5833523722726985e-03  5.3519872176873706e-03  5.1237352406410253e-03  4.8985555546731119e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.6764077964680517e-03  4.4572521193242398e-03  4.2410491867018174e-03  4.0277601658472717e-03  3.8173467214993595e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.6097710096757996e-03  3.4049956715283547e-03  3.2029838272795708e-03  3.0036990702375643e-03  2.8071054608720947e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.6131675209760674e-03  2.4218502278956500e-03  2.2331190088270558e-03  2.0469397351849938e-03  1.8632787170468346e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.6821026976564513e-03  1.5033788479984489e-03  1.3270747614446132e-03  1.1531584484569257e-03  9.8159833136179930e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.1236323918953968e-04  6.4542240257081662e-04  4.8074544870146951e-04  3.1830239637042901e-04  1.5806365104042985e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  0.                      0.                      0.                      0.                      0.", "%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );

   buf = bufOrig;
   for (int ii=1; ii<nR; ++ii)
   {
      real_t r = x0 + ii*dR;
      buf[ii] *= buf[ii] / r;
      buf[ii] *= hartreeToEv * bohrToAngs; // convert to eV
   }
   buf[0] = buf[1] + (buf[1] - buf[2]); // linear interpolation to get phi[0].
   initInterpolationObject(pot->phi, nR, x0, dR, buf);

   // read electron density rho
                sscanf("  0.                      5.4383329664155645e-05  9.3944898415945083e-04  4.3251847212615047e-03  1.2334244035325348e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.7137722173468548e-02  5.0697119791449641e-02  8.4607638668976470e-02  1.3001641279549414e-01  1.8759487452762702e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.5754900895683441e-01  3.3965493779430744e-01  4.3331024634064264e-01  5.3759384878832961e-01  6.5132908316254046e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.7314622535699939e-01  9.0154178511424377e-01  1.0349328562818201e+00  1.1717054897399350e+00  1.3102565818166738e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4490291582473986e+00  1.5865412121263560e+00  1.7214084470448441e+00  1.8523614026473965e+00  1.9782575145276269e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.0980886961566938e+00  2.2109850373516764e+00  2.3162151996095730e+00  2.4131840597491703e+00  2.5014281146549706e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.5806091153285706e+00  2.6505063508648590e+00  2.7110079545661563e+00  2.7621015568249447e+00  2.8038645637913220e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.8364542979766156e+00  2.8600981973448825e+00  2.8750842333755031e+00  2.8817516761559574e+00  2.8804823057701157e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.8716921439699092e+00  2.8558237581894161e+00  2.8333391711552594e+00  2.8047133934346959e+00  2.7704285829676252e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.7309688247181469e+00  2.6868155147671331e+00  2.6384433262347358e+00  2.5863167291097398e+00  2.5308870321738226e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.4725899125317596e+00  2.4118433966060167e+00  2.3490462556752334e+00  2.2845767789603002e+00  2.2187918877813502e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.1520265552815943e+00  2.0845934975626363e+00  2.0167831036919637e+00  1.9488635738636404e+00  1.8810812369508270e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.8136610207193371e+00  1.7468070500507196e+00  1.6807033505858371e+00  1.6155146372447149e+00  1.5513871690559142e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4884496536383409e+00  1.4268141864958608e+00  1.3665772120042590e+00  1.3078204945836447e+00  1.2506120900523854e+00","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.1950073085502879e+00  1.1410496616995687e+00  1.0887717878420631e+00  1.0381963502565981e+00  9.8933690422003551e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.4219872964247031e-01  8.9677962677415124e-01  8.5307067316958651e-01  8.1105694069385592e-01  7.7071817188505065e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.3202941544290212e-01  6.9496162100761794e-01  6.5948219372701189e-01  6.2555550939233484e-01  5.9314339115629977e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.6220554903693554e-01  5.3269998356387660e-01  5.0458335504023211e-01  4.7781131998032222e-01  4.5233883634534777e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.2812043923464138e-01  4.0511048870905242e-01  3.8326339142174781e-01  3.6253379771729577e-01  3.4287677583286325e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.2424796479760154e-01  3.0660370758054967e-01  2.8990116598452254e-01  2.7409841872609064e-01  2.5915454407883409e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.4502968839369110e-01  2.3168512174254197e-01  2.1908328186436687e-01  2.0718780752542632e-01  1.9596356233750800e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.8537665001230508e-01  1.7539442196444632e-01  1.6598547811304609e-01  1.5711966166996927e-01  1.4876804864444715e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4090293273673637e-01  1.3349780623990259e-01  1.2652733751724909e-01  1.1996734557434463e-01  1.1379477219856060e-01","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.0798765209582406e-01  1.0252508141368288e-01  9.7387185001678311e-02  9.2555082724584015e-02  8.8010855111109620e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.3737508589961873e-02  7.9718940536826377e-02  7.5939904329596963e-02  7.2385974585237101e-02  6.9043512729294765e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.5899633029043336e-02  6.2942169202580001e-02  6.0159641699440547e-02  5.7541225732930634e-02  5.5076720130546430e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.2756517056398833e-02  5.0571572648238083e-02  4.8513378601664936e-02  4.6573934725081756e-02  4.4745722480991068e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.3021679522073253e-02  4.1395175224364866e-02  3.9859987214311721e-02  3.8410278881708670e-02  3.7040577866510604e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.5745755503880039e-02  3.4521007208912380e-02  3.3361833779917971e-02  3.2264023597108116e-02  3.1223635691821294e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.0236983660070216e-02  2.9300620393215571e-02  2.8411323597772320e-02  2.7566082075896281e-02  2.6762082737777249e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.5996698317105604e-02  2.5267475760840985e-02  2.4572125264713973e-02  2.3908509926274246e-02  2.3274635987705516e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.2668643641204911e-02  2.2088798370316409e-02  2.1533482801290083e-02  2.1001189039288493e-02  2.0490511464994254e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.0000139967999431e-02  1.9528853594166895e-02  1.9075514584991349e-02  1.8639062787818239e-02  1.8218510416650235e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.7812937144080498e-02  1.7421485505751177e-02  1.7043356599549031e-02  1.6677806062561751e-02  1.6324140309613155e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.5981713017976018e-02  1.5649921843605585e-02  1.5328205354974755e-02  1.5016040171312250e-02  1.4712938292708366e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4418444610242331e-02  1.4132134584901757e-02  1.3853612084676337e-02  1.3582507369821917e-02  1.3318475216818060e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.3061193172097418e-02  1.2810359927147186e-02  1.2565693807050415e-02  1.2326931365025051e-02  1.2093826075940506e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.1866147122233661e-02  1.1643678266026136e-02  1.1426216801644407e-02  1.1213572583084475e-02  1.1005567121320226e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.0802032746662471e-02  1.0602811831688208e-02  1.0407756070544782e-02  1.0216725810699157e-02  1.0029589433467268e-02","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.8462227798860602e-03  9.6665086187306404e-03  9.4903361536790021e-03  9.3176005668363371e-03  9.1482025960089031e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.9820481433065535e-03  8.8190479128032462e-03  8.6591170751522117e-03  8.5021749571883021e-03  8.3481447546937537e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.1969532666261724e-03  8.0485306492223962e-03  7.9028101885199598e-03  7.7597280899136256e-03  7.6192232834934315e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.4812372439735375e-03  7.3457138241272979e-03  7.2125991007052359e-03  7.0818412319012813e-03  6.9533903254870300e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.8271983168139705e-03  6.7032188559211503e-03  6.5814072030662141e-03  6.4617201320263939e-03  6.3441158405819764e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.2285538676237207e-03  6.1149950163802147e-03  6.0034012832899109e-03  5.8937357920846312e-03  5.7859627326801166e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.6800473044990030e-03  5.5759556638887986e-03  5.4736548753111791e-03  5.3731128660109428e-03  5.2742983838981461e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.1771809583849582e-03  5.0817308639591330e-03  4.9879190862693046e-03  4.8957172905357560e-03  4.8050977921015592e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.7160335289582467e-03  4.6284980360953021e-03  4.5424654215287241e-03  4.4579103438822931e-03  4.3748079913988880e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.2931340622749670e-03  4.2128647462132407e-03  4.1339767071033873e-03  4.0564470667446839e-03  3.9802533895282599e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.9053736680121076e-03  3.8317863093158128e-03  3.7594701222811860e-03  3.6884043053326127e-03  3.6185684349951674e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.5499424550168301e-03  3.4825066660512660e-03  3.4162417158645347e-03  3.3511285900229004e-03  3.2871486030347646e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.2242833899080170e-03  3.1625148980992668e-03  3.1018253798278661e-03  3.0421973847258310e-03  2.9836137528083811e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.9260576077371064e-03  2.8695123503632708e-03  2.8139616525287708e-03  2.7593894511106498e-03  2.7057799422959966e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.6531175760685227e-03  2.6013870509009052e-03  2.5505733086344240e-03  2.5006615295404683e-03  2.4516371275501436e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.4034857456453340e-03  2.3561932514012535e-03  2.3097457326723414e-03  2.2641294934160616e-03  2.2193310496436136e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.1753371254977782e-03  2.1321346494441173e-03  2.0897107505768314e-03  2.0480527550303662e-03  2.0071481824917164e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.9669847428123305e-03  1.9275503327108034e-03  1.8888330325659355e-03  1.8508211032951805e-03  1.8135029833145980e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.7768672855772646e-03  1.7409027946878666e-03  1.7055984640891586e-03  1.6709434133182904e-03  1.6369269253308227e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.6035384438881917e-03  1.5707675710093030e-03  1.5386040644797400e-03  1.5070378354209296e-03  1.4760589459142243e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4456576066784674e-03  1.4158241748004133e-03  1.3865491515145517e-03  1.3578231800324136e-03  1.3296370434173130e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.3019816625059188e-03  1.2748480938728074e-03  1.2482275278369870e-03  1.2221112865106742e-03  1.1964908218862064e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.1713577139624703e-03  1.1467036689077198e-03  1.1225205172586891e-03  1.0988002121543120e-03  1.0755348276031765e-03","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.0527165567835728e-03  1.0303377103750150e-03  1.0083907149206553e-03  9.8686811121878604e-04  9.6576255274356815e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.4506680409354657e-04  9.2477373946662708e-04  9.0487634116191706e-04  8.8536769810608137e-04  8.6624100440530968e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.4748955791986991e-04  8.2910675886310736e-04  8.1108610842155551e-04  7.9342120739794852e-04  7.7610575487466887e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.5913354689786591e-04  7.4249847518158968e-04  7.2619452583109687e-04  7.1021577808524222e-04  6.9455640307671332e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.7921066261025093e-04  6.6417290795844214e-04  6.4943757867335500e-04  6.3499920141575628e-04  6.2085238879914031e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.0699183824991856e-04  5.9341233088238896e-04  5.8010873038847818e-04  5.6707598194186137e-04  5.5430911111587280e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.4180322281523891e-04  5.2955350022104025e-04  5.1755520374872563e-04  5.0580367001857793e-04  4.9429431083891986e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.8302261220136561e-04  4.7198413328763435e-04  4.6117450548847222e-04  4.5058943143359842e-04  4.4022468403297037e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.3007610552883886e-04  4.2013960655883260e-04  4.1041116522908330e-04  4.0088682619821882e-04  3.9156269977118005e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.8243496100300207e-04  3.7349984881274514e-04  3.6475366510662147e-04  3.5619277391102898e-04  3.4781360051482253e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.3961263062063513e-04  3.3158640950565685e-04  3.2373154119109092e-04  3.1604468762060252e-04  3.0852256784754707e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.0116195723081836e-04  2.9395968663908575e-04  2.8691264166377101e-04  2.8001776184017647e-04  2.7327203987681688e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.6667252089326854e-04  2.6021630166557681e-04  2.5390052988028163e-04  2.4772240339593181e-04  2.4167916951265550e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.3576812424967210e-04  2.2998661163024531e-04  2.2433202297460642e-04  2.1880179620031078e-04  2.1339341513026532e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.0810440880823181e-04  2.0293235082175821e-04  1.9787485863260665e-04  1.9292959291436311e-04  1.8809425689761319e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.8336659572205580e-04  1.7874439579616125e-04  1.7422548416372047e-04  1.6980772787763936e-04  1.6548903338088530e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.6126734589430591e-04  1.5714064881157744e-04  1.5310696310104604e-04  1.4916434671449329e-04  1.4531089400280153e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4154473513841234e-04  1.3786403554466153e-04  1.3426699533172857e-04  1.3075184873951283e-04  1.2731686358694039e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.2396034072819674e-04  1.2068061351527565e-04  1.1747604726729168e-04  1.1434503874632306e-04  1.1128601563955686e-04","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.0829743604811193e-04  1.0537778798212988e-04  1.0252558886227753e-04  9.9739385027582898e-05  9.7017751249615057e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  9.4359290252773662e-05  9.1762632240957511e-05  8.9226434430383569e-05  8.6749380588361721e-05  8.4330180578390864e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.1967569911181246e-05  7.9660309301724484e-05  7.7407184232279429e-05  7.5207004521348451e-05  7.3058603898526649e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  7.0960839585107720e-05  6.8912591880629977e-05  6.6912763755002085e-05  6.4960280446513426e-05  6.3054089065330086e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  6.1193158202771814e-05  5.9376477546041213e-05  5.7603057498502742e-05  5.5871928805544500e-05  5.4182142185708361e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.2532767967318744e-05  5.0922895730446966e-05  4.9351633954125953e-05  4.7818109668823321e-05  4.6321468114150300e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  4.4860872401664663e-05  4.3435503182825573e-05  4.2044558321957873e-05  4.0687252574273750e-05  3.9362817268785450e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.8070499996214428e-05  3.6809564301621984e-05  3.5579289382025496e-05  3.4378969788611451e-05  3.3207915133769052e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.2065449802711312e-05  3.0950912669766876e-05  2.9863656819185611e-05  2.8803049270468119e-05  2.7768470708167169e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.6759315216115260e-05  2.5774990015931323e-05  2.4814915209964844e-05  2.3878523528387922e-05  2.2965260080560611e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  2.2074582110528148e-05  2.1205958756658535e-05  2.0358870815317476e-05  1.9532810508535560e-05  1.8727281255713447e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.7941797449145505e-05  1.7175884233475961e-05  1.6429077288930018e-05  1.5700922618341645e-05  1.4990976337865471e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.4298804471386687e-05  1.3623982748522034e-05  1.2966096406226424e-05  1.2324739993882115e-05  1.1699517181902770e-05","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.1090040573734860e-05  1.0495931521266495e-05  9.9168199435395021e-06  9.3523441487842465e-06  8.8021506596591475e-06","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  8.2658940417265321e-06  7.7432367350197678e-06  7.2338488887770244e-06  6.7374081991923703e-06  6.2535997501888662e-06","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  5.7821158571569505e-06  5.3226559136389283e-06  4.8749262408651290e-06  4.4386399401326240e-06  4.0135167480073166e-06","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  3.5992828942305738e-06  3.1956709623667747e-06  2.8024197531120341e-06  2.4192741502208947e-06  2.0459849890155880e-06","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  1.6823089274468580e-06  1.3280083196495871e-06  9.8285109196557868e-07  6.4661062138351467e-07  3.1906561636122974e-07","%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );
        buf+=5; sscanf("  0.                      0.                      0.                      0.                      0.", "%lf %lf %lf %lf %lf", &buf[0], &buf[1], &buf[2], &buf[3], &buf[4] );

   buf = bufOrig;
   initInterpolationObject(pot->rho, nR, x0, dR, buf);

   ocrDbDestroy(DBK_buf);
}

void typeNotSupported(const char* callSite, const char* type)
{
   ocrPrintf("%s: Potential type %s not supported. Fatal Error.\n", callSite, type);
   ocrShutdown();
}

ocrGuid_t eamForce1_edt( EDT_ARGS )
{
    DEBUG_PRINTF(( "%s\n", __func__ ));
    u64 itimestep = paramv[0];

    s32 _idep;

    _idep = 0;
    ocrDBK_t DBK_rankH = depv[_idep++].guid;
    ocrDBK_t DBK_sim = depv[_idep++].guid;
    ocrDBK_t DBK_nAtoms  = depv[_idep++].guid;
    ocrDBK_t DBK_gid = depv[_idep++].guid;
    ocrDBK_t DBK_iSpecies = depv[_idep++].guid;
    ocrDBK_t DBK_r = depv[_idep++].guid;
    ocrDBK_t DBK_f = depv[_idep++].guid;
    ocrDBK_t DBK_U = depv[_idep++].guid;
    ocrDBK_t DBK_pot = depv[_idep++].guid;
    ocrDBK_t DBK_dfEmbed = depv[_idep++].guid;
    ocrDBK_t DBK_rhobar = depv[_idep++].guid;
    ocrDBK_t DBK_values_phi = depv[_idep++].guid;
    ocrDBK_t DBK_values_rho = depv[_idep++].guid;
    ocrDBK_t DBK_values_f = depv[_idep++].guid;

    _idep = 0;
    rankH_t* PTR_rankH = depv[_idep++].ptr;
    SimFlat* sim = depv[_idep++].ptr;
    int* nAtoms = depv[_idep++].ptr;
    int* gid = depv[_idep++].ptr;
    int* iSpecies = depv[_idep++].ptr;
    real3* r = depv[_idep++].ptr;
    real3* f = depv[_idep++].ptr;
    real_t* U = depv[_idep++].ptr;
    BasePotential* pot = depv[_idep++].ptr;
    real_t* dfEmbed = depv[_idep++].ptr;
    real_t* rhobar = depv[_idep++].ptr;
    real_t* values_phi = depv[_idep++].ptr;
    real_t* values_rho = depv[_idep++].ptr;
    real_t* values_f = depv[_idep++].ptr;

    sim->PTR_rankH = PTR_rankH;
    sim->pot = (BasePotential*) pot;

    sim->atoms = &sim->atoms_INST;
    sim->boxes = &sim->boxes_INST;
    sim->domain = &sim->domain_INST;
    sim->species = &sim->species_INST;
    sim->atomExchange = &sim->atomExchange_INST;

    sim->atoms->gid = gid;
    sim->atoms->iSpecies = iSpecies;
    sim->atoms->r = r;
    sim->atoms->f = f;
    sim->atoms->U = U;

    sim->boxes->nAtoms = nAtoms;

    EamPotential* epot = (EamPotential*) sim->pot;

    epot->dfEmbed = dfEmbed;
    epot->rhobar = rhobar;

    epot->phi = &epot->phi_INST;
    epot->rho = &epot->rho_INST;
    epot->f   = &epot->f_INST;

    epot->phi->values = values_phi;
    epot->rho->values = values_rho;
    epot->f->values = values_f;

    epot->phi->values++;
    epot->rho->values++;
    epot->f->values++;

    startTimer(sim->perfTimer, computeForceTimer);
    eamForce1(sim, itimestep);

    return NULL_GUID;
}

ocrGuid_t eamForce2_edt( EDT_ARGS )
{
    DEBUG_PRINTF(( "%s\n", __func__ ));
    u64 itimestep = paramv[0];

    s32 _idep;

    _idep = 0;
    ocrDBK_t DBK_rankH = depv[_idep++].guid;
    ocrDBK_t DBK_sim = depv[_idep++].guid;
    ocrDBK_t DBK_nAtoms  = depv[_idep++].guid;
    ocrDBK_t DBK_gid = depv[_idep++].guid;
    ocrDBK_t DBK_iSpecies = depv[_idep++].guid;
    ocrDBK_t DBK_r = depv[_idep++].guid;
    ocrDBK_t DBK_f = depv[_idep++].guid;
    ocrDBK_t DBK_pot = depv[_idep++].guid;
    ocrDBK_t DBK_dfEmbed = depv[_idep++].guid;
    ocrDBK_t DBK_values_rho = depv[_idep++].guid;

    _idep = 0;
    rankH_t* PTR_rankH = depv[_idep++].ptr;
    SimFlat* sim = depv[_idep++].ptr;
    int* nAtoms = depv[_idep++].ptr;
    int* gid = depv[_idep++].ptr;
    int* iSpecies = depv[_idep++].ptr;
    real3* r = depv[_idep++].ptr;
    real3* f = depv[_idep++].ptr;
    BasePotential* pot = depv[_idep++].ptr;
    real_t* dfEmbed = depv[_idep++].ptr;
    real_t* values_rho = depv[_idep++].ptr;

    sim->PTR_rankH = PTR_rankH;
    sim->pot = (BasePotential*) pot;

    sim->atoms = &sim->atoms_INST;
    sim->boxes = &sim->boxes_INST;
    sim->domain = &sim->domain_INST;
    sim->species = &sim->species_INST;
    sim->atomExchange = &sim->atomExchange_INST;

    sim->atoms->gid = gid;
    sim->atoms->iSpecies = iSpecies;
    sim->atoms->r = r;
    sim->atoms->f = f;

    sim->boxes->nAtoms = nAtoms;

    EamPotential* epot = (EamPotential*) sim->pot;
    epot->dfEmbed = dfEmbed;

    epot->rho = &epot->rho_INST;
    epot->rho->values = values_rho;
    epot->rho->values++;

    eamForce2(sim);
    stopTimer(sim->perfTimer, computeForceTimer);

    return NULL_GUID;
}

ocrGuid_t eamForce_edt( EDT_ARGS )
{
    DEBUG_PRINTF(( "%s\n", __func__ ));

    u64 itimestep = paramv[0];

    s32 _idep;

    _idep = 0;
    ocrDBK_t DBK_rankH = depv[_idep++].guid;
    ocrDBK_t DBK_sim = depv[_idep++].guid;
    ocrDBK_t DBK_pot = depv[_idep++].guid;

    _idep = 0;
    rankH_t* PTR_rankH = depv[_idep++].ptr;
    SimFlat* sim = depv[_idep++].ptr;
    BasePotential* pot = depv[_idep++].ptr;

    rankTemplateH_t* PTR_rankTemplateH = &(PTR_rankH->rankTemplateH);

    sim->atoms = &sim->atoms_INST;
    sim->boxes = &sim->boxes_INST;
    sim->domain = &sim->domain_INST;
    sim->atomExchange = &sim->atomExchange_INST;

    sim->PTR_rankH = PTR_rankH;
    sim->pot = pot;

    EamPotential* epot = (EamPotential*) sim->pot;
    epot->forceExchange = &epot->forceExchange_INST;

    ocrHint_t myEdtAffinityHNT = sim->PTR_rankH->myEdtAffinityHNT;
#ifdef ENABLE_SPAWNING_HINT
    ocrHint_t myEdtAffinitySpawnHNT = myEdtAffinityHNT;
    ocrSetHintValue(&myEdtAffinitySpawnHNT, OCR_HINT_EDT_SPAWNING, 1);
#endif

    ocrDBK_t DBK_nAtoms = sim->boxes->DBK_nAtoms;

    ocrDBK_t DBK_gid = sim->atoms->DBK_gid;
    ocrDBK_t DBK_iSpecies = sim->atoms->DBK_iSpecies;
    ocrDBK_t DBK_r = sim->atoms->DBK_r;
    ocrDBK_t DBK_p = sim->atoms->DBK_p;
    ocrDBK_t DBK_f = sim->atoms->DBK_f;
    ocrDBK_t DBK_U = sim->atoms->DBK_U;

    ocrDBK_t DBK_parms = epot->forceExchange->DBK_parms;

    ocrDBK_t DBK_dfEmbed = epot->DBK_dfEmbed;
    ocrDBK_t DBK_rhobar = epot->DBK_rhobar;

    epot->phi = &epot->phi_INST;
    epot->rho = &epot->rho_INST;
    epot->f   = &epot->f_INST;

    ocrDBK_t DBK_values_phi = epot->phi->DBK_values;
    ocrDBK_t DBK_values_rho = epot->rho->DBK_values;
    ocrDBK_t DBK_values_f = epot->f->DBK_values;

    ocrGuid_t force1TML = sim->pot->force1TML;
    ocrGuid_t force2TML = epot->force2TML;

   // set up halo exchange and internal storage on first call to forces.
    if( itimestep == 0 )
    {
        int maxTotalAtoms = MAXATOMS*sim->boxes->nTotalBoxes;

        ocrDbCreate( &epot->DBK_dfEmbed, (void**) &epot->dfEmbed, maxTotalAtoms*sizeof(real_t), 0, NULL_HINT, NO_ALLOC );
        ocrDbCreate( &epot->DBK_rhobar, (void**) &epot->rhobar, maxTotalAtoms*sizeof(real_t), 0, NULL_HINT, NO_ALLOC );

        epot->forceExchange = &epot->forceExchange_INST;
        initForceHaloExchange(epot->forceExchange, sim->domain, sim->boxes);

        DBK_dfEmbed = epot->DBK_dfEmbed;
        DBK_rhobar = epot->DBK_rhobar;

        ocrDbRelease(DBK_pot);
    }

    ocrGuid_t eamForce1TML, eamForce1EDT, eamForce1OEVT, eamForce1OEVTS;

    ocrEdtCreate( &eamForce1EDT, force1TML, //eamForce1_edt
                  EDT_PARAM_DEF, paramv, EDT_PARAM_DEF, NULL,
                  EDT_PROP_NONE, &myEdtAffinityHNT, &eamForce1OEVT );

    createEventHelper( &eamForce1OEVTS, 1);
    ocrAddDependence( eamForce1OEVT, eamForce1OEVTS, 0, DB_MODE_NULL );

    _idep = 0;
    ocrAddDependence( DBK_rankH, eamForce1EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_sim, eamForce1EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_nAtoms, eamForce1EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_gid, eamForce1EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_iSpecies, eamForce1EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_r, eamForce1EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_f, eamForce1EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_U, eamForce1EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_pot, eamForce1EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_dfEmbed, eamForce1EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_rhobar, eamForce1EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_values_phi, eamForce1EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_values_rho, eamForce1EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_values_f, eamForce1EDT, _idep++, DB_MODE_RO );

    ocrGuid_t haloExchangeTML, haloExchangeEDT, haloExchangeOEVT, haloExchangeOEVTS;

    u64 iAxis = 0;
    u64 paramv_haloExchange[2] = {iAxis, itimestep};

#ifdef ENABLE_SPAWNING_HINT
    ocrEdtCreate( &haloExchangeEDT, PTR_rankTemplateH->forceHaloExchangeTML, //forceHaloExchangeEdt
                  EDT_PARAM_DEF, paramv_haloExchange, EDT_PARAM_DEF, NULL,
                  EDT_PROP_FINISH, &myEdtAffinitySpawnHNT, &haloExchangeOEVT );
#else
    ocrEdtCreate( &haloExchangeEDT, PTR_rankTemplateH->forceHaloExchangeTML, //forceHaloExchangeEdt
                  EDT_PARAM_DEF, paramv_haloExchange, EDT_PARAM_DEF, NULL,
                  EDT_PROP_FINISH, &myEdtAffinityHNT, &haloExchangeOEVT );
#endif

    createEventHelper( &haloExchangeOEVTS, 1);
    ocrAddDependence( haloExchangeOEVT, haloExchangeOEVTS, 0, DB_MODE_NULL );

    _idep = 0;
    ocrAddDependence( DBK_rankH, haloExchangeEDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_sim, haloExchangeEDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_pot, haloExchangeEDT, _idep++, DB_MODE_RW );
    ocrAddDependence( eamForce1OEVTS, haloExchangeEDT, _idep++, DB_MODE_NULL );

    ocrGuid_t eamForce2TML, eamForce2EDT, eamForce2OEVT, eamForce2OEVTS;

    ocrEdtCreate( &eamForce2EDT, force2TML, //eamForce2_edt
                  EDT_PARAM_DEF, paramv, EDT_PARAM_DEF, NULL,
                  EDT_PROP_NONE, &myEdtAffinityHNT, &eamForce2OEVT );

    _idep = 0;
    ocrAddDependence( DBK_rankH, eamForce2EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_sim, eamForce2EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_nAtoms, eamForce2EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_gid, eamForce2EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_iSpecies, eamForce2EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_r, eamForce2EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_f, eamForce2EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_pot, eamForce2EDT, _idep++, DB_MODE_RW );
    ocrAddDependence( DBK_dfEmbed, eamForce2EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( DBK_values_rho, eamForce2EDT, _idep++, DB_MODE_RO );
    ocrAddDependence( haloExchangeOEVTS, eamForce2EDT, _idep++, DB_MODE_NULL );

    return NULL_GUID;
}

#include "rsbench.h"

// Handles all material creation tasks - returns Material struct
Materials get_materials(rankDataH_t* PTR_rankDataH, Inputs in)
{
    Materials M;
    M.num_nucs  = load_num_nucs( &PTR_rankDataH->DBK_num_nucs, in.n_nuclides);
    M.mats     = load_mats(PTR_rankDataH, M.num_nucs, in.n_nuclides);
    M.concs = load_concs(PTR_rankDataH, M.num_nucs);

    return M;
}

// num_nucs represents the number of nuclides that each material contains
int * load_num_nucs(ocrDBK_t* DBK_num_nucs, long n_isotopes)
{
    int * num_nucs;// = (int*)malloc(12*sizeof(int));

    ocrDbCreate( DBK_num_nucs, (void **) &num_nucs, 12*sizeof(int),
                    0, NULL_HINT, NO_ALLOC );

    // Material 0 is a special case (fuel). The H-M small reactor uses
    // 34 nuclides, while H-M larges uses 300.
    if( n_isotopes == 68 )
        num_nucs[0]  = 34; // HM Small is 34, H-M Large is 321
    else
        num_nucs[0]  = 321; // HM Small is 34, H-M Large is 321

    num_nucs[1]  = 5;
    num_nucs[2]  = 4;
    num_nucs[3]  = 4;
    num_nucs[4]  = 27;
    num_nucs[5]  = 21;
    num_nucs[6]  = 21;
    num_nucs[7]  = 21;
    num_nucs[8]  = 21;
    num_nucs[9]  = 21;
    num_nucs[10] = 9;
    num_nucs[11] = 9;

    return num_nucs;
}

void assign_mats_ptrs( int* num_nucs, int* mats_all, int** mats )
{
    int num_nucs_all = 0;
    for( int i = 0; i < 12; i++ )
    {
        mats[i] = (int *) &(mats_all[num_nucs_all]);
        num_nucs_all += num_nucs[i];
    }
}

// Assigns an array of nuclide ID's to each material
int ** load_mats( rankDataH_t* PTR_rankDataH, int * num_nucs, long n_isotopes )
{
    int ** mats; // = (int **) malloc( 12 * sizeof(int *) );
    ocrDbCreate( &PTR_rankDataH->DBK_mat_ptrs, (void **) &mats, 12*sizeof(int *),
                    0, NULL_HINT, NO_ALLOC );
    int num_nucs_all = 0;
    for( int i = 0; i < 12; i++ )
    {
        //mats[i] = (int *) malloc(num_nucs[i] * sizeof(int) );
        //ocrDbCreate( &PTR_rankDataH->DBK_mats[i], (void **) &mats[i], num_nucs[i]*sizeof(int),
        //                0, NULL_HINT, NO_ALLOC );
        num_nucs_all += num_nucs[i];
    }

    int* mats_all;
    ocrDbCreate( &PTR_rankDataH->DBK_mats_all, (void **) &mats_all, num_nucs_all*sizeof(int),
                    0, NULL_HINT, NO_ALLOC );

    assign_mats_ptrs( num_nucs, mats_all, mats );

    // Small H-M has 34 fuel nuclides
    int mats0_Sml[] =  { 58, 59, 60, 61, 40, 42, 43, 44, 45, 46, 1, 2, 3, 7,
                     8, 9, 10, 29, 57, 47, 48, 0, 62, 15, 33, 34, 52, 53,
                     54, 55, 56, 18, 23, 41 }; //fuel
    // Large H-M has 300 fuel nuclides
    int mats0_Lrg[321] =  { 58, 59, 60, 61, 40, 42, 43, 44, 45, 46, 1, 2, 3, 7,
                     8, 9, 10, 29, 57, 47, 48, 0, 62, 15, 33, 34, 52, 53,
                     54, 55, 56, 18, 23, 41 }; //fuel
    for( int i = 0; i < 321-34; i++ )
        mats0_Lrg[34+i] = 68 + i; // H-M large adds nuclides to fuel only

    // These are the non-fuel materials
    int mats1[] =  { 63, 64, 65, 66, 67 }; // cladding
    int mats2[] =  { 24, 41, 4, 5 }; // cold borated water
    int mats3[] =  { 24, 41, 4, 5 }; // hot borated water
    int mats4[] =  { 19, 20, 21, 22, 35, 36, 37, 38, 39, 25, 27, 28, 29,
                     30, 31, 32, 26, 49, 50, 51, 11, 12, 13, 14, 6, 16,
                     17 }; // RPV
    int mats5[] =  { 24, 41, 4, 5, 19, 20, 21, 22, 35, 36, 37, 38, 39, 25,
                     49, 50, 51, 11, 12, 13, 14 }; // lower radial reflector
    int mats6[] =  { 24, 41, 4, 5, 19, 20, 21, 22, 35, 36, 37, 38, 39, 25,
                     49, 50, 51, 11, 12, 13, 14 }; // top reflector / plate
    int mats7[] =  { 24, 41, 4, 5, 19, 20, 21, 22, 35, 36, 37, 38, 39, 25,
                     49, 50, 51, 11, 12, 13, 14 }; // bottom plate
    int mats8[] =  { 24, 41, 4, 5, 19, 20, 21, 22, 35, 36, 37, 38, 39, 25,
                     49, 50, 51, 11, 12, 13, 14 }; // bottom nozzle
    int mats9[] =  { 24, 41, 4, 5, 19, 20, 21, 22, 35, 36, 37, 38, 39, 25,
                     49, 50, 51, 11, 12, 13, 14 }; // top nozzle
    int mats10[] = { 24, 41, 4, 5, 63, 64, 65, 66, 67 }; // top of FA's
    int mats11[] = { 24, 41, 4, 5, 63, 64, 65, 66, 67 }; // bottom FA's

    // H-M large v small dependency
    if( n_isotopes == 68 )
        memcpy( mats[0],  mats0_Sml,  num_nucs[0]  * sizeof(int) );
    else
        memcpy( mats[0],  mats0_Lrg,  num_nucs[0]  * sizeof(int) );

    // Copy other materials
    memcpy( mats[1],  mats1,  num_nucs[1]  * sizeof(int) );
    memcpy( mats[2],  mats2,  num_nucs[2]  * sizeof(int) );
    memcpy( mats[3],  mats3,  num_nucs[3]  * sizeof(int) );
    memcpy( mats[4],  mats4,  num_nucs[4]  * sizeof(int) );
    memcpy( mats[5],  mats5,  num_nucs[5]  * sizeof(int) );
    memcpy( mats[6],  mats6,  num_nucs[6]  * sizeof(int) );
    memcpy( mats[7],  mats7,  num_nucs[7]  * sizeof(int) );
    memcpy( mats[8],  mats8,  num_nucs[8]  * sizeof(int) );
    memcpy( mats[9],  mats9,  num_nucs[9]  * sizeof(int) );
    memcpy( mats[10], mats10, num_nucs[10] * sizeof(int) );
    memcpy( mats[11], mats11, num_nucs[11] * sizeof(int) );

    // test
    /*
    for( int i = 0; i < 12; i++ )
        for( int j = 0; j < num_nucs[i]; j++ )
            PRINTF("material %d - Nuclide %d: %d\n",
                   i, j, mats[i][j]);
    */

    return mats;
}

void assign_concs_ptrs( int* num_nucs, double* concs_all, double** concs )
{
    int num_nucs_all = 0;
    for( int i = 0; i < 12; i++ )
    {
        concs[i] = (double*) &(concs_all[num_nucs_all]);
        num_nucs_all += num_nucs[i];
    }
}

// Creates a randomized array of 'concentrations' of nuclides in each mat
double ** load_concs( rankDataH_t* PTR_rankDataH, int * num_nucs )
{
    double ** concs; // = (double **)malloc( 12 * sizeof( double *) );
    ocrDbCreate( &PTR_rankDataH->DBK_conc_ptrs, (void **) &concs, 12*sizeof(double *),
                    0, NULL_HINT, NO_ALLOC );

    int num_nucs_all = 0;
    for( int i = 0; i < 12; i++ )
    {
    //  //concs[i] = (double *)malloc( num_nucs[i] * sizeof(double) );
    //    ocrDbCreate( &PTR_rankDataH->DBK_concs[i], (void **) &concs[i], num_nucs[i]*sizeof(double),
    //                    0, NULL_HINT, NO_ALLOC );
        num_nucs_all += num_nucs[i];
    }

    double* concs_all;
    ocrDbCreate( &PTR_rankDataH->DBK_concs_all, (void **) &concs_all, num_nucs_all*sizeof(double),
                    0, NULL_HINT, NO_ALLOC );

    assign_concs_ptrs( num_nucs, concs_all, concs );

    for( int i = 0; i < 12; i++ )
        for( int j = 0; j < num_nucs[i]; j++ )
            concs[i][j] = (double) rand() / (double) RAND_MAX;

    // test
    /*
    for( int i = 0; i < 12; i++ )
        for( int j = 0; j < num_nucs[i]; j++ )
            PRINTF("concs[%d][%d] = %lf\n", i, j, concs[i][j] );
    */

    return concs;
}

// picks a material based on a probabilistic distribution
int pick_mat( u64* seed )
{
    // I have a nice spreadsheet supporting these numbers. They are
    // the fractions (by volume) of material in the core. Not a
    // *perfect* approximation of where XS lookups are going to occur,
    // but this will do a good job of biasing the system nonetheless.

    // Also could be argued that doing fractions by weight would be
    // a better approximation, but volume does a good enough job for now.

    double dist[12];
    dist[0]  = 0.140;   // fuel
    dist[1]  = 0.052;   // cladding
    dist[2]  = 0.275;   // cold, borated water
    dist[3]  = 0.134;   // hot, borated water
    dist[4]  = 0.154;   // RPV
    dist[5]  = 0.064;   // Lower, radial reflector
    dist[6]  = 0.066;   // Upper reflector / top plate
    dist[7]  = 0.055;   // bottom plate
    dist[8]  = 0.008;   // bottom nozzle
    dist[9]  = 0.015;   // top nozzle
    dist[10] = 0.025;   // top of fuel assemblies
    dist[11] = 0.013;   // bottom of fuel assemblies

    //double roll = (double) rand() / (double) RAND_MAX;
    #ifdef VERIFICATION
    double roll = rn_v(seed);
    #else
    double roll = rn(seed);
    #endif

    // makes a pick based on the distro
    for( int i = 0; i < 12; i++ )
    {
        double running = 0;
        for( int j = i; j > 0; j-- )
            running += dist[j];
        if( roll < running )
            return i;
    }

    return 0;
}

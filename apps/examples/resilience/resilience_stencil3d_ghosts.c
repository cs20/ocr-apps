/*
 *  This file is subject to the license agreement located in the file LICENSE
 *  and cannot be distributed without it. This notice cannot be
 *  removed or modified.
 */
#include "ocr.h"

/* Example of an iterative 1D Stencil + "fork-join" pattern in OCR
 *
 * Implements the following dependence graph:
 *
 *             mainEdt
 *          /           \
 *         /             \
 *        /               \
 *    resEdt[1][0]        resEdt[1][1]      ...  resEdt[1][XDIM-1]
 *       / | \              / | \
 * locEdt ... locEdt  locEdt ... locEdt
 *       \ | /              \ | /
 *    resEdt[2][0]        resEdt[2][1]      ...  resEdt[2][XDIM-1]
 *       / | \              / | \
 * locEdt ... locEdt  locEdt ... locEdt
 *       \ | /              \ | /
 *                 ...
 *                 ...
 *                 ...
 *
 * resEdt[ITERS][0]       resEdt[ITERS][1]  ...  resEdt[ITERS][XDIM-1]
 *       / | \              / | \
 * locEdt ... locEdt  locEdt ... locEdt
 *       \ | /              \ | /
 *        \                 /
 *         \               /
 *          \             /
 *            shutdownEdt
 *
 * NOTE: resEdt[<iter-value>][<x-value>]
 *
 */

//Configurable params
#define N       4
#define XDIM    N   //Number of tiles or blocks in X-dimension
#define YDIM    N   //Number of tiles or blocks on Y-dimension
#define ZDIM    N   //Number of tiles or blocks on Y-dimension
#define ITERS   6   //Number of iterations in application
#define LOCAL_MESH_SIZE 8
#define LEFT_BC 4.0
#define RIGHT_BC 4.0
#define UP_BC 2.0
#define DOWN_BC 2.0
#define FRONT_BC 2.0
#define BACK_BC 2.0

//Derived params (DO NOT CHANGE)
#define NUM_DIMS 3 //(x,y,z)
#define NUM_ITERS (ITERS + 1)
#define ITER_SPACE (XDIM * YDIM * ZDIM)
#define ITER_INDEX(x, y, z) ((x * (YDIM * ZDIM)) + (y * ZDIM) + z)

#define NUM_GHOSTS 6
#define NUM_POINTS (NUM_GHOSTS + 1)
#define EVENT_SPACE_PER_ITER (ITER_SPACE * NUM_POINTS) //Iteration space including ghost points

#define USER_KEY(i, x, y, z)       ((i * EVENT_SPACE_PER_ITER) + ITER_INDEX(x, y, z))

#define USER_KEY_SELF(i, x, y, z)  ((i * EVENT_SPACE_PER_ITER) + ( ITER_INDEX(x, y, z) * NUM_POINTS) + 0)
#define USER_KEY_LEFT(i, x, y, z)  ((i * EVENT_SPACE_PER_ITER) + ( ITER_INDEX(x, y, z) * NUM_POINTS) + 1)
#define USER_KEY_RIGHT(i, x, y, z) ((i * EVENT_SPACE_PER_ITER) + ( ITER_INDEX(x, y, Z) * NUM_POINTS) + 2)
#define USER_KEY_UP(i, x, y, z)    ((i * EVENT_SPACE_PER_ITER) + ( ITER_INDEX(x, y, Z) * NUM_POINTS) + 3)
#define USER_KEY_DOWN(i, x, y ,z)  ((i * EVENT_SPACE_PER_ITER) + ( ITER_INDEX(x, y, z) * NUM_POINTS) + 4)
#define USER_KEY_FRONT(i, x, y, z)  ((i * EVENT_SPACE_PER_ITER) + ( ITER_INDEX(x, y, z) * NUM_POINTS) + 5)
#define USER_KEY_BACK(i, x, y, z)  ((i * EVENT_SPACE_PER_ITER) + ( ITER_INDEX(x, y, z) * NUM_POINTS) + 6)

#define GUID_PARAM (sizeof(ocrGuid_t)/sizeof(u64))
#define NUM_PARAMS (1/*iter value*/ + NUM_DIMS/*coordinate value*/ + (GUID_PARAM * 8)/*shutdown guid + cleanup DBs (5 dep DBs)*/)
#define NUM_DEPS 7 /* 7-point stencil */

#define NUM_LOCAL_EDTS 4
#define INJECT_FAULT 0

//Dummy function for local computation EDT on every iteration in each rank


ocrGuid_t localFunc(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("[Node %lu]: Hello from local EDT (%lu, %lu, %lu, %lu)\n", ocrGetLocation(), paramv[0], paramv[1], paramv[2], paramv[3]);
    return NULL_GUID;
}

//Function for creating local computation in every iteration in each rank
//
static void doLocalComputation(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[], ocrGuid_t db) {
    u64 i;
    u64 iter = paramv[0];
    u64 x = paramv[1];
    u64 y = paramv[2];

    //Create the local computation EDTs
    ocrGuid_t localEdt_template;
    ocrEdtTemplateCreate(&localEdt_template, localFunc, 4, 0);
    u64 paramsLocal[4];
    paramsLocal[0] = iter;
    paramsLocal[1] = x;
    paramsLocal[2] = y;
    for (i = 0; i < NUM_LOCAL_EDTS; i++) {
       ocrGuid_t localEdt;
       paramsLocal[3] = i;
       ocrEdtCreate(&localEdt, localEdt_template, 4, paramsLocal, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
    }
    return;
}


//Top-level function that runs on every iteration in each rank
ocrGuid_t resilientFunc(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 i, j, k;
    u64 iter = paramv[0];
    u64 x = paramv[1];
    u64 y = paramv[2];
    u64 z = paramv[3];

    if (iter < NUM_ITERS) {
       PRINTF("[Node %lu]: Hello from resilient EDT (%lu, %lu, %lu)\n", ocrGetLocation(), iter, x, y);
    } else if (iter == NUM_ITERS) {
       PRINTF("[Node %lu]: Hello from epilogue EDT (%lu, %lu)\n", ocrGetLocation(), x, y);
    } else {
       PRINTF("[Node %lu]: Hello from cleanup EDT (%lu, %lu)\n", ocrGetLocation(), x, y);
    }

    ocrGuid_t *guidParamv = (ocrGuid_t*)&(paramv[1 + NUM_DIMS]);

    /*/Cleanup past iteration (iter - 2) DB and (iter - 1) Events
    ocrDbDestroy(guidParamv[1]);
    ocrDbDestroy(guidParamv[2]);
    ocrDbDestroy(guidParamv[3]);
    ocrDbDestroy(guidParamv[4]);
    ocrDbDestroy(guidParamv[5]);

    if (iter > 2) {
        ocrGuid_t pEvt;
        ocrGuidTableRemove(USER_KEY_SELF((iter - 2), x, y), &pEvt);
        ocrEventDestroy(pEvt);

        if( x > 0 )
        {
             ocrGuidTableRemove(USER_KEY_LEFT((iter - 2), x, y), &pEvt);
             ocrEventDestroy(pEvt);
        }
        if( x < XDIM-1 ) {
            ocrGuidTableRemove(USER_KEY_RIGHT((iter - 2), x, y), &pEvt);
            ocrEventDestroy(pEvt);
        }
        if( y > 0 )
        {
             ocrGuidTableRemove(USER_KEY_UP((iter - 2), x, y), &pEvt);
             ocrEventDestroy(pEvt);
        }
        if( y < YDIM-1 ) {
            ocrGuidTableRemove(USER_KEY_DOWN((iter - 2), x, y), &pEvt);
            ocrEventDestroy(pEvt);
        }
    }*/

    //Check for termination condition on this iteration
    if (iter == (NUM_ITERS + 1)) {
        ocrGuid_t shutdownEdt = guidParamv[0];
        ocrAddDependence(NULL_GUID, shutdownEdt, ITER_INDEX(x, y), DB_MODE_NULL);
        return NULL_GUID;
    }

    double *dptr = (double *)depv[0].ptr;
    double *dptr_gp_from_left  = (double *)depv[1].ptr;
    double *dptr_gp_from_right = (double *)depv[2].ptr;
    double *dptr_gp_from_up    = (double *)depv[3].ptr;
    double *dptr_gp_from_down  = (double *)depv[4].ptr;
    double *dptr_gp_from_front = (double *)depv[5].ptr;
    double *dptr_gp_from_back  = (double *)depv[6].ptr;

#if INJECT_FAULT
    //Fault injection
    if (iter == NUM_ITERS/2 && x == 0 && y == 0) {
        PRINTF("[Node %lu]: Injecting fault from resilient EDT (%lu, %lu, %lu)\n", ocrGetLocation(), iter, x, y);
        ocrInjectNodeFailure();
    }
#endif

    ocrGuid_t db          = NULL_GUID;
    ocrGuid_t db_left_gp  = NULL_GUID;
    ocrGuid_t db_right_gp = NULL_GUID;
    ocrGuid_t db_up_gp    = NULL_GUID;
    ocrGuid_t db_down_gp  = NULL_GUID;
    ocrGuid_t db_front_gp = NULL_GUID;
    ocrGuid_t db_back_gp  = NULL_GUID;

    if (iter < NUM_ITERS) {
        //Create a resilient output DB of this iteration
        void *ptr = NULL;
        void *ptr_left = NULL;
        void *ptr_right = NULL;
        void *ptr_up = NULL;
        void *ptr_down = NULL;
        void *ptr_front = NULL;
        void *ptr_back = NULL;

        ocrDbCreate(&db, (void**)&ptr, sizeof(double)* LOCAL_MESH_SZIE * LOCAL_MESH_SIZE*LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
        ocrDbCreate(&db_left_gp, (void**)&ptr_left, sizeof(double)* LOCAL_MESH_SZIE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
        ocrDbCreate(&db_right_gp, (void**)&ptr_right, sizeof(double)* LOCAL_MESH_SZIE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
        ocrDbCreate(&db_up_gp, (void**)&ptr_up, sizeof(double) * LOCAL_MESH_SZIE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
        ocrDbCreate(&db_down_gp, (void**)&ptr_down, sizeof(double)* LOCAL_MESH_SZIE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
        ocrDbCreate(&db_front_gp, (void**)&ptr_front, sizeof(double)* LOCAL_MESH_SZIE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
        ocrDbCreate(&db_back_gp, (void**)&ptr_back, sizeof(double)* LOCAL_MESH_SZIE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);

        //Perform local computation for output DB of this iteration
        //doLocalComputation(paramc, paramv, depc, depv, db);

        double* dptr_out = (double *)ptr;
        double* dptr_left_out = (double *)ptr_left;
        double* dptr_right_out = (double *)ptr_right;
        double* dptr_up_out = (double *)ptr_up;
        double* dptr_down_out = (double *)ptr_down;
        double* dptr_front_out = (double *)ptr_front;
        double* dptr_back_out = (double *)ptr_back;
        double gp_from_left, gp_from_right, gp_from_up, gp_from_down, gp_from_front, gp_from_back;


        // Processing Boundary Condition
        if ( x > 0 )
        {
           gp_from_left = ((double *)depv[1].ptr)[0];
           //PRINTF("[Node %lu]: resilient EDT (%lu, %lu, %lu) uses its only value from left %f\n", ocrGetLocation() , iter, x, y, gp_from_left );
        }
        else
        {
           gp_from_left = LEFT_BC;
        }

        if ( x < XDIM-1 )
        {
           gp_from_right = ((double *)depv[2].ptr)[0];
          // PRINTF("[Node %lu]: resilient EDT (%lu, %lu) uses its only value from left %f\n", ocrGetLocation() , iter, x,gp_from_left );
        }
        else
        {
           gp_from_right = RIGHT_BC;
        }

        if ( y > 0 )
        {
           gp_from_up = ((double *)depv[3].ptr)[0];
          // PRINTF("[Node %lu]: resilient EDT (%lu, %lu) uses its only value from left %f\n", ocrGetLocation() , iter, x,gp_from_left );
        }
        else
        {
           gp_from_up = UP_BC;
        }

        if ( y < YDIM-1 )
        {
           gp_from_down = ((double *)depv[4].ptr)[0];
          // PRINTF("[Node %lu]: resilient EDT (%lu, %lu) uses its only value from left %f\n", ocrGetLocation() , iter, x,gp_from_left );
        }
        else
        {
           gp_from_down = DOWN_BC;
        }
        if ( z > 0 )
        {
           gp_from_front = ((double *)depv[5].ptr)[0];
          // PRINTF("[Node %lu]: resilient EDT (%lu, %lu) uses its only value from left %f\n", ocrGetLocation() , iter, x,gp_from_left );
        }
        else
        {
           gp_from_up = FRONT_BC;
        }

        if ( z < ZDIM-1 )
        {
           gp_from_back = ((double *)depv[6].ptr)[0];
          // PRINTF("[Node %lu]: resilient EDT (%lu, %lu) uses its only value from left %f\n", ocrGetLocation() , iter, x,gp_from_left );
        }
        else
        {
           gp_from_up = BACK_BC;
        }



        // Compute Local 3D Domain

#if 0
        dptr_left_out[0] = dptr_out[0] = (-dptr[0] + 0.5 * (  gp_from_left + dptr[1] ))*0.2 + dptr[0];
        for ( i = 1; i < LOCAL_MESH_SIZE_X-1; i++ )
        {
           dptr_left_out[0] = dptr_out[0] = (-dptr[0] + 0.5 * (  gp_from_left + dptr[1] ))*0.2 + dptr[0];
           for ( j = 1; j < LOCAL_MESH_SIZE_Y-1; j++ ) 
           {
               dptr_out[i * (LOCAL_MESH_SIZE_Y) + j] = ( -dptr[i] + 0.5 * (  dptr[i-1] + dptr[i+1] ) ) * 0.2 + dptr[i];
           }
           dptr_left_out[0] = dptr_out[0] = (-dptr[0] + 0.5 * (  gp_from_left + dptr[1] ))*0.2 + dptr[0];
        }
        dptr_right_out[0] =  dptr_out[LOCAL_MESH_SIZE-1] = (-dptr[LOCAL_MESH_SIZE-1] + 0.5 * (  gp_from_right + dptr[LOCAL_MESH_SIZE-2] ))* 0.2 + dptr[LOCAL_MESH_SIZE-1];
#endif
        //PRINTF("[Node %lu]: Value of DB[%d] is %f in resilient EDT (%lu, %lu, %lu)\n", ocrGetLocation(), 0, dptr[0], iter, x, y);

#if 0
        for( i = 0; i < LOCAL_MESH_SIZE; i++ )
        {
           PRINTF("[Node %lu]: Value of DB[%d] is %f in resilient EDT (%lu, %lu)\n", ocrGetLocation(), i, dptr_out[i], iter, x);
        }
#endif
    }


    //Satisfy event to transmit DB (x,y) from this iteration (iter) to
    //next iteration (iter+1) EDT neighbors [(x-1, y), (x, y-1), (x+1, y), (x, y+1)]
    ocrGuid_t oEvt;
    ocrGuidTableGet(USER_KEY_SELF((iter + 1), x, y, z), &oEvt);
    ocrEventSatisfy(oEvt, db);

    if ( x > 0 ) 
    {
        ocrGuidTableGet(USER_KEY_LEFT((iter + 1), x, y, z), &oEvt);
        ocrEventSatisfy(oEvt, db_left_gp);
    }

    if ( x < XDIM-1 ) 
    {
        ocrGuidTableGet(USER_KEY_RIGHT((iter + 1), x, y, z), &oEvt);
        ocrEventSatisfy(oEvt, db_right_gp);
    }

    if ( y > 0 ) 
    {
        ocrGuidTableGet(USER_KEY_UP((iter + 1), x, y, z), &oEvt);
        ocrEventSatisfy(oEvt, db_up_gp);
    }

    if ( y < YDIM-1 ) 
    {
        ocrGuidTableGet(USER_KEY_DOWN((iter + 1), x, y, z), &oEvt);
        ocrEventSatisfy(oEvt, db_down_gp);
    }

    if ( z > 0 ) 
    {
        ocrGuidTableGet(USER_KEY_FRONT((iter + 1), x ,y, z), &oEvt);
        ocrEventSatisfy(oEvt, db_front_gp);
    }

    if ( z < ZDIM-1 ) 
    {
        ocrGuidTableGet(USER_KEY_BACK((iter + 1), x, y, z), &oEvt);
        ocrEventSatisfy(oEvt, db_back_gp);
    }

    //Schedule EDT for next iteration (iter + 1)
    ocrGuid_t rankEdt_template;
    ocrEdtTemplateCreate(&rankEdt_template, resilientFunc, NUM_PARAMS, NUM_DEPS);

    //Setup params
    u64 params[NUM_PARAMS];
    for (i = 0; i < NUM_PARAMS; i++) params[i] = paramv[i];

    params[0] = iter + 1;
    ocrGuid_t *guidParams = (ocrGuid_t*)&(params[1 + NUM_DIMS]);
    guidParams[1] = depv[0].guid; //DB to destroy
    guidParams[2] = depv[1].guid; //DB to destroy
    guidParams[3] = depv[2].guid; //DB to destroy
    guidParams[4] = depv[3].guid; //DB to destroy
    guidParams[5] = depv[4].guid; //DB to destroy
    guidParams[6] = depv[5].guid; //DB to destroy
    guidParams[7] = depv[6].guid; //DB to destroy

    //Setup dependences inputs for 5-point stencil
    ocrGuid_t deps[NUM_DEPS];

    //Dependence input from current (x,y,z)
    deps[0] = db;

    //Dependence input from left (x-1, y, z), i.e. right ghost of left cell
    if (x > 0) {
        ocrGuidTableGet(USER_KEY_RIGHT((iter + 1), (x - 1), y, z), &deps[1]);
    } else {
        deps[1] = NULL_GUID;
    }

    //Dependence input from right (x+1, y, z), i.e. left ghost of right cell
    if (x < (XDIM - 1)) {
        ocrGuidTableGet(USER_KEY_LEFT((iter + 1), (x + 1), y, z), &deps[2]);
    } else {
        deps[2] = NULL_GUID;
    }

    //Dependence input from up (x, y-1, z), i.e. down ghost of up cell
    if (y > 0) {
        ocrGuidTableGet(USER_KEY_DOWN((iter + 1), x, (y - 1), z), &deps[3]);
    } else {
        deps[3] = NULL_GUID;
    }

    //Dependence input from down (x, y+1, z), i.e. up ghost of down cell
    if (y < (YDIM - 1)) {
        ocrGuidTableGet(USER_KEY_UP((iter + 1), x, (y + 1),z ), &deps[4]);
    } else {
        deps[4] = NULL_GUID;
    }

    //Dependence input from front (x, y, z-1), i.e. back ghost of front cell
    if (z > 0) {
        ocrGuidTableGet(USER_KEY_BACK((iter + 1), x, y, (z-1) ), &deps[5]);
    } else {
        deps[5] = NULL_GUID;
    }

    //Dependence input from back (x, y+1), i.e. front ghost of back cell
    if (z < (ZDIM - 1)) {
        ocrGuidTableGet(USER_KEY_FRONT((iter + 1), x, y, (z+1) ), &deps[6]);
    } else {
        deps[6] = NULL_GUID;
    }

    //Create EDT
    ocrGuid_t rankEdt, outputEvent;

    //Create EDT
    ocrGuid_t rankEdt, outputEvent;
    ocrEdtCreate(&rankEdt, rankEdt_template, NUM_PARAMS, params, NUM_DEPS, deps, EDT_PROP_RESILIENT, NULL_HINT, &outputEvent);

    //Create event for future iteration (iter + 2) and associate with user-defined key
    if ((iter + 2) <= (NUM_ITERS + 1)) {
        ocrGuid_t evt1,evt2,evt3,evt4,evt5,evt6,evt7;
        ocrEventCreate(&evt1, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);

        ocrGuidTablePut(USER_KEY_SELF((iter + 2), x, y, z), evt1);

        if ( x > 0 )
        {
            ocrEventCreate(&evt2, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            ocrGuidTablePut(USER_KEY_LEFT((iter + 2), x, y, z), evt2);
        }

        if ( x < XDIM-1 )
        {
            ocrEventCreate(&evt3, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            ocrGuidTablePut(USER_KEY_RIGHT((iter + 2), x, y, z ), evt3);
        }

        if ( y > 0 )
        {
            ocrEventCreate(&evt4, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            ocrGuidTablePut(USER_KEY_UP((iter + 2), x, y, z), evt4);
        }

        if ( y < YDIM-1 )
        {
            ocrEventCreate(&evt5, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            ocrGuidTablePut(USER_KEY_DOWN((iter + 2), x, y, z), evt5);
        }

        if ( z > 0 )
        {
            ocrEventCreate(&evt6, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            ocrGuidTablePut(USER_KEY_FRONT((iter + 2), x, y, z), evt6);
        }

        if ( z < ZDIM-1 )
        {
            ocrEventCreate(&evt7, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            ocrGuidTablePut(USER_KEY_BACK((iter + 2), x, y, z), evt7);
        }

    }
    return NULL_GUID;
}
    


//Sync EDT used to call shutdown
ocrGuid_t shutdownFunc(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 i, j;
    PRINTF("[Node %lu]: Hello from shutdownEdt\n", ocrGetLocation());
    //Cleanup past iteration events (NUM_ITERS)
    for (i = 0; i < XDIM; i++) {
        for (j = 0; j < YDIM; j++) {
            if (NUM_ITERS > 1) {
               ocrGuid_t pEvt;
               ocrGuidTableRemove(USER_KEY((NUM_ITERS + 1), i, j), &pEvt);
               ocrEventDestroy(pEvt);
            }
        }
    }
    ocrShutdown();
    return NULL_GUID;
}

//EDT to start the program
ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 i, j;
    PRINTF("[Node %lu]: Starting mainEdt\n", ocrGetLocation());

    //Create the shutdown EDT
    ocrGuid_t shutdown_template, shutdownEdt;
    ocrEdtTemplateCreate(&shutdown_template, shutdownFunc, 0, ITER_SPACE);
    ocrEdtCreate(&shutdownEdt, shutdown_template, 0, NULL, ITER_SPACE, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    //Create the rank EDT template
    ocrGuid_t rankEdt_template;
    ocrEdtTemplateCreate(&rankEdt_template, resilientFunc, NUM_PARAMS, NUM_DEPS);

    //Create the paramv
    u64 params[NUM_PARAMS];
    params[0] = 1; //start iteration at 1
    ocrGuid_t *guidParams = (ocrGuid_t*)&(params[1 + NUM_DIMS]);
    guidParams[0] = shutdownEdt;
    guidParams[1] = NULL_GUID; //DB to destroy
    guidParams[2] = NULL_GUID; //DB to destroy
    guidParams[3] = NULL_GUID; //DB to destroy
    guidParams[4] = NULL_GUID; //DB to destroy
    guidParams[5] = NULL_GUID; //DB to destroy
    guidParams[6] = NULL_GUID; //DB to destroy
    guidParams[7] = NULL_GUID; //DB to destroy

    //Create the initial set of DBs and Events per rank
    ocrGuid_t dbArray[ITER_SPACE];
    ocrGuid_t dbArray_left_gp[ITER_SPACE];
    ocrGuid_t dbArray_right_gp[ITER_SPACE];
    ocrGuid_t dbArray_up_gp[ITER_SPACE];
    ocrGuid_t dbArray_down_gp[ITER_SPACE];
    ocrGuid_t dbArray_front_gp[ITER_SPACE];
    ocrGuid_t dbArray_back_gp[ITER_SPACE];
    for (i = 0; i < XDIM; i++) {
        for (j = 0; j < YDIM; j++) {
            for (j = 0; j < ZDIM; j++) { 
                int ii;
                //Create a resilient DB for future iteration (iter + 1)
                ocrGuid_t db = NULL_GUID;
                ocrGuid_t db_left_gp = NULL_GUID;
                ocrGuid_t db_right_gp = NULL_GUID;
                ocrGuid_t db_up_gp = NULL_GUID;
                ocrGuid_t db_down_gp = NULL_GUID;
                ocrGuid_t db_front_gp = NULL_GUID;
                ocrGuid_t db_back_gp = NULL_GUID;
                void *ptr_left = NULL;
                void *ptr_right = NULL;
                void *ptr_down = NULL;
                void *ptr_up = NULL;
                void *ptr_front = NULL;
                void *ptr_back = NULL;
                void *ptr = NULL;

                // Creating the local data
                ocrDbCreate(&db,          (void**)&ptr,        sizeof(double) * LOCAL_MESH_SIZE  *LOCAL_MESH_SIZE *LOCAL_MESH_SIZE,  DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
                // Creating the left ghost point
                ocrDbCreate(&db_left_gp,  (void**)&ptr_left,   sizeof(double) * LOCAL_MESH_SIZE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
                // Creating the right ghost point
                ocrDbCreate(&db_right_gp, (void**)&ptr_right,  sizeof(double) * LOCAL_MESH_SIZE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
                // Creating the up ghost point
                ocrDbCreate(&db_up_gp,   (void**)&ptr_up,    sizeof(double) * LOCAL_MESH_SIZE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
                // Creating the down ghost point
                ocrDbCreate(&db_down_gp, (void**)&ptr_down, sizeof(double) * LOCAL_MESH_SIZE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);

                ocrDbCreate(&db_front_gp, (void**)&ptr_front, sizeof(double) * LOCAL_MESH_SIZE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);

                ocrDbCreate(&db_back_gp, (void**)&ptr_back, sizeof(double) * LOCAL_MESH_SIZE * LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
    //       PRINTF("[Node %lu]: DB created %lu from EDT (%lu, %lu)\n", ocrGetLocation(), db.guid, 0, i);

            // Initial data  (all ones)
                double* dptr = (double *)ptr;
                for( ii = 0; ii < LOCAL_MESH_SIZE*LOCAL_MESH_SIZE; ii++ )
                {
                    dptr[ii] = 1.0;
                }
                dptr = (double *)ptr_left;
                for( ii = 0; ii < LOCAL_MESH_SIZE; ii++ )
                {
                    dptr[ii] = 1.0;
                }
                dptr = (double *)ptr_right;
                for( ii = 0; ii < LOCAL_MESH_SIZE; ii++ )
                {
                    dptr[ii] = 1.0;
                }
                dptr = (double *)ptr_up;
                for( ii = 0; ii < LOCAL_MESH_SIZE; ii++ )
                {
                    dptr[ii] = 1.0;
                }
                dptr = (double *)ptr_down;
                for( ii = 0; ii < LOCAL_MESH_SIZE; ii++ )
                {
                    dptr[ii] = 1.0;
                }
                dptr = (double *)ptr_front;
                for( ii = 0; ii < LOCAL_MESH_SIZE; ii++ )
                {
                    dptr[ii] = 1.0;
                }
                dptr = (double *)ptr_back;
                for( ii = 0; ii < LOCAL_MESH_SIZE; ii++ )
                {
                    dptr[ii] = 1.0;
                }

                ocrDbRelease(db);
                ocrDbRelease(db_left_gp);
                ocrDbRelease(db_right_gp);
                ocrDbRelease(db_up_gp);
                ocrDbRelease(db_down_gp);
                ocrDbRelease(db_down_front);
                ocrDbRelease(db_down_back);
            //PRINTF("ITER INDEX %d %d\n",ITER_INDEX(i, j), ITER_SPACE);

                dbArray[ITER_INDEX(i, j, k)]          = db;
                dbArray_left_gp[ITER_INDEX(i, j, k)]  = db_left_gp;
                dbArray_right_gp[ITER_INDEX(i, j, k)] = db_right_gp;
                dbArray_up_gp[ITER_INDEX(i, j, k)]    = db_up_gp;
                dbArray_down_gp[ITER_INDEX(i, j, k)]  = db_down_gp;
                dbArray_front_gp[ITER_INDEX(i, j, k)] = db_front_gp;
                dbArray_back_gp[ITER_INDEX(i, j, k)]  = db_back_gp;

                //Create event for future iteration (iter + 2) and associate with user-defined key
                if (NUM_ITERS > 1) {
                    ocrGuid_t evt1,evt2,evt3,evt4,evt5,evt6,evt7;

                    ocrEventCreate(&evt1, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                    u64 key = USER_KEY_SELF(2, i, j, k);
                    ocrGuidTablePut(key, evt1);

                    if ( i > 0  )
                    {
                        ocrEventCreate(&evt2, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                        key = USER_KEY_LEFT(2, i, j, k);
                        ocrGuidTablePut(key, evt2);
                    }
                    if ( i < XDIM-1 ) 
                    {
                        ocrEventCreate(&evt3, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                        key = USER_KEY_RIGHT(2, i, j, k);
                        ocrGuidTablePut(key, evt3);
                    }
                    if ( j > 0  )
                    {
                        ocrEventCreate(&evt4, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                        key = USER_KEY_UP(2, i, j, k);
                        ocrGuidTablePut(key, evt4);
                    }
                    if ( j < YDIM-1 )
                    {
                        ocrEventCreate(&evt5, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                        key = USER_KEY_DOWN(2, i, j, k);
                        ocrGuidTablePut(key, evt5);
                    }
                    if ( k > 0  )
                    {
                        ocrEventCreate(&evt6, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                        key = USER_KEY_FRONT(2, i, j, k);
                        ocrGuidTablePut(key, evt6);
                    }
                    if ( k < ZDIM-1 )
                    {
                        ocrEventCreate(&evt7, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                        key = USER_KEY_BACK(2, i, j, k);
                        ocrGuidTablePut(key, evt7);
                    }

                }
            }
        }
    }

    //PRINTF("[Node %lu]: Initialized values\n", ocrGetLocation());
    //Create the initial rank EDTs (start iteration at 1)
    for (i = 0; i < XDIM; i++) {
        for (j = 0; j < YDIM; j++) {
            for (k = 0; k < ZDIM; k++) {
                params[1] = i; //x-value
                params[2] = j; //y-value
                params[3] = k; //z-value
                //7-point stencil
                ocrGuid_t deps[NUM_DEPS];
                deps[0] = dbArray[i];
                deps[1] = (i > 0) ? dbArray_right_gp[ITER_INDEX((i-1), j, k)] : NULL_GUID; // I am left neighbor's right.
                deps[2] = (i < (XDIM - 1)) ? dbArray_left_gp[ITER_INDEX((i+1), j, k)] : NULL_GUID; // I am right neighbor's left.
                deps[3] = (j > 0) ? dbArray_down_gp[ITER_INDEX(i, (j-1), k)] : NULL_GUID; // I am up neighbor's down.
                deps[4] = (j < (YDIM - 1)) ? dbArray_up_gp[ITER_INDEX(i, (j+1), k)] : NULL_GUID; // I am down neighbor's up.
                deps[5] = (k > 0) ? dbArray_back_gp[ITER_INDEX(i, j, (k-1))] : NULL_GUID; // I am up neighbor's down.
                deps[6] = (k < (ZDIM - 1)) ? dbArray_front_gp[ITER_INDEX(i, j, (k+1))] : NULL_GUID; // I am down neighbor's up.

                ocrGuid_t rankEdt, outputEvent;
                ocrEdtCreate(&rankEdt, rankEdt_template, NUM_PARAMS, params, NUM_DEPS, deps, EDT_PROP_RESILIENT, NULL_HINT, &outputEvent);
            }
        }
    }

    return NULL_GUID;
}

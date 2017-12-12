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
#define XDIM    N       //Number of tiles or blocks in X-dimension
#define ITERS  (N*N)    //Number of iterations in application
#define LOCAL_MESH_SIZE 8
#define LEFT_BC 4.0
#define RIGHT_BC 4.0

//Derived params (DO NOT CHANGE)
#define NUM_DIMS 1 //(x)
#define NUM_ITERS (ITERS + 1)
#define ITER_SPACE XDIM
#define NUM_GHOSTS 2
#define NUM_POINTS (NUM_GHOSTS + 1)
#define EVENT_SPACE_PER_ITER (ITER_SPACE * NUM_POINTS) //Iteration space including ghost points

#define USER_KEY(i, x) ((i * EVENT_SPACE_PER_ITER) + x)
#define USER_KEY_SELF(i, x)  ((i * EVENT_SPACE_PER_ITER) + (x * NUM_POINTS) + 0)
#define USER_KEY_LEFT(i, x)  ((i * EVENT_SPACE_PER_ITER) + (x * NUM_POINTS) + 1)
#define USER_KEY_RIGHT(i, x) ((i * EVENT_SPACE_PER_ITER) + (x * NUM_POINTS) + 2)

#define GUID_PARAM (sizeof(ocrGuid_t)/sizeof(u64))
#define NUM_PARAMS (1/*iter value*/ + NUM_DIMS/*coordinate value*/ + (GUID_PARAM * 4)/*shutdown guid + cleanup DBs (3 dep DBs)*/)
#define NUM_DEPS 3 /*3-point stencil*/

#define NUM_LOCAL_EDTS 4
#define INJECT_FAULT 0

//Dummy function for local computation EDT on every iteration in each rank
ocrGuid_t localFunc(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("[Node %lu]: Hello from local EDT (%lu, %lu, %lu)\n", ocrGetLocation(), paramv[0], paramv[1], paramv[2]);
    return NULL_GUID;
}

//Function for creating local computation in every iteration in each rank
static void doLocalComputation(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[], ocrGuid_t db,
                               ocrGuid_t db_left_gp, ocrGuid_t db_right_gp ) {
    u64 i;
    u64 iter = paramv[0];
    u64 x = paramv[1];

    //Create the local computation EDTs
#if 1
    ocrGuid_t localEdt_template;
    ocrEdtTemplateCreate(&localEdt_template, localFunc, 3, 0);
    u64 paramsLocal[3];
    paramsLocal[0] = iter;
    paramsLocal[1] = x;
    for (i = 0; i < NUM_LOCAL_EDTS; i++) {
        ocrGuid_t localEdt;
        paramsLocal[2] = i;
        ocrEdtCreate(&localEdt, localEdt_template, 3, paramsLocal, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
    }
#else
    double *dptr = (double *)depv[0].ptr;
    double gp_from_left;
 
    if ( depv[1] != NULL_GUID ) 
    {   
        gp_from_left = ((double *)depv[1].ptr)[0];
    } else 
    {
        gp_from_left = ((double *)depv[1].ptr)[0];

    }
    double *dptr_gp_from_right = (double *)depv[2].ptr;
    // For the first testing. Compute stencil without local EDTs
    for ( i = 0; i < LOCAL_MESH_SIZE; i++ ) 
    {
     
    }
#endif
    return;
}

//Top-level function that runs on every iteration in each rank
ocrGuid_t resilientFunc(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 i;
    u64 iter = paramv[0];
    u64 x = paramv[1];

    if (iter < NUM_ITERS) {
        PRINTF("[Node %lu]: Hello from resilient EDT (%lu, %lu)\n", ocrGetLocation(), iter, x);
    } else if (iter == NUM_ITERS) {
        PRINTF("[Node %lu]: Hello from epilogue EDT (%lu)\n", ocrGetLocation(), x);
    } else {
        PRINTF("[Node %lu]: Hello from cleanup EDT (%lu)\n", ocrGetLocation(), x);
    }

    //Cleanup past iteration (iter - 2) DB and (iter - 1) Events
    ocrGuid_t *guidParamv = (ocrGuid_t*)&(paramv[1 + NUM_DIMS]);
    ocrDbDestroy(guidParamv[1]);
    ocrDbDestroy(guidParamv[2]);
    ocrDbDestroy(guidParamv[3]);

    if (iter > 3) {
        ocrGuid_t pEvt;
        if (ocrGuidTableRemove(USER_KEY_SELF((iter - 2), x), &pEvt) == 0)
            ocrEventDestroy(pEvt);
        if( x > 0 )
        {
             if (ocrGuidTableRemove(USER_KEY_LEFT((iter - 2), x), &pEvt) == 0)
                 ocrEventDestroy(pEvt);
        }
        if( x < XDIM-1 ) {
            if (ocrGuidTableRemove(USER_KEY_RIGHT((iter - 2), x), &pEvt) == 0)
                ocrEventDestroy(pEvt);
        }
    }

    //Check for termination condition on this iteration
    if (iter == (NUM_ITERS + 1)) {
        ocrGuid_t shutdownEdt = guidParamv[0];
        ocrAddDependence(NULL_GUID, shutdownEdt, x, DB_MODE_NULL);
        return NULL_GUID;
    }

    //Verify the validity of the input DBs to this iteration (iter)
#if  0
    if ((*((u64*)depv[0].ptr) != USER_KEY(iter, x)) ||
       ((x > 0) && (*((u64*)depv[1].ptr) != USER_KEY(iter, (x - 1)))) ||
       ((x < (XDIM - 1)) && (*((u64*)depv[2].ptr) != USER_KEY(iter, (x + 1))))) {
        PRINTF("[Node %lu]: Data corruption detected in resilient EDT (%lu, %lu)\n", ocrGetLocation(), iter, x);
        ASSERT(0);
        return NULL_GUID;
    }
#endif
    double *dptr = (double *)depv[0].ptr;
    double *dptr_gp_from_left  = (double *)depv[1].ptr;
    double *dptr_gp_from_right = (double *)depv[2].ptr;

#if INJECT_FAULT
    // Fault injection
    if (iter == NUM_ITERS/2 && x == 0) {
        PRINTF("[Node %lu]: Injecting fault from resilient EDT (%lu, %lu)\n", ocrGetLocation(), iter, x);
        ocrInjectNodeFailure();
    }
#endif

    //Create event for future iteration (iter + 2) and associate with user-defined key
    if ((iter + 2) <= (NUM_ITERS + 1)) {
        ocrGuid_t evt1,evt2,evt3;
        ocrEventCreate(&evt1, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
        ocrGuidTablePut(USER_KEY_SELF((iter + 2), x), evt1);

        if ( x > 0 ) 
        {
            ocrEventCreate(&evt2, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            ocrGuidTablePut(USER_KEY_LEFT((iter + 2), x), evt2);
        }
        if ( x < XDIM-1 ) 
        {
            ocrEventCreate(&evt3, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            ocrGuidTablePut(USER_KEY_RIGHT((iter + 2), x), evt3);
        }
    }

    ocrGuid_t db = NULL_GUID;
    ocrGuid_t db_left_gp = NULL_GUID;
    ocrGuid_t db_right_gp = NULL_GUID;

    if (iter < NUM_ITERS) {
        //Create a resilient output DB of this iteration
        void *ptr = NULL;
        void *ptr_left = NULL;
        void *ptr_right = NULL;
        ocrDbCreate( &db, (void**)&ptr, sizeof(double)*LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC );
        ocrDbCreate( &db_left_gp, (void**)&ptr_left, sizeof(double), DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
        ocrDbCreate( &db_right_gp, (void**)&ptr_right, sizeof(double), DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);

        //Perform local computation for output DB of this iteration
#if 0
        doLocalComputation(paramc, paramv, depc, depv, db, db_left_gp, db_right_gp, );
#else
        double* dptr_out = (double *)ptr;
        double* dptr_left_out = (double *)ptr_left;
        double* dptr_right_out = (double *)ptr_right;
        double gp_from_left, gp_from_right;

        if ( x > 0)
        {
             gp_from_left = ((double *)depv[1].ptr)[0];
          //   PRINTF("[Node %lu]: resilient EDT (%lu, %lu) uses its only value from left %f\n", ocrGetLocation() , iter, x,gp_from_left );
        } 
        else
        {
             gp_from_left = LEFT_BC;

        }

        if ( x < XDIM-1 )
        {
             gp_from_right = ((double *)depv[2].ptr)[0];
          //   PRINTF("[Node %lu]: resilient EDT (%lu, %lu) uses its only value from right %f\n", ocrGetLocation() , iter, x, gp_from_right);
        } 
        else
        {
             gp_from_right = RIGHT_BC;
        }

        dptr_left_out[0] = dptr_out[0] = (-dptr[0] + 0.5 * (  gp_from_left + dptr[1] ))*0.2 + dptr[0];
        // For the first testing. Compute stencil without local EDTs
        for ( i = 1; i < LOCAL_MESH_SIZE-1; i++ )
        {
             dptr_out[i] = ( -dptr[i] + 0.5 * (  dptr[i-1] + dptr[i+1] ) ) * 0.2 + dptr[i];
        }
        dptr_right_out[0] =  dptr_out[LOCAL_MESH_SIZE-1] = (-dptr[LOCAL_MESH_SIZE-1] + 0.5 * (  gp_from_right + dptr[LOCAL_MESH_SIZE-2] ))* 0.2 + dptr[LOCAL_MESH_SIZE-1];
        for( i = 0; i < LOCAL_MESH_SIZE; i++ ) 
        {
           PRINTF("[Node %lu]: Value of DB[%d] is %f in resilient EDT (%lu, %lu)\n", ocrGetLocation(), i, dptr_out[i], iter, x);
        }
        // Create ghost point
#endif 
    }

    //Satisfy event to transmit DB from this iteration to next iteration (iter + 1) EDT neighbors (x-1, x+1)
    ocrGuid_t oEvt;
    ocrGuidTableGet(USER_KEY_SELF((iter + 1), x), &oEvt);
    ocrEventSatisfy(oEvt, db);
    
    //Satisfy event to transmit DB from this iteration to next iteration (iter + 1) EDT neighbor (x-1, x+1)
    if ( x > 0 ) // I have ghost point to the left
    {
        ocrGuidTableGet(USER_KEY_LEFT((iter + 1), x), &oEvt);
        ocrEventSatisfy(oEvt, db_left_gp);
    }

    if ( x < XDIM-1 ) // I have ghost point to the right
    {
        ocrGuidTableGet(USER_KEY_RIGHT((iter + 1), x), &oEvt);
        ocrEventSatisfy(oEvt, db_right_gp);
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

    //Setup dependences
    ocrGuid_t deps[NUM_DEPS];
    //Dependence input from current rank
    deps[0] = db; //We can directly use DB instead of using event

    //Dependence input from left, i.e. right ghost of left cell
    if (x > 0) {
        ocrGuidTableGet(USER_KEY_RIGHT((iter + 1), (x-1)), &deps[1]);
    } else {
        deps[1] = NULL_GUID;
    }

    //Dependence input from right, i.e. left ghost of right cell
    if (x < (XDIM - 1)) {
        ocrGuidTableGet(USER_KEY_LEFT((iter + 1), (x+1)), &deps[2]);
    } else {
        deps[2] = NULL_GUID;
    }

    //Create EDT
    ocrGuid_t rankEdt, outputEvent;
    ocrEdtCreate(&rankEdt, rankEdt_template, NUM_PARAMS, params, NUM_DEPS, deps, EDT_PROP_RESILIENT, NULL_HINT, &outputEvent);
    return NULL_GUID;
}

//Sync EDT used to call shutdown
ocrGuid_t shutdownFunc(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 i;
    PRINTF("[Node %lu]: Hello from shutdownEdt\n", ocrGetLocation());
    //Cleanup past iteration events (NUM_ITERS)
    for (i = 0; i < XDIM; i++) {
        if (NUM_ITERS > 1) {
            ocrGuid_t pEvt;
            if (ocrGuidTableRemove(USER_KEY((NUM_ITERS + 1), i), &pEvt) == 0)
                ocrEventDestroy(pEvt);
        }
    }
    ocrShutdown();
    return NULL_GUID;
}

//EDT to start the program
ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 i;
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

    //Create the initial set of DBs and Events per rank
    ocrGuid_t dbArray[ITER_SPACE];
    ocrGuid_t dbArray_left_gp[ITER_SPACE];
    ocrGuid_t dbArray_right_gp[ITER_SPACE];
    for (i = 0; i < XDIM; i++) {
        int j;
        //Create a resilient DB for future iteration (iter + 1)
        ocrGuid_t db = NULL_GUID;
        ocrGuid_t db_left_gp = NULL_GUID;
        ocrGuid_t db_right_gp = NULL_GUID;
        void *ptr_left = NULL;
        void *ptr_right = NULL;
        void *ptr = NULL;

        // Creating the local data
        ocrDbCreate(&db, (void**)&ptr, sizeof(double)*LOCAL_MESH_SIZE, DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);

        // Creating the left ghost point
        ocrDbCreate(&db_left_gp, (void**)&ptr_left, sizeof(double), DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);

        // Creating the right ghost point
        ocrDbCreate(&db_right_gp, (void**)&ptr_right, sizeof(double), DB_PROP_RESILIENT, NULL_HINT, NO_ALLOC);
    //    PRINTF("[Node %lu]: DB created %lu from EDT (%lu, %lu)\n", ocrGetLocation(), db.guid, 0, i);

        // Initial data  (all ones)
        double* dptr = (double *)ptr;
        for( j = 0; j < LOCAL_MESH_SIZE; j++ )
        {
           dptr[j] = 1.0;
        }
        dptr = (double *)ptr_left;
        *dptr = 1.0;
        dptr = (double *)ptr_right;
        *dptr = 1.0;

        ocrDbRelease(db);
        ocrDbRelease(db_left_gp);
        ocrDbRelease(db_right_gp);

        dbArray[i] = db;
        dbArray_left_gp[i] = db_left_gp;
        dbArray_right_gp[i] = db_right_gp;

        //Create event for future iteration (iter + 2) and associate with user-defined key
        if (NUM_ITERS > 1) {
            ocrGuid_t evt1,evt2,evt3;
            ocrEventCreate(&evt1, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
            u64 key = USER_KEY_SELF(2, i);
            ocrGuidTablePut(key, evt1);

            if ( i > 0  )
            {
                ocrEventCreate(&evt2, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                key = USER_KEY_LEFT(2, i);
                ocrGuidTablePut(key, evt2);
            }
            if ( i < XDIM-1 ) 
            {
                ocrEventCreate(&evt3, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG | EVT_PROP_RESILIENT);
                key = USER_KEY_RIGHT(2, i);
                ocrGuidTablePut(key, evt3);
            }
        }
    }

    //Create the initial rank EDTs (start iteration at 1)
    for (i = 0; i < XDIM; i++) {
        params[1] = i; //x-value

        ocrGuid_t deps[NUM_DEPS];
        deps[0] = dbArray[i];
        deps[1] = (i > 0) ? dbArray_right_gp[i-1] : NULL_GUID; // I am left neighbor's right.
        deps[2] = (i < (XDIM - 1)) ? dbArray_left_gp[i+1] : NULL_GUID; // I am right neighbor's left.

        ocrGuid_t rankEdt, outputEvent;
        ocrEdtCreate(&rankEdt, rankEdt_template, NUM_PARAMS, params, NUM_DEPS, deps, EDT_PROP_RESILIENT, NULL_HINT, &outputEvent);
    }

    return NULL_GUID;
}

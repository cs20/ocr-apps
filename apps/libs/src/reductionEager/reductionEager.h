/*
 Author: David S Scott
 Copyright Intel Corporation 2015

 This file is subject to the license agreement located in the file ../../../../LICENSE (apps/LICENSE)
 and cannot be distributed without it. This notice cannot be removed or modified.
*/

/*
an OCR "library" for computing global reductions using labeled GUIDs.


The library implements a reduction tree invoking the reduction operator at each stage.
Node zero returns the result by satisfying the event with a separate datablock

See README for more details

*/

#ifndef REDUCTION_H
#define REDUCTION_H
#include "ocr.h"
#include "extensions/ocr-labeling.h"

#define ARITY 2

typedef enum {
    REDUCTION_F8_ADD,
    REDUCTION_F8_MULTIPLY,
    REDUCTION_F8_MAX,
    REDUCTION_F8_MIN,
    REDUCTION_U8_ADD,
    REDUCTION_U8_MULTIPLY,
    REDUCTION_U8_MAX,
    REDUCTION_U8_MIN,
    REDUCTION_S8_MAX,
    REDUCTION_S8_MIN,
    REDUCTION_U8_BITAND,
    REDUCTION_U8_BITOR,
    REDUCTION_U8_BITXOR,
    REDUCTION_F4_ADD,
    REDUCTION_F4_MULTIPLY,
    REDUCTION_F4_MAX,
    REDUCTION_F4_MIN,
    REDUCTION_U4_ADD,
    REDUCTION_U4_MULTIPLY,
    REDUCTION_U4_MAX,
    REDUCTION_U4_MIN,
    REDUCTION_S4_MAX,
    REDUCTION_S4_MIN,
    REDUCTION_U4_BITAND,
    REDUCTION_U4_BITOR,
    REDUCTION_U4_BITXOR
    } reductionOperator_t;

typedef enum {
    REDUCE,
    ALLREDUCE,
    BROADCAST}
    reductionType;

typedef struct reductionPrivateBase {
    u64 new; //should be set to true on first call or if any parameters change
    u64 nrank;
    u64 myrank;
    u64 ndata;      //number of elements to be reduced
                    //for BROADCAST, number of BYTES being sent
    reductionType type; //one of REDUCE, ALLREDUCE, BROADCAST;
    reductionOperator_t reductionOperator; //which reduction to do (in reduction.h)
    ocrGuid_t rangeGUID; //nrank-1 labeled STICKY GUIDs (used only once to set up channels)
    ocrGuid_t returnEVT; //event to return the result, different for each rank
//used internally
    u64 size; //number of bytes being sent/received
    u64 phase;
//    phase == 0, start
//    phase == 1, i received up
//    phase == 2, i received down
    u64 toggle;
//toggle between two sets of events and datablocks
    ocrGuid_t sendUpEVT[2];
    ocrGuid_t sendUpDBK[2];
    ocrGuid_t recvUpEVT[ARITY][2];
    ocrGuid_t sendDownEVT[ARITY][2];
    ocrGuid_t sendDownDBK[ARITY][2];
    ocrGuid_t recvDownEVT[2];
    ocrGuid_t reductionTML;
    ocrGuid_t reductionSendDownAndBackTML;
    ocrGuid_t reductionSendUpTML;
    ocrHint_t myEdtAffinityHNT;
    ocrHint_t myDbkAffinityHNT;
} reductionPrivate_t;

//prototypes

u64 reductionsizeof(reductionOperator_t operator);

void reductionLaunch(reductionPrivate_t * rbPTR, ocrGuid_t reductionPrivateGUID, void * mydataPTR);

ocrGuid_t reductionEdt(u32 paramc, u64 * paramv, u32 depc, ocrEdtDep_t depv[]);
#endif

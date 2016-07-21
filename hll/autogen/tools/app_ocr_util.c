#ifndef APP_OCR_UTIL_H
#include "app_ocr_util.h"
#endif

Err_t ocrEdtXCreate(ocrEdt_t in_funcPtr,
                    u32 in_paramc, u64 * in_paramv, u32 in_depc, ocrGuid_t * in_depv,
                    u16 in_flags, ocrHint_t * in_hint,
                    ocrGuid_t * o_guid, ocrGuid_t * io_outputEvent
                   )
{
    *o_guid = NULL_GUID;
    Err_t err=0;
    while(!err){
        ocrGuid_t ta_guid = NULL_GUID;
        err = ocrEdtTemplateCreate( &ta_guid, in_funcPtr, in_paramc, in_depc); IFEB;

        err = ocrEdtCreate( o_guid, ta_guid, EDT_PARAM_DEF, in_paramv, EDT_PARAM_DEF, in_depv,
                           in_flags, in_hint, io_outputEvent); IFEB;
        err = ocrEdtTemplateDestroy( ta_guid ); IFEB;
        break;
    }
    return err;
}

Err_t ocrXHookup(ocrEventTypes_t in_eventType,
                 u16 in_eventFlags,
                 ocrGuid_t in_EDTguid,
                 u32 in_slotNumber,
                 ocrDbAccessMode_t in_accessMode,
                 ocrGuid_t in_DBKguid
                 )
{
    Err_t err=0;
    while(!err){
        ocrGuid_t eventGuid = NULL_GUID;
        err = ocrEventCreate( &eventGuid, in_eventType, in_eventFlags); IFEB;
        ocrAddDependence( eventGuid, in_EDTguid, in_slotNumber, in_accessMode); IFEB;
        ocrEventSatisfy( eventGuid, in_DBKguid); IFEB;

        break;
    }
    return err;
}








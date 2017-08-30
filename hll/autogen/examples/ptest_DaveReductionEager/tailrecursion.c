#ifndef INCLUSION_TAILRECURSION_H
#include "tailrecursion.h"
#endif

//#define TAILRECURSION_VERBOSE //Define this macro in order to get many PRINTF activated.
                              //Otherwise, to keep silent, undefine the macro.

Err_t tailRecurInitialize(long in_iteration_count, TailRecurIterate_t * io_iterate,
                          ocrGuid_t in_whereToGoWhenDone)
{
    Err_t err=0;
    while(!err){
        if( ! io_iterate) {err=__LINE__; IFEB;}
        if( in_iteration_count < 0) {err=__LINE__; IFEB;}

        io_iterate->whereToGoWhenDone = in_whereToGoWhenDone;

        //The iteration are in begin <= i < end
        io_iterate->increment = 1;
        io_iterate->begin = 0;
        io_iterate->end = in_iteration_count;

        long c = io_iterate->end - io_iterate->begin;
        //PRINTF("INFO: TAILR: Number of iteration requested= %ld\n", c);

        //current will be incremented before comparison.  See tailRecurCondition() below.
        io_iterate->current = io_iterate->begin - io_iterate->increment;

        break;  // while(!err)
    }
    return err;
}

int tailRecurCondition(TailRecurIterate_t * io_iterate) //Return 1 upon success; zero otherwise.
{
    io_iterate->current += io_iterate->increment;

    int x = 0; //By default: All done
    if(io_iterate->current < io_iterate->end){
        x = 1; //Process current iteration
    }

#   ifdef TAILRECURSION_VERBOSE
        PRINTF("TESTIO> CONDITION: Checking iteration %ld in [%ld, %ld[ by step of %ld : %s\n",
               io_iterate->current,
               io_iterate->begin,
               io_iterate->end,
               io_iterate->increment,
               ((x)?("True"):("False"))
               );
#   endif

    return x;
}

Err_t tailRecurIfThenClause(const TailRecurIterate_t * in_iterate)
{
    Err_t err=0;
    while(!err){
        if( !in_iterate ) {err=__LINE__; IFEB;}

        if(in_iterate->begin > in_iterate->current){
            err=__LINE__; IFEB;
        }

        if(in_iterate->current >= in_iterate->end ){
            err=__LINE__; IFEB;
        }

#       ifdef TAILRECURSION_VERBOSE
            PRINTF("TESTIO> IF-THEN: Performing iteration %ld in [%ld, %ld[",
                   in_iterate->current,
                   in_iterate->begin,
                   in_iterate->end
                   );
            PRINTF(" : Work is ok.\n");
#       endif
        break;  //  while(!err)
    }
    return err;
}

Err_t tailRecurElseClause(const TailRecurIterate_t * in_iterate)
{
    Err_t err=0;
    while(!err){
        if( !in_iterate ) {err=__LINE__; IFEB;}
        if(in_iterate->current != in_iterate->end ){
            err=__LINE__; IFEB;
        }

#       ifdef TAILRECURSION_VERBOSE
            PRINTF("TESTIO> ELSE: Iteration ends.\n");
#       endif
        break;
    }
    return err;
}

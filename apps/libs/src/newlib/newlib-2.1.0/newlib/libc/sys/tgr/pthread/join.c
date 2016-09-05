/* Linuxthreads - a simple clone()-based implementation of Posix        */
/* threads for Linux.                                                   */
/* Copyright (C) 1996 Xavier Leroy (Xavier.Leroy@inria.fr)              */
/*                                                                      */
/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU Library General Public License  */
/* as published by the Free Software Foundation; either version 2       */
/* of the License, or (at your option) any later version.               */
/*                                                                      */
/* This program is distributed in the hope that it will be useful,      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU Library General Public License for more details.                 */

/* Thread termination and joining */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "pthread.h"
#include "internals.h"
#include "spinlock.h"
#include "restart.h"

void pthread_exit(void * retval)
{
  __pthread_do_exit (retval, CURRENT_STACK_FRAME);
}

void __pthread_do_exit(void *retval, char *currentframe)
{
  pthread_descr self = thread_self();
  pthread_descr joining;

  /* Reset the cancellation flag to avoid looping if the cleanup handlers
     contain cancellation points */
  THREAD_SETMEM(self, p_canceled, 0);
  /* Call cleanup functions and destroy the thread-specific data */
  __pthread_perform_cleanup(currentframe);
  //__pthread_destroy_specifics();
  /* Store return value */
  __pthread_lock(THREAD_GETMEM(self, p_lock), self);
  THREAD_SETMEM(self, p_retval, retval);
  /* See whether we have to signal the death.  */
  if (THREAD_GETMEM(self, p_report_events))
    {
      /* See whether TD_DEATH is in any of the mask.  */
      int idx = __td_eventword (TD_DEATH);
      uint32_t mask = __td_eventmask (TD_DEATH);

      if ((mask & (__pthread_threads_events.event_bits[idx]
		   | THREAD_GETMEM_NC(self,
				      p_eventbuf.eventmask.event_bits[idx])))
	  != 0)
	{
	  /* Yep, we have to signal the death.  */
	  THREAD_SETMEM(self, p_eventbuf.eventnum, TD_DEATH);
	  THREAD_SETMEM(self, p_eventbuf.eventdata, self);
	  __pthread_last_event = self;

	  /* Now call the function to signal the event.  */
	  __linuxthreads_death_event();
	}
    }
  /* Say that we've terminated */
  THREAD_SETMEM(self, p_terminated, 1);

  /* See if someone is joining on us */
  joining = THREAD_GETMEM(self, p_joining);

  __pthread_unlock(THREAD_GETMEM(self, p_lock));

  /* Do the old manager routine of removing ourselves */
  __pthread_exited();

  /* Restart joining thread if any */
  if (joining != NULL) restart(joining);
  /* If this is the initial thread, block until all threads have terminated.
     If another thread calls exit, we'll be terminated from our signal
     handler. */
  if (self == __pthread_main_thread) {
    tgr_waitall();
    /* Main thread flushes stdio streams and runs atexit functions.
       It also calls a handler within LinuxThreads which sends a process exit
       request to the thread manager. */
    exit(0);
  }

  /* Threads other than the main one  terminate without flushing stdio streams
     or running atexit functions. */
  _exit(0);
}

/* Function called by pthread_cancel to remove the thread from
   waiting on a condition variable queue. */

static int join_extricate_func(void *obj, pthread_descr th)
{
  volatile pthread_descr self = thread_self();
  pthread_handle handle = obj;
  pthread_descr jo;
  int did_remove = 0;

  __pthread_lock(&handle->h_lock, self);
  jo = handle->h_descr;
  did_remove = jo->p_joining != NULL;
  jo->p_joining = NULL;
  __pthread_unlock(&handle->h_lock);

  return did_remove;
}

int pthread_join(pthread_t thread_id, void ** thread_return)
{
  volatile pthread_descr self = thread_self();
  pthread_handle handle = thread_handle(thread_id);
  pthread_descr th;
  pthread_extricate_if extr;
  int already_canceled = 0;

  /* Set up extrication interface */
  extr.pu_object = handle;
  extr.pu_extricate_func = join_extricate_func;

  __pthread_lock(&handle->h_lock, self);
  if (nonexisting_handle(handle, thread_id)) {
    __pthread_unlock(&handle->h_lock);
    return ESRCH;
  }
  th = handle->h_descr;
  if (th == self) {
    __pthread_unlock(&handle->h_lock);
    return EDEADLK;
  }
  /* If detached or already joined, error */
  if (th->p_detached || th->p_joining != NULL) {
    __pthread_unlock(&handle->h_lock);
    return EINVAL;
  }
  /* If not terminated yet, suspend ourselves. */
  if (! th->p_terminated) {
    /* Register extrication interface */
    __pthread_set_own_extricate_if(self, &extr);
    if (!(THREAD_GETMEM(self, p_canceled)
	&& THREAD_GETMEM(self, p_cancelstate) == PTHREAD_CANCEL_ENABLE))
      th->p_joining = self;
    else
      already_canceled = 1;
    __pthread_unlock(&handle->h_lock);

    if (already_canceled) {
      __pthread_set_own_extricate_if(self, 0);
      __pthread_do_exit(PTHREAD_CANCELED, CURRENT_STACK_FRAME);
    }

    suspend();
    /* Deregister extrication interface */
    __pthread_set_own_extricate_if(self, 0);

    /* This is a cancellation point */
    if (THREAD_GETMEM(self, p_woken_by_cancel)
	&& THREAD_GETMEM(self, p_cancelstate) == PTHREAD_CANCEL_ENABLE) {
      THREAD_SETMEM(self, p_woken_by_cancel, 0);
      __pthread_do_exit(PTHREAD_CANCELED, CURRENT_STACK_FRAME);
    }
    __pthread_lock(&handle->h_lock, self);
  }
  /* Get return value */
  if (thread_return != NULL) *thread_return = th->p_retval;
  __pthread_unlock(&handle->h_lock);
  __pthread_handle_free(thread_id);
  return 0;
}

int pthread_detach(pthread_t thread_id)
{
  int terminated;
  pthread_handle handle = thread_handle(thread_id);
  pthread_descr th;

  __pthread_lock(&handle->h_lock, NULL);
  if (nonexisting_handle(handle, thread_id)) {
    __pthread_unlock(&handle->h_lock);
    return ESRCH;
  }
  th = handle->h_descr;
  /* If already detached, error */
  if (th->p_detached) {
    __pthread_unlock(&handle->h_lock);
    return EINVAL;
  }
  /* If already joining, don't do anything. */
  if (th->p_joining != NULL) {
    __pthread_unlock(&handle->h_lock);
    return 0;
  }
  /* Mark as detached */
  th->p_detached = 1;
  terminated = th->p_terminated;
  __pthread_unlock(&handle->h_lock);
  __pthread_handle_free(thread_id);
  return 0;
}

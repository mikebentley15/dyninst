/*
 * Copyright (c) 1996 Barton P. Miller
 *
 * We provide the Paradyn Parallel Performance Tools (below
 * described as Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * This license is for research uses.  For such uses, there is no
 * charge. We define "research use" to mean you may freely use it
 * inside your organization for whatever purposes you see fit. But you
 * may not re-distribute Paradyn or parts of Paradyn, in any form
 * source or binary (including derivatives), electronic or otherwise,
 * to any other organization or entity without our permission.
 *
 * (for other uses, please contact us at paradyn@cs.wisc.edu)
 *
 * All warranties, including without limitation, any warranty of
 * merchantability or fitness for a particular purpose, are hereby
 * excluded.
 *
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 *
 * Even if advised of the possibility of such damages, under no
 * circumstances shall we (or any other person or entity with
 * proprietary rights in the software licensed hereunder) be liable
 * to you or any third party for direct, indirect, or consequential
 * damages of any character regardless of type of action, including,
 * without limitation, loss of profits, loss of use, loss of good
 * will, or computer failure or malfunction.  You agree to indemnify
 * us (and any other person or entity with proprietary rights in the
 * software licensed hereunder) for any and all liability it may
 * incur to third parties resulting from your use of Paradyn.
 */


#include "RTthread.h"
#include <assert.h>
#include <stdio.h>

void DEBUG_VIRTUAL_TIMER_START(tTimer *timer, int context) {
  switch(context) {
  case THREAD_UPDATE: {
    fprintf(stderr, "THREAD_UPDATE--->start virtual timer(%d), lwp_id=%d\n", timer-(RTsharedData->virtualTimers), timer->lwp_id);
    break ;
  }
  case THREAD_CREATE: {
    fprintf(stderr, "THREAD_CREATE--->start virtual timer(%d), lwp_id=%d\n", timer-(RTsharedData->virtualTimers), timer->lwp_id);
    break ;
  }
  case THREAD_DETECT: {
    fprintf(stderr, "THREAD_DETECT--->start virtual timer(%d), lwp_id=%d\n", timer-(RTsharedData->virtualTimers), timer->lwp_id);
    break ;
  }
  case VIRTUAL_TIMER_CREATE: {
    fprintf(stderr, "VIRTUAL_TIMER_CREATE--->start virtual timer(%d), lwp_id=%d\n", timer-(RTsharedData->virtualTimers), timer->lwp_id);
    break ;
  }
  case VIRTUAL_TIMER_START: {
    fprintf(stderr, "VIRTUAL_TIMER_START--->start virtual timer(%d), lwp_id=%d\n", timer-(RTsharedData->virtualTimers), timer->lwp_id);
    break ;
  }
  case THREAD_TIMER_START: {
    fprintf(stderr, "THREAD_TIMER_START--->start virtual timer(%d), lwp_id=%d\n", timer-(RTsharedData->virtualTimers), timer->lwp_id);
    break ;
  }
  }
}

/* Start the virtual timer running on the known lwp */

void _VirtualTimerStart(tTimer *timer, int context){
  assert(timer->protector1 == timer->protector2);
  timer->protector1++;

  if (timer->vtimer && timer->counter ==0) {
    fprintf(stderr, "Calling _LWP with lwp_id %d\n", timer->lwp_id);
    timer->start = DYNINSTgetCPUtime_LWP(timer->lwp_id);
  }
  timer->counter++;

  timer->protector2++;
  assert(timer->protector1 == timer->protector2);
}

void _VirtualTimerStop(tTimer* timer) {
  if (!timer) { return; }
  
  assert(timer->protector1 == timer->protector2);
  timer->protector1++;
  
  if (timer->counter == 1) {
    rawTime64 now;
    now = DYNINSTgetCPUtime_LWP(timer->lwp_id);
    if (now < timer->start) {
      fprintf(stderr, "WARNING: rtinst, cpu timer rollback. start=%lld, now=%lld, previous_lwp=%d\n",
	      timer->start,now,timer->lwp_id);
    }
    else
      timer->total += (now - timer->start);
  }
  timer->counter--;
  timer->protector2++;
  assert(timer->protector1 == timer->protector2);
}


/* called when the virtual timer is reused by another thread */
void DYNINST_VirtualTimerDestroy(tTimer* vt) {
  if (vt->vtimer)
    memset((char*) vt, '\0', sizeof(tTimer)) ;
}

/* getThreadCPUTime */
/* We're basically "sampling" the per-lwp virtual timer. So
   use the works: check p1 == p2, check rollbacks, the lot. */
rawTime64 getThreadCPUTime(tTimer *vt, int *valid) {
  volatile int protector1, protector2;
  volatile rawTime64 total, start ;
  volatile int    count, vt_lwp_id;
  tTimer *vtimer ;

  if (!vt->vtimer)
    return 0 ;

  protector2 = vt->protector2 ;
  count = vt->counter;
  total = vt->total;
  vt_lwp_id = vt->lwp_id;
  start = vt->start ;
  protector1 = vt->protector1 ;
  
  if (protector1 != protector2) {
    *valid = 0 ; /* not a valid value */
    return 0 ;
  }
  
  *valid = 1 ;
  if (count > 0) {
    /* Timer is running, so get intermediate value */
    unsigned long long now;
    int lwp_id = thread_self() ;
    
    /* always read the timer of the lwp of the vtimer*/
    /* Since this could be called by inferior RPC */
    now=DYNINSTgetCPUtime_LWP(vt_lwp_id);
    if (now >= start) {
      return total + (now-start) ;
    } else  {
      fprintf(stderr, "Timer goes back in getThreadCPUTime, lwp_id=%d, thread_self=%d ...\n",
	      vt_lwp_id, lwp_id) ;
      return total ;
    }
  } else {
    return total ;
  }
}

#define PRINTOUT_TIMER(t)   fprintf(stderr, "Timer (%x): total %lld, start %lld, counter %d, lwp %d, in_RPC %d, vtimer 0x%x, p1 %d, p2 %d\n", (unsigned) t, t->total, t->start, t->counter, t->lwp_id, t->in_inferiorRPC, (unsigned) t->vtimer, t->protector1, t->protector2);


/*
  //THREAD TIMER   
*/
void DYNINSTstartThreadTimer(tTimer* timer)
{
  rawTime64 start, old_start ;
  tTimer* vt ;
  int lwp_id ;
  int valid = 0;

  fprintf(stderr, "Entry (start) ...\n");
  PRINTOUT_TIMER(timer);

  assert(timer);
  timer->in_inferiorRPC=1;
  
  assert(timer->protector1 == timer->protector2);
  timer->protector1++;

  if (timer->counter == 0) {
    if (!(timer->vtimer)) {
      unsigned pos;
      pos = DYNINSTthreadPosFAST() ; /* in mini, so could use POS (maybe) */
      timer->vtimer = (struct tTimerRec *)&(RTsharedData->virtualTimers[pos]);
      fprintf(stderr, "POS %d, using timer 0x%x\n", pos, timer->vtimer);

      /* start the per-thread virtual timer if needed */
      if  (timer->vtimer->vtimer == NULL) {
	fprintf(stderr, "  VTIMER WAS NULL (%d)\n", timer->protector1);
	VIRTUAL_TIMER_MARK_CREATION(vt) ;
      }
    }
    /* a Hack, for times when we don't catch thread creation (?) */
    if ( timer->vtimer->counter == 0 ) {
      VIRTUAL_TIMER_MARK_LWPID(vt, lwp_id) ;
      _VirtualTimerStart(vt, THREAD_TIMER_START);
    }
    /* end of a hack */
    /* We sample the virtual timer, so we may need to retry */
    while (!valid) {
      start = getThreadCPUTime((tTimer *)timer->vtimer, &valid);
    }
    timer->start = start;
  }
  timer->counter++;
  timer->protector2++;

  fprintf(stderr, "Exit (start)...\n");
  PRINTOUT_TIMER(timer);

  assert(timer->protector1 == timer->protector2);
}

void DYNINSTstopThreadTimer(tTimer* timer)
{
  fprintf(stderr, "Entry (stop)...\n");
  PRINTOUT_TIMER(timer);

  assert(timer);

  assert(timer->protector1 == timer->protector2);
  timer->protector1++;

  if (timer->counter == 1) {
    int valid = 0;
    rawTime64 now;
    while (!valid) 
      now = getThreadCPUTime((tTimer *)timer->vtimer, &valid);
    if (now < timer->start) {
      assert(0 && "Rollback in DYNINSTstopThreadTimer");
    }
    timer->total += (now - timer->start);
  }
  timer->counter--;
  timer->protector2++;

  fprintf(stderr, "Exit (stop)...\n");
  PRINTOUT_TIMER(timer);
  assert(timer->protector1 == timer->protector2);
}


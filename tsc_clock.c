/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <sched.h>
#include <x86intrin.h>

#define USEC_PER_SEC 1000000
#define NSEC_PER_SEC 1000000000

static volatile bool force_quit;
int max, min, ns_min, ns_max;
uint64_t cnt, sum, ns_sum;

static inline uint64_t rdtsc_b()
{
    union{
        uint64_t tsc;
        __extension__ struct {
            uint32_t lo;
            uint32_t hi;
        };
    }tsc;
    /*
    //asm volatile ("cpuid\t\n");
    asm volatile ("rdtsc;"
    "shl $32,%%rdx;"
    "or %%rdx,%%rax;": "=a" (tsc.tsc) :: "%rcx", "%rdx", "memory");
    */
    asm volatile ("CPUID\n\t"
            "RDTSC\n\t"
            "mov %%edx, %0\n\t"
            "mov %%eax, %1\n\t": "=r" (tsc.hi), "=r" (tsc.lo)::
            "%rax", "%rbx", "%rcx", "%rdx");
    return tsc.tsc;
}

static inline uint64_t rdtsc_e()
{
    union{
        uint64_t tsc;
        __extension__ struct {
            uint32_t lo;
            uint32_t hi;
        };
    }tsc;
    /*
       asm volatile ("rdtscp;"
       "shl $32,%%rdx;"
       "or %%rdx,%%rax;"
       : "=a" (tsc.tsc) :: "%rcx", "%rdx" );
       asm volatile ("cpuid");
       */
    asm volatile("RDTSCP\n\t"
            "mov %%edx, %0\n\t"
            "mov %%eax, %1\n\t"
            "CPUID\n\t": "=r" (tsc.hi), "=r" (tsc.lo)::
            "%rax", "%rbx", "%rcx", "%rdx");

    return tsc.tsc;
}


    int
main(int argc, char **argv)
{
    int ret, elem_size;
    uint64_t prev_tsc, cur_tsc;
    int64_t delta;

    int i, j;

    char *p;
    force_quit = false;

    struct sched_param schedp;
    memset(&schedp, 0, sizeof(schedp));
    schedp.sched_priority = 95;

    if (sched_setscheduler(0, SCHED_FIFO, &schedp))
        printf("failed to set policy and priority\n");


    int nsec_per_msec = 1000000;
    struct timespec prev_ts, cur_ts, intv;
    intv.tv_sec = 0;
    intv.tv_nsec= nsec_per_msec; // 1ms
    bool logonce = true;
    while (!force_quit)
    {
        if (cnt % 10000000 == 1)
        {
            printf ("rdtsc  (cycles)  -  min: %d,  max: %d,  avg: %d\n"
                    "clock_gettime(ns)- cmin: %d, cmax: %d, cavg: %" PRIu64 "\n", 
                    min, max, sum/cnt,
                    ns_min, ns_max, ns_sum/cnt);
        }

        delta = 0;
        clock_gettime(CLOCK_MONOTONIC, &prev_ts);
        clock_gettime(CLOCK_MONOTONIC, &cur_ts);
        if (cur_ts.tv_sec < prev_ts.tv_sec || cur_ts.tv_sec > prev_ts.tv_sec+1)
            printf ("Error: current time - %d, %d, target - %d, %d\n", cur_ts.tv_sec, cur_ts.tv_nsec, prev_ts.tv_sec, prev_ts.tv_nsec);

        delta = NSEC_PER_SEC * ((int64_t)((int)cur_ts.tv_sec - (int)prev_ts.tv_sec)) + ((int)cur_ts.tv_nsec - (int)prev_ts.tv_nsec);
        if (delta > 10000 || delta < 0)
        {
            printf ("cur.sec: %d, cur.nsec: %d, pre.sec: %d, pre.nsec: %d", cur_ts.tv_sec, cur_ts.tv_nsec, prev_ts.tv_sec, prev_ts.tv_nsec);

            force_quit = true;
        }
        if (ns_min == 0 || ns_min > delta)
            ns_min = delta;
        if (ns_max < delta)
            ns_max = delta;
        ns_sum += delta;
/*
	   cur_ts.tv_nsec += intv.tv_nsec;
	   while (cur_ts.tv_nsec >= NSEC_PER_SEC)
	   {
	   cur_ts.tv_nsec -= NSEC_PER_SEC;
	   cur_ts.tv_sec++;
	   }
	   clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &cur_ts, NULL);

	 */

        prev_tsc = __rdtsc();//rdtsc_b(); //__rdtsc();
        cur_tsc = __rdtsc();//rdtsc_e();
        delta=cur_tsc - prev_tsc;

        if (min == 0 || min > delta)
            min = delta;
        if (max < delta)
            max = delta;
        sum +=delta;
        cnt++;
    }
    printf("Bye...\n");
    return ret;
}


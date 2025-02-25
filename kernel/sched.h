#ifndef _SCHED_H_
#define _SCHED_H_

#include "process.h"

//length of a time slice, in number of ticks
#define TIME_SLICE_LEN  2

typedef struct sem{
    int n;
    process* wait[5];
}Sem;

extern Sem sem_list[];
extern int cnt;

void insert_to_ready_queue( process* proc );
void insert_to_block_queue( process* proc );
void schedule();
void schedule_block(process* proc);

#endif

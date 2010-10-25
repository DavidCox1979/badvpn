/**
 * @file fairqueue_test.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include <stdio.h>

#include <misc/debug.h>
#include <system/BReactor.h>
#include <system/BLog.h>
#include <system/BTime.h>
#include <flow/PacketPassFairQueue.h>
#include <examples/FastPacketSource.h>
#include <examples/TimerPacketSink.h>

#define OUTPUT_INTERVAL 0
#define REMOVE_INTERVAL 1
#define NUM_INPUTS 3

BReactor reactor;
TimerPacketSink sink;
PacketPassFairQueue fq;
PacketPassFairQueueFlow flows[NUM_INPUTS];
FastPacketSource sources[NUM_INPUTS];
char *data[] = {"0 data", "1 datadatadata", "2 datadatadatadatadata"};
BTimer timer;
int current_cancel;

static void init_input (int i)
{
    PacketPassFairQueueFlow_Init(&flows[i], &fq);
    FastPacketSource_Init(&sources[i], PacketPassFairQueueFlow_GetInput(&flows[i]), (uint8_t *)data[i], strlen(data[i]), BReactor_PendingGroup(&reactor));
}

static void free_input (int i)
{
    FastPacketSource_Free(&sources[i]);
    PacketPassFairQueueFlow_Free(&flows[i]);
}

static void timer_handler (void *user)
{
    printf("removing %d\n", current_cancel);
    
    // release flow
    if (PacketPassFairQueueFlow_IsBusy(&flows[current_cancel])) {
        PacketPassFairQueueFlow_Release(&flows[current_cancel]);
    }
    
    // remove flow
    free_input(current_cancel);
    
    // init flow
    init_input(current_cancel);
    
    // increment cancel
    current_cancel = (current_cancel + 1) % NUM_INPUTS;
    
    // reset timer
    BReactor_SetTimer(&reactor, &timer);
}

int main ()
{
    // initialize logging
    BLog_InitStdout();
    
    // init time
    BTime_Init();
    
    // initialize reactor
    if (!BReactor_Init(&reactor)) {
        DEBUG("BReactor_Init failed");
        return 1;
    }
    
    // initialize sink
    TimerPacketSink_Init(&sink, &reactor, 500, OUTPUT_INTERVAL);
    
    // initialize queue
    PacketPassFairQueue_Init(&fq, TimerPacketSink_GetInput(&sink), BReactor_PendingGroup(&reactor));
    PacketPassFairQueue_EnableCancel(&fq);
    
    // initialize inputs
    for (int i = 0; i < NUM_INPUTS; i++) {
        init_input(i);
    }
    
    // init cancel timer
    BTimer_Init(&timer, REMOVE_INTERVAL, timer_handler, NULL);
    BReactor_SetTimer(&reactor, &timer);
    
    // init cancel counter
    current_cancel = 0;
    
    // run reactor
    int ret = BReactor_Exec(&reactor);
    BReactor_Free(&reactor);
    return ret;
}

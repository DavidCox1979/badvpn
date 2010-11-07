/**
 * @file PRStreamSink.c
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

#include <prerror.h>

#include <misc/debug.h>

#include <nspr_support/PRStreamSink.h>

static void report_error (PRStreamSink *s, int error)
{
    DEAD_ENTER(s->dead)
    FlowErrorReporter_ReportError(&s->rep, &error);
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(s->dead);
}

static void try_send (PRStreamSink *s)
{
    ASSERT(s->in_len > 0)
    
    int res = PR_Write(s->bprfd->prfd, s->in, s->in_len);
    if (res < 0 && PR_GetError() == PR_WOULD_BLOCK_ERROR) {
        // wait for socket in prfd_handler
        BPRFileDesc_EnableEvent(s->bprfd, PR_POLL_WRITE);
        return;
    }
    
    if (res < 0) {
        report_error(s, PRSTREAMSINK_ERROR_NSPR);
        return;
    }
    
    ASSERT(res > 0)
    ASSERT(res <= s->in_len)
    
    // finish packet
    s->in_len = -1;
    StreamPassInterface_Done(&s->input, res);
}

static void input_handler_send (PRStreamSink *s, uint8_t *data, int data_len)
{
    ASSERT(data_len > 0)
    ASSERT(s->in_len == -1)
    DebugObject_Access(&s->d_obj);
    
    // set packet
    s->in_len = data_len;
    s->in = data;
    
    try_send(s);
    return;
}

static void prfd_handler (PRStreamSink *s, PRInt16 event)
{
    ASSERT(s->in_len > 0)
    ASSERT(event == PR_POLL_WRITE)
    DebugObject_Access(&s->d_obj);
    
    try_send(s);
    return;
}

void PRStreamSink_Init (PRStreamSink *s, FlowErrorReporter rep, BPRFileDesc *bprfd, BPendingGroup *pg)
{
    // init arguments
    s->rep = rep;
    s->bprfd = bprfd;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BPRFileDesc_AddEventHandler(s->bprfd, PR_POLL_WRITE, (BPRFileDesc_handler)prfd_handler, s);
    
    // init input
    StreamPassInterface_Init(&s->input, (StreamPassInterface_handler_send)input_handler_send, s, pg);
    
    // have no input packet
    s->in_len = -1;
    
    DebugObject_Init(&s->d_obj);
}

void PRStreamSink_Free (PRStreamSink *s)
{
    DebugObject_Free(&s->d_obj);
    
    // free input
    StreamPassInterface_Free(&s->input);
    
    // remove socket event handler
    BPRFileDesc_RemoveEventHandler(s->bprfd, PR_POLL_WRITE);
    
    // free dead var
    DEAD_KILL(s->dead);
}

StreamPassInterface * PRStreamSink_GetInput (PRStreamSink *s)
{
    DebugObject_Access(&s->d_obj);
    
    return &s->input;
}

/**
 * @file StreamSocketSource.c
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

#include <misc/debug.h>

#include <flow/StreamSocketSource.h>

static void report_error (StreamSocketSource *s, int error)
{
    DEAD_ENTER(s->dead)
    FlowErrorReporter_ReportError(&s->rep, &error);
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(s->dead);
}

static void try_recv (StreamSocketSource *s)
{
    ASSERT(s->out_avail > 0)
    
    int res = BSocket_Recv(s->bsock, s->out, s->out_avail);
    if (res < 0 && BSocket_GetError(s->bsock) == BSOCKET_ERROR_LATER) {
        // wait for socket in socket_handler
        BSocket_EnableEvent(s->bsock, BSOCKET_READ);
        return;
    }
    
    if (res < 0) {
        report_error(s, STREAMSOCKETSOURCE_ERROR_BSOCKET);
        return;
    }
    
    if (res == 0) {
        report_error(s, STREAMSOCKETSOURCE_ERROR_CLOSED);
        return;
    }
    
    ASSERT(res > 0)
    ASSERT(res <= s->out_avail)
    
    // finish packet
    s->out_avail = -1;
    StreamRecvInterface_Done(&s->output, res);
}

static void output_handler_recv (StreamSocketSource *s, uint8_t *data, int data_avail)
{
    ASSERT(data_avail > 0)
    ASSERT(s->out_avail == -1)
    DebugObject_Access(&s->d_obj);
    
    // set packet
    s->out_avail = data_avail;
    s->out = data;
    
    try_recv(s);
    return;
}

static void socket_handler (StreamSocketSource *s, int event)
{
    ASSERT(s->out_avail > 0)
    ASSERT(event == BSOCKET_READ)
    DebugObject_Access(&s->d_obj);
    
    BSocket_DisableEvent(s->bsock, BSOCKET_READ);
    
    try_recv(s);
    return;
}

void StreamSocketSource_Init (StreamSocketSource *s, FlowErrorReporter rep, BSocket *bsock, BPendingGroup *pg)
{
    // init arguments
    s->rep = rep;
    s->bsock = bsock;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BSocket_AddEventHandler(s->bsock, BSOCKET_READ, (BSocket_handler)socket_handler, s);
    
    // init output
    StreamRecvInterface_Init(&s->output, (StreamRecvInterface_handler_recv)output_handler_recv, s, pg);
    
    // have no output packet
    s->out_avail = -1;
    
    DebugObject_Init(&s->d_obj);
}

void StreamSocketSource_Free (StreamSocketSource *s)
{
    DebugObject_Free(&s->d_obj);
    
    // free output
    StreamRecvInterface_Free(&s->output);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_READ);
    
    // free dead var
    DEAD_KILL(s->dead);
}

StreamRecvInterface * StreamSocketSource_GetOutput (StreamSocketSource *s)
{
    DebugObject_Access(&s->d_obj);
    
    return &s->output;
}

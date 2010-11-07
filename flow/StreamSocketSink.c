/**
 * @file StreamSocketSink.c
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

#include <flow/StreamSocketSink.h>

static void report_error (StreamSocketSink *s, int error)
{
    DEAD_ENTER(s->dead)
    FlowErrorReporter_ReportError(&s->rep, &error);
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(s->dead);
}

static void try_send (StreamSocketSink *s)
{
    ASSERT(s->in_len > 0)
    
    int res = BSocket_Send(s->bsock, s->in, s->in_len);
    if (res < 0 && BSocket_GetError(s->bsock) == BSOCKET_ERROR_LATER) {
        // wait for socket in socket_handler
        BSocket_EnableEvent(s->bsock, BSOCKET_WRITE);
        return;
    }
    
    if (res < 0) {
        report_error(s, STREAMSOCKETSINK_ERROR_BSOCKET);
        return;
    }
    
    ASSERT(res > 0)
    ASSERT(res <= s->in_len)
    
    // finish packet
    s->in_len = -1;
    StreamPassInterface_Done(&s->input, res);
}

static void input_handler_send (StreamSocketSink *s, uint8_t *data, int data_len)
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

static void socket_handler (StreamSocketSink *s, int event)
{
    ASSERT(s->in_len > 0)
    ASSERT(event == BSOCKET_WRITE)
    DebugObject_Access(&s->d_obj);
    
    BSocket_DisableEvent(s->bsock, BSOCKET_WRITE);
    
    try_send(s);
    return;
}

void StreamSocketSink_Init (StreamSocketSink *s, FlowErrorReporter rep, BSocket *bsock, BPendingGroup *pg)
{
    // init arguments
    s->rep = rep;
    s->bsock = bsock;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BSocket_AddEventHandler(s->bsock, BSOCKET_WRITE, (BSocket_handler)socket_handler, s);
    
    // init input
    StreamPassInterface_Init(&s->input, (StreamPassInterface_handler_send)input_handler_send, s, pg);
    
    // have no input packet
    s->in_len = -1;
    
    DebugObject_Init(&s->d_obj);
}

void StreamSocketSink_Free (StreamSocketSink *s)
{
    DebugObject_Free(&s->d_obj);
    
    // free input
    StreamPassInterface_Free(&s->input);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_WRITE);
    
    // free dead var
    DEAD_KILL(s->dead);
}

StreamPassInterface * StreamSocketSink_GetInput (StreamSocketSink *s)
{
    DebugObject_Access(&s->d_obj);
    
    return &s->input;
}

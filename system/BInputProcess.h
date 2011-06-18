/**
 * @file BInputProcess.h
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

#ifndef BADVPN_BINPUTPROCESS_H
#define BADVPN_BINPUTPROCESS_H

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <system/BConnection.h>
#include <system/BProcess.h>

typedef void (*BInputProcess_handler_terminated) (void *user, int normally, uint8_t normally_exit_status);
typedef void (*BInputProcess_handler_closed) (void *user, int is_error);

typedef struct {
    BReactor *reactor;
    BProcessManager *manager;
    void *user;
    BInputProcess_handler_terminated handler_terminated;
    BInputProcess_handler_closed handler_closed;
    int pipe_write_fd;
    int started;
    int have_process;
    BProcess process;
    int pipe_fd;
    BConnection pipe_con;
    DebugObject d_obj;
} BInputProcess;

int BInputProcess_Init (BInputProcess *o, BReactor *reactor, BProcessManager *manager, void *user,
                        BInputProcess_handler_terminated handler_terminated,
                        BInputProcess_handler_closed handler_closed) WARN_UNUSED;
void BInputProcess_Free (BInputProcess *o);
int BInputProcess_Start (BInputProcess *o, const char *file, char *const argv[], const char *username);
int BInputProcess_Terminate (BInputProcess *o);
int BInputProcess_Kill (BInputProcess *o);
StreamRecvInterface * BInputProcess_GetInput (BInputProcess *o);

#endif

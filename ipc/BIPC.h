/**
 * @file BIPC.h
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
 * 
 * @section DESCRIPTION
 * 
 * An abstraction of a reliable, sequenced, message-oriented two-way IPC.
 * Uses unix sockets on *nix systems, and named pipes on Windows.
 */

#ifndef BADVPN_IPC_BIPC_H
#define BADVPN_IPC_BIPC_H

#include <protocol/packetproto.h>
#include <misc/debug.h>
#include <misc/debugerror.h>
#include <system/BSocket.h>
#include <system/DebugObject.h>
#include <flow/StreamSocketSink.h>
#include <flow/StreamSocketSource.h>
#include <flow/PacketProtoEncoder.h>
#include <flow/PacketProtoDecoder.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/PacketCopier.h>
#include <flow/PacketStreamSender.h>
#include <ipc/BIPCServer.h>

/**
 * Handler function called when an error occurs.
 * The object must be freed from within this handler.
 * 
 * @param user as in {@link BIPC_InitConnect}
 */
typedef void (*BIPC_handler) (void *user);

/**
 * An abstraction of a reliable, sequenced, message-oriented two-way IPC.
 */
typedef struct {
    BSocket sock;
    FlowErrorDomain domain;
    BIPC_handler handler;
    void *user;
    
    // sending
    PacketCopier send_copier;
    PacketProtoEncoder send_encoder;
    SinglePacketBuffer send_buf;
    PacketStreamSender send_pss;
    StreamSocketSink send_sink;
    
    // receiving
    StreamSocketSource recv_source;
    PacketProtoDecoder recv_decoder;
    
    DebugObject d_obj;
    DebugError d_err;
} BIPC;

/**
 * Initializes the object by connecting to an IPC server.
 * 
 * @param o the object
 * @param path path of the IPC object. On *nix path of the unix socket, on Windows
 *             path of the named pipe.
 * @param send_mtu maximum packet size for sending. Must be >=0 and <=PACKETPROTO_MAXPAYLOAD.
 * @param recv_mtu maximum packet size for receiving. Must be >=0 and <=PACKETPROTO_MAXPAYLOAD.
 * @param handler handler function called when an error occurs
 * @param user value to pass to handler function
 * @param reactor reactor we live in
 * @return 1 on success, 0 on failure
 */
int BIPC_InitConnect (BIPC *o, const char *path, int send_mtu, PacketPassInterface *recv_if, BIPC_handler handler, void *user, BReactor *reactor) WARN_UNUSED;

/**
 * Initializes the object by acception a connection on an IPC server.
 * 
 * @param o the object
 * @param server IPC server to accept a connection on
 * @param send_mtu maximum packet size for sending. Must be >=0.
 * @param recv_mtu maximum packet size for receiving. Must be >=0.
 * @param handler handler function called when an error occurs
 * @param user value to pass to handler function
 * @param reactor reactor we live in
 * @return 1 on success, 0 on failure
 */
int BIPC_InitAccept (BIPC *o, BIPCServer *server, int send_mtu, PacketPassInterface *recv_if, BIPC_handler handler, void *user, BReactor *reactor) WARN_UNUSED;

/**
 * Frees the object.
 * 
 * @param o the object
 */
void BIPC_Free (BIPC *o);

/**
 * Returns the interface for sending.
 * The MTU of the interface will be as send_mtu in {@link BIPC_InitConnect}.
 * 
 * @param o the object
 * @return interface for sending
 */
PacketPassInterface * BIPC_GetSendInterface (BIPC *o);

#endif

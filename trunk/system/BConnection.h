/**
 * @file BConnection.h
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

#ifndef BADVPN_SYSTEM_BCONNECTION
#define BADVPN_SYSTEM_BCONNECTION

#include <misc/debug.h>
#include <flow/StreamPassInterface.h>
#include <flow/StreamRecvInterface.h>
#include <system/BAddr.h>
#include <system/BReactor.h>
#include <system/BNetwork.h>



/**
 * Checks if the given address is supported by {@link BConnection} and related objects.
 * 
 * @param addr address to check. Must be a proper {@link BAddr} object according to
 *             {@link BIPAddr_Assert}.
 * @return 1 if supported, 0 if not
 */
int BConnection_AddressSupported (BAddr addr);



struct BListener_s;

/**
 * Object which listens for connections on an address.
 * When a connection is ready, the {@link BListener_handler} handler is called, from which
 * the connection can be accepted into a new {@link BConnection} object.
 */
typedef struct BListener_s BListener;

/**
 * Handler called when a new connection is ready.
 * The connection can be accepted by calling {@link BConnection_Init} with the a
 * BCONNECTION_SOURCE_LISTENER 'source' argument.
 * If no attempt is made to accept the connection from the job closure of this handler,
 * the connection will be discarded.
 * 
 * @param user as in {@link BListener_Init}
 */
typedef void (*BListener_handler) (void *user);

/**
 * Initializes the object.
 * {@link BNetwork_GlobalInit} must have been done.
 * 
 * @param o the object
 * @param addr address to listen on. Must be supported according to {@link BConnection_AddressSupported}.
 * @param reactor reactor we live in
 * @param user argument to handler
 * @param handler handler called when a connection can be accepted
 * @return 1 on success, 0 on failure
 */
int BListener_Init (BListener *o, BAddr addr, BReactor *reactor, void *user,
                    BListener_handler handler) WARN_UNUSED;

#ifndef BADVPN_USE_WINAPI
/**
 * Initializes the object for listening on a Unix socket.
 * {@link BNetwork_GlobalInit} must have been done.
 * 
 * @param o the object
 * @param socket_path socket path for listening
 * @param reactor reactor we live in
 * @param user argument to handler
 * @param handler handler called when a connection can be accepted
 * @return 1 on success, 0 on failure
 */
int BListener_InitUnix (BListener *o, const char *socket_path, BReactor *reactor, void *user,
                        BListener_handler handler) WARN_UNUSED;
#endif

/**
 * Frees the object.
 * 
 * @param o the object
 */
void BListener_Free (BListener *o);



struct BConnector_s;

/**
 * Object which connects to an address.
 * When the connection attempt finishes, the {@link BConnector_handler} handler is called, from which,
 * if successful, the resulting connection can be moved to a new {@link BConnection} object.
 */
typedef struct BConnector_s BConnector;

/**
 * Handler called when the connection attempt finishes.
 * If the connection attempt succeeded (is_error==0), the new connection can be used by calling
 * {@link BConnection_Init} with a BCONNECTION_SOURCE_TYPE_CONNECTOR 'source' argument.
 * This handler will be called at most once. The connector object need not be freed after it
 * is called. 
 * 
 * @param user as in {@link BConnector_Init}
 * @param is_error whether the connection attempt succeeded (0) or failed (1)
 */
typedef void (*BConnector_handler) (void *user, int is_error);

/**
 * Initializes the object.
 * {@link BNetwork_GlobalInit} must have been done.
 * 
 * @param o the object
 * @param addr address to connect to. Must be supported according to {@link BConnection_AddressSupported}.
 * @param reactor reactor we live in
 * @param user argument to handler
 * @param handler handler called when the connection attempt finishes
 * @return 1 on success, 0 on failure
 */
int BConnector_Init (BConnector *o, BAddr addr, BReactor *reactor, void *user,
                     BConnector_handler handler) WARN_UNUSED;

/**
 * Frees the object.
 * 
 * @param o the object
 */
void BConnector_Free (BConnector *o);



#define BCONNECTION_SOURCE_TYPE_LISTENER 1
#define BCONNECTION_SOURCE_TYPE_CONNECTOR 2
#define BCONNECTION_SOURCE_TYPE_PIPE 3

struct BConnection_source {
    int type;
    union {
        struct {
            BListener *listener;
            BAddr *out_addr;
        } listener;
        struct {
            BConnector *connector;
        } connector;
#ifndef BADVPN_USE_WINAPI
        struct {
            int pipefd;
        } pipe;
#endif
    } u;
};

#define BCONNECTION_SOURCE_LISTENER(_listener, _out_addr) \
    ((struct BConnection_source){ \
        .type = BCONNECTION_SOURCE_TYPE_LISTENER, \
        .u.listener.listener = (_listener), \
        .u.listener.out_addr = (_out_addr) \
    })

#define BCONNECTION_SOURCE_CONNECTOR(_connector) \
    ((struct BConnection_source){ \
        .type = BCONNECTION_SOURCE_TYPE_CONNECTOR, \
        .u.connector.connector = (_connector) \
    })

#ifndef BADVPN_USE_WINAPI
#define BCONNECTION_SOURCE_PIPE(_pipefd) \
    ((struct BConnection_source){ \
        .type = BCONNECTION_SOURCE_TYPE_PIPE, \
        .u.pipe.pipefd = (_pipefd) \
    })
#endif



struct BConnection_s;

/**
 * Object which represents a stream connection. This is usually a TCP connection, either client
 * or server, but may also be used with any file descriptor (e.g. pipe) on Unix-like systems.
 * Sending and receiving is performed via {@link StreamPassInterface} and {@link StreamRecvInterface},
 * respectively.
 */
typedef struct BConnection_s BConnection;

#define BCONNECTION_EVENT_ERROR 1
#define BCONNECTION_EVENT_RECVCLOSED 2

/**
 * Handler called when an error occurs or the receive end of the connection was closed
 * by the remote peer.
 * - If event is BCONNECTION_EVENT_ERROR, the connection is no longer usable and must be freed
 *   from withing the job closure of this handler. No further I/O or interface initialization
 *   must occur.
 * - If event is BCONNECTION_EVENT_RECVCLOSED, no further receive I/O or receive interface
 *   initialization must occur. It is guarantted that the receive interface was initialized.
 * 
 * @param user as in {@link BConnection_Init} or {@link BConnection_SetHandlers}
 * @param event what happened - BCONNECTION_EVENT_ERROR or BCONNECTION_EVENT_RECVCLOSED
 */
typedef void (*BConnection_handler) (void *user, int event);

/**
 * Initializes the object.
 * {@link BNetwork_GlobalInit} must have been done.
 * 
 * @param o the object
 * @param source specifies what the connection comes from. This argument must be created with one of the
 *               following macros:
 *               - BCONNECTION_SOURCE_LISTENER(BListener *, BAddr *)
 *                 Accepts a connection ready on a {@link BListener} object. Must be called from the job
 *                 closure of the listener's {@link BListener_handler}, and must be the first attempt
 *                 for this handler invocation. The address of the client is written if the address
 *                 argument is not NULL (theoretically an invalid address may be returned).
 *               - BCONNECTION_SOURCE_CONNECTOR(Bconnector *)
 *                 Uses a connection establised with {@link BConnector}. Must be called from the job
 *                 closure of the connector's {@link BConnector_handler}, the handler must be reporting
 *                 successful connection, and must be the first attempt for this handler invocation.
 *               - BCONNECTION_SOURCE_PIPE(int)
 *                 On Unix-like systems, uses the provided file descriptor. The file descriptor number must
 *                 be >=0.
 * @param reactor reactor we live in
 * @param user argument to handler
 * @param handler handler called when an error occurs or the receive end of the connection was closed
 *                by the remote peer.
 * @return 1 on success, 0 on failure
 */
int BConnection_Init (BConnection *o, struct BConnection_source source, BReactor *reactor, void *user,
                      BConnection_handler handler) WARN_UNUSED;

/**
 * Frees the object.
 * The send and receive interfaces must not be initialized.
 * If the connection was created with a BCONNECTION_SOURCE_PIPE 'source' argument, the file descriptor
 * will not be closed.
 * 
 * @param o the object
 */
void BConnection_Free (BConnection *o);

/**
 * Updates the handler function.
 * 
 * @param o the object
 * @param user argument to handler
 * @param handler new handler function, as in {@link BConnection_Init}. Additionally, may be NULL to
 *                remove the current handler. In this case, a proper handler must be set before anything
 *                can happen with the connection. This is used when moving the connection ownership from
 *                one module to another.
 */
void BConnection_SetHandlers (BConnection *o, void *user, BConnection_handler handler);

/**
 * Sets the SO_SNDBUF socket option.
 * 
 * @param o the object
 * @param buf_size value for SO_SNDBUF option
 * @return 1 on success, 0 on failure
 */
int BConnection_SetSendBuffer (BConnection *o, int buf_size);

/**
 * Initializes the send interface for the connection.
 * The send interface must not be initialized.
 * 
 * @param o the object
 */
void BConnection_SendAsync_Init (BConnection *o);

/**
 * Frees the send interface for the connection.
 * The send interface must be initialized.
 * If the send interface was busy when this is called, the connection is no longer usable and must be
 * freed before any further I/O or interface initialization.
 * 
 * @param o the object
 */
void BConnection_SendAsync_Free (BConnection *o);

/**
 * Returns the send interface.
 * The send interface must be initialized.
 * 
 * @param o the object
 * @return send interface
 */
StreamPassInterface * BConnection_SendAsync_GetIf (BConnection *o);

/**
 * Initializes the receive interface for the connection.
 * The receive interface must not be initialized.
 * 
 * @param o the object
 */
void BConnection_RecvAsync_Init (BConnection *o);

/**
 * Frees the receive interface for the connection.
 * The receive interface must be initialized.
 * If the receive interface was busy when this is called, the connection is no longer usable and must be
 * freed before any further I/O or interface initialization.
 * 
 * @param o the object
 */
void BConnection_RecvAsync_Free (BConnection *o);

/**
 * Returns the receive interface.
 * The receive interface must be initialized.
 * 
 * @param o the object
 * @return receive interface
 */
StreamRecvInterface * BConnection_RecvAsync_GetIf (BConnection *o);



#ifdef BADVPN_USE_WINAPI
#include "BConnection_win.h"
#else
#include "BConnection_unix.h"
#endif

#endif

/**
 * @file ipc_server.c
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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <misc/offset.h>
#include <structure/LinkedList2.h>
#include <system/DebugObject.h>
#include <system/BLog.h>
#include <system/BSignal.h>
#include <flow/SinglePacketBuffer.h>
#include <ipc/BIPCServer.h>
#include <ipc/BIPC.h>

#define RECV_MTU 100

BReactor reactor;
BIPCServer server;
LinkedList2 clients;

struct client {
    dead_t dead;
    BIPC ipc;
    PacketPassInterface recv_if;
    PacketPassInterface *send_if;
    LinkedList2Node list_node;
};

static void signal_handler (void *user);
static void server_handler (void *user);
static void remove_client (struct client *client);
static void client_ipc_handler (struct client *client);
static int client_recv_handler_send (struct client *client, uint8_t *data, int data_len);
static void client_send_handler_done (struct client *client);

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
    if (argc != 2) {
        printf("Usage: %s <path>\n", argv[0]);
        return 1;
    }
    
    char *path = argv[1];
    
    BLog_InitStdout();
    
    if (!BReactor_Init(&reactor)) {
        DEBUG("BReactor_Init failed");
        goto fail1;
    }
    
    if (!BSignal_Init()) {
        DEBUG("BSignal_Init failed");
        goto fail2;
    }
    
    BSignal_Capture();
    
    if (!BSignal_SetHandler(&reactor, signal_handler, NULL)) {
        DEBUG("BSignal_SetHandler failed");
        goto fail2;
    }
    
    if (!BIPCServer_Init(&server, path, server_handler, NULL, &reactor)) {
        DEBUG("BIPCServer_Init failed");
        goto fail3;
    }
    
    LinkedList2_Init(&clients);
    
    int ret = BReactor_Exec(&reactor);
    
    BReactor_Free(&reactor);
    
    BLog_Free();
    
    DebugObjectGlobal_Finish();
    
    return ret;
    
fail3:
    BSignal_RemoveHandler();
fail2:
    BReactor_Free(&reactor);
fail1:
    BLog_Free();
    DebugObjectGlobal_Finish();
    return 1;
}

void signal_handler (void *user)
{
    DEBUG("termination requested");
    
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&clients)) {
        struct client *client = UPPER_OBJECT(node, struct client, list_node);
        remove_client(client);
    }
    
    BIPCServer_Free(&server);
    
    BSignal_RemoveHandler();
    
    BReactor_Quit(&reactor, 1);
}

void server_handler (void *user)
{
    struct client *client = malloc(sizeof(*client));
    if (!client) {
        DEBUG("failed to allocate client structure");
        goto fail0;
    }
    
    DEAD_INIT(client->dead);
    
    PacketPassInterface_Init(&client->recv_if, RECV_MTU, (PacketPassInterface_handler_send)client_recv_handler_send, client);
    
    if (!BIPC_InitAccept(&client->ipc, &server, 0, &client->recv_if, (BIPC_handler)client_ipc_handler, client, &reactor)) {
        DEBUG("BIPC_InitAccept failed");
        goto fail1;
    }
    
    client->send_if = BIPC_GetSendInterface(&client->ipc);
    PacketPassInterface_Sender_Init(client->send_if, (PacketPassInterface_handler_done)client_send_handler_done, client);
    
    LinkedList2_Append(&clients, &client->list_node);
    
    DEBUG("client connected");
    
    return;
    
fail1:
    PacketPassInterface_Free(&client->recv_if);
    free(client);
fail0:
    ;
}

void remove_client (struct client *client)
{
    DEBUG("removing client");
    
    LinkedList2_Remove(&clients, &client->list_node);
    
    BIPC_Free(&client->ipc);
    
    PacketPassInterface_Free(&client->recv_if);
    
    DEAD_KILL(client->dead);
    
    free(client);
}

void client_ipc_handler (struct client *client)
{
    remove_client(client);
}

int client_recv_handler_send (struct client *client, uint8_t *data, int data_len)
{
    // print message
    uint8_t buf[data_len + 1];
    memcpy(buf, data, data_len);
    buf[data_len] = '\0';
    printf("received: '%s'\n", buf);
    
    // send reply
    DEAD_ENTER(client->dead)
    int res = PacketPassInterface_Sender_Send(client->send_if, NULL, 0);
    if (DEAD_LEAVE(client->dead)) {
        return -1;
    }
    
    if (!res) {
        // wait for reply to be sent, then allow next message
        return 0;
    }
    
    return 1;
}

void client_send_handler_done (struct client *client)
{
    // allow next message
    PacketPassInterface_Done(&client->recv_if);
    return;
}

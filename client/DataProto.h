/**
 * @file DataProto.h
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
 * Mudule for frame sending used in the VPN client program.
 */

#ifndef BADVPN_CLIENT_DATAPROTO_H
#define BADVPN_CLIENT_DATAPROTO_H

#include <stdint.h>

#include <protocol/scproto.h>
#include <misc/debugcounter.h>
#include <misc/debug.h>
#include <structure/LinkedList2.h>
#include <structure/BAVL.h>
#include <system/DebugObject.h>
#include <system/BReactor.h>
#include <system/BPending.h>
#include <flow/PacketPassFairQueue.h>
#include <flow/PacketPassInactivityMonitor.h>
#include <flow/PacketPassNotifier.h>
#include <flow/DataProtoKeepaliveSource.h>
#include <flow/PacketRecvBlocker.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/BufferWriter.h>
#include <flow/PacketBuffer.h>
#include <flow/PacketPassConnector.h>

typedef void (*DataProtoDest_handler) (void *user, int up);

struct dp_relay_flow;

/**
 * Frame destination.
 * Represents a peer as a destination for sending frames to.
 */
typedef struct {
    BReactor *reactor;
    peerid_t dest_id;
    int mtu;
    int frame_mtu;
    PacketPassFairQueue queue;
    PacketPassInactivityMonitor monitor;
    PacketPassNotifier notifier;
    DataProtoKeepaliveSource ka_source;
    PacketRecvBlocker ka_blocker;
    SinglePacketBuffer ka_buffer;
    PacketPassFairQueueFlow ka_qflow;
    BTimer receive_timer;
    int up;
    DataProtoDest_handler handler;
    void *user;
    LinkedList2 relay_flows_list;
    BPending keepalive_job;
    DebugCounter flows_counter;
    DebugObject d_obj;
    #ifndef NDEBUG
    PacketPassInterface *d_output;
    int d_freeing;
    #endif
} DataProtoDest;

/**
 * Local frame source.
 * Buffers frames received from the TAP device, addressed to a particular peer.
 */
typedef struct {
    int frame_mtu;
    peerid_t source_id;
    peerid_t dest_id;
    BufferWriter ainput;
    PacketBuffer buffer;
    PacketPassConnector connector;
    DataProtoDest *dp;
    PacketPassFairQueueFlow dp_qflow;
    DebugObject d_obj;
    #ifndef NDEBUG
    int d_dp_released;
    #endif
} DataProtoLocalSource;

/**
 * Relay frame source.
 * Represents relaying of frames from one particular peer to other peers.
 */
typedef struct {
    peerid_t source_id;
    LinkedList2 relay_flows_list;
    BAVL relay_flows_tree;
    DebugObject d_obj;
} DataProtoRelaySource;

struct dp_relay_flow {
    DataProtoRelaySource *rs;
    DataProtoDest *dp;
    BufferWriter ainput;
    PacketBuffer buffer;
    PacketPassInactivityMonitor monitor;
    PacketPassFairQueueFlow qflow;
    LinkedList2Node source_list_node;
    BAVLNode source_tree_node;
    LinkedList2Node dp_list_node;
    BPending first_frame_job;
    uint8_t *first_frame;
    int first_frame_len;
};

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param reactor reactor we live in
 * @param dest_id ID of the peer this object sends to
 * @param output output interface. Must support cancel functionality. Its MTU must be
 *               >=sizeof(struct dataproto_header)+sizeof(struct dataproto_peer_id).
 * @param keepalive_time keepalive time
 * @param tolerance_time after how long of not having received anything from the peer
 *                       to consider the link down
 * @param handler up state handler
 * @param user value to pass to handler
 * @return 1 on success, 0 on failure
 */
int DataProtoDest_Init (DataProtoDest *o, BReactor *reactor, peerid_t dest_id, PacketPassInterface *output, btime_t keepalive_time, btime_t tolerance_time, DataProtoDest_handler handler, void *user) WARN_UNUSED;

/**
 * Frees the object.
 * There must be no local sources attached.
 * 
 * @param o the object
 */
void DataProtoDest_Free (DataProtoDest *o);

/**
 * Prepares for freeing the object by allowing freeing of local sources.
 * The object enters freeing state.
 * The object must be freed before returning control to the reactor,
 * and before any further I/O (output or submitting frames).
 * 
 * @param o the object
 */
void DataProtoDest_PrepareFree (DataProtoDest *o);

/**
 * Submits a relayed frame.
 * Must not be in freeing state.
 * Must not be called before it evaluates.
 * 
 * @param o the object
 * @param rs relay source object representing the peer this frame came from
 * @param data frame data. The frame must remain accessible until this evaluates.
 * @param data_len frame length. Must be >=0.
 *                 Must be <= (output MTU) - (sizeof(struct dataproto_header) + sizeof(struct dataproto_peer_id)).
 * @param buffer_num_packets number of packets the relay buffer should hold, in case it doesn't exist.
 *                           Must be >0.
 */
void DataProtoDest_SubmitRelayFrame (DataProtoDest *o, DataProtoRelaySource *rs, uint8_t *data, int data_len, int buffer_num_packets);

/**
 * Notifies the object that a packet was received from the peer.
 * Must not be in freeing state.
 * Must not be called from output Send calls.
 * May call the up state handler.
 * May invoke output I/O.
 * 
 * @param o the object
 * @param peer_receiving whether the DATAPROTO_FLAGS_RECEIVING_KEEPALIVES flag was set in the packet.
 *                       Must be 0 or 1.
 */
void DataProtoDest_Received (DataProtoDest *o, int peer_receiving);

/**
 * Initializes the object.
 * The object is initialized in not attached state.
 * 
 * @param o the object
 * @param frame_mtu maximum frame size. Must be >=0.
 *                  Must be <= INT_MAX - (sizeof(struct dataproto_header) + sizeof(struct dataproto_peer_id)).
 * @param source_id ID of the peer from which the frames submitted to this object originate from,
 *                  i.e. our ID
 * @param dest_id ID of the peer to which the frames are to be delivered to
 * @param num_packets number of packets the buffer should hold. Must be >0.
 * @param reactor reactor we live in. Must be the same as with all destinations this
 *                source will be attached to.
 * @return 1 on success, 0 on failure
 */
int DataProtoLocalSource_Init (DataProtoLocalSource *o, int frame_mtu, peerid_t source_id, peerid_t dest_id, int num_packets, BReactor *reactor) WARN_UNUSED;

/**
 * Frees the object.
 * The object must be in not attached state.
 * 
 * @param o the object
 */
void DataProtoLocalSource_Free (DataProtoLocalSource *o);

/**
 * Submits a frame.
 * If the object is in attached state:
 * - The object must be in not released state.
 * - The destination must not be in freeing state.
 * - Must not be called from destination's output Send calls.
 * - May invoke the destination's output I/O.
 * 
 * @param o the object
 * @param data frame data
 * @param data_len frame length. Must be >=0 and <=frame_mtu.
 */
void DataProtoLocalSource_SubmitFrame (DataProtoLocalSource *o, uint8_t *data, int data_len);

/**
 * Attaches the object to a destination.
 * The object must be in not attached state.
 * The object enters attached and not released state.
 * 
 * @param o the object
 * @param dp destination to attach to. This object's frame_mtu must be <= destination's
 *           (output MTU)-(sizeof(struct dataproto_header)+sizeof(struct dataproto_peer_id)).
 */
void DataProtoLocalSource_Attach (DataProtoLocalSource *o, DataProtoDest *dp);

/**
 * Releases the object to allow detaching it from the destination.
 * The object must be in attached and not released state.
 * The destination must not be in freeing state.
 * The object enters attached and released state.
 * Must not be called from destination's output Send calls.
 * May invoke the destination's output Cancel call.
 * 
 * @param o the object
 */
void DataProtoLocalSource_Release (DataProtoLocalSource *o);

/**
 * Detaches the object from a destination.
 * The object must be in attached state.
 * Either the object must be in released state, or the destination must be in freeing state.
 * Unless the destination is in freeing state, must not be called from destination's
 * output Send calls.
 * 
 * @param o the object
 */
void DataProtoLocalSource_Detach (DataProtoLocalSource *o);

/**
 * Initializes the object
 * 
 * @param o the object
 * @param source_id ID of the peer whose relayed frames this object represents
 */
void DataProtoRelaySource_Init (DataProtoRelaySource *o, peerid_t source_id);

/**
 * Frees the object.
 * The object must have no relay flows (guaranteed if no frames have been submitted
 * with it using {@link DataProtoDest_SubmitRelayFrame}).
 * 
 * @param o the object
 */
void DataProtoRelaySource_Free (DataProtoRelaySource *o);

/**
 * Checks if the object has no relay flows.
 * 
 * @param o the object
 * @return 1 if there are no relay flows, 0 if at least one
 */
int DataProtoRelaySource_IsEmpty (DataProtoRelaySource *o);

/**
 * Removes all relay flows by releasing them.
 * None of the destinations must be in freeing state.
 * Must not be called from any of the destinations' output Send calls.
 * May invoke the destinations' output Cancel calls.
 * 
 * @param o the object
 */
void DataProtoRelaySource_Release (DataProtoRelaySource *o);

/**
 * Removes all relay flows by putting destinations into freeing state.
 * May put destinations into freeing state (as if {@link DataProtoDest_PrepareFree}
 * was called on them).
 * This should only be used while freeing the entire frame sending system.
 * 
 * @param o the object
 */
void DataProtoRelaySource_FreeRelease (DataProtoRelaySource *o);

#endif

/**
 * @file client.h
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
#include <stdint.h>

#include <protocol/scproto.h>
#include <structure/LinkedList2.h>
#include <structure/BAVL.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/PacketPassFairQueue.h>
#include <tuntap/BTap.h>
#include <client/DatagramPeerIO.h>
#include <client/StreamPeerIO.h>
#include <client/DataProto.h>
#include <client/FrameDecider.h>

// NOTE: all time values are in milliseconds

// name of the program
#define PROGRAM_NAME "client"

// server output buffer size
#define SERVER_BUFFER_MIN_PACKETS 200

// maximum UDP payload size
#define CLIENT_UDP_MTU 1472

// maximum number of peers
#define MAX_PEERS 29
// maximum number of peer's MAC addresses to remember
#define PEER_MAX_MACS 16
// maximum number of multicast addresses per peer
#define PEER_MAX_GROUPS 16
// how long we wait for a packet to reach full size before sending it (see FragmentProtoDisassembler latency argument)
#define PEER_DEFAULT_FRAGMENTATION_LATENCY 0
// keep-alive packet interval for p2p communication
#define PEER_KEEPALIVE_INTERVAL 10000
// keep-alive receive timer for p2p communication (after how long to consider the link down)
#define PEER_KEEPALIVE_RECEIVE_TIMER 22000
// size of frame send buffer, in number of frames
#define PEER_DEFAULT_SEND_BUFFER_SIZE 32
// size of frame send buffer for relayed packets, in number of frames
#define PEER_DEFAULT_SEND_BUFFER_RELAY_SIZE 32
// retry time
#define PEER_RETRY_TIME 5000

// for how long a peer can send no Membership Reports for a group
// before the peer and group are disassociated
#define IGMP_GROUP_MEMBERSHIP_INTERVAL 260000
// how long to wait for joins after a Group Specific query has been
// forwarded to a peer before assuming there are no listeners at the peer
#define IGMP_LAST_MEMBER_QUERY_TIME 2000

// maximum bind addresses
#define MAX_BIND_ADDRS 8

// maximum external addresses per bind address
#define MAX_EXT_ADDRS 8

// maximum scopes
#define MAX_SCOPES 8

struct device_data {
    BTap btap;
    int mtu;
    
    // input
    SinglePacketBuffer input_buffer;
    PacketPassInterface input_interface;
    
    // output
    PacketPassFairQueue output_queue;
};

struct peer_data {
    // peer identifier
    peerid_t id;
    
    // flags provided by the server
    int flags;
    
    // certificate reported by the server, defined only if using SSL
    uint8_t cert[SCID_NEWCLIENT_MAX_CERT_LEN];
    int cert_len;
    
    // local flow
    DataProtoLocalSource local_dpflow;
    
    // local receive flow
    PacketPassInterface *local_recv_if;
    PacketPassFairQueueFlow local_recv_qflow;
    
    // relay source
    DataProtoRelaySource relay_source;
    
    // flag if link objects are initialized
    int have_link;
    
    // link sending
    DataProtoDest send_dp;
    
    // link receive interface
    PacketPassInterface recv_ppi;
    
    // transport-specific link objects
    union {
        struct {
            DatagramPeerIO pio;
            uint16_t sendseed_nextid;
            int sendseed_sent;
            uint16_t sendseed_sent_id;
            uint8_t *sendseed_sent_key;
            uint8_t *sendseed_sent_iv;
        } udp;
        struct {
            StreamPeerIO pio;
        } tcp;
    } pio;
    
    // flag if relaying is installed
    int have_relaying;
    
    // relaying objects
    struct peer_data *relaying_peer; // peer through which we are relaying
    LinkedList2Node relaying_list_node; // node in relay peer's relay_users
    
    // waiting for relay data
    int waiting_relay;
    LinkedList2Node waiting_relay_list_node;
    
    // retry timer
    BTimer reset_timer;
    
    // frame decider peer
    FrameDeciderPeer decider_peer;
    
    // relay server specific
    int is_relay;
    LinkedList2Node relay_list_node;
    LinkedList2 relay_users;
    
    // binding state
    int binding;
    int binding_addrpos;
    
    // jobs
    BPending job_send_seed_after_binding;
    
    // peers linked list node
    LinkedList2Node list_node;
    // peers-by-ID hash table node
    BAVLNode tree_node;
};

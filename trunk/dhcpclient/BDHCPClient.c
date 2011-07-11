/**
 * @file BDHCPClient.c
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
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <linux/filter.h>

#include <misc/debug.h>
#include <misc/byteorder.h>
#include <misc/ethernet_proto.h>
#include <misc/ipv4_proto.h>
#include <misc/udp_proto.h>
#include <base/BLog.h>

#include <dhcpclient/BDHCPClient.h>

#include <generated/blog_channel_BDHCPClient.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define IPUDP_OVERHEAD (sizeof(struct ipv4_header) + sizeof(struct udp_header))

static const struct sock_filter dhcp_sock_filter[] = {
    BPF_STMT(BPF_LD + BPF_B + BPF_ABS, 9),                        // A <- IP protocol
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, IPV4_PROTOCOL_UDP, 0, 3), // IP protocol = UDP ?
    BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 22),                       // A <- UDP destination port
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, DHCP_CLIENT_PORT, 0, 1),  // UDP destination port = DHCP client ?
    BPF_STMT(BPF_RET + BPF_K, 65535),                             // return all
    BPF_STMT(BPF_RET + BPF_K, 0)                                  // ignore
};

static void dgram_handler (BDHCPClient *o, int event)
{
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_ERROR, "packet socket error");
    
    // report error
    DEBUGERROR(&o->d_err, o->handler(o->user, BDHCPCLIENT_EVENT_ERROR));
    return;
}

static void dhcp_handler (BDHCPClient *o, int event)
{
    DebugObject_Access(&o->d_obj);
    
    switch (event) {
        case BDHCPCLIENTCORE_EVENT_UP:
            ASSERT(!o->up)
            o->up = 1;
            o->handler(o->user, BDHCPCLIENT_EVENT_UP);
            return;
            
        case BDHCPCLIENTCORE_EVENT_DOWN:
            ASSERT(o->up)
            o->up = 0;
            o->handler(o->user, BDHCPCLIENT_EVENT_DOWN);
            return;
            
        default:
            ASSERT(0);
    }
}

static int get_iface_info (const char *ifname, uint8_t *out_mac, int *out_mtu, int *out_ifindex)
{
    struct ifreq ifr;
    
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (!s) {
        BLog(BLOG_ERROR, "socket failed");
        goto fail0;
    }
    
    // get MAC
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
        BLog(BLOG_ERROR, "ioctl(SIOCGIFHWADDR) failed");
        goto fail1;
    }
    if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
        BLog(BLOG_ERROR, "hardware address not ethernet");
        goto fail1;
    }
    memcpy(out_mac, ifr.ifr_hwaddr.sa_data, 6);
    
    // get MTU
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFMTU, &ifr)) {
        BLog(BLOG_ERROR, "ioctl(SIOCGIFMTU) failed");
        goto fail1;
    }
    *out_mtu = ifr.ifr_mtu;
    
    // get interface index
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr)) {
        BLog(BLOG_ERROR, "ioctl(SIOCGIFINDEX) failed");
        goto fail1;
    }
    *out_ifindex = ifr.ifr_ifindex;
    
    close(s);
    
    return 1;
    
fail1:
    close(s);
fail0:
    return 0;
}

int BDHCPClient_Init (BDHCPClient *o, const char *ifname, BReactor *reactor, BDHCPClient_handler handler, void *user)
{
    // init arguments
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // get interface information
    uint8_t if_mac[6];
    int if_mtu;
    int if_index;
    if (!get_iface_info(ifname, if_mac, &if_mtu, &if_index)) {
        BLog(BLOG_ERROR, "failed to get interface information");
        goto fail0;
    }
    
    BLog(BLOG_INFO, "if_mac=%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8" if_mtu=%d if_index=%d",
          if_mac[0], if_mac[1], if_mac[2], if_mac[3], if_mac[4], if_mac[5], if_mtu, if_index);
    
    if (if_mtu < IPUDP_OVERHEAD) {
        BLog(BLOG_ERROR, "MTU is too small for UDP/IP !?!");
        goto fail0;
    }
    
    int dhcp_mtu = if_mtu - IPUDP_OVERHEAD;
    
    // init dgram
    if (!BDatagram_Init(&o->dgram, BADDR_TYPE_PACKET, o->reactor, o, (BDatagram_handler)dgram_handler)) {
        BLog(BLOG_ERROR, "BDatagram_Init failed");
        goto fail0;
    }
    
    // set socket filter
    {
        struct sock_filter filter[sizeof(dhcp_sock_filter) / sizeof(dhcp_sock_filter[0])];
        memcpy(filter, dhcp_sock_filter, sizeof(filter));
        struct sock_fprog fprog = {
            .len = sizeof(filter) / sizeof(filter[0]),
            .filter = filter
        };
        if (setsockopt(BDatagram_GetFd(&o->dgram), SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof(fprog)) < 0) {
            BLog(BLOG_NOTICE, "not using socket filter");
        }
    }
    
    // bind dgram
    BAddr bind_addr;
    BAddr_InitPacket(&bind_addr, hton16(ETHERTYPE_IPV4), if_index, BADDR_PACKET_HEADER_TYPE_ETHERNET, BADDR_PACKET_PACKET_TYPE_HOST, if_mac);
    if (!BDatagram_Bind(&o->dgram, bind_addr)) {
        BLog(BLOG_ERROR, "BDatagram_Bind failed");
        goto fail1;
    }
    
    // set dgram send addresses
    BAddr dest_addr;
    uint8_t broadcast_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    BAddr_InitPacket(&dest_addr, hton16(ETHERTYPE_IPV4), if_index, BADDR_PACKET_HEADER_TYPE_ETHERNET, BADDR_PACKET_PACKET_TYPE_BROADCAST, broadcast_mac);
    BIPAddr local_addr;
    BIPAddr_InitInvalid(&local_addr);
    BDatagram_SetSendAddrs(&o->dgram, dest_addr, local_addr);

    // init dgram interfaces
    BDatagram_SendAsync_Init(&o->dgram, if_mtu);
    BDatagram_RecvAsync_Init(&o->dgram, if_mtu);
    
    // init sending
    
    // init copier
    PacketCopier_Init(&o->send_copier, dhcp_mtu, BReactor_PendingGroup(o->reactor));
    
    // init encoder
    DHCPIpUdpEncoder_Init(&o->send_encoder, PacketCopier_GetOutput(&o->send_copier), BReactor_PendingGroup(o->reactor));
    
    // init buffer
    if (!SinglePacketBuffer_Init(&o->send_buffer, DHCPIpUdpEncoder_GetOutput(&o->send_encoder), BDatagram_SendAsync_GetIf(&o->dgram), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail2;
    }
    
    // init receiving
    
    // init copier
    PacketCopier_Init(&o->recv_copier, dhcp_mtu, BReactor_PendingGroup(o->reactor));
    
    // init decoder
    DHCPIpUdpDecoder_Init(&o->recv_decoder, PacketCopier_GetInput(&o->recv_copier), BReactor_PendingGroup(o->reactor));
    
    // init buffer
    if (!SinglePacketBuffer_Init(&o->recv_buffer, BDatagram_RecvAsync_GetIf(&o->dgram), DHCPIpUdpDecoder_GetInput(&o->recv_decoder), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail3;
    }
    
    // init dhcp
    if (!BDHCPClientCore_Init(&o->dhcp, PacketCopier_GetInput(&o->send_copier), PacketCopier_GetOutput(&o->recv_copier), if_mac, o->reactor, (BDHCPClientCore_handler)dhcp_handler, o)) {
        BLog(BLOG_ERROR, "BDHCPClientCore_Init failed");
        goto fail4;
    }
    
    // set not up
    o->up = 0;
    
    DebugError_Init(&o->d_err, BReactor_PendingGroup(o->reactor));
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail4:
    SinglePacketBuffer_Free(&o->recv_buffer);
fail3:
    DHCPIpUdpDecoder_Free(&o->recv_decoder);
    PacketCopier_Free(&o->recv_copier);
    SinglePacketBuffer_Free(&o->send_buffer);
fail2:
    DHCPIpUdpEncoder_Free(&o->send_encoder);
    PacketCopier_Free(&o->send_copier);
    BDatagram_RecvAsync_Free(&o->dgram);
    BDatagram_SendAsync_Free(&o->dgram);
fail1:
    BDatagram_Free(&o->dgram);
fail0:
    return 0;
}

void BDHCPClient_Free (BDHCPClient *o)
{
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
    
    // free dhcp
    BDHCPClientCore_Free(&o->dhcp);
    
    // free receiving
    SinglePacketBuffer_Free(&o->recv_buffer);
    DHCPIpUdpDecoder_Free(&o->recv_decoder);
    PacketCopier_Free(&o->recv_copier);
    
    // free sending
    SinglePacketBuffer_Free(&o->send_buffer);
    DHCPIpUdpEncoder_Free(&o->send_encoder);
    PacketCopier_Free(&o->send_copier);
    
    // free dgram interfaces
    BDatagram_RecvAsync_Free(&o->dgram);
    BDatagram_SendAsync_Free(&o->dgram);
    
    // free dgram
    BDatagram_Free(&o->dgram);
}

int BDHCPClient_IsUp (BDHCPClient *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->up;
}

void BDHCPClient_GetClientIP (BDHCPClient *o, uint32_t *out_ip)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    BDHCPClientCore_GetClientIP(&o->dhcp, out_ip);
}

void BDHCPClient_GetClientMask (BDHCPClient *o, uint32_t *out_mask)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    BDHCPClientCore_GetClientMask(&o->dhcp, out_mask);
}

int BDHCPClient_GetRouter (BDHCPClient *o, uint32_t *out_router)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    return BDHCPClientCore_GetRouter(&o->dhcp, out_router);
}

int BDHCPClient_GetDNS (BDHCPClient *o, uint32_t *out_dns_servers, size_t max_dns_servers)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    return BDHCPClientCore_GetDNS(&o->dhcp, out_dns_servers, max_dns_servers);
}

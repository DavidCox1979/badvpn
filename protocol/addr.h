/**
 * @file addr.h
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
 * AddrProto, a protocol for encoding network addresses.
 * 
 * AddrProto is built with BProto, the protocol and code generator for building
 * custom message protocols. The BProto specification file is addr.bproto.
 */

#ifndef BADVPN_PROTOCOL_ADDR_H
#define BADVPN_PROTOCOL_ADDR_H

#include <stdint.h>
#include <string.h>

#include <misc/debug.h>
#include <system/BAddr.h>

#include <generated/bproto_addr.h>

#define ADDR_TYPE_IPV4 1
#define ADDR_TYPE_IPV6 2

#define ADDR_SIZE_IPV4 (addr_SIZEtype + addr_SIZEip_port + addr_SIZEipv4_addr)
#define ADDR_SIZE_IPV6 (addr_SIZEtype + addr_SIZEip_port + addr_SIZEipv6_addr)

/**
 * Determines if the given address is supported by AddrProto.
 * Depends only on the type of the address.
 *
 * @param addr address to check. Must be recognized according to {@link BAddr_IsRecognized}.
 * @return 1 if supported, 0 if not
 */
static int addr_supported (BAddr addr);

/**
 * Determines the size of the given address when encoded by AddrProto.
 * Depends only on the type of the address.
 *
 * @param addr address to check. Must be supported according to {@link addr_supported}.
 * @return encoded size
 */
static int addr_size (BAddr addr);

/**
 * Encodes an address according to AddrProto.
 *
 * @param out output buffer. Must have at least addr_size(addr) space.
 * @param addr address to encode. Must be supported according to {@link addr_supported}.
 */
static void addr_write (uint8_t *out, BAddr addr);

/**
 * Decodes an address according to AddrProto.
 *
 * @param data input buffer containing the address to decode
 * @param data_len size of input. Must be >=0.
 * @param out_addr the decoded address will be stored here on success
 * @return 1 on success, 0 on failure
 */
static int addr_read (uint8_t *data, int data_len, BAddr *out_addr) WARN_UNUSED;

int addr_supported (BAddr addr)
{
    BAddr_Assert(&addr);
    
    switch (addr.type) {
        case BADDR_TYPE_IPV4:
        case BADDR_TYPE_IPV6:
            return 1;
        default:
            return 0;
    }
}

int addr_size (BAddr addr)
{
    ASSERT(addr_supported(addr))
    
    switch (addr.type) {
        case BADDR_TYPE_IPV4:
            return ADDR_SIZE_IPV4;
        case BADDR_TYPE_IPV6:
            return ADDR_SIZE_IPV6;
        default:
            ASSERT(0)
            return 0;
    }
}

void addr_write (uint8_t *out, BAddr addr)
{
    ASSERT(addr_supported(addr))
    
    addrWriter writer;
    addrWriter_Init(&writer, out);
    
    switch (addr.type) {
        case BADDR_TYPE_IPV4: {
            addrWriter_Addtype(&writer, ADDR_TYPE_IPV4);
            uint8_t *out_port = addrWriter_Addip_port(&writer);
            memcpy(out_port, &addr.ipv4.port, sizeof(addr.ipv4.port));
            uint8_t *out_addr = addrWriter_Addipv4_addr(&writer);
            memcpy(out_addr, &addr.ipv4.ip, sizeof(addr.ipv4.ip));
        } break;
        case BADDR_TYPE_IPV6: {
            addrWriter_Addtype(&writer, ADDR_TYPE_IPV6);
            uint8_t *out_port = addrWriter_Addip_port(&writer);
            memcpy(out_port, &addr.ipv6.port, sizeof(addr.ipv6.port));
            uint8_t *out_addr = addrWriter_Addipv6_addr(&writer);
            memcpy(out_addr, addr.ipv6.ip, sizeof(addr.ipv6.ip));
        } break;
        default:
            ASSERT(0);
    }
    
    int len = addrWriter_Finish(&writer);
    
    ASSERT(len == addr_size(addr))
}

int addr_read (uint8_t *data, int data_len, BAddr *out_addr)
{
    ASSERT(data_len >= 0)
    
    addrParser parser;
    if (!addrParser_Init(&parser, data, data_len)) {
        DEBUG("failed to parse addr");
        return 0;
    }
    
    uint8_t type;
    addrParser_Gettype(&parser, &type);
    
    switch (type) {
        case ADDR_TYPE_IPV4: {
            uint8_t *port_data;
            if (!addrParser_Getip_port(&parser, &port_data)) {
                DEBUG("port missing for IPv4 address");
                return 0;
            }
            uint8_t *addr_data;
            if (!addrParser_Getipv4_addr(&parser, &addr_data)) {
                DEBUG("address missing for IPv4 address");
                return 0;
            }
            uint16_t port;
            uint32_t addr;
            memcpy(&port, port_data, sizeof(port));
            memcpy(&addr, addr_data, sizeof(addr));
            BAddr_InitIPv4(out_addr, addr, port);
        } break;
        case ADDR_TYPE_IPV6: {
            uint8_t *port_data;
            if (!addrParser_Getip_port(&parser, &port_data)) {
                DEBUG("port missing for IPv6 address");
                return 0;
            }
            uint8_t *addr_data;
            if (!addrParser_Getipv6_addr(&parser, &addr_data)) {
                DEBUG("address missing for IPv6 address");
                return 0;
            }
            uint16_t port;
            memcpy(&port, port_data, sizeof(port));
            BAddr_InitIPv6(out_addr, addr_data, port);
        } break;
        default:
            DEBUG("unknown address type %d", (int)type);
            return 0;
    }
    
    return 1;
}

#endif

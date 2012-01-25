/**
 * @file net_ipv4_dhcp.c
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
 * DHCP client module.
 * 
 * Synopsis: net.ipv4.dhcp(string ifname, [list opts])
 * Description:
 *   Runs a DHCP client on a network interface. When an address is obtained,
 *   transitions up (but does not assign anything). If the lease times out,
 *   transitions down.
 *   The interface must already be up.
 *   Supported options (in the opts argument):
 *   - "hostname", (string value): send this hostname to the DHCP server
 *   - "vendorclassid", (string value): send this vendor class identifier
 *   - "auto_clientid": send a client identifier generated from the MAC address
 * Variables:
 *   string addr - assigned IP address ("A.B.C.D")
 *   string prefix - address prefix length ("N")
 *   string gateway - router address ("A.B.C.D"), or "none" if not provided
 *   list(string) dns_servers - DNS server addresses ("A.B.C.D" ...)
 *   string server_mac - MAC address of the DHCP server (6 two-digit caps hexadecimal values
 *     separated with colons, e.g."AB:CD:EF:01:02:03")
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <misc/debug.h>
#include <misc/ipaddr.h>
#include <dhcpclient/BDHCPClient.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_net_ipv4_dhcp.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    BDHCPClient dhcp;
    int up;
};

static void instance_free (struct instance *o);

static void dhcp_handler (struct instance *o, int event)
{
    switch (event) {
        case BDHCPCLIENT_EVENT_UP: {
            ASSERT(!o->up)
            o->up = 1;
            NCDModuleInst_Backend_Up(o->i);
        } break;
        
        case BDHCPCLIENT_EVENT_DOWN: {
            ASSERT(o->up)
            o->up = 0;
            NCDModuleInst_Backend_Down(o->i);
        } break;
        
        case BDHCPCLIENT_EVENT_ERROR: {
            NCDModuleInst_Backend_SetError(o->i);
            instance_free(o);
            return;
        } break;
        
        default: ASSERT(0);
    }
}

static void func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *ifname_arg;
    NCDValue *opts_arg = NULL;
    if (!NCDValue_ListRead(i->args, 1, &ifname_arg) && !NCDValue_ListRead(i->args, 2, &ifname_arg, &opts_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(ifname_arg) != NCDVALUE_STRING || (opts_arg && NCDValue_Type(opts_arg) != NCDVALUE_LIST)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    char *ifname = NCDValue_StringValue(ifname_arg);
    
    struct BDHCPClient_opts opts = {};
    
    // read options
    for (NCDValue *opt = (opts_arg ? NCDValue_ListFirst(opts_arg) : NULL); opt; opt = NCDValue_ListNext(opts_arg, opt)) {
        // read name
        if (NCDValue_Type(opt) != NCDVALUE_STRING) {
            ModuleLog(o->i, BLOG_ERROR, "wrong option name type");
            goto fail1;
        }
        char *optname = NCDValue_StringValue(opt);
        
        if (!strcmp(optname, "hostname") || !strcmp(optname, "vendorclassid")) {
            // read value
            NCDValue *val = NCDValue_ListNext(opts_arg, opt);
            if (!val) {
                ModuleLog(o->i, BLOG_ERROR, "option value missing");
                goto fail1;
            }
            if (NCDValue_Type(val) != NCDVALUE_STRING) {
                ModuleLog(o->i, BLOG_ERROR, "wrong option value type");
                goto fail1;
            }
            char *optval = NCDValue_StringValue(val);
            
            if (!strcmp(optname, "hostname")) {
                opts.hostname = optval;
            } else {
                opts.vendorclassid = optval;
            }
            
            opt = val;
        }
        else if (!strcmp(optname, "auto_clientid")) {
            opts.auto_clientid = 1;
        }
        else {
            ModuleLog(o->i, BLOG_ERROR, "unknown option name");
            goto fail1;
        }
    }
    
    // init DHCP
    if (!BDHCPClient_Init(&o->dhcp, ifname, opts, o->i->reactor, (BDHCPClient_handler)dhcp_handler, o)) {
        ModuleLog(o->i, BLOG_ERROR, "BDHCPClient_Init failed");
        goto fail1;
    }
    
    // set not up
    o->up = 0;
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void instance_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    
    // free DHCP
    BDHCPClient_Free(&o->dhcp);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    instance_free(o);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    ASSERT(o->up)
    
    if (!strcmp(name, "addr")) {
        uint32_t addr;
        BDHCPClient_GetClientIP(&o->dhcp, &addr);
        
        uint8_t *b = (uint8_t *)&addr;
        char str[50];
        sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, b[0], b[1], b[2], b[3]);
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "prefix")) {
        uint32_t addr;
        BDHCPClient_GetClientIP(&o->dhcp, &addr);
        uint32_t mask;
        BDHCPClient_GetClientMask(&o->dhcp, &mask);
        
        struct ipv4_ifaddr ifaddr;
        if (!ipaddr_ipv4_ifaddr_from_addr_mask(addr, mask, &ifaddr)) {
            ModuleLog(o->i, BLOG_ERROR, "bad netmask");
            return 0;
        }
        
        char str[10];
        sprintf(str, "%d", ifaddr.prefix);
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "gateway")) {
        char str[50];
        
        uint32_t addr;
        if (!BDHCPClient_GetRouter(&o->dhcp, &addr)) {
            strcpy(str, "none");
        } else {
            uint8_t *b = (uint8_t *)&addr;
            sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, b[0], b[1], b[2], b[3]);
        }
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "dns_servers")) {
        uint32_t servers[BDHCPCLIENT_MAX_DOMAIN_NAME_SERVERS];
        int num_servers = BDHCPClient_GetDNS(&o->dhcp, servers, BDHCPCLIENT_MAX_DOMAIN_NAME_SERVERS);
        
        NCDValue list;
        NCDValue_InitList(&list);
        
        for (int i = 0; i < num_servers; i++) {
            uint8_t *b = (uint8_t *)&servers[i];
            char str[50];
            sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, b[0], b[1], b[2], b[3]);
            
            NCDValue server;
            if (!NCDValue_InitString(&server, str)) {
                ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
                goto fail1;
            }
            
            if (!NCDValue_ListAppend(&list, server)) {
                ModuleLog(o->i, BLOG_ERROR, "NCDValue_ListAppend failed");
                NCDValue_Free(&server);
                goto fail1;
            }
        }
        
        *out = list;
        return 1;
        
    fail1:
        NCDValue_Free(&list);
        return 0;
    }
    
    if (!strcmp(name, "server_mac")) {
        uint8_t mac[6];
        BDHCPClient_GetServerMAC(&o->dhcp, mac);
        
        char str[18];
        sprintf(str, "%02"PRIX8":%02"PRIX8":%02"PRIX8":%02"PRIX8":%02"PRIX8":%02"PRIX8,
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "net.ipv4.dhcp",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_ipv4_dhcp = {
    .modules = modules
};

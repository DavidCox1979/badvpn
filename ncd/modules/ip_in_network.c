/**
 * @file ip_in_network.c
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
 * Module for checking whether two IP addresses belong to the same network.
 * 
 * Synopsis: ip_in_network(string addr1, string addr2, string netprefix)
 * Variables:
 *   string (empty) - "true" if addr1 and addr2 are in the same network, with
 *     netprefix prefix, "false" if not (IPv4 only).
 */

#include <stdlib.h>
#include <string.h>

#include <misc/ipaddr.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_ip_in_network.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    int value;
};

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
    
    // read arguments
    NCDValue *arg_addr1;
    NCDValue *arg_addr2;
    NCDValue *arg_netprefix;
    if (!NCDValue_ListRead(o->i->args, 3, &arg_addr1, &arg_addr2, &arg_netprefix)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(arg_addr1) != NCDVALUE_STRING || NCDValue_Type(arg_addr2) != NCDVALUE_STRING || NCDValue_Type(arg_netprefix) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // parse
    uint32_t addr1;
    uint32_t addr2;
    int netprefix;
    if (!ipaddr_parse_ipv4_addr(NCDValue_StringValue(arg_addr1), &addr1)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong addr1");
        goto fail1;
    }
    if (!ipaddr_parse_ipv4_addr(NCDValue_StringValue(arg_addr2), &addr2)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong addr2");
        goto fail1;
    }
    if (!ipaddr_parse_ipv4_prefix(NCDValue_StringValue(arg_netprefix), &netprefix)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong netprefix");
        goto fail1;
    }
    
    // test
    o->value = ipaddr_ipv4_addrs_in_network(addr1, addr2, netprefix);
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        const char *v = (o->value ? "true" : "false");
        
        if (!NCDValue_InitString(out, v)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "ip_in_network",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_ip_in_network = {
    .modules = modules
};

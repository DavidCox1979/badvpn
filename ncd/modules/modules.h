/**
 * @file modules.h
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

#ifndef BADVPN_NCD_MODULES_MODULES_H
#define BADVPN_NCD_MODULES_MODULES_H

#include <stddef.h>

#include <ncd/NCDModule.h>

extern const struct NCDModuleGroup ncdmodule_var;
extern const struct NCDModuleGroup ncdmodule_list;
extern const struct NCDModuleGroup ncdmodule_depend;
extern const struct NCDModuleGroup ncdmodule_concat;
extern const struct NCDModuleGroup ncdmodule_concatlist;
extern const struct NCDModuleGroup ncdmodule_if;
extern const struct NCDModuleGroup ncdmodule_strcmp;
extern const struct NCDModuleGroup ncdmodule_logical;
extern const struct NCDModuleGroup ncdmodule_ip_in_network;
extern const struct NCDModuleGroup ncdmodule_run;
extern const struct NCDModuleGroup ncdmodule_net_backend_physical;
extern const struct NCDModuleGroup ncdmodule_net_backend_waitdevice;
extern const struct NCDModuleGroup ncdmodule_net_backend_badvpn;
extern const struct NCDModuleGroup ncdmodule_net_backend_wpa_supplicant;
extern const struct NCDModuleGroup ncdmodule_net_backend_rfkill;
extern const struct NCDModuleGroup ncdmodule_net_dns;
extern const struct NCDModuleGroup ncdmodule_net_iptables;
extern const struct NCDModuleGroup ncdmodule_net_ipv4_addr;
extern const struct NCDModuleGroup ncdmodule_net_ipv4_route;
extern const struct NCDModuleGroup ncdmodule_net_ipv4_dhcp;

static const struct NCDModuleGroup *ncd_modules[] = {
    &ncdmodule_var,
    &ncdmodule_list,
    &ncdmodule_depend,
    &ncdmodule_concat,
    &ncdmodule_concatlist,
    &ncdmodule_if,
    &ncdmodule_strcmp,
    &ncdmodule_logical,
    &ncdmodule_ip_in_network,
    &ncdmodule_run,
    &ncdmodule_net_backend_physical,
    &ncdmodule_net_backend_waitdevice,
    &ncdmodule_net_backend_badvpn,
    &ncdmodule_net_backend_wpa_supplicant,
    &ncdmodule_net_backend_rfkill,
    &ncdmodule_net_dns,
    &ncdmodule_net_iptables,
    &ncdmodule_net_ipv4_addr,
    &ncdmodule_net_ipv4_route,
    &ncdmodule_net_ipv4_dhcp,
    NULL
};

#endif

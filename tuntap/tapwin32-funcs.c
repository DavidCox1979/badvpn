/**
 * @file tapwin32-funcs.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <misc/debug.h>

#include "wintap-common.h"

#include <tuntap/tapwin32-funcs.h>

static int split_spec (char *name, char *sep, char *out_fields[], int num_fields)
{
    ASSERT(num_fields > 0)
    ASSERT(strlen(sep) > 0)
    
    size_t seplen = strlen(sep);
    
    int i;
    for (i = 0; i < num_fields - 1; i++) {
        char *s = strstr(name, sep);
        if (!s) {
            DEBUG("missing separator number %d", (i + 1));
            return 0;
        }
        
        int flen = s - name;
        memcpy(out_fields[i], name, flen);
        out_fields[i][flen] = '\0';
        
        name = s + seplen;
    }
    
    int flen = strlen(name);
    memcpy(out_fields[i], name, flen);
    out_fields[i][flen] = '\0';
    
    return 1;
}

static int parse_ipv4_addr (char *name, uint8_t out_addr[4])
{
    if (strlen(name) > 15) {
        return 0;
    }
    
    char (nums[4])[16];
    
    char *out_fields[] = { nums[0], nums[1], nums[2], nums[3] };
    
    if (!split_spec(name, ".", out_fields, 4)) {
        return 0;
    }
    
    for (int i = 0; i < 4; i++) {
        if (strlen(nums[i]) > 3) {
            return 0;
        }
        
        int num = atoi(nums[i]);
        
        if (!(num >= 0 && num < 256)) {
            return 0;
        }
        
        out_addr[i] = num;
    }
    
    return 1;
}

int tapwin32_parse_tap_spec (char *name, char *out_component_id, char *out_human_name)
{
    char *out_fields[] = { out_component_id, out_human_name };
    
    return split_spec(name, ":", out_fields, 2);
}

int tapwin32_parse_tun_spec (char *name, char *out_component_id, char *out_human_name, uint32_t out_addrs[3])
{
    int namelen = strlen(name);
    
    char (addr_strs[3])[namelen + 1];
    
    char *out_fields[] = { out_component_id, out_human_name, addr_strs[0], addr_strs[1], addr_strs[2] };
    
    if (!split_spec(name, ":", out_fields, 5)) {
        return 0;
    }
    
    for (int i = 0; i < 3; i++) {
        if (!parse_ipv4_addr(addr_strs[i], (uint8_t *)(out_addrs + i))) {
            return 0;
        }
    }
    
    return 1;
}

int tapwin32_find_device (char *device_component_id, char *device_name, char (*device_path)[TAPWIN32_MAX_REG_SIZE])
{
    // open adapter key
    // used to find all devices with the given ComponentId
    HKEY adapter_key;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, &adapter_key) != ERROR_SUCCESS) {
        DEBUG("Error opening adapter key");
        return 0;
    }
    
    char net_cfg_instance_id[TAPWIN32_MAX_REG_SIZE];
    int found = 0;
    
    DWORD i;
    for (i = 0;; i++) {
        DWORD len;
        DWORD type;
        
        char key_name[TAPWIN32_MAX_REG_SIZE];
        len = sizeof(key_name);
        if (RegEnumKeyEx(adapter_key, i, key_name, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
            break;
        }
        
        char unit_string[TAPWIN32_MAX_REG_SIZE];
        snprintf(unit_string, sizeof(unit_string), "%s\\%s", ADAPTER_KEY, key_name);
        HKEY unit_key;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, unit_string, 0, KEY_READ, &unit_key) != ERROR_SUCCESS) {
            continue;
        }
        
        char component_id[TAPWIN32_MAX_REG_SIZE];
        len = sizeof(component_id);
        if (RegQueryValueEx(unit_key, "ComponentId", NULL, &type, component_id, &len) != ERROR_SUCCESS || type != REG_SZ) {
            ASSERT_FORCE(RegCloseKey(unit_key) == ERROR_SUCCESS)
            continue;
        }
        
        len = sizeof(net_cfg_instance_id);
        if (RegQueryValueEx(unit_key, "NetCfgInstanceId", NULL, &type, net_cfg_instance_id, &len) != ERROR_SUCCESS || type != REG_SZ) {
            ASSERT_FORCE(RegCloseKey(unit_key) == ERROR_SUCCESS)
            continue;
        }
        
        RegCloseKey(unit_key);
        
        // check if ComponentId matches
        if (!strcmp(component_id, device_component_id)) {
            // if no name was given, use the first device with the given ComponentId
            if (!device_name) {
                found = 1;
                break;
            }
            
            // open connection key
            char conn_string[TAPWIN32_MAX_REG_SIZE];
            snprintf(conn_string, sizeof(conn_string), "%s\\%s\\Connection", NETWORK_CONNECTIONS_KEY, net_cfg_instance_id);
            HKEY conn_key;
            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, conn_string, 0, KEY_READ, &conn_key) != ERROR_SUCCESS) {
                continue;
            }
            
            // read name
            char name[TAPWIN32_MAX_REG_SIZE];
            len = sizeof(name);
            if (RegQueryValueEx(conn_key, "Name", NULL, &type, name, &len) != ERROR_SUCCESS || type != REG_SZ) {
                ASSERT_FORCE(RegCloseKey(conn_key) == ERROR_SUCCESS)
                continue;
            }
            
            ASSERT_FORCE(RegCloseKey(conn_key) == ERROR_SUCCESS)
            
            // check name
            if (!strcmp(name, device_name)) {
                found = 1;
                break;
            }
        }
    }
    
    ASSERT_FORCE(RegCloseKey(adapter_key) == ERROR_SUCCESS)
    
    if (!found) {
        return 0;
    }
    
    snprintf(*device_path, sizeof(*device_path), "\\\\.\\Global\\%s.tap", net_cfg_instance_id);
    
    return 1;
}

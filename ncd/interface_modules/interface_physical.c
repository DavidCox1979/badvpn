/**
 * @file interface_physical.c
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

#include <stdlib.h>
#include <string.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <ncd/NCDInterfaceModule.h>
#include <ncd/NCDIfConfig.h>
#include <ncd/NCDInterfaceMonitor.h>

#define STATE_WAITDEVICE 1
#define STATE_WAITLINK 2
#define STATE_FINISHED 3

struct instance {
    struct NCDInterfaceModuleNCD params;
    NCDInterfaceMonitor monitor;
    int state;
    DebugObject d_obj;
    #ifndef NDEBUG
    dead_t d_dead;
    #endif
};

static void report_error (struct instance *o)
{
    #ifndef NDEBUG
    DEAD_ENTER(o->d_dead)
    #endif
    
    NCDInterfaceModuleNCD_Event(&o->params, NCDINTERFACEMODULE_EVENT_ERROR);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(o->d_dead);
    #endif
}

static int try_start (struct instance *o)
{
    // query interface state
    int flags = NCDIfConfig_query(o->params.conf->name);
    
    if (!(flags&NCDIFCONFIG_FLAG_EXISTS)) {
        NCDInterfaceModuleNCD_Log(&o->params, BLOG_INFO, "device doesn't exist");
        
        // waiting for device
        o->state = STATE_WAITDEVICE;
    } else {
        if ((flags&NCDIFCONFIG_FLAG_UP)) {
            NCDInterfaceModuleNCD_Log(&o->params, BLOG_ERROR, "device already up - NOT configuring");
            return 0;
        }
        
        // set interface up
        if (!NCDIfConfig_set_up(o->params.conf->name)) {
            NCDInterfaceModuleNCD_Log(&o->params, BLOG_ERROR, "failed to set device up");
            return 0;
        }
        
        NCDInterfaceModuleNCD_Log(&o->params, BLOG_INFO, "waiting for link");
        
        // waiting for link
        o->state = STATE_WAITLINK;
    }
    
    return 1;
}

static void monitor_handler (struct instance *o, const char *ifname, int if_flags)
{
    DebugObject_Access(&o->d_obj);
    
    if (strcmp(ifname, o->params.conf->name)) {
        return;
    }
    
    if (!(if_flags&NCDIFCONFIG_FLAG_EXISTS)) {
        if (o->state > STATE_WAITDEVICE) {
            int prev_state = o->state;
            
            NCDInterfaceModuleNCD_Log(&o->params, BLOG_INFO, "device down");
            
            // set state
            o->state = STATE_WAITDEVICE;
            
            // report
            if (prev_state == STATE_FINISHED) {
                NCDInterfaceModuleNCD_Event(&o->params, NCDINTERFACEMODULE_EVENT_DOWN);
                return;
            }
        }
    } else {
        if (o->state == STATE_WAITDEVICE) {
            NCDInterfaceModuleNCD_Log(&o->params, BLOG_INFO, "device up");
            
            if (!try_start(o)) {
                report_error(o);
                return;
            }
            
            return;
        }
        
        if ((if_flags&NCDIFCONFIG_FLAG_RUNNING)) {
            if (o->state == STATE_WAITLINK) {
                NCDInterfaceModuleNCD_Log(&o->params, BLOG_INFO, "link up");
                
                // set state
                o->state = STATE_FINISHED;
                
                // report
                NCDInterfaceModuleNCD_Event(&o->params, NCDINTERFACEMODULE_EVENT_UP);
                return;
            }
        } else {
            if (o->state == STATE_FINISHED) {
                NCDInterfaceModuleNCD_Log(&o->params, BLOG_INFO, "link down");
                
                // set state
                o->state = STATE_WAITLINK;
                
                // report
                NCDInterfaceModuleNCD_Event(&o->params, NCDINTERFACEMODULE_EVENT_DOWN);
                return;
            }
        }
    }
}

static void * func_new (struct NCDInterfaceModuleNCD params, int *initial_up_state)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        NCDInterfaceModuleNCD_Log(&params, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->params = params;
    
    // init monitor
    if (!NCDInterfaceMonitor_Init(&o->monitor, o->params.reactor, (NCDInterfaceMonitor_handler)monitor_handler, o)) {
        NCDInterfaceModuleNCD_Log(&o->params, BLOG_ERROR, "NCDInterfaceMonitor_Init failed");
        goto fail1;
    }
    
    if (!try_start(o)) {
        goto fail2;
    }
    
    DebugObject_Init(&o->d_obj);
    #ifndef NDEBUG
    DEAD_INIT(o->d_dead);
    #endif
    
    *initial_up_state = 0;
    return o;
    
fail2:
    NCDInterfaceMonitor_Free(&o->monitor);
fail1:
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    DebugObject_Free(&o->d_obj);
    #ifndef NDEBUG
    DEAD_KILL(o->d_dead);
    #endif
    
    // set interface down
    if (o->state > STATE_WAITDEVICE) {
        NCDIfConfig_set_down(o->params.conf->name);
    }
    
    // free monitor
    NCDInterfaceMonitor_Free(&o->monitor);
    
    // free instance
    free(o);
}

const struct NCDInterfaceModule ncd_interface_physical = {
    .type = "physical",
    .func_new = func_new,
    .func_free = func_free
};

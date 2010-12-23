/**
 * @file var.c
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
 * Variable module.
 * 
 * Synopsis: var(value)
 * Variables:
 *   (empty) - value
 */

#include <stdlib.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_var.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDValue *arg;
};

static void * func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    // read argument
    if (!NCDValue_ListRead(o->i->args, 1, &o->arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // free instance
    free(o);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDValue_InitCopy(out, o->arg)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

const struct NCDModule ncdmodule_var = {
    .type = "var",
    .func_new = func_new,
    .func_free = func_free,
    .func_getvar = func_getvar
};

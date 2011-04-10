/**
 * @file list.c
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
 * List construction module.
 * 
 * Synopsis: list(elem1, ..., elemN)
 * Variables:
 *   (empty) - list containing elem1, ..., elemN
 * 
 * Synopsis: list::append(arg)
 * 
 * Synopsis: list::length()
 * Variables:
 *   (empty) - number of elements in list at the time of initialization
 * 
 * Synopsis: list::get(string index)
 * Variables:
 *   (empty) - element of list at position index (starting from zero) at the time of initialization
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_list.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDValue list;
};

struct append_instance {
    NCDModuleInst *i;
};

struct length_instance {
    NCDModuleInst *i;
    uint64_t length;
};

struct get_instance {
    NCDModuleInst *i;
    NCDValue value;
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
    
    // copy list
    if (!NCDValue_InitCopy(&o->list, o->i->args)) {
        ModuleLog(i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail1;
    }
    
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
    
    // free list
    NCDValue_Free(&o->list);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDValue_InitCopy(out, &o->list)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static void append_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct append_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *arg;
    if (!NCDValue_ListRead(o->i->args, 1, &arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // append
    NCDValue v;
    if (!NCDValue_InitCopy(&v, arg)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail1;
    }
    if (!NCDValue_ListAppend(&mo->list, v)) {
        NCDValue_Free(&v);
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_ListAppend failed");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void append_func_die (void *vo)
{
    struct append_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void length_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct length_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(o->i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // count elements
    o->length = 0;
    NCDValue *e = NCDValue_ListFirst(&mo->list);
    while (e) {
        if (o->length == UINT64_MAX) {
            ModuleLog(o->i, BLOG_ERROR, "too many elements");
            goto fail1;
        }
        o->length++;
        e = NCDValue_ListNext(&mo->list, e);
    }
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void length_func_die (void *vo)
{
    struct length_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static int length_func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct length_instance *o = vo;
    
    if (!strcmp(name, "")) {
        char str[30];
        snprintf(str, sizeof(str), "%"PRIu64, o->length);
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}
    
static void get_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct get_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *index_arg;
    if (!NCDValue_ListRead(o->i->args, 1, &index_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(index_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    uint64_t index;
    if (sscanf(NCDValue_StringValue(index_arg), "%"SCNu64, &index) != 1) {
        ModuleLog(o->i, BLOG_ERROR, "wrong value");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // find requested element
    uint64_t pos = 0;
    NCDValue *e = NCDValue_ListFirst(&mo->list);
    while (e) {
        if (pos == index) {
            break;
        }
        pos++;
        e = NCDValue_ListNext(&mo->list, e);
    }
    
    if (!e) {
        ModuleLog(o->i, BLOG_ERROR, "no element at index %"PRIu64, index);
        goto fail1;
    }
    
    // copy value
    if (!NCDValue_InitCopy(&o->value, e)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void get_func_die (void *vo)
{
    struct get_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free value
    NCDValue_Free(&o->value);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static int get_func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct get_instance *o = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDValue_InitCopy(out, &o->value)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "list",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "list::append",
        .func_new = append_func_new,
        .func_die = append_func_die
    }, {
        .type = "list::length",
        .func_new = length_func_new,
        .func_die = length_func_die,
        .func_getvar = length_func_getvar
    }, {
        .type = "list::get",
        .func_new = get_func_new,
        .func_die = get_func_die,
        .func_getvar = get_func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_list = {
    .modules = modules
};

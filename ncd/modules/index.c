/**
 * @file index.c
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
 * 
 * @section DESCRIPTION
 * 
 * Synopsis:
 *   index index(string value)
 *   index index::next()
 * 
 * Description:
 *   Non-negative integer with range of a size_t.
 *   The first form creates an index from the given decimal string.
 *   The second form cretes an index with value one more than an existing
 *   index.
 * 
 * Variables:
 *   string (empty) - the index value. Note this may be different from
 *     than the value given to index() if it was not in normal form.
 */

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include <misc/parse_number.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_index.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    size_t value;
};

static void func_new_templ (NCDModuleInst *i, size_t value)
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
    o->value = value;
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_from_value (NCDModuleInst *i)
{
    // read arguments
    NCDValRef arg_value;
    if (!NCDVal_ListRead(i->args, 1, &arg_value)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(arg_value)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // parse value
    uintmax_t value;
    if (!parse_unsigned_integer(NCDVal_StringValue(arg_value), &value)) {
        ModuleLog(i, BLOG_ERROR, "wrong value");
        goto fail0;
    }
    
    // check overflow
    if (value > SIZE_MAX) {
        ModuleLog(i, BLOG_ERROR, "value too large");
        goto fail0;
    }
    
    func_new_templ(i, value);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_from_index (NCDModuleInst *i)
{
    struct instance *index = NCDModuleInst_Backend_GetUser((NCDModuleInst *)i->method_user);
    
    // check overflow
    if (index->value == SIZE_MAX) {
        ModuleLog(i, BLOG_ERROR, "overflow");
        goto fail0;
    }
    
    func_new_templ(i, index->value + 1);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        char str[64];
        snprintf(str, sizeof(str), "%zu", o->value);
        
        *out = NCDVal_NewString(mem, str);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewString failed");
        }
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "index",
        .func_new = func_new_from_value,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "index::next",
        .base_type = "index",
        .func_new = func_new_from_index,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_index = {
    .modules = modules
};

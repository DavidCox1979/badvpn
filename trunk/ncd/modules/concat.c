/**
 * @file concat.c
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
 * String concatenation module.
 * 
 * Synopsis: concat(string elem1, ..., string elemN)
 * Variables:
 *   string (empty) - elem1, ..., elemN concatenated
 */

#include <stddef.h>
#include <string.h>

#include <misc/balloc.h>
#include <misc/offset.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDRefTarget.h>
#include <ncd/static_strings.h>

#include <generated/blog_channel_ncd_concat.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct result {
    NCDRefTarget ref_target;
    size_t length;
    char data[];
};

struct instance {
    NCDModuleInst *i;
    struct result *result;
};

static void result_ref_target_func_release (NCDRefTarget *ref_target)
{
    struct result *result = UPPER_OBJECT(ref_target, struct result, ref_target);
    
    BFree(result);
}

static void func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct instance *o = vo;
    o->i = i;
    
    size_t count = NCDVal_ListCount(params->args);
    bsize_t result_size = bsize_fromsize(sizeof(struct result));
    
    // check arguments and compute result size
    for (size_t j = 0; j < count; j++) {
        NCDValRef arg = NCDVal_ListGet(params->args, j);
        
        if (!NCDVal_IsString(arg)) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail0;
        }
        
        result_size = bsize_add(result_size, bsize_fromsize(NCDVal_StringLength(arg)));
    }
    
    // allocate result
    o->result = BAllocSize(result_size);
    if (!o->result) {
        ModuleLog(i, BLOG_ERROR, "BAllocSize failed");
        goto fail0;
    }
    
    // init ref target
    NCDRefTarget_Init(&o->result->ref_target, result_ref_target_func_release);
    
    // copy data to result
    o->result->length = 0;
    for (size_t j = 0; j < count; j++) {
        NCDValRef arg = NCDVal_ListGet(params->args, j);
        size_t this_len = NCDVal_StringLength(arg);
        memcpy(o->result->data + o->result->length, NCDVal_StringData(arg), this_len);
        o->result->length += this_len;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    // release result reference
    NCDRefTarget_Deref(&o->result->ref_target);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static int func_getvar2 (void *vo, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    
    if (name == NCD_STRING_EMPTY) {
        *out = NCDVal_NewExternalString(mem, o->result->data, o->result->length, &o->result->ref_target);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewExternalString failed");
        }
        return 1;
    }
    
    return 0;
}

static struct NCDModule modules[] = {
    {
        .type = "concat",
        .func_new2 = func_new,
        .func_die = func_die,
        .func_getvar2 = func_getvar2,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_concat = {
    .modules = modules
};

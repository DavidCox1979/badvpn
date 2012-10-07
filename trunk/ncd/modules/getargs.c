/**
 * @file getargs.c
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
 *   getargs()
 * 
 * Variables:
 *   (empty) - list of extra command line arguments that were passed to the intrepreter
 */

#include <string.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_getargs.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

static void func_new (void *unused, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    NCDModuleInst_Backend_SetUser(i, i);
    
    // check arguments
    if (!NCDVal_ListRead(params->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static int func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    NCDModuleInst *i = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDModuleInst_Backend_InterpGetArgs(i, mem, out)) {
            ModuleLog(i, BLOG_ERROR, "NCDModuleInst_Backend_InterpGetArgs failed");
            return 0;
        }
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "getargs",
        .func_new2 = func_new,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_getargs = {
    .modules = modules
};

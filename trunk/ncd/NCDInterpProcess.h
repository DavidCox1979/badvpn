/**
 * @file NCDInterpProcess.h
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

#ifndef BADVPN_NCDINTERPPROCESS_H
#define BADVPN_NCDINTERPPROCESS_H

#include <stddef.h>

#include <misc/debug.h>
#include <structure/CStringTrie.h>
#include <base/DebugObject.h>
#include <ncd/NCDAst.h>
#include <ncd/NCDVal.h>
#include <ncd/NCDPlaceholderDb.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDModuleIndex.h>
#include <ncd/NCDMethodIndex.h>

#include "NCDInterpProcess_trie.h"
#include <structure/CStringTrie_decl.h>

struct NCDInterpProcess__stmt {
    const char *name;
    const char *cmdname;
    char *objnames;
    size_t num_objnames;
    char *arg_data;
    size_t arg_len;
    union {
        const struct NCDModule *simple_module;
        int method_name_id;
    } binding;
    NCDValSafeRef arg_ref;
    NCDValReplaceProg arg_prog;
    int alloc_size;
    int prealloc_offset;
    int trie_next;
};

typedef struct {
    struct NCDInterpProcess__stmt *stmts;
    int num_stmts;
    int prealloc_size;
    NCDInterpProcess__Trie trie;
    NCDProcess *process;
    DebugObject d_obj;
} NCDInterpProcess;

int NCDInterpProcess_Init (NCDInterpProcess *o, NCDProcess *process, NCDPlaceholderDb *pdb, NCDModuleIndex *module_index, NCDMethodIndex *method_index) WARN_UNUSED;
void NCDInterpProcess_Free (NCDInterpProcess *o);
int NCDInterpProcess_FindStatement (NCDInterpProcess *o, int from_index, const char *name);
const char * NCDInterpProcess_StatementCmdName (NCDInterpProcess *o, int i);
void NCDInterpProcess_StatementObjNames (NCDInterpProcess *o, int i, const char **out_objnames, size_t *out_num_objnames);
const struct NCDModule * NCDInterpProcess_StatementGetSimpleModule (NCDInterpProcess *o, int i);
const struct NCDModule * NCDInterpProcess_StatementGetMethodModule (NCDInterpProcess *o, int i, const char *obj_type, NCDMethodIndex *method_index);
int NCDInterpProcess_CopyStatementArgs (NCDInterpProcess *o, int i, NCDValMem *out_valmem, NCDValRef *out_val, NCDValReplaceProg *out_prog) WARN_UNUSED;
void NCDInterpProcess_StatementBumpAllocSize (NCDInterpProcess *o, int i, int alloc_size);
int NCDInterpProcess_StatementPreallocSize (NCDInterpProcess *o, int i);
int NCDInterpProcess_PreallocSize (NCDInterpProcess *o);
int NCDInterpProcess_StatementPreallocOffset (NCDInterpProcess *o, int i);
NCDProcess * NCDInterpProcess_Process (NCDInterpProcess *o);

#endif

/**
 * @file NCDStringIndex.h
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

#ifndef BADVPN_NCD_STRING_INDEX_H
#define BADVPN_NCD_STRING_INDEX_H

#include <limits.h>

#include <misc/debug.h>
#include <structure/CHash.h>
#include <base/DebugObject.h>

#define NCDSTRINGINDEX_INITIAL_CAPACITY 1
#define NCDSTRINGINDEX_HASH_BUCKETS 100

typedef int NCD_string_id_t;

#define NCD_STRING_ID_MAX INT_MAX

struct NCDStringIndex__entry {
    char *str;
    NCD_string_id_t hash_next;
};

typedef const char *NCDStringIndex_hash_key;
typedef struct NCDStringIndex__entry *NCDStringIndex_hash_arg;

#include "NCDStringIndex_hash.h"
#include <structure/CHash_decl.h>

#define NCD_EMPTY_STRING_ID ((NCD_string_id_t)0)

typedef struct {
    struct NCDStringIndex__entry *entries;
    NCD_string_id_t entries_capacity;
    NCD_string_id_t entries_size;
    NCDStringIndex__Hash hash;
    DebugObject d_obj;
} NCDStringIndex;

struct NCD_string_request {
    const char *str;
    NCD_string_id_t id;
};

int NCDStringIndex_Init (NCDStringIndex *o) WARN_UNUSED;
void NCDStringIndex_Free (NCDStringIndex *o);
NCD_string_id_t NCDStringIndex_Lookup (NCDStringIndex *o, const char *str);
NCD_string_id_t NCDStringIndex_Get (NCDStringIndex *o, const char *str);
const char * NCDStringIndex_Value (NCDStringIndex *o, NCD_string_id_t id);
int NCDStringIndex_GetRequests (NCDStringIndex *o, struct NCD_string_request *requests) WARN_UNUSED;

#endif

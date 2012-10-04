/**
 * @file NCDAst.c
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

#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <misc/offset.h>

#include "NCDAst.h"

struct NCDValue__list_element {
    LinkedList1Node list_node;
    NCDValue v;
};

struct NCDValue__map_element {
    LinkedList1Node list_node;
    NCDValue key;
    NCDValue val;
};

static void value_assert (NCDValue *o)
{
    switch (o->type) {
        case NCDVALUE_STRING:
        case NCDVALUE_LIST:
        case NCDVALUE_MAP:
        case NCDVALUE_VAR:
            return;
        default:
            ASSERT(0);
    }
}

void NCDValue_Free (NCDValue *o)
{
    switch (o->type) {
        case NCDVALUE_STRING: {
            free(o->string);
        } break;
        
        case NCDVALUE_LIST: {
            LinkedList1Node *n;
            while (n = LinkedList1_GetFirst(&o->list)) {
                struct NCDValue__list_element *e = UPPER_OBJECT(n, struct NCDValue__list_element, list_node);
                
                NCDValue_Free(&e->v);
                LinkedList1_Remove(&o->list, &e->list_node);
                free(e);
            }
        } break;
        
        case NCDVALUE_MAP: {
            LinkedList1Node *n;
            while (n = LinkedList1_GetFirst(&o->map_list)) {
                struct NCDValue__map_element *e = UPPER_OBJECT(n, struct NCDValue__map_element, list_node);
                
                LinkedList1_Remove(&o->map_list, &e->list_node);
                NCDValue_Free(&e->key);
                NCDValue_Free(&e->val);
                free(e);
            }
        } break;
        
        case NCDVALUE_VAR: {
            free(o->var_name);
        } break;
        
        default:
            ASSERT(0);
    }
}

int NCDValue_Type (NCDValue *o)
{
    value_assert(o);
    
    return o->type;
}

int NCDValue_InitString (NCDValue *o, const char *str)
{
    return NCDValue_InitStringBin(o, (const uint8_t *)str, strlen(str));
}

int NCDValue_InitStringBin (NCDValue *o, const uint8_t *str, size_t len)
{
    if (len == SIZE_MAX) {
        return 0;
    }
    
    if (!(o->string = malloc(len + 1))) {
        return 0;
    }
    
    memcpy(o->string, str, len);
    o->string[len] = '\0';
    o->string_len = len;
    
    o->type = NCDVALUE_STRING;
    
    return 1;
}

char * NCDValue_StringValue (NCDValue *o)
{
    ASSERT(o->type == NCDVALUE_STRING)
    
    return (char *)o->string;
}

size_t NCDValue_StringLength (NCDValue *o)
{
    ASSERT(o->type == NCDVALUE_STRING)
    
    return o->string_len;
}

void NCDValue_InitList (NCDValue *o)
{
    o->type = NCDVALUE_LIST;
    LinkedList1_Init(&o->list);
    o->list_count = 0;
}

size_t NCDValue_ListCount (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    return o->list_count;
}

int NCDValue_ListAppend (NCDValue *o, NCDValue v)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    value_assert(&v);
    
    if (o->list_count == SIZE_MAX) {
        return 0;
    }
    
    struct NCDValue__list_element *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    e->v = v;
    LinkedList1_Append(&o->list, &e->list_node);
    
    o->list_count++;
    
    return 1;
}

int NCDValue_ListPrepend (NCDValue *o, NCDValue v)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    value_assert(&v);
    
    if (o->list_count == SIZE_MAX) {
        return 0;
    }
    
    struct NCDValue__list_element *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    e->v = v;
    LinkedList1_Prepend(&o->list, &e->list_node);
    
    o->list_count++;
    
    return 1;
}

NCDValue * NCDValue_ListFirst (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->list);
    
    if (!ln) {
        return NULL;
    }
    
    struct NCDValue__list_element *e = UPPER_OBJECT(ln, struct NCDValue__list_element, list_node);
    
    return &e->v;
}

NCDValue * NCDValue_ListNext (NCDValue *o, NCDValue *ev)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    struct NCDValue__list_element *cur_e = UPPER_OBJECT(ev, struct NCDValue__list_element, v);
    LinkedList1Node *ln = LinkedList1Node_Next(&cur_e->list_node);
    
    if (!ln) {
        return NULL;
    }
    
    struct NCDValue__list_element *e = UPPER_OBJECT(ln, struct NCDValue__list_element, list_node);
    
    return &e->v;
}

void NCDValue_InitMap (NCDValue *o)
{
    o->type = NCDVALUE_MAP;
    LinkedList1_Init(&o->map_list);
    o->map_count = 0;
}

size_t NCDValue_MapCount (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    
    return o->map_count;
}

NCDValue * NCDValue_MapPrepend (NCDValue *o, NCDValue key, NCDValue val)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    value_assert(&key);
    value_assert(&val);
    
    if (o->map_count == SIZE_MAX) {
        return NULL;
    }
    
    struct NCDValue__map_element *e = malloc(sizeof(*e));
    if (!e) {
        return NULL;
    }
    
    e->key = key;
    e->val = val;
    LinkedList1_Prepend(&o->map_list, &e->list_node);
    
    o->map_count++;
    
    return &e->key;
}

NCDValue * NCDValue_MapFirstKey (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->map_list);
    
    if (!ln) {
        return NULL;
    }
    
    struct NCDValue__map_element *e = UPPER_OBJECT(ln, struct NCDValue__map_element, list_node);
    
    value_assert(&e->key);
    value_assert(&e->val);
    
    return &e->key;
}

NCDValue * NCDValue_MapNextKey (NCDValue *o, NCDValue *ekey)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    
    struct NCDValue__map_element *e0 = UPPER_OBJECT(ekey, struct NCDValue__map_element, key);
    value_assert(&e0->key);
    value_assert(&e0->val);
    
    LinkedList1Node *ln = LinkedList1Node_Next(&e0->list_node);
    
    if (!ln) {
        return NULL;
    }
    
    struct NCDValue__map_element *e = UPPER_OBJECT(ln, struct NCDValue__map_element, list_node);
    
    value_assert(&e->key);
    value_assert(&e->val);
    
    return &e->key;
}

NCDValue * NCDValue_MapKeyValue (NCDValue *o, NCDValue *ekey)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    
    struct NCDValue__map_element *e = UPPER_OBJECT(ekey, struct NCDValue__map_element, key);
    value_assert(&e->key);
    value_assert(&e->val);
    
    return &e->val;
}

int NCDValue_InitVar (NCDValue *o, const char *var_name)
{
    ASSERT(var_name)
    
    if (!(o->var_name = strdup(var_name))) {
        return 0;
    }
    
    o->type = NCDVALUE_VAR;
    
    return 1;
}

const char * NCDValue_VarName (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_VAR)
    
    return o->var_name;
}

int NCDValue_Compare (NCDValue *o, NCDValue *v)
{
    value_assert(o);
    value_assert(v);
    
    int cmp = (o->type > v->type) - (o->type < v->type);
    if (cmp) {
        return cmp;
    }
    
    ASSERT(o->type == v->type)
    
    if (o->type == NCDVALUE_STRING) {
        size_t min_len = o->string_len < v->string_len ? o->string_len : v->string_len;
        
        int cmp = memcmp(o->string, v->string, min_len);
        if (cmp) {
            return (cmp > 0) - (cmp < 0);
        }
        
        return (o->string_len > v->string_len) - (o->string_len < v->string_len);
    }
    
    if (o->type == NCDVALUE_LIST) {
        NCDValue *x = NCDValue_ListFirst(o);
        NCDValue *y = NCDValue_ListFirst(v);
        
        while (1) {
            if (!x && y) {
                return -1;
            }
            if (x && !y) {
                return 1;
            }
            if (!x && !y) {
                return 0;
            }
            
            int res = NCDValue_Compare(x, y);
            if (res) {
                return res;
            }
            
            x = NCDValue_ListNext(o, x);
            y = NCDValue_ListNext(v, y);
        }
    }
    
    if (o->type == NCDVALUE_MAP) {
        NCDValue *key1 = NCDValue_MapFirstKey(o);
        NCDValue *key2 = NCDValue_MapFirstKey(v);
        
        while (1) {
            if (!key1 && key2) {
                return -1;
            }
            if (key1 && !key2) {
                return 1;
            }
            if (!key1 && !key2) {
                return 0;
            }
            
            int res = NCDValue_Compare(key1, key2);
            if (res) {
                return res;
            }
            
            NCDValue *val1 = NCDValue_MapKeyValue(o, key1);
            NCDValue *val2 = NCDValue_MapKeyValue(v, key2);
            
            res = NCDValue_Compare(val1, val2);
            if (res) {
                return res;
            }
            
            key1 = NCDValue_MapNextKey(o, key1);
            key2 = NCDValue_MapNextKey(v, key2);
        }
    }
    
    if (o->type == NCDVALUE_VAR) {
        int res = strcmp(o->var_name, v->var_name);
        
        return (res > 0) - (res < 0);
    }
    
    ASSERT(0)
    return 0;
}

void NCDProgram_Init (NCDProgram *o)
{
    LinkedList1_Init(&o->processes_list);
    o->num_processes = 0;
}

void NCDProgram_Free (NCDProgram *o)
{
    LinkedList1Node *ln;
    while (ln = LinkedList1_GetFirst(&o->processes_list)) {
        struct ProgramProcess *e = UPPER_OBJECT(ln, struct ProgramProcess, processes_list_node);
        NCDProcess_Free(&e->p);
        LinkedList1_Remove(&o->processes_list, &e->processes_list_node);
        free(e);
    }
}

NCDProcess * NCDProgram_PrependProcess (NCDProgram *o, NCDProcess p)
{
    if (o->num_processes == SIZE_MAX) {
        return NULL;
    }
    
    struct ProgramProcess *e = malloc(sizeof(*e));
    if (!e) {
        return NULL;
    }
    
    LinkedList1_Prepend(&o->processes_list, &e->processes_list_node);
    e->p = p;
    
    o->num_processes++;
    
    return &e->p;
}

NCDProcess * NCDProgram_FirstProcess (NCDProgram *o)
{
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->processes_list);
    if (!ln) {
        return NULL;
    }
    
    struct ProgramProcess *e = UPPER_OBJECT(ln, struct ProgramProcess, processes_list_node);
    
    return &e->p;
}

NCDProcess * NCDProgram_NextProcess (NCDProgram *o, NCDProcess *ep)
{
    ASSERT(ep)
    
    struct ProgramProcess *cur_e = UPPER_OBJECT(ep, struct ProgramProcess, p);
    
    LinkedList1Node *ln = LinkedList1Node_Next(&cur_e->processes_list_node);
    if (!ln) {
        return NULL;
    }
    
    struct ProgramProcess *e = UPPER_OBJECT(ln, struct ProgramProcess, processes_list_node);
    
    return &e->p;
}

size_t NCDProgram_NumProcesses (NCDProgram *o)
{
    return o->num_processes;
}

int NCDProcess_Init (NCDProcess *o, int is_template, const char *name, NCDBlock block)
{
    ASSERT(is_template == !!is_template)
    ASSERT(name)
    
    if (!(o->name = strdup(name))) {
        return 0;
    }
    
    o->is_template = is_template;
    o->block = block;
    
    return 1;
}

void NCDProcess_Free (NCDProcess *o)
{
    NCDBlock_Free(&o->block);
    free(o->name);
}

int NCDProcess_IsTemplate (NCDProcess *o)
{
    return o->is_template;
}

const char * NCDProcess_Name (NCDProcess *o)
{
    return o->name;
}

NCDBlock * NCDProcess_Block (NCDProcess *o)
{
    return &o->block;
}

void NCDBlock_Init (NCDBlock *o)
{
    LinkedList1_Init(&o->statements_list);
    o->count = 0;
}

void NCDBlock_Free (NCDBlock *o)
{
    LinkedList1Node *ln;
    while (ln = LinkedList1_GetFirst(&o->statements_list)) {
        struct BlockStatement *e = UPPER_OBJECT(ln, struct BlockStatement, statements_list_node);
        NCDStatement_Free(&e->s);
        LinkedList1_Remove(&o->statements_list, &e->statements_list_node);
        free(e);
    }
}

int NCDBlock_PrependStatement (NCDBlock *o, NCDStatement s)
{
    return NCDBlock_InsertStatementAfter(o, NULL, s);
}

int NCDBlock_InsertStatementAfter (NCDBlock *o, NCDStatement *after, NCDStatement s)
{
    struct BlockStatement *after_e = NULL;
    if (after) {
        after_e = UPPER_OBJECT(after, struct BlockStatement, s);
    }
    
    if (o->count == SIZE_MAX) {
        return 0;
    }
    
    struct BlockStatement *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    if (after_e) {
        LinkedList1_InsertAfter(&o->statements_list, &e->statements_list_node, &after_e->statements_list_node);
    } else {
        LinkedList1_Prepend(&o->statements_list, &e->statements_list_node);
    }
    e->s = s;
    
    o->count++;
    
    return 1;
}

NCDStatement * NCDBlock_ReplaceStatement (NCDBlock *o, NCDStatement *es, NCDStatement s)
{
    ASSERT(es)
    
    struct BlockStatement *e = UPPER_OBJECT(es, struct BlockStatement, s);
    
    NCDStatement_Free(&e->s);
    e->s = s;
    
    return &e->s;
}

NCDStatement * NCDBlock_FirstStatement (NCDBlock *o)
{
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->statements_list);
    if (!ln) {
        return NULL;
    }
    
    struct BlockStatement *e = UPPER_OBJECT(ln, struct BlockStatement, statements_list_node);
    
    return &e->s;
}

NCDStatement * NCDBlock_NextStatement (NCDBlock *o, NCDStatement *es)
{
    ASSERT(es)
    
    struct BlockStatement *cur_e = UPPER_OBJECT(es, struct BlockStatement, s);
    
    LinkedList1Node *ln = LinkedList1Node_Next(&cur_e->statements_list_node);
    if (!ln) {
        return NULL;
    }
    
    struct BlockStatement *e = UPPER_OBJECT(ln, struct BlockStatement, statements_list_node);
    
    return &e->s;
}

size_t NCDBlock_NumStatements (NCDBlock *o)
{
    return o->count;
}

int NCDStatement_InitReg (NCDStatement *o, const char *name, const char *objname, const char *cmdname, NCDValue args)
{
    ASSERT(cmdname)
    ASSERT(NCDValue_Type(&args) == NCDVALUE_LIST)
    
    o->name = NULL;
    o->reg.objname = NULL;
    o->reg.cmdname = NULL;
    
    if (name && !(o->name = strdup(name))) {
        goto fail;
    }
    
    if (objname && !(o->reg.objname = strdup(objname))) {
        goto fail;
    }
    
    if (!(o->reg.cmdname = strdup(cmdname))) {
        goto fail;
    }
    
    o->type = NCDSTATEMENT_REG;
    o->reg.args = args;
    
    return 1;
    
fail:
    free(o->name);
    free(o->reg.objname);
    free(o->reg.cmdname);
    return 0;
}

int NCDStatement_InitIf (NCDStatement *o, const char *name, NCDIfBlock ifblock)
{
    o->name = NULL;
    
    if (name && !(o->name = strdup(name))) {
        return 0;
    }
    
    o->type = NCDSTATEMENT_IF;
    o->ifc.ifblock = ifblock;
    o->ifc.have_else = 0;
    
    return 1;
}

int NCDStatement_InitForeach (NCDStatement *o, const char *name, NCDValue collection, const char *name1, const char *name2, NCDBlock block)
{
    ASSERT(name1)
    
    o->name = NULL;
    o->foreach.name1 = NULL;
    o->foreach.name2 = NULL;
    
    if (name && !(o->name = strdup(name))) {
        goto fail;
    }
    
    if (!(o->foreach.name1 = strdup(name1))) {
        goto fail;
    }
    
    if (name2 && !(o->foreach.name2 = strdup(name2))) {
        goto fail;
    }
    
    o->type = NCDSTATEMENT_FOREACH;
    o->foreach.collection = collection;
    o->foreach.block = block;
    o->foreach.is_grabbed = 0;
    
    return 1;
    
fail:
    free(o->name);
    free(o->foreach.name1);
    free(o->foreach.name2);
    return 0;
}

void NCDStatement_Free (NCDStatement *o)
{
    switch (o->type) {
        case NCDSTATEMENT_REG: {
            NCDValue_Free(&o->reg.args);
            free(o->reg.cmdname);
            free(o->reg.objname);
        } break;
        
        case NCDSTATEMENT_IF: {
            if (o->ifc.have_else) {
                NCDBlock_Free(&o->ifc.else_block);
            }
            
            NCDIfBlock_Free(&o->ifc.ifblock);
        } break;
        
        case NCDSTATEMENT_FOREACH: {
            if (!o->foreach.is_grabbed) {
                NCDBlock_Free(&o->foreach.block);
                NCDValue_Free(&o->foreach.collection);
            }
            free(o->foreach.name2);
            free(o->foreach.name1);
        } break;
        
        default: ASSERT(0);
    }
    
    free(o->name);
}

int NCDStatement_Type (NCDStatement *o)
{
    return o->type;
}

const char * NCDStatement_Name (NCDStatement *o)
{
    return o->name;
}

const char * NCDStatement_RegObjName (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_REG)
    
    return o->reg.objname;
}

const char * NCDStatement_RegCmdName (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_REG)
    
    return o->reg.cmdname;
}

NCDValue * NCDStatement_RegArgs (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_REG)
    
    return &o->reg.args;
}

NCDIfBlock * NCDStatement_IfBlock (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_IF)
    
    return &o->ifc.ifblock;
}

void NCDStatement_IfAddElse (NCDStatement *o, NCDBlock else_block)
{
    ASSERT(o->type == NCDSTATEMENT_IF)
    ASSERT(!o->ifc.have_else)
    
    o->ifc.have_else = 1;
    o->ifc.else_block = else_block;
}

NCDBlock * NCDStatement_IfElse (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_IF)
    
    if (!o->ifc.have_else) {
        return NULL;
    }
    
    return &o->ifc.else_block;
}

NCDBlock NCDStatement_IfGrabElse (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_IF)
    ASSERT(o->ifc.have_else)
    
    o->ifc.have_else = 0;
    
    return o->ifc.else_block;
}

NCDValue * NCDStatement_ForeachCollection (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_FOREACH)
    ASSERT(!o->foreach.is_grabbed)
    
    return &o->foreach.collection;
}

const char * NCDStatement_ForeachName1 (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_FOREACH)
    
    return o->foreach.name1;
}

const char * NCDStatement_ForeachName2 (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_FOREACH)
    
    return o->foreach.name2;
}

NCDBlock * NCDStatement_ForeachBlock (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_FOREACH)
    ASSERT(!o->foreach.is_grabbed)
    
    return &o->foreach.block;
}

void NCDStatement_ForeachGrab (NCDStatement *o, NCDValue *out_collection, NCDBlock *out_block)
{
    ASSERT(o->type == NCDSTATEMENT_FOREACH)
    ASSERT(!o->foreach.is_grabbed)
    
    *out_collection = o->foreach.collection;
    *out_block = o->foreach.block;
    o->foreach.is_grabbed = 1;
}

void NCDIfBlock_Init (NCDIfBlock *o)
{
    LinkedList1_Init(&o->ifs_list);
}

void NCDIfBlock_Free (NCDIfBlock *o)
{
    LinkedList1Node *ln;
    while (ln = LinkedList1_GetFirst(&o->ifs_list)) {
        struct IfBlockIf *e = UPPER_OBJECT(ln, struct IfBlockIf, ifs_list_node);
        NCDIf_Free(&e->ifc);
        LinkedList1_Remove(&o->ifs_list, &e->ifs_list_node);
        free(e);
    }
}

int NCDIfBlock_PrependIf (NCDIfBlock *o, NCDIf ifc)
{
    struct IfBlockIf *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    LinkedList1_Prepend(&o->ifs_list, &e->ifs_list_node);
    e->ifc = ifc;
    
    return 1;
}

NCDIf * NCDIfBlock_FirstIf (NCDIfBlock *o)
{
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->ifs_list);
    if (!ln) {
        return NULL;
    }
    
    struct IfBlockIf *e = UPPER_OBJECT(ln, struct IfBlockIf, ifs_list_node);
    
    return &e->ifc;
}

NCDIf * NCDIfBlock_NextIf (NCDIfBlock *o, NCDIf *ei)
{
    ASSERT(ei)
    
    struct IfBlockIf *cur_e = UPPER_OBJECT(ei, struct IfBlockIf, ifc);
    
    LinkedList1Node *ln = LinkedList1Node_Next(&cur_e->ifs_list_node);
    if (!ln) {
        return NULL;
    }
    
    struct IfBlockIf *e = UPPER_OBJECT(ln, struct IfBlockIf, ifs_list_node);
    
    return &e->ifc;
}

NCDIf NCDIfBlock_GrabIf (NCDIfBlock *o, NCDIf *ei)
{
    ASSERT(ei)
    
    struct IfBlockIf *e = UPPER_OBJECT(ei, struct IfBlockIf, ifc);
    
    NCDIf old_ifc = e->ifc;
    
    LinkedList1_Remove(&o->ifs_list, &e->ifs_list_node);
    free(e);
    
    return old_ifc;
}

void NCDIf_Init (NCDIf *o, NCDValue cond, NCDBlock block)
{
    o->cond = cond;
    o->block = block;
}

void NCDIf_Free (NCDIf *o)
{
    NCDValue_Free(&o->cond);
    NCDBlock_Free(&o->block);
}

void NCDIf_FreeGrab (NCDIf *o, NCDValue *out_cond, NCDBlock *out_block)
{
    *out_cond = o->cond;
    *out_block = o->block;
}

NCDValue * NCDIf_Cond (NCDIf *o)
{
    return &o->cond;
}

NCDBlock * NCDIf_Block (NCDIf *o)
{
    return &o->block;
}

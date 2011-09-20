/**
 * @file NCDValue.c
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
#include <stdarg.h>
#include <inttypes.h>

#include <misc/debug.h>
#include <misc/offset.h>

#include <ncd/NCDValue.h>

static void value_assert (NCDValue *o)
{
    switch (o->type) {
        case NCDVALUE_STRING:
        case NCDVALUE_LIST:
            return;
        default:
            ASSERT(0);
    }
}

int NCDValue_InitCopy (NCDValue *o, NCDValue *v)
{
    value_assert(v);
    
    switch (v->type) {
        case NCDVALUE_STRING: {
            return NCDValue_InitString(o, v->string);
        } break;
        
        case NCDVALUE_LIST: {
            NCDValue_InitList(o);
            
            LinkedList2Iterator it;
            LinkedList2Iterator_InitForward(&it, &v->list);
            LinkedList2Node *n;
            while (n = LinkedList2Iterator_Next(&it)) {
                NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
                
                NCDValue tmp;
                if (!NCDValue_InitCopy(&tmp, &e->v)) {
                    goto fail;
                }
                
                if (!NCDValue_ListAppend(o, tmp)) {
                    NCDValue_Free(&tmp);
                    goto fail;
                }
            }
            
            return 1;
            
        fail:
            LinkedList2Iterator_Free(&it);
            NCDValue_Free(o);
            return 0;
        } break;
        
        default:
            ASSERT(0);
    }
    
    return 0;
}

void NCDValue_Free (NCDValue *o)
{
    switch (o->type) {
        case NCDVALUE_STRING: {
            free(o->string);
        } break;
        
        case NCDVALUE_LIST: {
            LinkedList2Node *n;
            while (n = LinkedList2_GetFirst(&o->list)) {
                NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
                
                NCDValue_Free(&e->v);
                LinkedList2_Remove(&o->list, &e->list_node);
                free(e);
            }
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
    size_t len = strlen(str);
    
    if (!(o->string = malloc(len + 1))) {
        return 0;
    }
    
    memcpy(o->string, str, len);
    o->string[len] = '\0';
    
    o->type = NCDVALUE_STRING;
    
    return 1;
}

char * NCDValue_StringValue (NCDValue *o)
{
    ASSERT(o->type == NCDVALUE_STRING)
    
    return o->string;
}

void NCDValue_InitList (NCDValue *o)
{
    LinkedList2_Init(&o->list);
    o->list_count = 0;
    
    o->type = NCDVALUE_LIST;
}

int NCDValue_ListAppend (NCDValue *o, NCDValue v)
{
    value_assert(o);
    value_assert(&v);
    ASSERT(o->type == NCDVALUE_LIST)
    
    if (o->list_count == SIZE_MAX) {
        return 0;
    }
    
    NCDListElement *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    LinkedList2_Append(&o->list, &e->list_node);
    o->list_count++;
    e->v = v;
    
    return 1;
}

int NCDValue_ListAppendList (NCDValue *o, NCDValue l)
{
    value_assert(o);
    value_assert(&l);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(l.type == NCDVALUE_LIST)
    
    if (l.list_count > SIZE_MAX - o->list_count) {
        return 0;
    }
    
    LinkedList2Node *n;
    while (n = LinkedList2_GetFirst(&l.list)) {
        NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
        LinkedList2_Remove(&l.list, &e->list_node);
        LinkedList2_Append(&o->list, &e->list_node);
    }
    
    o->list_count += l.list_count;
    
    return 1;
}

size_t NCDValue_ListCount (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    return o->list_count;
}

NCDValue * NCDValue_ListFirst (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    if (LinkedList2_IsEmpty(&o->list)) {
        return NULL;
    }
    
    NCDListElement *e = UPPER_OBJECT(LinkedList2_GetFirst(&o->list), NCDListElement, list_node);
    
    return &e->v;
}

NCDValue * NCDValue_ListNext (NCDValue *o, NCDValue *ev)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    NCDListElement *e = UPPER_OBJECT(ev, NCDListElement, v);
    
    LinkedList2Iterator it;
    LinkedList2Iterator_Init(&it, &o->list, 1, &e->list_node);
    LinkedList2Iterator_Next(&it);
    LinkedList2Node *nen = LinkedList2Iterator_Next(&it);
    LinkedList2Iterator_Free(&it);
    
    if (!nen) {
        return NULL;
    }
    
    NCDListElement *ne = UPPER_OBJECT(nen, NCDListElement, list_node);
    
    return &ne->v;
}

int NCDValue_ListRead (NCDValue *o, int num, ...)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(num >= 0)
    
    if (num != NCDValue_ListCount(o)) {
        return 0;
    }
    
    va_list ap;
    va_start(ap, num);
    
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &o->list);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
        
        NCDValue **dest = va_arg(ap, NCDValue **);
        *dest = &e->v;
    }
    
    va_end(ap);
    
    return 1;
}

int NCDValue_ListReadHead (NCDValue *o, int num, ...)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(num >= 0)
    
    if (num > NCDValue_ListCount(o)) {
        return 0;
    }
    
    va_list ap;
    va_start(ap, num);
    
    LinkedList2Node *n = LinkedList2_GetFirst(&o->list);
    while (num > 0) {
        ASSERT(n)
        NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
        
        NCDValue **dest = va_arg(ap, NCDValue **);
        *dest = &e->v;
        
        n = LinkedList2Node_Next(n);
        num--;
    }
    
    va_end(ap);
    
    return 1;
}

NCDValue * NCDValue_ListGet (NCDValue *o, size_t pos)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(pos < o->list_count)
    
    NCDValue *e = NCDValue_ListFirst(o);
    while (e) {
        if (pos == 0) {
            break;
        }
        pos--;
        e = NCDValue_ListNext(o, e);
    }
    ASSERT(e)
    
    return e;
}

NCDValue NCDValue_ListShift (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(o->list_count > 0)
    
    NCDListElement *e = UPPER_OBJECT(LinkedList2_GetFirst(&o->list), NCDListElement, list_node);
    
    NCDValue v = e->v;
    
    LinkedList2_Remove(&o->list, &e->list_node);
    o->list_count--;
    free(e);
    
    return v;
}

int NCDValue_Compare (NCDValue *o, NCDValue *v)
{
    value_assert(o);
    value_assert(v);
    
    if (o->type == NCDVALUE_STRING && v->type == NCDVALUE_LIST) {
        return -1;
    }
    
    if (o->type == NCDVALUE_LIST && v->type == NCDVALUE_STRING) {
        return 1;
    }
    
    if (o->type == NCDVALUE_STRING && v->type == NCDVALUE_STRING) {
        int cmp = strcmp(o->string, v->string);
        if (cmp < 0) {
            return -1;
        }
        if (cmp > 0) {
            return 1;
        }
        return 0;
    }
    
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

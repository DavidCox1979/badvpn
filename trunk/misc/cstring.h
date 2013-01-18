/**
 * @file cstring.h
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

#ifndef BADVPN_COMPOSED_STRING_H
#define BADVPN_COMPOSED_STRING_H

#include <stddef.h>
#include <string.h>
#include <limits.h>

#include <misc/debug.h>
#include <misc/balloc.h>

struct b_cstring_s;

/**
 * Callback function which is called by {@link b_cstring_get} to access the underlying resource.
 * \a cstr points to the cstring being accessed, and the callback can use the userN members to
 * retrieve any state information.
 * \a offset is the offset from the beginning of the string; offset < cstr->length.
 * This callback must set *\a out_length and return a pointer, representing a continuous
 * region of the string that starts at the byte at index \a offset. Returning a region that
 * spans past the end of the string is allowed.
 */
typedef const char * (*b_cstring_func) (const struct b_cstring_s *cstr, size_t offset, size_t *out_length);

/**
 * An abstract string which is not necessarily continuous. Its data can be read using
 * {@link b_cstring_get}, which internally invokes the {@link b_cstring_func} callback.
 */
typedef struct b_cstring_s {
    size_t length;
    b_cstring_func func;
    union {
        size_t size;
        void *ptr;
        void (*fptr) (void);
    } user1;
    union {
        size_t size;
        void *ptr;
        void (*fptr) (void);
    } user2;
    union {
        size_t size;
        void *ptr;
        void (*fptr) (void);
    } user3;
} b_cstring;

/**
 * Convenience function which makes a cstring pointing to a buffer.
 */
static b_cstring b_cstring_make_buf (const char *data, size_t length);

/**
 * Retrieves a pointer to a continuous region of the string.
 * \a offset specifies the starting offset of the region to retrieve, and must be < cstr.length.
 * \a maxlen specifies the maximum length of the returned region, and must be > 0.
 * The length of the region will be stored in *\a out_chunk_len, and it will always be > 0.
 * It is possible that the returned region spans past the end of the string, unless limited
 * by \a maxlen. The pointer to the region will be returned; it will point to the byte
 * at offset exactly \a offset into the string.
 */
static const char * b_cstring_get (b_cstring cstr, size_t offset, size_t maxlen, size_t *out_chunk_len);

/**
 * Asserts that the range given by \a offset and \a length is valid for the string.
 */
static void b_cstring_assert_range (b_cstring cstr, size_t offset, size_t length);

/**
 * Copies a range to an external buffer.
 */
static void b_cstring_copy_to_buf (b_cstring cstr, size_t offset, size_t length, char *dest);

/**
 * Performs a memcmp-like operation on the given ranges of two cstrings.
 */
static int b_cstring_memcmp (b_cstring cstr1, b_cstring cstr2, size_t offset1, size_t offset2, size_t length);

/**
 * Allocates a buffer for a range and copies it. The buffer is allocated using {@link BAlloc}.
 * An extra null byte will be appended. On failure, returns NULL.
 */
static char * b_cstring_strdup (b_cstring cstr, size_t offset, size_t length);

/**
 * Macro which iterates the continuous regions of a range within a cstring.
 * For reach region, the statements in \a body are executed, in order.
 * \a cstr is the string to be iterated.
 * \a offset and \a length specify the range of the string to iterate; they must
 * refer to a valid range for the string.
 * \a rel_pos_var, \a chunk_data_var and \a chunk_length_var specify names of variables
 * which will be available in \a body.
 * \a rel_pos_var will hold the offset of the current continuous region, relative to the beginning.
 * \a chunk_data_var will hold a pointer (const char *) to the beginning of the region, and
 * \a chunk_length_var will hold its length.
 * 
 * See the implementation of {@link b_cstring_copy_to_buf} for a usage example.
 */
#define B_CSTRING_LOOP_RANGE(cstr, offset, length, rel_pos_var, chunk_data_var, chunk_length_var, body) \
{ \
    size_t rel_pos_var = 0; \
    while (rel_pos_var < (length)) { \
        size_t chunk_length_var; \
        const char *chunk_data_var = b_cstring_get((cstr), (offset) + rel_pos_var, (length) - rel_pos_var, &chunk_length_var); \
        { body } \
        rel_pos_var += chunk_length_var; \
    } \
}

/**
 * Like {@link B_CSTRING_LOOP_RANGE}, but iterates the entire string,
 * i.e. offset==0 and length==cstr.length.
 */
#define B_CSTRING_LOOP(cstr, rel_pos_var, chunk_data_var, chunk_length_var, body) B_CSTRING_LOOP_RANGE(cstr, 0, (cstr).length, rel_pos_var, chunk_data_var, chunk_length_var, body)

static const char * b_cstring__buf_func (const b_cstring *cstr, size_t offset, size_t *out_length)
{
    ASSERT(offset < cstr->length)
    ASSERT(out_length)
    ASSERT(cstr->func == b_cstring__buf_func)
    ASSERT(cstr->user1.ptr)
    
    *out_length = cstr->length - offset;
    return (const char *)cstr->user1.ptr + offset;
}

static b_cstring b_cstring_make_buf (const char *data, size_t length)
{
    ASSERT(length == 0 || data)
    
    b_cstring cstr;
    cstr.length = length;
    cstr.func = b_cstring__buf_func;
    cstr.user1.ptr = (void *)data;
    return cstr;
}

static const char * b_cstring_get (b_cstring cstr, size_t offset, size_t maxlen, size_t *out_chunk_len)
{
    ASSERT(offset < cstr.length)
    ASSERT(maxlen > 0)
    ASSERT(out_chunk_len)
    
    const char *data = cstr.func(&cstr, offset, out_chunk_len);
    ASSERT(data)
    ASSERT(*out_chunk_len > 0)
    
    if (*out_chunk_len > maxlen) {
        *out_chunk_len = maxlen;
    }
    
    return data;
}

static void b_cstring_assert_range (b_cstring cstr, size_t offset, size_t length)
{
    ASSERT(offset <= cstr.length)
    ASSERT(length <= cstr.length - offset)
}

static void b_cstring_copy_to_buf (b_cstring cstr, size_t offset, size_t length, char *dest)
{
    b_cstring_assert_range(cstr, offset, length);
    ASSERT(length == 0 || dest)
    
    B_CSTRING_LOOP_RANGE(cstr, offset, length, pos, chunk_data, chunk_length, {
        memcpy(dest + pos, chunk_data, chunk_length);
    })
}

static int b_cstring_memcmp (b_cstring cstr1, b_cstring cstr2, size_t offset1, size_t offset2, size_t length)
{
    b_cstring_assert_range(cstr1, offset1, length);
    b_cstring_assert_range(cstr2, offset2, length);
    
    B_CSTRING_LOOP_RANGE(cstr1, offset1, length, pos1, chunk_data1, chunk_len1, {
        B_CSTRING_LOOP_RANGE(cstr2, offset2 + pos1, chunk_len1, pos2, chunk_data2, chunk_len2, {
            int cmp = memcmp(chunk_data1 + pos2, chunk_data2, chunk_len2);
            if (cmp) {
                return cmp;
            }
        })
    })
    
    return 0;
}

static char * b_cstring_strdup (b_cstring cstr, size_t offset, size_t length)
{
    b_cstring_assert_range(cstr, offset, length);
    
    if (length == SIZE_MAX) {
        return NULL;
    }
    
    char *buf = BAlloc(length + 1);
    if (buf) {
        b_cstring_copy_to_buf(cstr, offset, length, buf);
        buf[length] = '\0';
    }
    
    return buf;
}

#endif

/**
 * @file NCDConfigParser_parse.y
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

%include {

#include <string.h>
#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDConfig.h>

#define AST_TYPE_NONE 0
#define AST_TYPE_STRING 1
#define AST_TYPE_LIST 2

struct parser_out {
    int out_of_memory;
    int syntax_error;
    int ast_type;
    char *ast_string;
    struct NCDConfig_list *ast_list;
};

}

%extra_argument {struct parser_out *parser_out}

%token_type {void *}

%token_destructor { free($$); }

%type list_contents {struct NCDConfig_list *}
%type list {struct NCDConfig_list *}

%destructor list_contents { NCDConfig_free_list($$); }
%destructor list { NCDConfig_free_list($$); }

%stack_size 0

%syntax_error {
    parser_out->syntax_error = 1;
}

// workaroud Lemon bug: if the stack overflows, the token that caused the overflow will be leaked
%stack_overflow {
    if (yypMinor) {
        free(yypMinor->yy0);
    }
}

input ::= STRING(A). {
    ASSERT(parser_out->ast_type == AST_TYPE_NONE)

    parser_out->ast_string = A;
    parser_out->ast_type = AST_TYPE_STRING;
}

input ::= list(A). {
    ASSERT(parser_out->ast_type == AST_TYPE_NONE)

    parser_out->ast_list = A;
    parser_out->ast_type = AST_TYPE_LIST;
}

list_contents(R) ::= STRING(A). {
    R = NCDConfig_make_list_string(A, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

list_contents(R) ::= list(A). {
    R = NCDConfig_make_list_list(A, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

list_contents(R) ::= STRING(A) COMMA list_contents(N). {
    R = NCDConfig_make_list_string(A, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

list_contents(R) ::= list(A) COMMA list_contents(N). {
    R = NCDConfig_make_list_list(A, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

list(R) ::= CURLY_OPEN CURLY_CLOSE. {
    R = NULL;
}

list(R) ::= CURLY_OPEN list_contents(A) CURLY_CLOSE. {
    R = A;
}

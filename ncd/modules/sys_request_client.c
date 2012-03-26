/**
 * @file sys_request_client.c
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
 *   sys.request_client(string socket_path)
 * 
 * Description:
 *   Connects to a request server (sys.request_server()) over a Unix socket.
 *   Goes up when the connection, and dies with error when it is broken.
 *   When requested to die, dies immediately, breaking the connection.
 * 
 * Synopsis:
 *   sys.request_client::request(request_data, string reply_handler, string finished_handler, list args)
 * 
 * Description:
 *   Sends a request to the server and dispatches replies to the provided handlers.
 * 
 *   The 'request_data' argument is sent as part of the request and is used by the server
 *   to determine what to do with the request.
 * 
 *   When a reply is received, a new template process is created from 'reply_handler' to process the
 *   reply. This process can access the reply data sent by the server using '_reply.data'.
 *   Similarly, if the server finishes the request, a process is created from 'finished_handler'.
 *   In both cases, the process can access objects as seen from the request statement via "_caller".
 *   Termination of these processes is initiated immediately after they completes. They are created
 *   synchronously - if a reply or a finished message arrives before a previous process is has
 *   finished, it is queued. Once the finished message has been processed by 'finished_handler', no
 *   more processes will be created.
 * 
 *   When the request statement is requested to terminate, it initiates termination of the current
 *   handler process and waits for it to terminate (if any is running), and then dies.
 *   If the corresponding client statement dies after being requested to die, or as a result of
 *   an error, the request statement will not react to this. It will dispatch any pending messages
 *   and then proceed to do nothing. In this case, if a finished message was not received, it will
 *   not be dispatched.
 * 
 *   The request statement may however die at any time due to errors. In this case, it will
 *   initiate termination of the current process and wait for it to terminate (if any) before dying.
 * 
 *   The request protocol and the server allow the client the abort requests at any time, and to
 *   have the client notified only after the request has been completely aborted (i.e. the handler
 *   process of sys.request_server() has deinitialized completely). This client implementation will
 *   automatically request abortion of active requests when the request statement is requested
 *   to die. However, the request statement will not wait for the abortion to finish before dying.
 *   This means, for instance, that if you initialize a request statement right after having
 *   deinitiazed it, the requests may overlap on the server side.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>
#include <structure/LinkedList0.h>
#include <structure/LinkedList1.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDRequestClient.h>

#include <generated/blog_channel_ncd_sys_request_client.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define CSTATE_CONNECTING 1
#define CSTATE_CONNECTED 2

#define RRSTATE_SENDING_REQUEST 1
#define RRSTATE_READY 2
#define RRSTATE_GONE_BAD 3
#define RRSTATE_GONE_GOOD 4

#define RPSTATE_NONE 1
#define RPSTATE_WORKING 2
#define RPSTATE_TERMINATING 3

#define RDSTATE_NONE 1
#define RDSTATE_DYING 2
#define RDSTATE_DYING_ERROR 3

struct instance {
    NCDModuleInst *i;
    NCDRequestClient client;
    LinkedList0 requests_list;
    int state;
};

struct request_instance {
    NCDModuleInst *i;
    char *reply_handler;
    char *finished_handler;
    NCDValue *args;
    struct instance *client;
    NCDRequestClientRequest request;
    LinkedList0Node requests_list_node;
    LinkedList1 replies_list;
    NCDModuleProcess process;
    int process_is_finished;
    NCDValue process_reply_data;
    int rstate;
    int pstate;
    int dstate;
};

struct reply {
    LinkedList1Node replies_list_node;
    NCDValue val;
};

static void client_handler_error (struct instance *o);
static void client_handler_connected (struct instance *o);
static void request_handler_sent (struct request_instance *o);
static void request_handler_reply (struct request_instance *o, NCDValue reply_data);
static void request_handler_finished (struct request_instance *o, int is_error);
static void request_process_handler_event (struct request_instance *o, int event);
static int request_process_func_getspecialobj (struct request_instance *o, const char *name, NCDObject *out_object);
static int request_process_caller_obj_func_getobj (struct request_instance *o, const char *name, NCDObject *out_object);
static int request_process_reply_obj_func_getvar (struct request_instance *o, const char *name, NCDValue *out_value);
static void request_gone (struct request_instance *o, int is_bad);
static void request_terminate_process (struct request_instance *o);
static void request_die (struct request_instance *o, int is_error);
static void request_free_reply (struct request_instance *o, struct reply *r, int have_value);
static int request_init_reply_process (struct request_instance *o, NCDValue reply_data);
static int request_init_finished_process (struct request_instance *o);
static void instance_free (struct instance *o, int with_error);
static void request_instance_free (struct request_instance *o, int with_error);

static void client_handler_error (struct instance *o)
{
    ModuleLog(o->i, BLOG_ERROR, "client error");
    
    // free instance
    instance_free(o, 1);
}

static void client_handler_connected (struct instance *o)
{
    ASSERT(o->state == CSTATE_CONNECTING)
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    // set state connected
    o->state = CSTATE_CONNECTED;
}

static void request_handler_sent (struct request_instance *o)
{
    ASSERT(o->rstate == RRSTATE_SENDING_REQUEST)
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    // set rstate ready
    o->rstate = RRSTATE_READY;
}

static void request_handler_reply (struct request_instance *o, NCDValue reply_data)
{
    ASSERT(o->rstate == RRSTATE_READY)
    
    // queue reply if process is running
    if (o->pstate != RPSTATE_NONE) {
        struct reply *r = malloc(sizeof(*r));
        if (!r) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            goto fail;
        }
        r->val = reply_data;
        LinkedList1_Append(&o->replies_list, &r->replies_list_node);
        return;
    }
    
    // start reply process
    if (!request_init_reply_process(o, reply_data)) {
        goto fail;
    }
    
    return;
    
fail:
    NCDValue_Free(&reply_data);
    request_die(o, 1);
}

static void request_handler_finished (struct request_instance *o, int is_error)
{
    ASSERT(o->rstate == RRSTATE_SENDING_REQUEST || o->rstate == RRSTATE_READY)
    ASSERT(is_error || o->rstate == RRSTATE_READY)
    
    if (is_error) {
        ModuleLog(o->i, BLOG_ERROR, "received error reply");
        goto fail;
    }
    
    // request gone good
    request_gone(o, 0);
    
    // start process for reporting finished, if possible
    if (o->pstate == RPSTATE_NONE) {
        if (!request_init_finished_process(o)) {
            goto fail;
        }
    }
    
    return;
    
fail:
    request_die(o, 1);
}

static void request_process_handler_event (struct request_instance *o, int event)
{
    ASSERT(o->pstate != RPSTATE_NONE)
    
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(o->pstate == RPSTATE_WORKING)
            
            // request process termination
            request_terminate_process(o);
        } break;
        
        case NCDMODULEPROCESS_EVENT_DOWN: {
            ASSERT(0)
        } break;
        
        case NCDMODULEPROCESS_EVENT_TERMINATED: {
            ASSERT(o->pstate == RPSTATE_TERMINATING)
            ASSERT(o->rstate != RRSTATE_SENDING_REQUEST)
            
            // free process
            NCDModuleProcess_Free(&o->process);
            
            // free reply data
            if (!o->process_is_finished) {
                NCDValue_Free(&o->process_reply_data);
            }
            
            // set process state none
            o->pstate = RPSTATE_NONE;
            
            // die finally if requested
            if (o->dstate == RDSTATE_DYING || o->dstate == RDSTATE_DYING_ERROR) {
                request_instance_free(o, o->dstate == RDSTATE_DYING_ERROR);
                return;
            }
            
            if (!LinkedList1_IsEmpty(&o->replies_list)) {
                // get first reply
                struct reply *r = UPPER_OBJECT(LinkedList1_GetFirst(&o->replies_list), struct reply, replies_list_node);
                
                // start reply process
                if (!request_init_reply_process(o, r->val)) {
                    goto fail;
                }
                
                // free reply
                request_free_reply(o, r, 0);
            }
            else if (o->rstate == RRSTATE_GONE_GOOD && !o->process_is_finished) {
                // start process for reporting finished
                if (!request_init_finished_process(o)) {
                    goto fail;
                }
            }
            
            return;
            
        fail:
            request_die(o, 1);
        } break;
    }
}

static int request_process_func_getspecialobj (struct request_instance *o, const char *name, NCDObject *out_object)
{
    ASSERT(o->pstate != RPSTATE_NONE)
    
    if (!strcmp(name, "_caller")) {
        *out_object = NCDObject_Build(NULL, o, NULL, (NCDObject_func_getobj)request_process_caller_obj_func_getobj);
        return 1;
    }
    
    if (!o->process_is_finished && !strcmp(name, "_reply")) {
        *out_object = NCDObject_Build(NULL, o, (NCDObject_func_getvar)request_process_reply_obj_func_getvar, NULL);
        return 1;
    }
    
    return 0;
}

static int request_process_caller_obj_func_getobj (struct request_instance *o, const char *name, NCDObject *out_object)
{
    ASSERT(o->pstate != RPSTATE_NONE)
    
    return NCDModuleInst_Backend_GetObj(o->i, name, out_object);
}

static int request_process_reply_obj_func_getvar (struct request_instance *o, const char *name, NCDValue *out_value)
{
    ASSERT(o->pstate != RPSTATE_NONE)
    ASSERT(!o->process_is_finished)
    
    if (!strcmp(name, "data")) {
        if (!NCDValue_InitCopy(out_value, &o->process_reply_data)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        return 1;
    }
    
    return 0;
}

static void request_gone (struct request_instance *o, int is_bad)
{
    ASSERT(o->rstate != RRSTATE_GONE_BAD)
    ASSERT(o->rstate != RRSTATE_GONE_GOOD)
    
    // remove from requests list
    LinkedList0_Remove(&o->client->requests_list, &o->requests_list_node);
    
    // free request
    NCDRequestClientRequest_Free(&o->request);
    
    // set state over
    o->rstate = (is_bad ? RRSTATE_GONE_BAD : RRSTATE_GONE_GOOD);
}

static void request_terminate_process (struct request_instance *o)
{
    ASSERT(o->pstate == RPSTATE_WORKING)
    
    // request process termination
    NCDModuleProcess_Terminate(&o->process);
    
    // set process state terminating
    o->pstate = RPSTATE_TERMINATING;
}

static void request_die (struct request_instance *o, int is_error)
{
    // if we have no process, die right away, else we have to wait for process to terminate
    if (o->pstate == RPSTATE_NONE) {
        request_instance_free(o, is_error);
        return;
    }
    
    // release request
    if (o->rstate != RRSTATE_GONE_BAD && o->rstate != RRSTATE_GONE_GOOD) {
        request_gone(o, 1);
    }
    
    // initiate process termination, if needed
    if (o->pstate != RPSTATE_TERMINATING) {
        request_terminate_process(o);
    }
    
    // set dstate
    o->dstate = (is_error ? RDSTATE_DYING_ERROR : RDSTATE_DYING);
}

static void request_free_reply (struct request_instance *o, struct reply *r, int have_value)
{
    // remove from replies list
    LinkedList1_Remove(&o->replies_list, &r->replies_list_node);
    
    // free value
    if (have_value) {
        NCDValue_Free(&r->val);
    }
    
    // free structure
    free(r);
}

static int request_init_reply_process (struct request_instance *o, NCDValue reply_data)
{
    ASSERT(o->pstate == RPSTATE_NONE)
    
    // set parameters
    o->process_is_finished = 0;
    o->process_reply_data = reply_data;
    
    // copy arguments
    NCDValue args;
    if (!NCDValue_InitCopy(&args, o->args)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail0;
    }
    
    // init process
    if (!NCDModuleProcess_Init(&o->process, o->i, o->reply_handler, args, o, (NCDModuleProcess_handler_event)request_process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        NCDValue_Free(&args);
        goto fail0;
    }
    
    // set special objects function
    NCDModuleProcess_SetSpecialFuncs(&o->process, (NCDModuleProcess_func_getspecialobj)request_process_func_getspecialobj);
    
    // set process state working
    o->pstate = RPSTATE_WORKING;
    return 1;
    
fail0:
    return 0;
}

static int request_init_finished_process (struct request_instance *o)
{
    ASSERT(o->pstate == RPSTATE_NONE)
    
    // set parameters
    o->process_is_finished = 1;
    
    // copy arguments
    NCDValue args;
    if (!NCDValue_InitCopy(&args, o->args)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail0;
    }
    
    // init process
    if (!NCDModuleProcess_Init(&o->process, o->i, o->finished_handler, args, o, (NCDModuleProcess_handler_event)request_process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        NCDValue_Free(&args);
        goto fail0;
    }
    
    // set special objects function
    NCDModuleProcess_SetSpecialFuncs(&o->process, (NCDModuleProcess_func_getspecialobj)request_process_func_getspecialobj);
    
    // set process state working
    o->pstate = RPSTATE_WORKING;
    return 1;
    
fail0:
    return 0;
}

static void func_new (NCDModuleInst *i)
{
    // allocate structure
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // check arguments
    NCDValue *socket_path_arg;
    if (!NCDValue_ListRead(i->args, 1, &socket_path_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(socket_path_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    char *socket_path = NCDValue_StringValue(socket_path_arg);
    
    // init client
    if (!NCDRequestClient_Init(&o->client, NCDREQUESTCLIENT_UNIX_ADDR(socket_path), i->params->reactor, o,
        (NCDRequestClient_handler_error)client_handler_error,
        (NCDRequestClient_handler_connected)client_handler_connected)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDRequestClient_Init failed");
        goto fail1;
    }
    
    // init requests list
    LinkedList0_Init(&o->requests_list);
    
    // set state connecting
    o->state = CSTATE_CONNECTING;
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void instance_free (struct instance *o, int with_error)
{
    NCDModuleInst *i = o->i;
    
    // deal with requests
    LinkedList0Node *ln;
    while (ln = LinkedList0_GetFirst(&o->requests_list)) {
        struct request_instance *req = UPPER_OBJECT(ln, struct request_instance, requests_list_node);
        request_gone(req, 1);
    }
    
    // free client
    NCDRequestClient_Free(&o->client);
    
    // free structure
    free(o);
    
    if (with_error) {
        NCDModuleInst_Backend_SetError(i);
    }
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    instance_free(o, 0);
}

static void request_func_new (NCDModuleInst *i)
{
    // allocate structure
    struct request_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // check arguments
    NCDValue *request_data_arg;
    NCDValue *reply_handler_arg;
    NCDValue *finished_handler_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(i->args, 4, &request_data_arg, &reply_handler_arg, &finished_handler_arg, &args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(reply_handler_arg) != NCDVALUE_STRING || NCDValue_Type(finished_handler_arg) != NCDVALUE_STRING ||
        NCDValue_Type(args_arg) != NCDVALUE_LIST
    ) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->reply_handler = NCDValue_StringValue(reply_handler_arg);
    o->finished_handler = NCDValue_StringValue(finished_handler_arg);
    o->args = args_arg;
    
    // get client
    struct instance *client = ((NCDModuleInst *)i->method_user)->inst_user;
    o->client = client;
    
    // check client state
    if (client->state != CSTATE_CONNECTED) {
        ModuleLog(o->i, BLOG_ERROR, "client is not connected");
        goto fail1;
    }
    
    // init request
    if (!NCDRequestClientRequest_Init(&o->request, &client->client, request_data_arg, o,
        (NCDRequestClientRequest_handler_sent)request_handler_sent,
        (NCDRequestClientRequest_handler_reply)request_handler_reply,
        (NCDRequestClientRequest_handler_finished)request_handler_finished)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDRequestClientRequest_Init failed");
        goto fail1;
    }
    
    // add to requests list
    LinkedList0_Prepend(&client->requests_list, &o->requests_list_node);
    
    // init replies list
    LinkedList1_Init(&o->replies_list);
    
    // set state
    o->rstate = RRSTATE_SENDING_REQUEST;
    o->pstate = RPSTATE_NONE;
    o->dstate = RDSTATE_NONE;
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void request_instance_free (struct request_instance *o, int with_error)
{
    ASSERT(o->pstate == RPSTATE_NONE)
    NCDModuleInst *i = o->i;
    
    // free replies
    LinkedList1Node *ln;
    while (ln = LinkedList1_GetFirst(&o->replies_list)) {
        struct reply *r = UPPER_OBJECT(ln, struct reply, replies_list_node);
        request_free_reply(o, r, 1);
    }
    
    // release request
    if (o->rstate != RRSTATE_GONE_BAD && o->rstate != RRSTATE_GONE_GOOD) {
        request_gone(o, 1);
    }
    
    // free structure
    free(o);
    
    if (with_error) {
        NCDModuleInst_Backend_SetError(i);
    }
    NCDModuleInst_Backend_Dead(i);
}

static void request_func_die (void *vo)
{
    struct request_instance *o = vo;
    
    request_die(o, 0);
}

static const struct NCDModule modules[] = {
    {
        .type = "sys.request_client",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = "sys.request_client::request",
        .func_new = request_func_new,
        .func_die = request_func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_sys_request_client = {
    .modules = modules
};

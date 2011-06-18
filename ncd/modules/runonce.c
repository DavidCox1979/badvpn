/**
 * @file runonce.c
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
 * 
 * @section DESCRIPTION
 * 
 * Imperative program execution module. On initialization, starts the process.
 * Goes to UP state when the process terminates. When requested to die, waits for
 * the process to terminate if it's running, without sending any signals.
 * 
 * Synopsis: runonce(list(string) cmd)
 * Arguments:
 *   cmd - Command to run on startup. The first element is the full path
 *     to the executable, other elements are command line arguments (excluding
 *     the zeroth argument).
 * Variables:
 *   string exit_status - if the program exited normally, the non-negative exit code, otherwise -1
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <misc/cmdline.h>
#include <system/BProcess.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_runonce.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define STATE_RUNNING 1
#define STATE_RUNNING_DIE 2
#define STATE_FINISHED 3

struct instance {
    NCDModuleInst *i;
    int state;
    BProcess process;
    int exit_status;
};

static void instance_free (struct instance *o);

static int build_cmdline (NCDModuleInst *i, NCDValue *cmd_arg, char **exec, CmdLine *cl)
{
    if (NCDValue_Type(cmd_arg) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // read exec
    NCDValue *exec_arg = NCDValue_ListFirst(cmd_arg);
    if (!exec_arg) {
        ModuleLog(i, BLOG_ERROR, "missing executable name");
        goto fail0;
    }
    if (NCDValue_Type(exec_arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    if (!(*exec = strdup(NCDValue_StringValue(exec_arg)))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail0;
    }
    
    // start cmdline
    if (!CmdLine_Init(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Init failed");
        goto fail1;
    }
    
    // add header
    if (!CmdLine_Append(cl, *exec)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
        goto fail2;
    }
    
    // add additional arguments
    NCDValue *arg = exec_arg;
    while (arg = NCDValue_ListNext(cmd_arg, arg)) {
        if (NCDValue_Type(arg) != NCDVALUE_STRING) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail2;
        }
        
        if (!CmdLine_Append(cl, NCDValue_StringValue(arg))) {
            ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
            goto fail2;
        }
    }
    
    // finish
    if (!CmdLine_Finish(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Finish failed");
        goto fail2;
    }
    
    return 1;
    
fail2:
    CmdLine_Free(cl);
fail1:
    free(*exec);
fail0:
    return 0;
}

static void process_handler (struct instance *o, int normally, uint8_t normally_exit_status)
{
    ASSERT(o->state == STATE_RUNNING || o->state == STATE_RUNNING_DIE)
    
    // free process
    BProcess_Free(&o->process);
    
    // if we were requested to die, die now
    if (o->state == STATE_RUNNING_DIE) {
        instance_free(o);
        return;
    }
    
    // remember exit status
    o->exit_status = (normally ? normally_exit_status : -1);
    
    // set state
    o->state = STATE_FINISHED;
    
    // set up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
}

static void func_new (NCDModuleInst *i)
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
    
    // read arguments
    NCDValue *cmd_arg;
    if (!NCDValue_ListRead(i->args, 1, &cmd_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // build cmdline
    char *exec;
    CmdLine cl;
    if (!build_cmdline(o->i, cmd_arg, &exec, &cl)) {
        goto fail1;
    }
    
    // start process
    if (!BProcess_Init(&o->process, o->i->manager, (BProcess_handler)process_handler, o, exec, CmdLine_Get(&cl), NULL)) {
        ModuleLog(i, BLOG_ERROR, "BProcess_Init failed");
        CmdLine_Free(&cl);
        free(exec);
        goto fail1;
    }
    
    CmdLine_Free(&cl);
    free(exec);
    
    // set state
    o->state = STATE_RUNNING;
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void instance_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(o->state != STATE_RUNNING_DIE)
    
    if (o->state == STATE_FINISHED) {
        instance_free(o);
        return;
    }
    
    o->state = STATE_RUNNING_DIE;
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    ASSERT(o->state == STATE_FINISHED)
    
    if (!strcmp(name, "exit_status")) {
        char str[30];
        snprintf(str, sizeof(str), "%d", o->exit_status);
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "runonce",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_runonce = {
    .modules = modules
};

/**
 * @file command_template.c
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

#include <misc/debug.h>

#include <ncd/modules/command_template.h>

#define STATE_ADDING_LOCK 1
#define STATE_ADDING 2
#define STATE_ADDING_NEED_DELETE 3
#define STATE_DONE 4
#define STATE_DELETING_LOCK 5
#define STATE_DELETING 6

static void lock_handler (command_template_instance *o);
static void process_handler (command_template_instance *o, int normally, uint8_t normally_exit_status);
static void free_template (command_template_instance *o, int is_error);

static void lock_handler (command_template_instance *o)
{
    ASSERT(o->state == STATE_ADDING_LOCK || o->state == STATE_DELETING_LOCK)
    ASSERT(!o->have_process)
    ASSERT(!(o->state == STATE_ADDING_LOCK) || o->do_exec)
    ASSERT(!(o->state == STATE_DELETING_LOCK) || o->undo_exec)
    
    if (o->state == STATE_ADDING_LOCK) {
        // start process
        if (!BProcess_Init(&o->process, o->i->manager, (BProcess_handler)process_handler, o, o->do_exec, CmdLine_Get(&o->do_cmdline), NULL)) {
            NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "BProcess_Init failed");
            free_template(o, 1);
            return;
        }
        
        // set have process
        o->have_process = 1;
        
        // set state
        o->state = STATE_ADDING;
    } else {
        // start process
        if (!BProcess_Init(&o->process, o->i->manager, (BProcess_handler)process_handler, o, o->undo_exec, CmdLine_Get(&o->undo_cmdline), NULL)) {
            NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "BProcess_Init failed");
            free_template(o, 1);
            return;
        }
        
        // set have process
        o->have_process = 1;
        
        // set state
        o->state = STATE_DELETING;
    }
}

static void process_handler (command_template_instance *o, int normally, uint8_t normally_exit_status)
{
    ASSERT(o->have_process)
    ASSERT(o->state == STATE_ADDING || o->state == STATE_ADDING_NEED_DELETE || o->state == STATE_DELETING)
    
    // release lock
    BEventLockJob_Release(&o->elock_job);
    
    // free process
    BProcess_Free(&o->process);
    
    // set have no process
    o->have_process = 0;
    
    if (!normally || normally_exit_status != 0) {
        NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "command failed");
        
        free_template(o, 1);
        return;
    }
    
    switch (o->state) {
        case STATE_ADDING: {
            // set state
            o->state = STATE_DONE;
            
            // signal up
            NCDModuleInst_Backend_Up(o->i);
        } break;
        
        case STATE_ADDING_NEED_DELETE: {
            if (o->undo_exec) {
                // wait for lock
                BEventLockJob_Wait(&o->elock_job);
                
                // set state
                o->state = STATE_DELETING_LOCK;
            } else {
                free_template(o, 0);
                return;
            }
        } break;
        
        case STATE_DELETING: {
            // finish
            free_template(o, 0);
            return;
        } break;
    }
}

void command_template_new (command_template_instance *o, NCDModuleInst *i, command_template_build_cmdline build_cmdline, command_template_free_func free_func, void *user, int blog_channel, BEventLock *elock)
{
    // init arguments
    o->i = i;
    o->build_cmdline = build_cmdline;
    o->free_func = free_func;
    o->user = user;
    o->blog_channel = blog_channel;
    
    // build do command
    if (!o->build_cmdline(o->i, 0, &o->do_exec, &o->do_cmdline)) {
        NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "build_cmdline do callback failed");
        goto fail0;
    }
    
    // build undo command
    if (!o->build_cmdline(o->i, 1, &o->undo_exec, &o->undo_cmdline)) {
        NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "build_cmdline undo callback failed");
        goto fail1;
    }
    
    // init lock job
    BEventLockJob_Init(&o->elock_job, elock, (BEventLock_handler)lock_handler, o);
    
    // set have no process
    o->have_process = 0;
    
    if (o->do_exec) {
        // wait for lock
        BEventLockJob_Wait(&o->elock_job);
        
        // set state
        o->state = STATE_ADDING_LOCK;
    } else {
        // set state
        o->state = STATE_DONE;
        
        // signal up
        NCDModuleInst_Backend_Up(o->i);
    }
    
    return;
    
fail1:
    if (o->do_exec) {
        free(o->do_exec);
        CmdLine_Free(&o->do_cmdline);
    }
fail0:
    o->free_func(o->user, 1);
}

static void free_template (command_template_instance *o, int is_error)
{
    ASSERT(!o->have_process)
    
    // free lock job
    BEventLockJob_Free(&o->elock_job);
    
    // free undo command
    if (o->undo_exec) {
        free(o->undo_exec);
        CmdLine_Free(&o->undo_cmdline);
    }
    
    // free do command
    if (o->do_exec) {
        free(o->do_exec);
        CmdLine_Free(&o->do_cmdline);
    }
    
    // call free function
    o->free_func(o->user, is_error);
}

void command_template_die (command_template_instance *o)
{
    ASSERT(o->state == STATE_ADDING_LOCK || o->state == STATE_ADDING || o->state == STATE_DONE)
    
    switch (o->state) {
        case STATE_ADDING_LOCK: {
            ASSERT(!o->have_process)
            
            free_template(o, 0);
            return;
        } break;
        
        case STATE_ADDING: {
            ASSERT(o->have_process)
            
            // set state
            o->state = STATE_ADDING_NEED_DELETE;
        } break;
        
        case STATE_DONE: {
            ASSERT(!o->have_process)
            
            if (o->undo_exec) {
                // wait for lock
                BEventLockJob_Wait(&o->elock_job);
                
                // set state
                o->state = STATE_DELETING_LOCK;
            } else {
                free_template(o, 0);
                return;
            }
        } break;
    }
}

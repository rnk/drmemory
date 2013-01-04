/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* strace: test of the Dr. Syscall Extension.
 *
 * Currently this doesn't do much testing beyond drsyscall_client.c but
 * the idea is to turn this into a sample strace client.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drsyscall.h"
#include <string.h>
#ifdef WINDOWS
# include <windows.h>
#endif

#undef ASSERT /* we don't want msgbox */
#define ASSERT(cond, msg) \
    ((void)((!(cond)) ? \
     (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)", \
                 __FILE__,  __LINE__, #cond, msg), \
      dr_abort(), 0) : 0))

static bool verbose = true;

static bool
drsys_iter_memarg_cb(drsys_arg_t *arg, void *user_data)
{
    ASSERT(arg->valid, "no args should be invalid in this app");
    ASSERT(arg->mc != NULL, "mc check");
    ASSERT(arg->drcontext == dr_get_current_drcontext(), "dc check");

    if (verbose) {
        dr_fprintf(STDERR, "\tmemarg %d: name=%s, type=%d %s, start="PFX", size="PIFX"\n",
                   arg->ordinal, arg->arg_name == NULL ? "\"\"" : arg->arg_name,
                   arg->type, arg->type_name == NULL ? "\"\"" : arg->type_name,
                   arg->start_addr, arg->size);
    } 

    return true; /* keep going */
}

static bool
drsys_iter_arg_cb(drsys_arg_t *arg, void *user_data)
{
    ptr_uint_t val;

    ASSERT(arg->valid, "no args should be invalid in this app");
    ASSERT(arg->mc != NULL, "mc check");
    ASSERT(arg->drcontext == dr_get_current_drcontext(), "dc check");

    if (arg->reg == DR_REG_NULL && arg->mode != DRSYS_PARAM_RETVAL) {
        ASSERT((byte *)arg->start_addr >= (byte *)arg->mc->xsp &&
               (byte *)arg->start_addr < (byte *)arg->mc->xsp + PAGE_SIZE,
               "mem args should be on stack");
    }

    if (arg->mode == DRSYS_PARAM_RETVAL) {
        ASSERT(arg->pre || arg->value == dr_syscall_get_result(dr_get_current_drcontext()),
               "return val wrong");
    } else {
        if (drsys_pre_syscall_arg(arg->drcontext, arg->ordinal, &val) != DRMF_SUCCESS)
            ASSERT(false, "drsys_pre_syscall_arg failed");
        ASSERT(val == arg->value, "values do not match");
    }

    /* We could test drsys_handle_is_current_process() but we'd have to
     * locate syscalls operating on processes.  Currently drsyscall.c
     * already tests this call.
     */

    return true; /* keep going */
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    drsys_syscall_t *syscall;
    drsys_sysnum_t sysnum_full;
    bool known;
    drsys_param_type_t ret_type;
    const char *name;

    if (drsys_cur_syscall(drcontext, &syscall) != DRMF_SUCCESS)
        ASSERT(false, "drsys_cur_syscall failed");
    if (drsys_syscall_number(syscall, &sysnum_full) != DRMF_SUCCESS)
        ASSERT(false, "drsys_get_sysnum failed");
    ASSERT(sysnum == sysnum_full.number, "primary should match DR's num");
    if (drsys_syscall_name(syscall, &name) != DRMF_SUCCESS)
        ASSERT(false, "drsys_syscall_name failed");

    if (verbose) {
        dr_fprintf(STDERR, "syscall %d.%d = %s\n", sysnum_full.number,
                   sysnum_full.secondary, name);
    }

    if (drsys_syscall_return_type(syscall, &ret_type) != DRMF_SUCCESS ||
        ret_type == DRSYS_TYPE_INVALID || ret_type == DRSYS_TYPE_UNKNOWN)
        ASSERT(false, "failed to get syscall return type");
    if (verbose)
        dr_fprintf(STDERR, "\treturn type: %d\n", ret_type);

    if (drsys_syscall_is_known(syscall, &known) != DRMF_SUCCESS || !known)
        ASSERT(false, "no syscalls in this app should be unknown");

    if (drsys_iterate_args(drcontext, drsys_iter_arg_cb, NULL) != DRMF_SUCCESS)
        ASSERT(false, "drsys_iterate_args failed");
    if (drsys_iterate_memargs(drcontext, drsys_iter_memarg_cb, NULL) != DRMF_SUCCESS)
        ASSERT(false, "drsys_iterate_memargs failed");

    return true;
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    drsys_syscall_t *syscall;
    drsys_sysnum_t sysnum_full;
    bool success = false;

    if (drsys_cur_syscall(drcontext, &syscall) != DRMF_SUCCESS)
        ASSERT(false, "drsys_cur_syscall failed");
    if (drsys_syscall_number(syscall, &sysnum_full) != DRMF_SUCCESS)
        ASSERT(false, "drsys_get_sysnum failed");
    ASSERT(sysnum == sysnum_full.number, "primary should match DR's num");

    if (drsys_iterate_args(drcontext, drsys_iter_arg_cb, NULL) != DRMF_SUCCESS)
        ASSERT(false, "drsys_iterate_args failed");

    if (verbose) {
        dr_fprintf(STDERR, "\tsyscall returned "PFX"\n",
                   dr_syscall_get_result(drcontext));
    }
    if (drsys_syscall_succeeded(syscall, dr_syscall_get_result(drcontext), &success) !=
        DRMF_SUCCESS || !success) {
        if (verbose)
            dr_fprintf(STDERR, "\tsyscall failed\n");
    } else {
        if (drsys_iterate_memargs(drcontext, drsys_iter_memarg_cb, NULL) != DRMF_SUCCESS)
            ASSERT(false, "drsys_iterate_memargs failed");
    }
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true; /* intercept everything */
}

static
void exit_event(void)
{
    if (drsys_exit() != DRMF_SUCCESS)
        ASSERT(false, "drsys failed to exit");
    dr_fprintf(STDERR, "TEST PASSED\n");
    drmgr_exit();
}

DR_EXPORT
void dr_init(client_id_t id)
{
    drsys_options_t ops = { sizeof(ops), 0, };
    drmgr_init();
    if (drsys_init(id, &ops) != DRMF_SUCCESS)
        ASSERT(false, "drsys failed to init");
    dr_register_exit_event(exit_event);

    dr_register_filter_syscall_event(event_filter_syscall);
    drmgr_register_pre_syscall_event(event_pre_syscall);
    drmgr_register_post_syscall_event(event_post_syscall);
    if (drsys_filter_all_syscalls() != DRMF_SUCCESS)
        ASSERT(false, "drsys_filter_all_syscalls should never fail");
}

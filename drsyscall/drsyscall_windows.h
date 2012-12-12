/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
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

#ifndef _DRSYSCALL_WINDOWS_H_
#define _DRSYSCALL_WINDOWS_H_ 1

/* drsyscall_windows.c */

extern dr_os_version_info_t win_ver;

bool
handle_unicode_string_access(sysarg_iter_info_t *ii, const syscall_arg_t *arg_info,
                             app_pc start, uint size, bool ignore_len);

bool
handle_cwstring(sysarg_iter_info_t *ii, const char *id,
                byte *start, size_t size, int ordinal, uint arg_flags, wchar_t *safe,
                bool check_addr);


/* drsyscall_wingdi.c */

drmf_status_t
drsyscall_wingdi_init(void *drcontext, app_pc ntdll_base, dr_os_version_info_t *ver);

void
drsyscall_wingdi_exit(void);

bool
wingdi_shared_process_syscall(void *drcontext, cls_syscall_t *pt,
                              sysarg_iter_info_t *ii);

void
wingdi_shadow_process_syscall(void *drcontext, cls_syscall_t *pt,
                              sysarg_iter_info_t *ii);

bool
wingdi_process_arg(sysarg_iter_info_t *iter_info,
                   const syscall_arg_t *arg_info, app_pc start, uint size);

#endif /* _DRSYSCALL_WINDOWS_H_ */

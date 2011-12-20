/* **********************************************************
 * Copyright (c) 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <unistd.h>

#define MAXIMUM_PATH 260

int main(void)
{
    /* TODO(rnk): Implement windows version. */
    char dir[MAXIMUM_PATH];

    if (getcwd(dir, sizeof(dir)) == NULL) {
        printf("getcwd failed\n");
    }
    printf("current directory:\n%s\n", dir);

    /* cd to / and quit, DrMemory should try to access the symcache, but we
     * don't seem to abspath it.
     */
    if (chdir("/") != 0) {
        printf("chdir failed\n");
    }

    printf("quitting\n");

    return 0;
}

/* **********************************************************
 * Copyright (c) 2009 VMware, Inc.  All rights reserved.
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

/* Test delay-free feature (PR 406762) */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define NUM_MALLOC 20

int
main()
{
    void *p[NUM_MALLOC];
    int i;
    char c;

    for (i = 0; i < NUM_MALLOC; i++) {
        /* Make allocations and free right away.  Normally the next alloc
         * will re-use the same slot if it fits (w/ alignment many will).
         * Only delayed frees will catch all bad accesses below.
         */
        p[i] = malloc(NUM_MALLOC - i);
        free(p[i]);
    }

    for (i = 0; i < NUM_MALLOC; i++) {
        c = *(((char *)p[i])+3); /* error: unaddressable, if delayed free */
    }

    printf("all done\n");
    return 0;
}

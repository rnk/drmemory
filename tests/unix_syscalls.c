/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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


#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#define BUFSZ 1024

#define CHECKERRNO() do { \
    if (errno) { \
        perror(__func__); \
    } \
    errno = 0; \
} while (0)

/* Put some padding around the memory used for the string so native malloc
 * libraries don't scribble over our string after we free it.
 */
typedef struct _padded_str_t {
    char pad1[BUFSZ];
    char str[BUFSZ];
    char pad2[BUFSZ];
} padded_str_t;

/* Create an unaddr usage passed to the open syscall.  We try to make this work
 * when run natively by not using malloc between the free and the use.
 */
void unaddr_open(void)
{
    const char devnull[] = "/dev/null";
    padded_str_t *dangling_ptr;
    int fd;

    dangling_ptr = malloc(sizeof(padded_str_t));
    memcpy(dangling_ptr->str, devnull, sizeof(devnull));
    free(dangling_ptr);

    /* open unaddr use */
    fd = open(dangling_ptr->str, O_WRONLY);
    close(fd);
}

/* Create an uninit usage passed to the open syscall.  We try to make this work
 * when run natively by initializing it on the stack once, calling it again, and
 * praying that the previous stack frame lines up with the current one.
 */
void uninit_open(int initialize)
{
    const char devnull[] = "/dev/null";
    padded_str_t stack_str;
    int fd;

    if (initialize) {
        memcpy(stack_str.str, devnull, sizeof(devnull));
    }

    /* open uninit use */
    fd = open(stack_str.str, O_WRONLY);
    close(fd);
}

void access_filesystem(void)
{
    char tmpdir[] = "syscallsXXXXXX";
    int fd;
    char *dir;
    char buf[BUFSZ];
    int n;

    errno = 0;
    chdir("/tmp/");                     CHECKERRNO();
    mkdtemp(tmpdir);                    CHECKERRNO();
    chdir(tmpdir);                      CHECKERRNO();
    fd = creat("foo.txt", S_IRWXU);     CHECKERRNO();
    write(fd, "asdf\n", 5);             CHECKERRNO();
    close(fd);                          CHECKERRNO();
    chmod("foo.txt", S_IRUSR|S_IWUSR);  CHECKERRNO();
    fd = open("foo.txt", O_RDONLY);     CHECKERRNO();
    n = read(fd, buf, BUFSZ);           CHECKERRNO();
    if (n != 5 || strncmp(buf, "asdf\n", 5) != 0) {
        printf("foo.txt didn't round trip!\n");
    }
    link("foo.txt", "bar.txt");         CHECKERRNO();
    rename("bar.txt", "baz.txt");       CHECKERRNO();
    symlink("foo.txt", "qux.txt");      CHECKERRNO();
    n = readlink("qux.txt", buf, BUFSZ);
    if (n != 7 || strncmp(buf, "foo.txt", n) != 0) {
        printf("readlink provided wrong answer!\n");
    }
    unlink("foo.txt");                  CHECKERRNO();
    unlink("baz.txt");                  CHECKERRNO();
    unlink("qux.txt");                  CHECKERRNO();
    chdir("..");                        CHECKERRNO();
    rmdir(tmpdir);                      CHECKERRNO();
}

int main(void)
{
    access_filesystem();
    unaddr_open();
    uninit_open(1);
    uninit_open(0);
    printf("done\n");
    return 0;
}

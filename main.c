#include "ramfs.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define test(func, expect, ...) assert(func(__VA_ARGS__) == expect)
#define succopen(var, ...) assert((var = ropen(__VA_ARGS__)) >= 0)
#define failopen(var, ...) assert((var = ropen(__VA_ARGS__)) == -1)

int main() {
    init_ramfs();
    int fd;
    test(rmkdir, -1, "/000000000000000000000000000000001");

    test(rmkdir, 0, "/it");
    test(rmkdir, 0, "/it/has");
    test(rmkdir, 0, "/it/has/been");
    test(rmkdir, 0, "/it/has/been/a");
    test(rmkdir, 0, "/it/has/been/a/long");

    succopen(fd, "/it/has/been/a/long", O_CREAT);
    failopen(fd, "it/has/been/a/long", O_CREAT);
    char buf[105];
    test(rread, -1, fd, buf, 100);
    test(rwrite, -1, fd, "a", 1);
    test(rrmdir, -1, "/it/has/been");
    test(rrmdir, 0, "/it/has/been/a/long");
    test(rwrite, -1, fd, "a", 1);
    test(rread, -1, fd, buf, 100);

    init_shell();
    close_shell();
    close_ramfs();
    return 0;
}

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


    init_shell();
    close_shell();
    close_ramfs();
    return 0;
}
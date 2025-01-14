#include "ramfs.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

char s[105] = "Hello World!\n\0";
int main() {
    init_ramfs();
    init_shell();
    int fd1 = ropen("/test", O_CREAT | O_RDWR | O_APPEND);;

    close_shell();
    close_ramfs();
    return 0;
}
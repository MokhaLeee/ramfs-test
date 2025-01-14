#include "ramfs.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

char s[105] = "Hello World!\n";
int main() {
    init_ramfs();
    init_shell();

    close_shell();
    close_ramfs();
    return 0;
}
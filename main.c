
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
    int fd1 = ropen("/test", O_CREAT | O_RDWR | O_APPEND);
    rwrite(fd1, s, strlen(s));
    rseek(fd1, 2, SEEK_SET);
    rwrite(fd1, s, strlen(s));

    scat("/test");

    int fd2 = ropen("/test", O_TRUNC | O_RDWR);
    
    scat("/test");
    rwrite(fd2, s, strlen(s));
    
    scat("/test");

    close_shell();
    close_ramfs();
    return 0;
}
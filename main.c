#include "ramfs.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

int test1();
int test2();
int test3();
int test4();
int test5();

static const char *content = "export PATH=/usr/bin/\n";
static const char *ct = "export PATH=/home:$PATH";

static void local_test(void)
{
    int fd;

    init_ramfs();

    assert(rmkdir("/home") == 0);
    assert(rmkdir("/home/ubuntu") == 0);
    assert(rmkdir("/usr") == 0);
    assert(rmkdir("/usr/bin") == 0);
    assert(rwrite(ropen("/home///ubuntu//.bashrc", O_CREAT | O_WRONLY), content, strlen(content)) == strlen(content));

    init_shell();

    close_shell();
    close_ramfs();
}

int main() {
    printf("[CRIT] case 1\n");
    test1();
    printf("[CRIT] case 2\n");
    test2();
    printf("[CRIT] case 3\n");
    test3();
    printf("[CRIT] case 4\n");
    test4();
    printf("[CRIT] case 5\n");
    test5();
    return 0;
}

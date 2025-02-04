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
static const char *ct1 = "export PATH=/home:$PATH\n";
static const char *ct2 = "export PATH=////home:$PATH\n";
static const char *ct3 = "export PATH=   sdss: sscc :$PATH\n";

#define _assert(x) \
	if (!(x)) LOCAL_ERROR("failed!\n"); \

static void local_test(void)
{
    int fd;

    init_ramfs();

    _assert(rmkdir("/home") == 0);
    _assert(rmkdir("/home/ubuntu") == 0);
    _assert(rmkdir("/usr") == 0);
    _assert(rmkdir("/usr/bin") == 0);

    fd = ropen("/home///ubuntu//.bashrc", O_CREAT | O_WRONLY);
    _assert(rwrite(fd, content, strlen(content)) == strlen(content));
    _assert(rwrite(fd, ct1, strlen(ct1)) == strlen(ct1));
    _assert(rwrite(fd, ct2, strlen(ct2)) == strlen(ct2));
    _assert(rwrite(fd, ct3, strlen(ct3)) == strlen(ct3));

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

    local_test();
    return 0;
}

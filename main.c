#include "ramfs.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

const char *content = "export PATH=/usr/bin/\n";
const char *ct = "export PATH=/home:$PATH";

#define _assert(x) \
	if (!(x)) LOCAL_ERROR("%s\n", "failed"); \

static void file_rcu(const char *fpath)
{
    int fd;
    const char *content = "helloworld\n";
    char buf[BUFSIZ];

    fd = ropen(fpath, O_CREAT | O_RDWR);
    _assert(fd >= 0);
    _assert(rwrite(fd, content, strlen(content)) == strlen(content));
    _assert(rclose(fd) == 0);

    fd = ropen(fpath, O_RDWR | O_APPEND);
    _assert(fd >= 0);
    _assert(rwrite(fd, content, strlen(content)) == strlen(content));
    _assert(rclose(fd) == 0);

    fd = ropen(fpath, 0);
    _assert(fd >= 0);
    _assert(rread(fd, buf, sizeof(buf)) > 0);
    _assert(rclose(fd) == 0);
    printf("read: %s\n", buf);

    _assert(scat(fpath) == 0);
}

int main() {
    init_ramfs();

    assert(rmkdir("/home") == 0);
    assert(rmkdir("//home") == -1);
    assert(rmkdir("/test/1") == -1);
    assert(rmkdir("/home/ubuntu") == 0);
    assert(rmkdir("/usr") == 0);
    assert(rmkdir("/usr/bin") == 0);
    assert(rwrite(ropen("/home///ubuntu//.bashrc", O_CREAT | O_WRONLY), content, strlen(content)) == strlen(content));

    int fd = ropen("/home/ubuntu/.bashrc", O_RDONLY);
    char buf[105] = {0};

    assert(rread(fd, buf, 100) == strlen(content));
    assert(!strcmp(buf, content));
    assert(rwrite(ropen("/home////ubuntu//.bashrc", O_WRONLY | O_APPEND), ct, strlen(ct)) == strlen(ct));
    memset(buf, 0, sizeof(buf));
    assert(rread(fd, buf, 100) == strlen(ct));
    assert(!strcmp(buf, ct));
    assert(rseek(fd, 0, SEEK_SET) == 0);
    memset(buf, 0, sizeof(buf));
    assert(rread(fd, buf, 100) == strlen(content) + strlen(ct));
    char ans[205] = {0};
    strcat(ans, content);
    strcat(ans, ct);
    assert(!strcmp(buf, ans));

    fd = ropen("/home/ubuntu/text.txt", O_CREAT | O_RDWR);
    assert(rwrite(fd, "hello", 5) == 5);
    assert(rseek(fd, 7, SEEK_SET) == 7);
    assert(rwrite(fd, "world", 5) == 5);
    char buf2[100] = {0};
    assert(rseek(fd, 0, SEEK_SET) == 0);
    assert(rread(fd, buf2, 100) == 12);
    assert(!memcmp(buf2, "hello\0\0world", 12));

    init_shell();

    assert(scat("/home/ubuntu/.bashrc") == 0);
    assert(stouch("/home/ls") == 0);
    assert(stouch("/home///ls") == 0);
    assert(swhich("ls") == 0);
    assert(stouch("/usr/bin/ls") == 0);
    assert(swhich("ls") == 0);
    assert(secho("hello world\\n") == 0);
    assert(secho("\\$PATH is $PATH") == 0);


    dump_ramfs();

    file_rcu("/helloworld");
    file_rcu("/home/helloworld");
    file_rcu("/home/ubuntu/helloworld");

    file_rcu("    ");
    file_rcu(" /sdsdas/s");

    dump_ramfs();

    close_shell();
    close_ramfs();
}

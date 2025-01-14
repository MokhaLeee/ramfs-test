#include "ramfs.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

const char *content = "export PATH=/usr/bin/\n";
const char *ct1 = "export PATH=/home:$PATH\n";
const char *ct2 = "export PATH=/home:$PATH\n";
const char *ct3 = "export PATH=/home:$PATH\n";
const char *ct4 = "export PATH=/home:$PATH\n";
const char *ct5 = "export mokha_PATH=/home:$PATH\n";

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
  assert(rwrite(ropen("/home////ubuntu//.bashrc", O_WRONLY | O_APPEND), ct1, strlen(ct1)) == strlen(ct1));
  memset(buf, 0, sizeof(buf));
  assert(rread(fd, buf, 100) == strlen(ct1));
  assert(!strcmp(buf, ct1));
  assert(rseek(fd, 0, SEEK_SET) == 0);
  memset(buf, 0, sizeof(buf));
  assert(rread(fd, buf, 100) == strlen(content) + strlen(ct1));
  char ans[205] = {0};
  strcat(ans, content);
  strcat(ans, ct1);
  assert(!strcmp(buf, ans));

  fd = ropen("/home/ubuntu/text.txt", O_CREAT | O_RDWR);
  assert(rwrite(fd, "hello", 5) == 5);
  assert(rseek(fd, 7, SEEK_SET) == 7);
  assert(rwrite(fd, "world", 5) == 5);
  char buf2[100] = {0};
  assert(rseek(fd, 0, SEEK_SET) == 0);
  assert(rread(fd, buf2, 100) == 12);
  assert(!memcmp(buf2, "hello\0\0world", 12));

  assert(rwrite(ropen("/home////ubuntu//.bashrc", O_WRONLY | O_APPEND), ct1, strlen(ct1)) == strlen(ct1));
  assert(rwrite(ropen("/home////ubuntu//.bashrc", O_WRONLY | O_APPEND), ct2, strlen(ct2)) == strlen(ct2));
  assert(rwrite(ropen("/home////ubuntu//.bashrc", O_WRONLY | O_APPEND), ct3, strlen(ct3)) == strlen(ct3));
  assert(rwrite(ropen("/home////ubuntu//.bashrc", O_WRONLY | O_APPEND), ct4, strlen(ct4)) == strlen(ct4));
  assert(rwrite(ropen("/home////ubuntu//.bashrc", O_WRONLY | O_APPEND), ct5, strlen(ct5)) == strlen(ct5));

  init_shell();

  assert(scat("/home/ubuntu/.bashrc") == 0);
  assert(stouch("/home/ls") == 0);
  assert(stouch("/home///ls") == 0);
  assert(swhich("ls") == 0);
  assert(stouch("/usr/bin/ls") == 0);
  assert(swhich("ls") == 0);
  assert(secho("hello world\\n") == 0);
  assert(secho("\\$PATH is $PATH") == 0);

  close_shell();
  close_ramfs();
}
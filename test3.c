
#include "ramfs.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

int test3() {
  printf("1\n");
  init_ramfs();
  printf("1\n");
  init_shell();
  
  printf("1\n");
  assert(sls("/home") == 1);
  printf("1\n");
  assert(scat("/home/ubuntu/.bashrc") == 1);
  printf("1\n");
  assert(scat("/") == 1);
  printf("1\n");
  assert(smkdir("/home") == 0);
  assert(smkdir("/test/1") == 1);
  assert(stouch("/home/1") == 0);
  assert(smkdir("/home/1/1") == 1);
  assert(stouch("/test/1") == 1);
  assert(swhich("notexist") == 1);

  close_shell();
  close_ramfs();
}
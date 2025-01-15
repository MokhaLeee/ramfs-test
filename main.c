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
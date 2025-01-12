#include "ramfs.h"
#include "shell.h"
#ifndef ONLINE_JUDGE
	#define print(...) printf("\033[31m");printf(__VA_ARGS__);printf("\033[0m");
#else
	#define print(...) 
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int sls(const char *pathname)
{
	print("ls %s\n", pathname);
	return 0;
}

int scat(const char *pathname)
{
	print("cat %s\n", pathname);
	return 0;
}

int smkdir(const char *pathname)
{
	print("mkdir %s\n", pathname);
	return 0;
}

int stouch(const char *pathname)
{
	print("touch %s\n", pathname);
	return 0;
}

int secho(const char *content)
{
	print("echo %s\n", content);
	return 0;
}

int swhich(const char *cmd)
{
	print("which %s\n", cmd);
	return 0;
}

void init_shell(void) {}

void close_shell(void) {}

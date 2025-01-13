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
	char ch;
	int fd, ret;

	print("cat %s\n", pathname);

	fd = ropen(pathname, O_RDONLY);
	if (fd < 0) {
		ERROR("%d\n", ret);
		return -1;
	}

	ret = rseek(fd, 0, SEEK_SET);
	if (ret != 0) {
		ERROR("%d\n", ret);
		return -1;
	}

	while (1) {
		ret = rread(fd, &ch, 1);
		if (ret != 1)
			break;

		printf("%c", ch);
	}

	if (ch != '\n')
		printf("\n");

	return 0;
}

int smkdir(const char *pathname)
{
	print("mkdir %s\n", pathname);

	rmkdir(pathname);
	return 0;
}

int stouch(const char *pathname)
{
	int fd, ret;

	print("touch %s\n", pathname);

	fd = ropen(pathname, O_RDWR | O_CREAT);
	if (fd < 0) {
		ERROR("%d\n", fd);
		return -1;
	}

	ret = rclose(fd);
	if (ret < 0) {
		ERROR("%d\n", ret);
		return -1;
	}

	return 0;
}

int secho(const char *content)
{
	print("echo %s\n", content);

	printf("%s\n", content);
	return 0;
}

int swhich(const char *cmd)
{
	print("which %s\n", cmd);
	return 0;
}

void init_shell(void) {}

void close_shell(void) {}

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
#include <assert.h>

int sls(const char *pathname)
{
	node *fnode, *parent;
	struct local_filename *filename;
	struct local_token *token, *token_bak;

	print("ls %s\n", pathname);

	if (!pathname || !*pathname)
		pathname = "/\0";

	filename = get_local_filename(pathname);
	if(!filename|| !filename->head) {
		// as readme file, we did not handle this error
		LOCAL_ERROR("invalid filename: %s\n", pathname);
		return 0;
	}

	/**
	 * find the node
	 */
	parent = working_dir;
	token_bak = token = filename->head;

	while (token) {
		LOCAL_TRACE("find token: current=%s, token=%s\n", parent->name, token->tok_name);
		fnode = next_node(token, parent, token->next ? DNODE : ANY_NODE);


		if (!fnode) {
			LOCAL_ERROR("failed to find fnode: %s\n", token->tok_name);
			/**
			 * error check
			 */
			if (token->next) {
				fnode = next_node(token, parent, FNODE);
				if (fnode) {
					fprintf(stdout, "ls: cannot access '%s': Not a directory\n", fnode->name);
					return 1;
				}
			}

			fprintf(stdout, "ls: cannot access '%s': No such file or directory\n", token_bak->tok_name);
			return 1;
		}

		parent = fnode;
		token_bak = token;
		token = token->next;
	}

	if (!fnode) {
		fprintf(stdout, "ls: cannot access '%s': No such file or directory\n", pathname);
		return 1;
	}

	LOCAL_TRACE("find node=%s\n", fnode->name);

	if (fnode->type == FNODE) {
		fprintf(stdout, "%s\n", fnode->name);
	} else {
		int i;

		if (fnode->nrde > 0) {
			assert(fnode->dirents != NULL);

			for (i = 0; i < fnode->nrde; i++) {
				if (i != 0)
					fprintf(stdout, " ");

				fprintf(stdout, "%s", fnode->dirents[i]->name);
			}
		}

		fprintf(stdout, "\n");
	}
	return 0;
}

int scat(const char *pathname)
{
	char ch;
	int fd, ret;

	print("cat %s\n", pathname);

	fd = ropen(pathname, O_RDONLY);
	if (fd < 0) {
		LOCAL_ERROR("%d\n", ret);
		return 1;
	}

	ret = rseek(fd, 0, SEEK_SET);
	if (ret != 0) {
		LOCAL_ERROR("%d\n", ret);
		return 1;
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

	return rmkdir(pathname) == 0 ? 0 : 1;
}

int stouch(const char *pathname)
{
	int fd, ret;

	print("touch %s\n", pathname);

	fd = ropen(pathname, O_RDWR | O_CREAT);
	if (fd < 0) {
		LOCAL_ERROR("%d\n", fd);
		return 1;
	}

	ret = rclose(fd);
	if (ret < 0) {
		LOCAL_ERROR("%d\n", ret);
		return 1;
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

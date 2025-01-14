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

/**
 * internal
 */
#define BUFFER_SIZE 4096

struct shell_path {
	struct shell_path *pre, *next;
	char *fpath;
};

static struct shell_path *shell_path_head = NULL;

#if 0
static bool path_exists(const char *fpath)
{
	struct shell_path *path;

	for (path = shell_path_head; path != NULL; path = path->next) {
		assert(path->fpath);

		if (strcpy(path->fpath, fpath))
			return true;
	}
	return false;
}
#endif

static void do_init_shell(void)
{
	int fd;
	ssize_t newline_offset;
	char buffer[BUFFER_SIZE];

	shell_path_head = malloc(sizeof(*shell_path_head));
	assert(shell_path_head != NULL);

	shell_path_head->fpath = malloc(2);
	assert(shell_path_head->fpath != NULL);

	shell_path_head->fpath[0] = '/';
	shell_path_head->fpath[1] = '\0';
	shell_path_head->pre = shell_path_head->next = NULL;

	fd = ropen("/home/ubuntu/.bashrc\0", O_RDONLY);
	if (fd < 0) {
		// LOCAL_ERROR("open bashrc: %d\n", fd);
		return;
	}

	newline_offset = 0;
	while (1) {
		ssize_t i, bytes_read;
		int prefix_len = 12; // strlen("export PATH=\n");

		(void)rseek(fd, newline_offset, SEEK_SET);

		bytes_read = rread(fd, buffer, BUFFER_SIZE);
		if (bytes_read <= 0)
			break;

		if (buffer[0] == '\0')
			break;

		for (i = 0; i < bytes_read; i++) {
			char ch = buffer[i];

			if (ch == '\n' || ch == '\0')
				break;
		}

		buffer[i] = '\0';
		newline_offset += i + 1;

		LOCAL_TRACE("buf=%s\n", buffer);

		if (strncmp(buffer, "export PATH=\n", prefix_len) == 0) {
			/**
			 * nice shoot!
			 */
			size_t j, start, end, len;
			struct shell_path *new_path;

			start = end = prefix_len;
			len = strlen(buffer) + 1;

			for (j = start; j < len; j++) {
				if (buffer[j] == ':' || buffer[j] == '\0') {
					end = j - 1;

					if (strncmp(buffer + start, "$PATH", 5) == 0) {
						start = j + 1;
						continue;
					}

					new_path = malloc(sizeof(struct shell_path));
					assert(new_path != NULL);

					new_path->fpath = malloc(end - start + 1);
					assert(new_path->fpath != NULL);

					memset(new_path->fpath, 0, end - start + 1);
					strncpy(new_path->fpath, buffer + start, end - start + 1);

					if (shell_path_head)
						shell_path_head->pre = new_path;

					new_path->next = shell_path_head;
					new_path->pre = NULL;
					shell_path_head = new_path;

					start = j + 1;
					LOCAL_TRACE("get path: %s\n", new_path->fpath);
				}
			}
		}
	}
}

static int do_swhich(const char *cmd)
{
	node *fnode;
	struct shell_path *path;
	char buf[BUFFER_SIZE];

	if (!cmd || !*cmd || !valid_fpath(cmd))
		return 1;

	for (path = shell_path_head; path != NULL; path = path->next) {
		snprintf(buf, BUFFER_SIZE, "%s/%s", path->fpath, cmd);

		fnode = find(buf, FNODE);
		if (fnode) {
			fprintf(stdout, "%s\n", buf);
			return 0;
		}
	}

	return 1;
}

/**
 * API
 */
int sls(const char *pathname)
{
	node *fnode;

	print("ls %s\n", pathname);

	if (!pathname || !*pathname)
		pathname = "/\0";

	fnode = find(pathname, ANY_NODE);
	if (!fnode) {
		int scan_ret = scan_fpath(pathname);

		LOCAL_ERROR("scan_ret=%d on fpath=%s\n", scan_ret, pathname);

		switch (scan_ret) {
		case SCAN_FPATH_PASS_FNODE:
		case SCAN_FPATH_PASS_DNODE:
		case SCAN_FPATH_INVALID:
			break;

		case SCAN_FPATH_NODIR:
		case SCAN_FPATH_NOTARGET:
			fprintf(stdout, "ls: cannot access '%s': No such file or directory\n", pathname);
			break;

		case SCAN_FPATH_ISNOTDIR:
		default:
			fprintf(stdout, "ls: cannot access '%s': Not a directory\n", pathname);
			break;
		}

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

	if (!valid_fpath(pathname)) {
		fprintf(stdout, "No such file or directory\n");
		return 1;
	}

	fd = ropen(pathname, O_RDONLY);
	if (fd < 0) {
		int scan_ret = scan_fpath(pathname);

		LOCAL_ERROR("scan_ret=%d on fpath=%s\n", scan_ret, pathname);

		switch (scan_ret) {
		case SCAN_FPATH_PASS_FNODE:
		case SCAN_FPATH_INVALID:
			break;

		case SCAN_FPATH_NODIR:
		case SCAN_FPATH_NOTARGET:
			fprintf(stdout, "cat: %s: No such file or directory\n", pathname);
			break;

		case SCAN_FPATH_ISNOTDIR:
		case SCAN_FPATH_PASS_DNODE:
		default:
			fprintf(stdout, "cat: %s: Not a directory\n", pathname);
			break;
		}

		return 1;
	}

	ret = rseek(fd, 0, SEEK_SET);
	if (ret != 0) {
		LOCAL_ERROR("%d\n", ret);
		return 1;
	}

	while (1) {
		ret = rread(fd, &ch, 1);
		if (ret != 1 || ch == '\0')
			break;

		printf("%c", ch);
	}

	if (ch != '\n')
		printf("\n");

	return 0;
}

int smkdir(const char *pathname)
{
	int ret;

	print("mkdir %s\n", pathname);

	ret = rmkdir(pathname);

	if (ret < 0) {
		int scan_ret = scan_fpath(pathname);

		LOCAL_ERROR("scan_ret=%d on fpath=%s\n", scan_ret, pathname);

		switch (scan_ret) {
		case SCAN_FPATH_PASS_DNODE:
			fprintf(stdout, "mkdir: cannot create directory '%s': File exists\n", pathname);
			break;

		case SCAN_FPATH_INVALID:
			break;

		case SCAN_FPATH_NODIR:
		case SCAN_FPATH_NOTARGET:
			fprintf(stdout, "mkdir: cannot create directory '%s': No such file or directory\n", pathname);
			break;

		case SCAN_FPATH_PASS_FNODE:
		case SCAN_FPATH_ISNOTDIR:
		default:
			fprintf(stdout, "mkdir: cannot create directory '%s': Not a directory\n", pathname);
			break;
		}

		return 1;
	}

	return 0;
}

int stouch(const char *pathname)
{
	int fd, ret;

	print("touch %s\n", pathname);

	if (!valid_fpath(pathname)) {
		fprintf(stdout, "No such file or directory\n");
		return 1;
	}

	fd = ropen(pathname, O_RDWR | O_CREAT);
	if (fd < 0) {
		int scan_ret = scan_fpath(pathname);

		LOCAL_ERROR("scan_ret=%d on fpath=%s\n", scan_ret, pathname);

		switch (scan_ret) {
		case SCAN_FPATH_PASS_FNODE:
		case SCAN_FPATH_PASS_DNODE:
			return 0;
			break;

		case SCAN_FPATH_INVALID:
			break;

		case SCAN_FPATH_NODIR:
		case SCAN_FPATH_NOTARGET:
			fprintf(stdout, "touch: cannot touch '%s': No such file or directory\n", pathname);
			break;

		case SCAN_FPATH_ISNOTDIR:
		default:
			fprintf(stdout, "touch: cannot touch '%s': Not a directory\n", pathname);
			break;
		}

		return 1;
	}

	ret = rclose(fd);
	if (ret < 0) {
		LOCAL_ERROR("%d\n", ret);
		// return 1;
		return 0;
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
	return do_swhich(cmd);
}

void init_shell(void)
{
	do_init_shell();
}

void close_shell(void)
{
	struct shell_path *path = shell_path_head;

	while (path) {
		struct shell_path *next = path->next;

		free(path);
		path = next;
	}
}

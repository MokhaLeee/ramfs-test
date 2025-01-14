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
static int shell_ret;

#define ERR_RET(err_no) {              \
	shell_ret = (err_no);              \
	if ((err_no) < 0)                  \
		LOCAL_ERROR("%d\n", (err_no)); \
	goto err_ret;                      \
}

struct shell_path {
	struct shell_path *pre, *next;
	char *name, *fpath;
};

static struct shell_path *shell_path_head = NULL;
static struct shell_path *shell_vars = NULL;

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

void free_var(struct shell_path *var)
{
	if (!var)
		return;

	if (var->pre)
		var->pre = var->next;

	if (var->next)
		var->next = var->pre;

	if (var->fpath)
		free(var->fpath);

	if (var->name)
		free(var->name);

	free(var);
}

struct shell_path *get_value_from_var(const char *var_name)
{
	struct shell_path *var;

	for (var = shell_vars; var != NULL; var++) {
		assert(var->name != NULL);

		if (strcmp(var_name, var->name) == 0) {
			LOCAL_INFO("find var: $%s=%s\n", var->name, var->fpath);
			return var;
		}
	}
	LOCAL_INFO("failed to find var: %s\n", var_name);
	return NULL;
}

void parse_str(char *dst, char *src, int len)
{
	int i;
	char ch;

	for (i = 0; i < len; i++) {
		ch = src[i];

		if (ch == '\0') {
			*dst++ = ch;
			return;
		}

		if (ch == '$') {
			struct shell_path *var = get_value_from_var(src + i + 1);

			if (var) {
				strcpy(dst, var->fpath);
				dst += strlen(var->fpath);
				i += strlen(var->name);
				continue;
			}
		}

		*dst++ = ch;
	}
}

static void do_init_vars(void)
{
	int fd;
	ssize_t newline_offset;
	char buffer[BUFFER_SIZE];
	char varname[BUFFER_SIZE];
	char var_val[BUFFER_SIZE];
	char parsed_var_val[BUFFER_SIZE];

	fd = ropen("/home/ubuntu/.bashrc\0", O_RDONLY);
	if (fd < 0) {
		// LOCAL_ERROR("open bashrc: %d\n", fd);
		return;
	}

	newline_offset = 0;
	while (1) {
		ssize_t i, bytes_read;
		int prefix_len = 7; // strlen("export \0");

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

		if (strncmp(buffer, "export \n", prefix_len) == 0) {
			/**
			 * nice shoot!
			 */
			size_t j, start, end, len;

			off_t name_start, name_end, val_start, val_end;

			start = end = prefix_len;
			len = strlen(buffer) + 1;

			name_start = name_end = val_start = val_end = prefix_len;

			for (j = start; j < len; j++) {
				if (buffer[j] == '\0') {
					if (name_end > 0 && val_start > name_end) {
						struct shell_path *new_var;

						// valid
						val_end = j;

						memset(varname, 0, sizeof(varname));
						memset(var_val, 0, sizeof(var_val));

						strncpy(varname, buffer + name_start, name_end - name_start);
						parse_str(var_val, buffer + val_start, val_end - val_start);

						LOCAL_INFO("get val(%s)=%s\n", varname, var_val);

						new_var = get_value_from_var(varname);
						if (new_var)
							free_var(new_var);

						new_var = malloc(sizeof(*new_var));
						assert(new_var);
						new_var->name  = malloc(strlen(varname) + 1);
						new_var->fpath = malloc(strlen(var_val) + 1);
						assert(new_var->name);
						assert(new_var->fpath);

						strcpy(new_var->name, varname);
						strcpy(new_var->fpath, var_val);

						if (shell_vars)
							shell_vars->pre = new_var;

						new_var->next = shell_vars;
						new_var->pre = NULL;
						shell_vars = new_var;

						LOCAL_INFO("set val(%s)=%s\n", new_var->name, new_var->fpath);
					}
					end = j + 1;
					break;
				}

				if (buffer[j] == '=') {
					if (name_end == prefix_len && val_start == prefix_len && val_end == prefix_len) {
						// valid
						name_end = j;
						val_start = j + 1;
					}
				}
			}
		}
	}
}

static void do_init_path(void)
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

					new_path->name = NULL;
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

static void do_init_shell(void)
{
	do_init_vars();
	do_init_path();
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

		ERR_RET(1);
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

err_ret:
	return shell_ret;
}

int scat(const char *pathname)
{
	char ch;
	int fd, ret;

	print("cat %s\n", pathname);

	if (!valid_fpath(pathname)) {
		fprintf(stdout, "No such file or directory\n");
		ERR_RET(1);
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

		ERR_RET(1);
	}

	ret = rseek(fd, 0, SEEK_SET);
	if (ret != 0) {
		LOCAL_ERROR("%d\n", ret);
		ERR_RET(1);
	}

	while (1) {
		ret = rread(fd, &ch, 1);
		if (ret != 1 || ch == '\0')
			break;

		printf("%c", ch);
	}

	if (ch != '\n')
		printf("\n");

	shell_ret = 0;

err_ret:
	return shell_ret;
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

		ERR_RET(1);
	}

	shell_ret = 0;

err_ret:
	return shell_ret;
}

int stouch(const char *pathname)
{
	int fd, ret;

	print("touch %s\n", pathname);

	if (!valid_fpath(pathname)) {
		fprintf(stdout, "No such file or directory\n");
		ERR_RET(1);
	}

	fd = ropen(pathname, O_RDWR | O_CREAT);
	if (fd < 0) {
		int scan_ret = scan_fpath(pathname);

		LOCAL_ERROR("scan_ret=%d on fpath=%s\n", scan_ret, pathname);

		switch (scan_ret) {
		case SCAN_FPATH_PASS_FNODE:
		case SCAN_FPATH_PASS_DNODE:
			ERR_RET(0);
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

		ERR_RET(1);
	}

	ret = rclose(fd);
	if (ret < 0) {
		LOCAL_ERROR("%d\n", ret);
		// ERR_RET(1);
		ERR_RET(0);
	}

	shell_ret = 0;

err_ret:
	return shell_ret;
}

int secho(const char *content)
{
	print("echo %s\n", content);

	printf("%s\n", content);
	shell_ret = 0;

err_ret:
	return shell_ret;
}

int swhich(const char *cmd)
{
	print("which %s\n", cmd);

	shell_ret = do_swhich(cmd);
	return shell_ret;
}

void init_shell(void)
{
	do_init_shell();

	shell_ret = 0;
}

void close_shell(void)
{
	if (shell_vars) {
		while (shell_vars->next)
			free_var(shell_vars->next);

		free(shell_vars);
	}

	if (shell_path_head) {
		while (shell_path_head->next)
			free_var(shell_path_head->next);

		free(shell_path_head);
	}
}

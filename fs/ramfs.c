#include "ramfs.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

node *root = NULL;

#define NRFD 4096
FD fdesc[NRFD];

node *find(const char *pathname)
{
	return NULL;
}

int ropen(const char *pathname, int flags)
{
	return 0;
}

int rclose(int fd)
{
	return 0;
}

ssize_t rwrite(int fd, const void *buf, size_t count)
{
	return 0;
}

ssize_t rread(int fd, void *buf, size_t count)
{
	size_t copied;
	FD *file = fdesc + fd;
	node *node = file->f;

	if (file->used != true || !(file->flags & (O_RDONLY | O_RDWR)))
		return -EBADF;

	if (!node)
		return -EBADF;

	if (node->type != FNODE)
		return -EISDIR;

	if (!node->content || node->size <= 0)
		return -EBADF;

	copied = node->size > count ? count : node->size;
	memcpy(buf, node->content, copied);

	if (copied < count)
		memset(buf, 0, count - copied);

	return copied;
}

off_t rseek(int fd, off_t offset, int whence)
{
	return 0;
}

int rmkdir(const char *pathname)
{
	return 0;
}

int rrmdir(const char *pathname)
{
	return 0;
}

int runlink(const char *pathname)
{
	return 0;
}

void init_ramfs()
{
	root = NULL;
	memset(fdesc, 0, sizeof(fdesc));
}

void close_ramfs() {}

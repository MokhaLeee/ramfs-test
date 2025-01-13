#include "ramfs.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

node *root = NULL;
node *working_dir = NULL;

#define NRFD 4096
FD fdesc[NRFD];

/**
 * string ops
 */
const char *get_token(const char *s)
{
	while (*s && *s == '/')
		s++;

	while (*s && *s != '/')
		s++;

	while (*s && *s == '/')
		s++;

	return s;
}

/**
 * node ops
 */
static node *find_node(const char *fpath)
{
	return NULL;
}

node *create_root(void)
{
	struct node *fnode;
	const char *name = "/\0";

	fnode = malloc(sizeof(node));
	if (!fnode)
		return NULL;

	fnode->type = DNODE;
	fnode->dirents = NULL;
	fnode->nrde = 0;
	fnode->name = malloc(strlen(name) + 1);
	strcpy(fnode->name, name);
	fnode->size = 0;

	return fnode;
}

node *create_node(const char *fpath, node *parent, int type)
{
	struct node *fnode;
	const char *name, *tmp;

	if (*fpath == '\0')
		return NULL;

	if (!parent || *fpath == '/')
		parent = root;

	fnode = malloc(sizeof(node));
	if (!fnode)
		return NULL;

	name = tmp = fpath;
	while (*tmp != '\0') {
		name = tmp;
		tmp = get_token(tmp);
	}

	fnode->name = malloc(strlen(name) + 1);
	strcpy(fnode->name, name);

	/**
	 * setup: todo
	 */
	fnode->type = type;

	return fnode;
}

node *next_node(const char **name, node *current)
{
	return NULL;
}

void remove_node(node *fnode) {}

/**
 * API
 */
int ropen(const char *fpath, int flags)
{
	int fd;
	bool fd_alloced;
	node *fnode;

	/**
	 * find the node
	 */
	fnode = find_node(fpath);
	if (!fnode) {
		/**
		 * try to create the file
		 */
		const char *tmp, *name;
		node *parent;

		/**
		 * find the parent node step by step
		 */
		tmp = name = fpath;
		parent = fnode = working_dir;
		while (1) {
			fnode = next_node(&tmp, fnode);

			if (!*tmp || !fnode)
				break;

			name = tmp;
			parent = fnode;
		}

		/**
		 * check the parent node type
		 */
		if (!parent || parent->type != DNODE)
			return -1;

		/**
		 * check the name valid: todo
		 */

		/**
		 * create node
		 */
		fnode = create_node(tmp, parent, FNODE);
		if (!fnode)
			return -1;
	}

	if (fnode->type != FNODE)
		return -1;

	/**
	 * alloc the desc
	 */
	fd_alloced = false;

	for (fd = 0; fd < NRFD; fd++) {
		FD *file = &fdesc[fd];

		if (file->used == false) {
			file->flags = flags;
			file->offset = 0;
			file->f = fnode;

			fd_alloced = true;
		}
	}

	if (!fd_alloced)
		return -1;

	return fd;
}

int rclose(int fd)
{
	FD *file;

	/**
	 * check the fd valid
	 */	
	if (fd < 0 || fd > NRFD)
		return -EBADF;

	file = &fdesc[fd];
	if (file->used != true)
		return -EBADF;

	/**
	 * release the desc
	 */
	file->used = false;
	return 0;
}

ssize_t rwrite(int fd, const void *buf, size_t count)
{
	FD *file;

	/**
	 * check the fd valid
	 */	
	if (fd < 0 || fd > NRFD)
		return -EBADF;

	file = &fdesc[fd];
	if (file->used != true)
		return -EBADF;

	/**
	 * check the node valid
	 */
	if (file->f->type != FNODE)
		return -EISDIR;

	if ((file->offset + count) > file->f->size)
		return -ENOMEM;

	memcpy(file->f->content + file->offset, buf, count);

	file->offset += count;
	return 0;
}

ssize_t rread(int fd, void *buf, size_t count)
{
	FD *file;

	/**
	 * check the fd valid
	 */	
	if (fd < 0 || fd > NRFD)
		return -EBADF;

	file = &fdesc[fd];
	if (file->used != true)
		return -EBADF;

	/**
	 * check the node valid
	 */
	if (file->f->type != FNODE)
		return -EISDIR;

	if ((file->offset + count) > file->f->size)
		return -ENOMEM;

	memcpy(buf, file->f->content + file->offset, count);

	file->offset += count;
	return 0;
}

off_t rseek(int fd, off_t offset, int whence)
{
	off_t new_offset;
	FD *file = &fdesc[fd];

	if (file->used != true)
		return -EINVAL;

	switch (whence) {
	case SEEK_SET:
		new_offset = offset;
		break;

	case SEEK_CUR:
		new_offset = file->offset + offset;
		break;

	case SEEK_END:
		new_offset = file->f->size - offset;
		break;

	default:
		return -EINVAL;
	}

	if (new_offset < 0 || new_offset > file->f->size)
		return -1;

	file->offset = new_offset;
	return 0;
}

int rmkdir(const char *fpath)
{
	const char *tmp, *name;
	node *fnode, *parent;

	/**
	 * 1. find the existing node
	 */
	fnode = find_node(fpath);
	if (fnode)
		return -1;

	/**
	 * 2. find the parent node step by step
	 */
	tmp = name = fpath;
	parent = fnode = working_dir;
	while (1) {
		fnode = next_node(&tmp, fnode);

		if (!*tmp || !fnode)
			break;

		name = tmp;
		parent = fnode;
	}

	/**
	 * 3. check the parent node type
	 */
	if (!parent || parent->type != DNODE)
		return -1;

	/**
	 * 4. check the name valid: todo
	 */

	/**
	 * create node
	 */
	fnode = create_node(tmp, parent, DNODE);
	if (!fnode)
		return -1;

	return 0;
}

int rrmdir(const char *fpath)
{
	node *fnode;

	/**
	 * find node
	 */
	fnode = find_node(fpath);
	if (!fnode)
		return -1;

	/**
	 * 2. check the node type
	 */
	if (fnode->type != DNODE)
		return -1;

	/**
	 * 3. check deletable
	 */
	if (fnode == root || fnode == working_dir)
		return -1;

	/**
	 * 4. has child (other wise not delect ??)
	 */
	if (fnode->nrde > 0)
		return -1;

	/**
	 * 5. remove node
	 */
	remove_node(fnode);
	return 0;
}

int runlink(const char *fpath)
{
	node *fnode;

	/**
	 * 1. find the node
	 */
	fnode = find_node(fpath);
	if (!fnode)
		return -1;

	/**
	 * 2. check the node type
	 */
	if (fnode->type != FNODE)
		return -1;

	/**
	 * 3. remove
	 */
	remove_node(fnode);
	return 0;
}

void init_ramfs()
{
	working_dir = root = create_root();
	memset(fdesc, 0, sizeof(fdesc));

	assert(root);
}

void close_ramfs()
{
	remove_node(root);
}

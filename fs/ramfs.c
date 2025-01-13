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

#define ERR_RET(err_no) { \
	ret = (err_no); \
	if ((err_no) < 0) \
		ERROR("%d\n", (err_no)); \
	goto err_ret; \
}

/**
 * string ops
 */
struct local_token {
	struct local_token *next;
	char *tok_name;
};

struct local_filename {
	const char *uptr;
	struct local_token *head;
};

static void get_token(const char *s, int *start, int *len)
{
	*start = *len = 0;

	while (*s && *s == '/')
		s++, (*start)++;

	while (*s && *s != '/')
		s++, (*len)++;

	while (*s && *s == '/')
		s++;
}

static struct local_filename *get_local_filename(const char *fpath)
{
	int start, len;
	struct local_token *token;
	struct local_filename *filename = malloc(sizeof(*filename));

	assert(filename != NULL);

	filename->uptr = fpath;
	filename->head = token = NULL;

	for (;;) {
		struct local_token *new_token;

		get_token(fpath, &start, &len);

		if (len == 0)
			break;

		new_token = malloc(sizeof(*new_token));
		assert(new_token != NULL);

		new_token->tok_name = malloc(len + 1);
		assert(new_token->tok_name != NULL);

		memcpy(new_token->tok_name, fpath + start, len);
		new_token->tok_name[len] = '\0';

		new_token->next = NULL;

		if (token == NULL)
			filename->head = new_token;
		else
			token->next = new_token;

		TRACE("trace token generation: token=%s, start=%d, len=%d\n", new_token->tok_name, start, len);

		fpath = fpath + start + len;
		token = new_token;
	}
	return filename;
}

static int get_token_depth(struct local_token *token)
{
	int ret = 0;

	while (token) {
		assert(token->tok_name != NULL);
		TRACE("depth=%d, token=%s\n", ret, token->tok_name);

		ret++;
		token = token->next;
	}
	return ret;
}

static void free_local_filename(struct local_filename *filename)
{
	struct local_token *token;

	if (!filename)
		return;

	token = filename->head;
	while (token) {
		struct local_token *next = token->next;

		if (token->tok_name)
			free(token->tok_name);

		free(token);
		token = next;
	}

	free(token);
}

/**
 * node ops
 */
node *next_node(const struct local_token *token, node *current)
{
	int i;
	node *next_node = NULL;

	if (token == NULL)
		return NULL;

	if (current->type != DNODE || current->dirents == NULL || current->nrde == 0)
		return NULL;

	if (current == NULL)
		current = root;

	// todo: ./../?

	for (i = 0; i < current->nrde; i++) {
		node *child = current->dirents[i];

		assert(child != NULL && child->name != NULL);

		if (strcmp(child->name, token->tok_name) == 0) {
			next_node = child;
			break;
		}
	}

	return next_node;
}

static node *find_node(const struct local_token *token, node *current)
{
	struct node *fnode;

	if (!token)
		return NULL;

	fnode = current;
	while (token) {
		fnode = next_node(token, fnode);

		if (!fnode || !token)
			break;
	}
	return fnode;
}

static node *spawn_node(const struct local_token *token, node *parent, int type)
{
	struct node *fnode;

	assert(type == DNODE || type == FNODE);

	fnode = malloc(sizeof(node));
	assert(fnode != NULL);

	fnode->type = type;
	fnode->parent = parent;
	fnode->dirents = NULL;
	fnode->content = NULL;
	fnode->nrde = fnode->nrde_max = 0;
	fnode->name = malloc(strlen(token->tok_name) + 1);
	assert(fnode->name != NULL);
	strcpy(fnode->name, token->tok_name);
	fnode->size = 0;

	if (parent) {
		// link node

		assert(parent->type == DNODE);
		assert(parent->nrde <= parent->nrde_max);

		if (!parent->dirents) {
			parent->dirents = malloc(NRDE_GRP * sizeof(struct node *));
			assert(parent->dirents != NULL);

			parent->dirents[0] = fnode;
			parent->nrde = 1;
			parent->nrde_max = NRDE_GRP;
		} else {
			if (parent->nrde == parent->nrde_max) {
				// oops
				struct node **new_dirents = malloc((parent->nrde_max + NRDE_GRP) * sizeof(struct node *));

				assert(new_dirents != NULL);
				memcpy(new_dirents, parent->dirents, parent->nrde_max * sizeof(struct node *));

				free(parent->dirents);
				parent->dirents = new_dirents;
			}

			parent->dirents[parent->nrde++] = fnode;
		}
	}

	return fnode;
}

static node *create_root(void)
{
	const struct local_token token = {
		.tok_name = "/\0",
	};
	return spawn_node(&token, NULL, DNODE);
}

static node *create_node(const struct local_token *token, node *parent, int type)
{
	if (!token || !token->tok_name)
		return NULL;

	// todo: how should we handle the case parent does not exist?

	return spawn_node(token, parent, type);
}

static void remove_node(node *fnode)
{
	int i;
	node *parent;

	if (!fnode)
		return;

	if (fnode->type == DNODE && fnode->nrde > 0) {
		assert(fnode->dirents != NULL);

		for (i = 0; i < fnode->nrde; i++) {
			node *child = fnode->dirents[i];

			assert(child != NULL);
			remove_node(child);
		}
	}

	/**
	 * unlink from parent
	 */
	parent = fnode->parent;
	if (parent) {
		assert(parent->nrde > 0);
		assert(parent->dirents != NULL);

		for (i = 0; i < parent->nrde; i++) {
			node *brother = parent->dirents[i];

			assert(brother != NULL);

			if (brother == fnode)
				break;
		}

		assert(i < parent->nrde);

		for (; i < parent->nrde; i++)
			parent->dirents[i] = parent->dirents[i + 1];

		parent->nrde--;
	}
}

/**
 * API
 */
int ropen(const char *fpath, int flags)
{
	int fd, ret = -1;
	bool fd_alloced;
	node *fnode, *parent;
	struct local_filename *filename;
	struct local_token *token;

	/**
	 * Judge invalid flags
	 */
	switch (FLAG_GET_RD(flags)) {
	case O_RDONLY:
		if (flags & O_TRUNC)
			flags &= ~O_TRUNC;
		break;

	case O_WRONLY:
		break;

	case O_RDWR:
		break;

	default:
		flags = FLAG_SET_RD(flags, O_WRONLY);
		break;
	}

	filename = get_local_filename(fpath);
	if(!filename|| !filename->head)
		ERR_RET(-EPERM);

	/**
	 * find the node
	 */
	parent = working_dir;
	token = filename->head;

	while (token) {
		fnode = next_node(token, parent);

		if (!fnode)
			break;

		parent = fnode;
		token = token->next;
	}

	if (fnode && (flags & O_TRUNC) && (flags & (O_WRONLY | O_RDWR))) {
		remove_node(fnode);
		fnode = NULL;
	}

	if (!fnode) {
		/**
		 * try to create the file
		 */
		if (get_token_depth(token) > 1 || !parent || parent->type != DNODE)
			ERR_RET(-EINVAL);

		/**
		 * create node
		 */
		fnode = create_node(token, parent, FNODE);
		if (!fnode)
			ERR_RET(-ENOMEM);
	}

	if (fnode->type != FNODE)
		ERR_RET(-ENOANO);

	/**
	 * alloc the desc
	 */
	fd_alloced = false;

	for (fd = 0; fd < NRFD; fd++) {
		FD *file = &fdesc[fd];

		if (file->used == false) {
			file->used = true;
			file->flags = flags;
			file->offset = 0;
			file->f = fnode;

			if (flags & O_APPEND)
				fdesc[fd].offset = fnode->size;
			else
				fdesc[fd].offset = 0;

			ret = fd;
			fd_alloced = true;
			break;
		}
	}

	if (!fd_alloced)
		ERR_RET(-ENOMEM);

err_ret:
	free_local_filename(filename);
	return ret;
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
	int ret = 0;
	size_t max_size;
	FD *file;

	/**
	 * check the fd valid
	 */	
	if (fd < 0 || fd > NRFD)
		ERR_RET(-EBADF);

	file = &fdesc[fd];
	if (file->used != true || FLAG_GET_RD(file->flags) == O_RDONLY)
		ERR_RET(-EBADF);

	/**
	 * check the node valid
	 */
	if (file->f->type != FNODE)
		ERR_RET(-EISDIR);

	max_size = file->offset + count + 1;
	if (max_size > file->f->size) {
		void *new_content = malloc(max_size);

		if (!new_content)
			ERR_RET(-ENOMEM);

		memcpy(new_content, file->f->content, file->f->size);
		memset(new_content + file->f->size, 0, max_size - file->f->size);

		free(file->f->content);
		file->f->size = max_size;
		file->f->content = new_content;
	}

	memcpy(file->f->content + file->offset, buf, count);

	file->offset += count;
	TRACE("file=%s, offset=%d\n", file->f->name, file->offset);
	TRACE("src=%s\n", (const char *)buf);
	TRACE("dst=%s\n", (char *)file->f->content);

err_ret:
	return ret;
}

ssize_t rread(int fd, void *buf, size_t count)
{
	int ret = 0;
	FD *file;

	/**
	 * check the fd valid
	 */	
	if (fd < 0 || fd > NRFD)
		ERR_RET(-EBADF);

	file = &fdesc[fd];
	if (file->used != true)
		ERR_RET(-EBADF);

	TRACE("flags=0x%04X\n", file->flags);

	switch (FLAG_GET_RD(file->flags)) {
	case O_RDONLY:
	case O_RDWR:
		break;

	default:
		ERR_RET(-EBADF);
	}

	/**
	 * check the node valid
	 */
	if (file->f->type != FNODE)
		ERR_RET(-EISDIR);

	if ((file->offset + count) > file->f->size)
		count = file->f->size - file->offset;

	if (count <= 0) {
		ret = 0;
		goto err_ret;
	}

	memcpy(buf, file->f->content + file->offset, count);

	file->offset += count;
	ret = count;

	TRACE("file=%s, len=%d, buf=%s\n", file->f->name, ret, (char *)buf);

err_ret:
	return ret;
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

	TRACE("offset=%d\n", file->offset);
	return 0;
}

int rmkdir(const char *fpath)
{
	int depth, ret = 0;
	node *fnode, *parent;
	struct local_filename *filename;
	struct local_token *token;

	filename = get_local_filename(fpath);
	if(!filename|| !filename->head)
		ERR_RET(-EINVAL);

	/**
	 * find the node
	 */
	parent = working_dir;
	token = filename->head;

	while (token) {
		fnode = next_node(token, parent);

		TRACE("find node: %s\n", token->tok_name);

		if (!fnode)
			break;

		parent = fnode;
		token = token->next;
	}

	if (fnode)
		ERR_RET(-EISDIR);


	/**
	 * try to create the file
	 */
	depth = get_token_depth(token);
	if (depth > 1 || !parent || parent->type != DNODE) {
		ERROR("no parrent, depth=%d\n", depth);
		ERR_RET(-EINVAL);
	}

	/**
	 * create node
	 */
	fnode = create_node(token, parent, DNODE);
	if (!fnode)
		ERR_RET(-ENOMEM);

err_ret:
	free_local_filename(filename);
	return ret;
}

int rrmdir(const char *fpath)
{
	int ret = 0;
	node *fnode;
	struct local_filename *filename;

	filename = get_local_filename(fpath);
	if(!filename|| !filename->head)
		ERR_RET(-EINVAL);

	/**
	 * find node
	 */
	fnode = find_node(filename->head, working_dir);
	if (!fnode)
		ERR_RET(-EPERM);

	/**
	 * 2. check the node type
	 */
	if (fnode->type != DNODE)
		ERR_RET(-1);

	/**
	 * 3. check deletable
	 */
	if (fnode == root || fnode == working_dir)
		ERR_RET(-1);

	/**
	 * 4. has child (other wise not delect ??)
	 */
	if (fnode->nrde > 0)
		ERR_RET(-1);

	/**
	 * 5. remove node
	 */
	remove_node(fnode);

err_ret:
	free_local_filename(filename);

	if (ret)
		ERROR("%d\n", ret);

	return ret;
}

int runlink(const char *fpath)
{
	int ret = 0;
	node *fnode;
	struct local_filename *filename;

	filename = get_local_filename(fpath);
	if(!filename|| !filename->head)
		ERR_RET(-1);

	/**
	 * 1. find the node
	 */
	fnode = find_node(filename->head, working_dir);
	if (!fnode)
		ERR_RET(-1);

	/**
	 * 2. check the node type
	 */
	if (fnode->type != FNODE)
		ERR_RET(-1);

	/**
	 * 3. remove
	 */
	remove_node(fnode);

err_ret:
	free_local_filename(filename);
	return ret;
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

	root = NULL;
}

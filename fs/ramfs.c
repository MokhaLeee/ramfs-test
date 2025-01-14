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

#define FNAME_MAX_LEN 32

#define ERR_RET(err_no) {              \
	ret = (err_no);                    \
	if ((err_no) < 0)                  \
		LOCAL_ERROR("%d\n", (err_no)); \
	goto err_ret;                      \
}

node *get_root(void)
{
	return root;
}

node *get_working_dir(void)
{
	return working_dir;
}

/**
 * string ops
 */

bool valid_fpath(const char *s)
{
	if (!s || !*s)
		return false;

	while (*s) {
		switch (*s++) {
		case '0' ... '9':
		case 'a' ... 'z':
		case 'A' ... 'Z':
			break;

		case '/':
		case '.':
			break;

		default:
			return false;
		}
	}
	return true;
}

int get_token_depth(struct local_token *token)
{
	int ret = 0;

	while (token) {
		assert(token->tok_name != NULL);
		LOCAL_TRACE("depth=%d, token=%s\n", ret, token->tok_name);

		ret++;
		token = token->next;
	}
	return ret >= 0 ? ret : -1;
}

bool token_is_leaf(struct local_token *token)
{
#if 0
	// not fast enough
	return get_token_depth(token) == 0;
#endif
	return (!token || !token->next);
}

void free_local_filename(struct local_filename *filename)
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

static void spawn_token(const char *s, int *start, int *len)
{
	*start = *len = 0;

	while (*s && *s == '/')
		s++, (*start)++;

	while (*s && *s != '/')
		s++, (*len)++;

	while (*s && *s == '/')
		s++;
}

struct local_filename *get_local_filename(const char *fpath)
{
	int ret, start, len;
	struct local_token *token;
	struct local_filename *filename;

	ret = 0;

	if (!valid_fpath(fpath))
		return NULL;

	filename = malloc(sizeof(*filename));
	assert(filename != NULL);

	filename->uptr = fpath;
	filename->head = token = NULL;

	if (*fpath == '/') {
		struct local_token *new_token;
		// root
		new_token = malloc(sizeof(*new_token));
		assert(new_token != NULL);
		new_token->tok_name = malloc(2);
		assert(new_token->tok_name != NULL);

		new_token->tok_name[0] = '/';
		new_token->tok_name[1] = '\0';
		new_token->next = NULL;

		filename->head = new_token;
		new_token->pre = filename->head;

		fpath++;
	}

	for (;;) {
		struct local_token *new_token;

		spawn_token(fpath, &start, &len);

		if (len == 0)
			break;

		if (len > FNAME_MAX_LEN)
			ERR_RET(-1);

		new_token = malloc(sizeof(*new_token));
		assert(new_token != NULL);

		new_token->tok_name = malloc(len + 1);
		assert(new_token->tok_name != NULL);

		memcpy(new_token->tok_name, fpath + start, len);
		new_token->tok_name[len] = '\0';

		new_token->next = NULL;

		if (token == NULL) {
			filename->head = new_token;
			new_token->pre = filename->head;
		} else {
			token->next = new_token;
			new_token->pre = token;
		}

		LOCAL_TRACE("trace token generation: token=%s, start=%d, len=%d\n", new_token->tok_name, start, len);

		fpath = fpath + start + len;
		token = new_token;
	}

err_ret:
	if (ret < 0) {
		free_local_filename(filename);
		return NULL;
	}

	return filename;
}

/**
 * node ops
 */
node *next_node(const struct local_token *token, node *current, int type)
{
	int i;
	node *next_node = NULL;

	if (token == NULL)
		return NULL;

	if (current == NULL)
		current = working_dir;

	if (token->tok_name[0] == '/') {
		// current = root;
		return root;
	}

	if (current->type != DNODE || current->dirents == NULL || current->nrde == 0)
		return NULL;

	assert(token->tok_name != NULL);

	if (token->next != NULL) {
		LOCAL_TRACE("node=%s, type=%d, next=%s\n", token->tok_name, type, token->next->tok_name);
		// assert(type != FNODE);
		type = DNODE;
	}

	// todo: ./../?

	for (i = 0; i < current->nrde; i++) {
		node *child = current->dirents[i];

		assert(child != NULL && child->name != NULL);

		if (strcmp(child->name, token->tok_name) == 0) {
			/**
			 * check the type
			 */
			if (type != ANY_NODE && type != child->type)
				continue;

			next_node = child;
			break;
		}
	}

	return next_node;
}

int scan_fpath(const char *fpath)
{
	// detect is there any file in side a fpath
	// hooly shit....

	int ret;
	node *fnode, *parent;
	struct local_filename *filename;
	struct local_token *token;

	filename = get_local_filename(fpath);
	if(!filename|| !filename->head) {
		LOCAL_ERROR("invalid filename: %s\n", fpath);
		ERR_RET(SCAN_FPATH_INVALID);
	}

	/**
	 * find the node
	 */
	parent = working_dir;
	token = filename->head;

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
					ERR_RET(SCAN_FPATH_ISNOTDIR);
				} else {
					ERR_RET(SCAN_FPATH_NODIR);
				}
			}

			// although we failed to
			ERR_RET(SCAN_FPATH_NOTARGET);
		}

		parent = fnode;
		token = token->next;
	}

	ret = SCAN_FPATH_PASS;

err_ret:
	free_local_filename(filename);
	return ret;
}

static node *find_node(const struct local_token *token, node *current, int type)
{
	struct node *fnode;

	if (!token)
		return NULL;

	fnode = current;
	while (token) {
		LOCAL_TRACE("current=%s, child=%s\n", fnode->name, token->tok_name);
		fnode = next_node(token, fnode, type);
		token = token->next;

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

	LOCAL_TRACE("node=%s, type=%d\n", fnode->name, fnode->type);
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
	if (!token || !token->tok_name) {
		LOCAL_ERROR("invalid token, parent=%s!\n", parent ? parent->name : "no parent");
		return NULL;
	}

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

static void node_realloc_content(node *fnode, int max_size)
{
	void *new_content;

	assert(max_size >= fnode->size);

	new_content = malloc(max_size);
	assert(new_content);

	memset(new_content, 0, max_size);

	if (fnode->content) {
		memcpy(new_content, fnode->content, fnode->size);
		free(fnode->content);
	}

	fnode->size = max_size;
	fnode->content = new_content;
}

/**
 * API
 */
node *find(const char *fpath, int type)
{
	struct local_filename *filename;

	filename = get_local_filename(fpath);
	if(!filename|| !filename->head)
		return NULL;

	return find_node(filename->head, working_dir, type);
}

int ropen(const char *fpath, int flags)
{
	int fd, ret = -1;
	bool fd_alloced;
	node *fnode, *parent;
	struct local_filename *filename;
	struct local_token *token, *token_bak;

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
	token_bak = token = filename->head;

	while (token) {
		LOCAL_TRACE("find token: current=%s, token=%s\n", parent->name, token->tok_name);
		fnode = next_node(token, parent, token->next ? DNODE : FNODE);

		if (!fnode)
			break;

		parent = fnode;
		token_bak = token;
		token = token->next;
	}

	if (!parent)
		parent = get_root();

	if (fnode) {
		LOCAL_TRACE("find node=%s, type=%d\n", fnode->name, fnode->type);
	}

	if (fnode && fnode->type == FNODE && flags & O_CREAT) {
		ERR_RET(-EPERM);
	}

	/**
	 * ?
	 */
	if (fnode && fnode->type == DNODE) {
		LOCAL_TRACE("find a DNODE with same name: %s\n", fnode->name);
		token = token_bak;
		fnode = NULL;
	}

	if (fnode && fnode->type == FNODE && (flags & O_TRUNC) && (flags & (O_WRONLY | O_RDWR))) {
		LOCAL_TRACE("trunc node=%s\n", fnode->name);
		remove_node(fnode);
		token = token_bak;
		fnode = NULL;
	}

#if 0
	if (fnode && (flags & O_CREAT))
		ERR_RET(-EINVAL);
#endif

	if (!fnode && (flags & (O_CREAT | O_TRUNC))) {
		/**
		 * try to create the file
		 */
		int depath=get_token_depth(token);
		if (depath > 1 || parent->type != DNODE) {
			LOCAL_ERROR("error depth=%d, token=%s, parent (%d)=%s\n",
					depath, token->tok_name, parent->type, parent->name);
			ERR_RET(-EINVAL);
		}

		LOCAL_INFO("create node=%s\n", token->tok_name);

		/**
		 * create node
		 */
		fnode = create_node(token, parent, FNODE);
		if (!fnode)
			ERR_RET(-ENOMEM);
	}

	if (!fnode)
		ERR_RET(-1);

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
	return ret >= 0 ? ret : -1;
}

int rclose(int fd)
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

	/**
	 * release the desc
	 */
	file->used = false;

err_ret:
	return ret >= 0 ? ret : -1;
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

	max_size = file->offset + count;
	if (max_size > file->f->size)
		node_realloc_content(file->f, max_size);

	memcpy(file->f->content + file->offset, buf, count);

	file->offset += count;
	ret = count;

	LOCAL_TRACE("file=%s, offset=%d\n", file->f->name, file->offset);
	LOCAL_TRACE("src=%s\n", (const char *)buf);
	LOCAL_TRACE("dst=%s\n", (char *)file->f->content);

err_ret:
	return ret >= 0 ? ret : -1;
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

	LOCAL_TRACE("flags=0x%04X\n", file->flags);

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

	LOCAL_TRACE("file=%s, len=%d, buf=%s\n", file->f->name, ret, (char *)buf);

err_ret:
	return ret >= 0 ? ret : -1;
}

off_t rseek(int fd, off_t offset, int whence)
{
	int ret = 0;
	off_t new_offset;
	FD *file = &fdesc[fd];

	if (file->used != true)
		ERR_RET(-EINVAL);

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
		ERR_RET(-EINVAL);
	}

	if (new_offset < 0)
		new_offset = 0;

	LOCAL_TRACE("file=%s, offset=%ld, new_offset=%ld, whence=%d, file size=%d\n",
			file->f->name, offset, new_offset, whence, file->f->size);

	if (new_offset > file->f->size)
		node_realloc_content(file->f, new_offset);

	file->offset = new_offset;
	ret = file->offset;

err_ret:
	return ret >= 0 ? ret : -1;
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
		LOCAL_TRACE("current=%s, child=%s\n", parent->name, token->tok_name);
		fnode = next_node(token, parent, DNODE);

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
		LOCAL_ERROR("no parrent, depth=%d\n", depth);
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
	return ret >= 0 ? ret : -1;
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
	fnode = find_node(filename->head, working_dir, DNODE);
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
	return ret >= 0 ? ret : -1;
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
	fnode = find_node(filename->head, working_dir, FNODE);
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
	return ret >= 0 ? ret : -1;
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

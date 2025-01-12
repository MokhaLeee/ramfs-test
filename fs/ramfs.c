#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ramfs.h"

node *root = NULL;
node *working_dir = NULL;

#define NRFD 4096
FD fdesc[NRFD];

/**
 * utils
 */
#define BIT_MATCH(flags, bitmask) (((flags) & (bitmask)) == (bitmask))

static inline bool is_slash(char ch)
{
	return (ch == '/' || ch == '\\');
}

static inline bool is_fpath_terminator(char ch)
{
	return (ch == '\0' || is_slash(ch));
}

static bool slashed_strcmp(const char *s1, const char *s2)
{
	while (1) {
		char ch1 = *s1++;
		char ch2 = *s2++;

		if (is_slash(ch1))
			ch1 = '\0';

		if (is_slash(ch2))
			ch2 = '\0';

		if (ch1 != ch2)
			return -1;

		if (ch1 == '\0' || ch2 == '\0') {
			return (ch1 == ch2);
		}
	}
}

static size_t slashed_strlen(const char *str)
{
	size_t ret = 0;

	while (1) {
		char ch = *str++;

		if (ch == '\0' || is_slash(ch))
			break;

		ret++;
	}
	return ret;
}

static void slashed_strcpy(char *dst, const char *src)
{
	while (1) {
		char ch = *src++;

		if (is_slash(ch))
			ch = '\0';

		*dst++ = ch;

		if (ch == '\0')
			break;
	}
}

static int get_path_depth(const char *str)
{
	int ret = 0;

	while (1) {
		char ch = *str;

		if (ch == '\0')
			break;

		if (is_slash(ch) && *(str + 1) != '\0')
				ret++;

		str++;
	}
	return ret;
}

static bool fpath_valid(const char *str, bool is_dir)
{
	int i;

	for (i = 0; str[i] != '\0'; i++) {
		switch (str[i]) {
		case '0' ... '9':
			break;

		case '.':
			break;

		case '/':
		case '\\':
			break;

		case 'a' ... 'z':
			break;

		case 'A' ... 'Z':
			break;

		default:
			return false;
		}
	}

	if (i <= 0)
		return false;

	if (!is_dir && is_slash(str[i - 1]))
		return false;

	return true;
}

static bool root_fpath(const char *fpath)
{
	return (fpath[0] == '/' && fpath[1] == '\0');
}

/**
 * node ops
 */

void dump_fs(node *parent)
{
	node *inode;

	if (parent == NULL)
		parent = root;

	LTRACE("parent=%s\n", parent->name);

	for (inode = parent->child; inode != NULL; inode = inode->next) {
		LTRACE("child=%s, %s\n", inode->name, inode->type == FNODE ? "file": "dir");

		if (inode->child)
			dump_fs(inode);
	}
}

static node *find_child_node(node *parent, const char *name)
{
	node *tmp;

	if (name[0] == '.' && is_slash(name[1]))
		return parent;

	if (name[0] == '.' && name[1] == '.' && is_slash(name[2]))
		return parent->parent;

	LTRACE("parent=%s\n", parent->name);

	for (tmp = parent->child; tmp != NULL; tmp = tmp->next) {
		LTRACE("child=%s, parent=%s\n", tmp->name, parent->name);

		if (tmp->name && slashed_strcmp(tmp->name, name))
			return tmp;
	}

	LTRACE("failed to find child=%s, parent=%s\n", name, parent->name);
	return NULL;
}

node *find_node(const char *path)
{
	node *child, *cur_node = working_dir;

	if (is_slash(path[0])) {
		cur_node = root;
		path = path + 1;
	}

	LTRACE("find from: %s\n", cur_node->name);

	while (!is_fpath_terminator(*path)) {
		LTRACE("path=%s\n", path);

		child = find_child_node(cur_node, path);
		if (child == NULL)
			return NULL;

		cur_node = child;
		path += slashed_strlen(path) + 1;
	}
	return cur_node;
}

static int link_node(node *self, node *parent)
{
	if (!parent || !self)
		return -1;

	if (parent->type != DNODE)
		return -1;

	LTRACE("parent=%s, child=%s\n", parent->name, self->name);

	self->parent = parent;

	if (!parent->child) {
		self->pre = NULL;
		parent->child = self;
	} else {
		node *tmp;

		for (tmp = parent->child; tmp->next != NULL; tmp = tmp->next)
			;

		tmp->next = self;
		self->pre = tmp;
	}
	return 0;
}

void unlink_node(node *fnode)
{
	if (!fnode)
		return;

	if (fnode->parent) {
		node *tmp;

		for (tmp = fnode->parent->child; tmp != NULL; tmp = tmp->next) {
			if (tmp == fnode) {
				if (fnode->pre)
					fnode->pre->next = fnode->next;
				else {
					// if no pre, then he is the first child
					fnode->parent->child = fnode->next;
				}
				break;
			}
		}
	}
}

static node *spawn_node(const char *name, enum node_type type)
{
	int len;
	node *new_node = malloc(sizeof(node));

	if (!new_node) {
		ERROR("[ERR]: failed to malloc node: %s", name);
		return NULL;
	}

	new_node->type = type;
	new_node->next = new_node->parent = NULL;

	new_node->child = NULL;
	new_node->content = NULL;
	new_node->size = 0;

	len = slashed_strlen(name);
	if (len <= 0)
		len = 1;

	new_node->name = malloc(slashed_strlen(name) + 1);
	if (!new_node->name) {
		ERROR("[ERR]: failed to malloc node name: %s", name);
		free(new_node);
		return NULL;
	}

	if (root_fpath(name))
		strcpy(new_node->name, name);
	else
		slashed_strcpy(new_node->name, name);

	return new_node;
}

node *create_node(const char *path, enum node_type type, bool create_dir)
{
	node *child, *cur_node = working_dir;

	if (is_slash(path[0])) {
		cur_node = root;
		path = path + 1;
	}

	LTRACE("create from: %s\n", cur_node->name);

	/**
	 * find exists sub dir
	 */
	while (!is_fpath_terminator(*path)) {
		child = find_child_node(cur_node, path);
		if (child == NULL)
			break;

		LTRACE("find path=%s\n", path);
		cur_node = child;
		path += slashed_strlen(path) + 1;
	}

	if (is_fpath_terminator(*path)) {
		/**
		 * already exists
		 */
		return cur_node;
	}

	if (!create_dir && get_path_depth(path) != 0)
		return NULL;

	/**
	 * create new nodes
	 */
	while (!is_fpath_terminator(*path)) {
		child = spawn_node(path, DNODE);
		assert(child != NULL);

		link_node(child, cur_node);
		LTRACE("create path=%s, parent=%s\n", child->name, cur_node->name);

		cur_node = child;
		path += slashed_strlen(path) + 1;
	}

	cur_node->type = type;
	return cur_node;
}

void free_node(node *fnode)
{
	node *child;

	if (!fnode)
		return;

	for (child = fnode->child; child != NULL; child = child->next)
		free_node(child);

	if (fnode->content)
		free(fnode->content);

	free(fnode);
}

void unlink_and_free_node(node *fnode)
{
	unlink_node(fnode);
	free_node(fnode);
}

/**
 * fd ops
 */
static FD *get_file(int fd)
{
	if (fd < 0 || fd >= NRFD)
		return NULL;

	return &fdesc[fd];
}

static int alloc_file(void)
{
	int fd;

	for (fd = 0; fd < NRFD; fd++)
		if (fdesc[fd].used == false)
			return fd;

	return -1;
}

int find_fd_from_node(node *fnode)
{
	int fd;

	for (fd = 0; fd < NRFD; fd++) {
		FD *file = fdesc + fd;

		if (file->used && file->f == fnode)
			return fd;
	}

	return -1;
}

static FD *find_file_from_path(const char *fpath)
{
	int fd;
	node *fnode;
	
	fnode = find_node(fpath);
	if (!fnode)
		return NULL;

	fd = find_fd_from_node(fnode);
	if (fd < 0)
		return NULL;

	return get_file(fd);
}

static int remove_fpath(const char *fpath, bool is_dir)
{
	FD *file = find_file_from_path(fpath);

	if (!file)
		return -ENOENT;

	if (!file->f)
		return -ENOENT;

	if (is_dir) {
		if (file->f->type != DNODE)
			return -ENOTDIR;
	} else {
		if (file->f->type != FNODE)
			return -EISDIR;
	}

	if (file->used) {
		// do nothing
	}

	if (file) {
		unlink_and_free_node(file->f);
		file->used = false;
	}

	return 0;
}

/**
 * file ops
 */

node *find(const char *pathname)
{
	return find_node(pathname);
}

int ropen(const char *pathname, int _flags)
{
	int fd;
	node *fnode;
	int flags = _flags;

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

	if (!fpath_valid(pathname, false))
		return -EINVAL;

	fnode = find_node(pathname);
	if (fnode && (flags & O_TRUNC) && (flags & (O_WRONLY | O_RDWR))) {
		unlink_and_free_node(fnode);
		fnode = NULL;
	}

	if (!fnode || fnode->parent == NULL) {
		fnode = create_node(pathname, FNODE, (flags & O_CREAT) ? true : false);
		if (!fnode)
			return -EPERM;
	}

	fd = alloc_file();
	if (fd < 0)
		return -ENOMEM;

	fdesc[fd].used = true;
	fdesc[fd].flags = flags;
	fdesc[fd].f = fnode;

	if (flags & O_APPEND)
		fdesc[fd].offset = fnode->size;
	else
		fdesc[fd].offset = 0;

	return fd;
}

int rclose(int fd)
{
	FD *file = get_file(fd);

	if (!file)
		return -EINVAL;

	file->used = false;
	return 0;
}

ssize_t rwrite(int fd, const void *buf, size_t count)
{
	size_t offset, max_size;
	FD *file = get_file(fd);
	node *fnode = file->f;

	if (!file)
		return -EINVAL;

	if (file->used != true || !(file->flags & (O_WRONLY | O_RDWR)))
		return -EBADF;

	if (!fnode)
		return -EBADF;

	if (fnode->type != FNODE)
		return -EISDIR;

	offset = file->offset;
	max_size = offset + count;

	if (max_size > fnode->size) {
		void *new_content = malloc(max_size);

		if (!new_content)
			return -ENOMEM;

		memcpy(new_content, fnode->content, fnode->size);
		memset(new_content + fnode->size, 0, max_size - fnode->size);

		free(fnode->content);
		fnode->size = max_size;
		fnode->content = new_content;
	}

	memcpy(fnode->content + offset, buf, count);
	file->offset += count;

	LTRACE("file=%s, contents=%s\n", fnode->name, (char *)fnode->content);
	return 0;
}

ssize_t rread(int fd, void *buf, size_t count)
{
	size_t copy;
	FD *file = get_file(fd);
	node *fnode = file->f;

	LTRACE("flags=0x%04X\n", file->flags);

	switch (FLAG_GET_RD(file->flags)) {
	case O_RDONLY:
	case O_RDWR:
		break;

	default:
		return -EBADF;
	}

	if (!fnode)
		return -EBADF;

	if (fnode->type != FNODE)
		return -EISDIR;

	if (!fnode->content || (fnode->size - file->offset) <= 0)
		return -EBADF;

	LTRACE("size=%ld, offset=%ld\n", fnode->size, file->offset);

	copy = fnode->size - file->offset;
	if (copy > count)
		copy = count;

	if (fnode->size < file->offset)
		return -EBADF;

	memcpy(buf, fnode->content + file->offset, copy);

	LTRACE("node=%s, offset=%ld, ch=%c", fnode->name, file->offset, ((char *)buf)[0]);
	file->offset += count;
	return copy;
}

off_t rseek(int fd, off_t offset, int whence)
{
	off_t real_off;
	FD *file = get_file(fd);

	if (file->used != true)
		return -EINVAL;

	switch (whence) {
	case SEEK_SET:
		real_off = offset;
		break;

	case SEEK_CUR:
		real_off = file->offset + offset;
		break;

	case SEEK_END:
		real_off = file->f->size - offset;
		break;

	default:
		return -EINVAL;
	}

	if (real_off < 0 || real_off > file->f->size)
		return -EINVAL;

	file->offset = real_off;

	LTRACE("offset=%ld\n", file->offset);
	return real_off;
}

int rmkdir(const char *pathname)
{
	node *fnode;

	if (!fpath_valid(pathname, true))
		return -EINVAL;

	fnode = find_node(pathname);
	if (fnode)
		return -EEXIST;

#if 0
	if (fnode->parent == NULL)
		return -ENOENT;

	if (fnode->parent->type != DNODE)
		return -ENOTDIR;

	fnode = create_node(pathname, DNODE, false);
#else
	fnode = create_node(pathname, DNODE, true);
#endif

	if (!fnode)
		return -1;

	return 0;
}

int rrmdir(const char *pathname)
{
	if (!fpath_valid(pathname, true))
		return -EINVAL;

	return remove_fpath(pathname, true);
}

int runlink(const char *pathname)
{
	if (!fpath_valid(pathname, false))
		return -EINVAL;

	return remove_fpath(pathname, false);
}

void init_ramfs()
{
	memset(fdesc, 0, sizeof(fdesc));
	working_dir = root = spawn_node("/", DNODE);

	/**
	 * it should be better not to free fd 0
	 */
	fdesc[0].used = true;
	fdesc[0].f = root;
}

void close_ramfs()
{
	free_node(root);

	working_dir = root = NULL;
}

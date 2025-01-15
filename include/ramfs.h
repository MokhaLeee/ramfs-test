#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define O_APPEND 02000
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR 02

#define FLAG_GET_RD(flags) (flags & 3)
#define FLAG_SET_RD(flags, rd) ((flags & (~3)) | (rd))

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define NRDE_GRP 40

typedef struct node {
	enum { FNODE, DNODE, ANY_NODE } type;
	struct node *parent;
	struct node **dirents; // if DTYPE
	void *content;
	int nrde, nrde_max;
	char *name;
	int size;
} node;

typedef struct FD {
	bool used;
	int offset;
	int flags;
	node *f;
} FD;

typedef intptr_t ssize_t;
typedef uintptr_t size_t;
typedef long off_t;

struct local_token {
	struct local_token *pre, *next;
	char *tok_name;
};

struct local_filename {
	const char *uptr;
	struct local_token *head;
};

bool valid_fpath(const char *s);
bool valid_root_fpath(const char *s);
bool token_is_leaf(struct local_token *token);
void free_local_filename(struct local_filename *filename);
struct local_filename *get_local_filename(const char *fpath);
int get_token_depth(struct local_token *token);

node *next_node(const struct local_token *token, node *current, int type);

int ropen(const char *pathname, int flags);
int rclose(int fd);
ssize_t rwrite(int fd, const void *buf, size_t count);
ssize_t rread(int fd, void *buf, size_t count);
off_t rseek(int fd, off_t offset, int whence);
int rmkdir(const char *pathname);
int rrmdir(const char *pathname);
int runlink(const char *pathname);
void init_ramfs(void);
void close_ramfs(void);
node *find(const char *fpath, int type);

extern node *root;
extern node *working_dir;

node *get_root(void);
node *get_working_dir(void);

enum scan_fpath_ref {
	SCAN_FPATH_PASS_FNODE,
	SCAN_FPATH_PASS_DNODE,
	SCAN_FPATH_INVALID,
	SCAN_FPATH_NODIR,
	SCAN_FPATH_NOTARGET,
	SCAN_FPATH_ISNOTDIR,
};

int scan_fpath(const char *fpath);
void modify_fpath(char *fpath);

/**
 * internal
 */

#define TRACE_EN 1
#define INFO_EN  1
#define ERROR_EN 1

#define dprintf(prefix, format, ...) printf(prefix"(%s:%d: %s) "format, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#if TRACE_EN
#define LOCAL_TRACE(...) dprintf("[TRACE]", __VA_ARGS__)
#else
#define LOCAL_TRACE(...)
#endif

#if INFO_EN
#define LOCAL_INFO(...)  dprintf("[INFO ]", __VA_ARGS__)
#else
#define LOCAL_INFO(...)
#endif

#if ERROR_EN
#define LOCAL_ERROR(...) dprintf("[ERROR]", __VA_ARGS__)
#else
#define LOCAL_ERROR(...)
#endif

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef intptr_t ssize_t;
typedef uintptr_t size_t;
typedef long off_t;

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

enum node_type {
	FNODE,
	DNODE,
};

typedef struct node {
	enum node_type type;
	struct node *parent, *child;
	struct node *pre, *next;
	char *name;

	void *content;
	size_t size;
} node;

typedef struct FD {
	bool used;
	off_t offset;
	int flags;
	node *f;
} FD;

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
node *find(const char *pathname);

/**
 * internal
 */

#define dprintf(format, ...) printf("(%s): "format, __func__, __VA_ARGS__)

#define LTRACE(...) // dprintf(__VA_ARGS__)
#define INFO(...) dprintf(__VA_ARGS__)
#define ERROR(...) // dprintf(__VA_ARGS__)

void dump_fs(node *parent);

#include "ramfs.h"
#include "shell.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

extern node *root;
const char *content = "export PATH=$PATH:/usr/bin/\n\0";
const char *ct = "export PATH=/home:$PATH\0";

int main(void)
{
	init_ramfs();

	smkdir("/home\0");
	smkdir("/home/ubuntu\0");
	smkdir("/usr\0");
	smkdir("/usr/bin\0");
	stouch("/home/ubuntu/.bashrc\0");
	rwrite(ropen("/home/ubuntu/.bashrc\0", O_WRONLY), content, strlen(content));
	rwrite(ropen("/home/ubuntu/.bashrc\0", O_WRONLY | O_APPEND), ct, strlen(ct));
	scat("/home/ubuntu/.bashrc\0");

	init_shell();
	swhich("ls");
	stouch("/usr/bin/ls");
	swhich("ls");
	stouch("/home/ls");
	swhich("ls");
	secho("hello world");
	secho("The Environment Variable PATH is:\\$PATH");
	close_ramfs();
	close_shell();
	assert(root==NULL);

	exit(EXIT_SUCCESS);
}

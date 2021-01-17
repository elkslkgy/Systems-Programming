/*b05902118 陳盈如*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	kill(getppid(), SIGUSR1);
fprintf(stderr, "kill(%d)\n", getppid());
	return 0;
}

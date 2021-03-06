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
	char filename[128], buf[512];
	int fd, ret;
	strcpy(filename, argv[1]);
// fprintf(stderr, "%s\n", filename);
	fd = open(filename, O_RDONLY, 0);
	if (fd == -1) {
		write(1, "NOT FOUND", 9);
		close(fd);
		exit(1);
		fprintf(stderr, "file_reader doesn't found <<%s>>\n", filename);
	}
	else {
		while (1) {
			memset(buf, 0, sizeof(buf));
			ret = read(fd, buf, sizeof(buf));
			if (ret < 0) {
				fprintf(stderr, "Error when reading file %s\n", filename);
                break;
			}
			else if (ret == 0) break;
			write(1, buf, ret);
		}
		fprintf(stderr, "Done reading file [%s]\n", filename);
		close(fd);
		exit(0);
	}
}

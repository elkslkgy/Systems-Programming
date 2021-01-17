/*b05902118 陳盈如*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[])
{
	int host_id, random_key, fd[2], money[4], pay;
	char player_index, buffer[25], input[15], output[15];
	host_id = atoi(argv[1]);
	player_index = argv[2][0];
	random_key = atoi(argv[3]);
	sprintf(input, "host%d_%c.FIFO", host_id, player_index);
	sprintf(output, "host%d.FIFO", host_id);
	//fd[0] = input_fd fd[1] = output_fd
	//fd[]prefer to use O_RDWR directily (by行健)
	fd[0] = open(input, O_RDWR);
	fd[1] = open(output, O_RDWR);
	for (int i = 0; i < 10; i++)
	{
		read(fd[0], buffer, sizeof(buffer));
		sscanf(buffer, "%d %d %d %d", &money[0], &money[1], &money[2], &money[3]);
		switch(player_index - 'A')
		{
		case 0:
			if (i == 0 || i == 4 || i == 8)
				pay = 18;
			else if (i == 7)
				pay = money[player_index - 'A'] - 18;
			else if (i == 9)
				pay = money[player_index - 'A'];
			else
				pay = 0;
			break;
		case 1:
			if (i == 1 || i == 5 || i == 9)
				pay = 11;
			else if (i == 8)
				pay = money[player_index - 'A'] - 11;
			else
				pay = 0;
			break;
		case 2:
			if (i == 2 || i == 6)
				pay = 12;
			else if (i == 5)
				pay = money[player_index - 'A'] - 12;
			else if (i == 9)
				pay = money[player_index - 'A'];
			else
				pay = 0;
			break;
		case 3:
			if (i == 3 || i == 7)
				pay = 17;
			else if (i == 6)
				pay = money[player_index - 'A'] - 17;
			else if (i == 9)
				pay = money[player_index - 'A'];
			else
				pay = 0;
			break;
		}
	//	pay = (money[player_index - 'A']) - (money[player_index - 'A'] / (119 * (i+1)));
		sprintf(buffer, "%c %d %d\n", player_index, random_key, pay);
		write(fd[1], buffer, strlen(buffer));
		fsync(fd[1]);
	}
	close(fd[0]);
	close(fd[1]);
	return 0;
}

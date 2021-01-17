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

void combine(int set[], int total, int choice, int subset[]);
void hand_out(int a, int b, int c, int d);
void insertion_sort(int set[][2], int len);

int host_num, player_num, rank[25] = {}, ready[25] = {}, rest = 0;
int fd_in[25][2] = {};	//fd_in:bidding_system write to host.c
int fd_out[25][2] = {};	//fd_out:host.c write to bidding_system
int score[25][2] = {};	//score[][1]:the order of player
int pid[25] = {};	//pid[num]: the order of the host
fd_set rset, rset_copy;
struct timeval t;

int main(int argc, char *argv[])
{
	int player[25] = {};	//player[num]: the order of the player
	int four_player[25] = {};
	int sort[25][2];
	host_num = atoi(argv[1]);
	player_num = atoi(argv[2]);
	rest = player_num * (player_num - 1) * (player_num - 2) * (player_num - 3) / 24;
//fprintf(stderr, "total:%d\n", rest);
	FD_ZERO(&rset); FD_ZERO(&rset_copy);
//fprintf(stderr, "hey, im in line28.\n");
	for (int i = 0; i < player_num; i++)
		player[i] = score[i][1] = i + 1;
//for (int i = 0; i < player_num; i++)
//	fprintf(stderr, "score[%d][1] = %d\n", i, score[i][1]);
	for (int i = 1; i <= host_num; i++)
	{
		pipe(fd_in[i - 1]); pipe(fd_out[i - 1]);
		char host_id[5];
		sprintf(host_id, "%d", i);
		if ((pid[i] = fork()) == 0)
		{
			dup2(fd_in[i - 1][0], 0);
			dup2(fd_out[i - 1][1], 1);
			if (execlp("./host", "./host", host_id, (char *)NULL) < 0)
				printf("execlp_host.c error\n");
		}
		FD_SET(fd_out[i - 1][0], &rset);
//fprintf(stderr, "fd_out[%d - 1][0] = %d\n", i, fd_out[i - 1][0]);
	}
//fprintf(stderr, "before combine\n");
	for (int i = 1; i <= player_num - 3; i++)
	{
		for (int j = i + 1; j <= player_num - 2; j++)
		{
			for (int k = j + 1; k <= player_num - 1; k++)
			{
				for (int l = k + 1; l <= player_num; l++)
				{
//total++;
//fprintf(stderr, "total:%d\n", total);
					hand_out(i, j, k, l);
				}
			}
		}
	}
//fprintf(stderr, "rest in current:%d\n", rest);
	int change = 0;
	int maxfd = getdtablesize();
	t.tv_sec = 0; t.tv_usec = 0;
	while (rest)
	{
		rset_copy = rset;
		change = select(maxfd, &rset_copy, NULL, NULL, &t);
		rest -= change;
//fprintf(stderr, "after selecting.\n");
//fprintf(stderr, "change = %d\nrest = %d\n\n", change, rest);
		if (change <= 0)
			continue;
		for (int i = 0; i < host_num; i++)
		{
//fprintf(stderr, "Yes, we are inside!\n");
//fprintf(stderr, "fd_in[%d][1] = %d\n", i, fd_in[i][1]);
//fprintf(stderr, "FD_ISSET(fd_in[%d][1], &rset_copy) = %d\n", i, FD_ISSET(fd_in[i][1], &rset_copy));
			if (FD_ISSET(fd_out[i][0], &rset_copy))
			{
//fprintf(stderr, "FD_ISSet(fd_out)\n");
				char buffer[100];
				int player[5], price[5];
//fprintf(stderr, "before read\n");
				read(fd_out[i][0], buffer, sizeof(buffer));
//fprintf(stderr, "after read\n");
				ready[i] = 0;
				sscanf(buffer, "%d %d\n%d %d\n%d %d\n%d %d\n", &player[0], &price[0], &player[1], &price[1], &player[2], &price[2], &player[3], &price[3]);
//fprintf(stderr, "buffer:%s\n", buffer);
				for (int j = 0; j < 4; j++)
				{
//fprintf(stderr, "score:%d\n", player[j] - 1);
					score[player[j] - 1][0] += 4 - price[j];
				}
			}
		}
//rest += change;
//		break;
	}
//	combine(player, player_num, 4, four_player);
//fprintf(stderr, "after combine\n");
	for (int i = 0; i < host_num; i++)
	{
		write(fd_in[i][1], "-1 -1 -1 -1\n", 15);
		fsync(fd_in[i][1]);
	}
	memcpy(sort, score, sizeof(score));
//for (int i = 0; i < player_num; i++)
//{
//	fprintf(stderr, "score[%d][0] = %d score[%d][1] = %d\n", i, score[i][0], i, score[i][1]);
//}
	insertion_sort(sort, player_num);
//for (int i = 0; i < player_num; i++)
//	fprintf(stderr, "%d\n", sort[i][0]);
//fprintf(stderr, "hey, im in line52.\n");
	int prize = 1;
	for (int i = 0; i < player_num; i++)
	{
		if (i != 0 && sort[i][0] == sort[i - 1][0])
		{
			rank[sort[i][1] - 1] = rank[sort[i - 1][1] - 1];
		}
		else
		{
			rank[sort[i][1] - 1] = prize;
		}
		prize++;
	}
	for (int i = 0; i < player_num; i++)
	{
		printf("%d %d\n", i + 1, rank[i]);
	}
	for (int i = 0; i < host_num; i++)
	{
		close(fd_in[i][0]);
		close(fd_in[i][1]);
		close(fd_out[i][0]);
		close(fd_out[i][1]);
	}
	return 0;
}

void hand_out(int a, int b, int c, int d)
{
	for (int i = 0; i < host_num; i++)
	{
		if (ready[i] == 0)
		{
			char buffer[30];
			sprintf(buffer, "%d %d %d %d\n", a, b, c, d);
//fprintf(stderr, "im gonna write to the host %d", i);
			write(fd_in[i][1], buffer, strlen(buffer));
			fsync(fd_out[i][1]);
			ready[i] = 1;
			return;
		}
	}
//fprintf(stderr, "there is no host to call, and the rest = %d\n", rest);
	int change = 0;
	int maxfd = getdtablesize();
	t.tv_sec = 0; t.tv_usec = 0;
	while (1)
	{
		rset_copy = rset;
		change = select(maxfd, &rset_copy, NULL, NULL, &t);
		rest -= change;
//fprintf(stderr, "after selecting.\n");
//fprintf(stderr, "change = %d\n", change);
		if (change <= 0)
			continue;
		for (int i = 0; i < host_num; i++)
		{
//fprintf(stderr, "Yes, we are inside!\n");
//fprintf(stderr, "fd_in[%d][1] = %d\n", i, fd_in[i][1]);
//fprintf(stderr, "FD_ISSET(fd_in[%d][1], &rset_copy) = %d\n", i, FD_ISSET(fd_in[i][1], &rset_copy));
			if (FD_ISSET(fd_out[i][0], &rset_copy))
			{
//fprintf(stderr, "FD_ISSet(fd_out)\n");
				char buffer[100];
				int player[5], price[5];
//fprintf(stderr, "before read\n");
				read(fd_out[i][0], buffer, sizeof(buffer));
//fprintf(stderr, "after read\n");
				ready[i] = 0;
				sscanf(buffer, "%d %d\n%d %d\n%d %d\n%d %d\n", &player[0], &price[0], &player[1], &price[1], &player[2], &price[2], &player[3], &price[3]);
//fprintf(stderr, "buffer:%s\n", buffer);
				for (int j = 0; j < 4; j++)
				{
//fprintf(stderr, "score:%d\n", score[player[j] - 1][0] + 4 - price[j]);
					score[player[j] - 1][0] += 4 - price[j];
				}
			}
		}
		break;
	}
	for (int i = 0; i < host_num; i++)
	{
		if (ready[i] == 0)
		{
			char buffer[30];
			sprintf(buffer, "%d %d %d %d\n", a, b, c, d);
//fprintf(stderr, "im gonna write to the host %d", i);
			write(fd_in[i][1], buffer, strlen(buffer));
			fsync(fd_out[i][1]);
			ready[i] = 1;
			return;
		}
	}
}

void insertion_sort(int set[][2], int len)
{
	int i, j;
	for (i = 1; i < len; i++)
	{
		j = i - 1;
		while (j >= 0 && set[j][0] < set[i][0])
		{	//swap
			int tmp = set[j][0];
			set[j][0] = set[i][0];
			set[i][0] = tmp;
			tmp = set[j][1];
			set[j][1] = set[i][1];
			set[i][1] = tmp;
			i = j;
			j = i - 1;
		}
	}	
}

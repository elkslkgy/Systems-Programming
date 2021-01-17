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
#include <sys/wait.h>
void insertion_sort(int set[][2]);

int main(int argc, char *argv[])
{
	int host_id;
	host_id = atoi(argv[1]);
	char fifo_name[15], fifo_nameA[15], fifo_nameB[15], fifo_nameC[15], fifo_nameD[15];
	sprintf(fifo_name, "host%d.FIFO", host_id);
	sprintf(fifo_nameA, "host%d_A.FIFO", host_id);
	sprintf(fifo_nameB, "host%d_B.FIFO", host_id);
	sprintf(fifo_nameC, "host%d_C.FIFO", host_id);
	sprintf(fifo_nameD, "host%d_D.FIFO", host_id);
	mkfifo(fifo_name, 0777);
	mkfifo(fifo_nameA, 0777);
	mkfifo(fifo_nameB, 0777);
	mkfifo(fifo_nameC, 0777);
	mkfifo(fifo_nameD, 0777);
//fprintf(stderr, "mkfifo\n");
	
	while(1)
	{
		char buffer[100];
		int player[4], rank[4];
		read(0, buffer, sizeof(buffer));
		sscanf(buffer, "%d %d %d %d\n", &player[0], &player[1], &player[2], &player[3]);
//fprintf("%s", buffer);
		if (player[0] == -1 && player[1] == -1 && player[2] == -1 && player[3] == -1)
		{
			break;
		}

		srand(time(NULL));	//0 ~ 32767 32768 ~ 65535
		int random_key[4];
		random_key[0] = rand() % 65536;
		random_key[1] = rand() % 65536;
		random_key[2] = rand() % 65536;
		random_key[3] = rand() % 65536;
//fprintf("random_key:%d %d %d %d\n", random_key[0], random_key[1], random_key[2], random_key[3]);
		char rand_keyA[10] = {}, rand_keyB[10] = {}, rand_keyC[10] = {}, rand_keyD[10] = {};
		sprintf(rand_keyA, "%d", random_key[0]);
		sprintf(rand_keyB, "%d", random_key[1]);
		sprintf(rand_keyC, "%d", random_key[2]);
		sprintf(rand_keyD, "%d", random_key[3]);
//fprintf("rand_key:%s %s %s %s\n", rand_keyA, rand_keyB, rand_keyC, rand_keyD);

		pid_t pid_A, pid_B, pid_C, pid_D;
		if ((pid_A = fork()) == 0)
		{
			if (execlp("./player", "./player", argv[1], "A", rand_keyA, (char *)NULL) < 0)
				printf("execlp_A error\n");
		}
		else if ((pid_B = fork()) == 0)
		{
			if (execlp("./player", "./player", argv[1], "B", rand_keyB, (char *)NULL) < 0)
				printf("execlp_B error\n");
		}
		else if ((pid_C = fork()) == 0)
		{
			if (execlp("./player", "./player", argv[1], "C", rand_keyC, (char *)NULL))
				printf("execlp_C error\n");
		}
		else if ((pid_C = fork()) == 0)
		{
			if (execlp("./player","./player", argv[1], "D", rand_keyD, (char *)NULL))
				printf("execlp_D error\n");
		}
//fprintf(stderr, "execlp()\n");
		int fd, fd_A, fd_B, fd_C, fd_D;
		fd = open(fifo_name, O_RDWR);
		fd_A = open(fifo_nameA, O_RDWR);
		fd_B = open(fifo_nameB, O_RDWR);
		fd_C = open(fifo_nameC, O_RDWR);
		fd_D = open(fifo_nameD, O_RDWR);

		int win[4] = {};
		int money_next[4] = {};
		int money_prev[4] = {};
		int pay[4][2] = {}, sort[4][2] = {}, price[4][2] = {};
		for (int i = 0; i < 10; i++)
		{
//fprintf(stderr, "i is %d\n", i);
			for (int j = 0; j < 4; j++)
			{
				pay[j][1] = j + 1;
				price[j][1] = j + 1;
			}
			for (int j = 0; j < 4; j++)
			{
//fprintf(stderr,"%d = 1000 + %d - %d * %d\n", money_next[j], money_prev[j], win[j], pay[j][0]);
				money_next[j] = 1000 + money_prev[j] - win[j] * pay[j][0];
			}
	//		memset(pay, 0, sizeof(pay));
	//		memset(win, 0, sizeof(win));
			char all_money[25];
			memset(all_money, 0, sizeof(all_money));	
			sprintf(all_money, "%d %d %d %d\n", money_next[0], money_next[1], money_next[2], money_next[3]);
//fprintf(stderr,"all_money:%s", all_money);
			write(fd_A, all_money, strlen(all_money));
			write(fd_B, all_money, strlen(all_money));
			write(fd_C, all_money, strlen(all_money));
			write(fd_D, all_money, strlen(all_money));
			fsync(fd_A);
			fsync(fd_B);
			fsync(fd_C);
			fsync(fd_D);
			if (i == 9)
			{	waitpid(pid_A, NULL, 0);
				waitpid(pid_B, NULL, 0);
				waitpid(pid_C, NULL, 0);
				waitpid(pid_D, NULL, 0);
//fprintf(stderr,"hello wait\n");
			}
			char return_FIFO[100], random[10], player_index;
			int get[5] = {0}, Up_pay;
			while (1)
			{
				memset(random,0,sizeof(random));
				memset(return_FIFO, 0, sizeof(return_FIFO));
				read(fd, return_FIFO, sizeof(return_FIFO));
//fprintf(stderr,"return_FIFO:%s", return_FIFO);
				int len = strlen(return_FIFO);
				sscanf(return_FIFO,"%c %s %d\n", &player_index, &random, &Up_pay);
//fprintf(stderr,"player_input:%c %s %d\n", player_index, random, Up_pay);
				if (random_key[player_index - 'A'] == atoi(random))
				{
//fprintf(".\n");
					pay[player_index - 'A'][0] = Up_pay;
					get[player_index - 'A'] = 1;
				}
				for (int j = 0; j < len; j++)
				{
					int k = 0;
					if (return_FIFO[j] == '\n' && j != len - 1)
					{
						k = j + 1;
						sscanf(&(return_FIFO[k]), "%c %s %d\n", &player_index, &random, &Up_pay);
//fprintf(stderr,"player_input:%c %s %d\n", player_index, random, Up_pay);
						if (random_key[player_index - 'A'] == atoi(random))
						{
//fprintf(".\n");
							pay[player_index - 'A'][0] = Up_pay;
							get[player_index - 'A'] = 1;
						}
					}
				}
				if (get[0] && get[1] && get[2] && get[3])
				{
					memset(return_FIFO, 0, sizeof(return_FIFO));
//fprintf("fuck\n");
					break;
				}
			}

//fprintf("here\n");
			memcpy(sort, pay, sizeof(pay));
			insertion_sort(sort);
			for (int j = 0; j < 4; j++)
			{
				win[j] = 0;
			}
			for (int j = 0; j < 4;)
			{
				if (sort[j][0] == sort[j + 1][0])
					j++;
				else
				{
					if (j == 0)
					{
						win[sort[j][1] - 1] = 1;
						price[sort[j][1] - 1][0]++;
					}
					else
					{
						win[sort[j + 1][1] - 1] = 1;
						price[sort[j][1] - 1][0]++;
					}
					break;
				}
			}
			memcpy(money_prev, money_next, sizeof(money_next));
		}

		memcpy(sort, price, sizeof(price));
		insertion_sort(sort);
		int current_prize = 1;
		rank[sort[0][1] - 1] = current_prize;
		for (int i = 1; i < 4; i++)
		{
			current_prize++;
			if (sort[i][0] == sort[i - 1][0])
				rank[sort[i][1] - 1] = rank[sort[i - 1][1] - 1];
			else
				rank[sort[i][1] - 1] = current_prize;
		}
		sprintf(buffer, "%d %d\n%d %d\n%d %d\n%d %d\n", player[0], rank[0], player[1], rank[1], player[2], rank[2], player[3], rank[3]);
//fprintf(stderr, "host_return:\n%s", buffer);
		write(1, buffer, strlen(buffer));
		fsync(1);
//for (int i = 0; i < 4; i++)
//	fprintf(stderr, "price:%d %d\n", price[i][0], price[i][1]);
//for (int i = 0; i < 4; i++)
//	fprintf(stderr, "sort:%d %d\n", sort[i][0], sort[i][1]);

		close(fd);
		close(fd_A);
		close(fd_B);
		close(fd_C);
		close(fd_D);
	}
	unlink(fifo_name);
	unlink(fifo_nameA);
	unlink(fifo_nameB);
	unlink(fifo_nameC);
	unlink(fifo_nameD);
	return 0;
}

void insertion_sort(int set[][2])
{
	int i, j;
	for (i = 1; i < 4; i++)
	{
		j = i - 1;
		while (j >= 0 && set[j][0] < set[i][0])
		{
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

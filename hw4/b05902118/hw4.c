/*b05902118 陳盈如*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define TOTAL_DATA 25150
#define FEATURE 33
#define SIZE 500
#define THREAD 2

typedef struct tree{
	int judge, label;
	double num;
	struct tree *left, *right;
}TREE;

double training_dataset[TOTAL_DATA][FEATURE + 1] = {};	//index is ID	[0~32]:feature	[33]:0 for good, 1 for bad
double testing_dataset[FEATURE + 1] = {};
TREE *forest[THREAD];
int ans = 0;

void make_tree(void *forest);
void testing(TREE *forest);
void construct(int *tree, TREE *forest, int size);
void sort(int *tree, int feature, int size);
// void test(void);

int main(int argc, char *argv[])
{
	int id;
	char training_data[25];
	sprintf(training_data, "%s/training_data", argv[2]);
	FILE *fp = fopen(training_data, "r");
//	fprintf(stderr, "%s\n", training_data);
	if (fp == NULL) fprintf(stderr, "fuck you dam it\n");
	for (int i = 0; i < TOTAL_DATA; i++) {
		fscanf(fp, "%d", &id);
		for (int j = 0; j < FEATURE + 1; j++) {
			fscanf(fp, "%lf", &training_dataset[id][j]);
		}
	}
	srand(7122);
	pthread_t tid[THREAD];
	for (int i = 0; i < THREAD; i++) {
		if (forest[i] == NULL) {
			forest[i] = (TREE *)malloc(sizeof(TREE));
			forest[i]->left = forest[i]->right = NULL;
		}
		pthread_create(&tid[i], NULL, (void *)make_tree, (void *)forest[i]);
	}
	for (int i = 0; i < THREAD; i++) {
		pthread_join(tid[i], NULL);
	}
//fprintf(stderr, "after creating thread for constructing tree\n");
	char testing_data[25];
	sprintf(testing_data, "%s/testing_data", argv[2]);
//fprintf(stderr, "%s\n", testing_data);
	fp = fopen(testing_data, "r");
	if (fp == NULL) {
		fprintf(stderr, "testing_data open fail\n");
	}
	FILE *fp1 = fopen(argv[4], "w");
	if (fp1 == NULL) {
		fprintf(stderr, "write place open fail\n");
	}
	fprintf(fp1, "id,label\n");
	void *treeAns[THREAD];
	int test_id, guy[25100] = {}, test_amount = 0;
//	fprintf(stderr,"before testing\n");
	while ((fscanf(fp, "%d", &test_id)) != EOF) {
		test_amount++;
		ans = 0;
		for (int j = 0; j < FEATURE ; j++) {
			fscanf(fp, "%lf", &testing_dataset[j]);
		}
//		fprintf(stderr,"done reading test data\n");
//		fprintf(stderr, "input testing_data success\n");
		for (int i = 0; i < THREAD; i++) {
			pthread_create(&tid[i], NULL, (void *)testing, (void *)forest[i]);
		}
		for (int i = 0; i < THREAD; i++) {
			pthread_join(tid[i], NULL);
		}
	//	for (int i = 0; i < THREAD; i++) {
	//		testing(forest[i]);
	//	}
	//	fprintf(stderr, "test_id:%d\n", test_id);
		if (ans < 0) {
			guy[test_id] = 1;
		}
	}
//	fprintf(stderr, "after testing\n");
	for (int i = 0; i < test_amount; i++) {
		fprintf(fp1, "%d,%d\n", i, guy[i]);
	}
	// for (int i = 0; i < THREAD; i++) {
	// 	pthread_create(&tid[i], NULL, (void *)test, NULL);
	// }
	// for (int i = 0; i < THREAD; i++) {
	// 	pthread_join(tid[i], NULL);
	// }
	for (int i = 0; i < THREAD; i++) {
		free(forest[i]);
	}
	return 0;
}
// void test(void)
// {
// 	printf("fuck\n");
// }
void make_tree(void *forest)
{
	int input_tree[SIZE];
	for (int i = 0; i < SIZE; i++) {
		int id = rand() % TOTAL_DATA;
		input_tree[i] = id;
	}
	construct(input_tree, forest, SIZE);
}

void construct(int *tree, TREE *forest, int size)
{
//fprintf(stderr, "size:%d\n", size);
	int gini_index1, gini_index2, mini_index[FEATURE];
	double mini_gini[FEATURE] = {}, final_gini = 2147483647;
	for (int i = 0; i < FEATURE; i++) {
		mini_gini[i] = 2147483647;
		sort(tree, i, size);
		double now_gini[size - 1];
		for (int j = 0; j < size - 1; j++) {
			double left_good = 0, left_bad = 0, right_good = 0, right_bad = 0;
			for (int left = 0; left < j + 1; left++) {
				if (training_dataset[*(tree + left)][33] == 0) left_good++;
				else if (training_dataset[*(tree + left)][33] == 1) left_bad++;
			}
			for (int right = j + 1; right < size; right++) {
				if (training_dataset[*(tree + right)][33] == 0) right_good++;
				else if (training_dataset[*(tree + right)][33] == 1) right_bad++;
			}
			now_gini[j] = 2 * left_good * left_bad / ((left_good + left_bad) * (left_good + left_bad)) + 2 * right_good * right_bad / ((right_good + right_bad) * (right_good + right_bad));
		}
		for (int j = 0; j < size - 1; j++) {
			if (now_gini[j] < mini_gini[i]) {
				mini_gini[i] = now_gini[j];
				mini_index[i] = j;
			}
		}
	}
	for (int i = 0; i < FEATURE; i++) {
		if (mini_gini[i] < final_gini) {
			final_gini = mini_gini[i];
			gini_index1 = i;	//第幾個維度
			gini_index2 = mini_index[i];	//第幾個和第幾個index之間
		}
	}
	forest->judge = gini_index1;
	sort(tree, forest->judge, size);
	forest->num = (training_dataset[*(tree + gini_index2)][gini_index1] + training_dataset[*(tree + gini_index2 + 1)][gini_index1]) / 2;
	int left = 0, right = 0;
	for (int i = 0; i < size; i++)	{
		if (training_dataset[*(tree + i)][gini_index1] < forest->num) left++;
		if (training_dataset[*(tree + i)][gini_index1] > forest->num) right++;
	}
	int lab = training_dataset[*tree][FEATURE], go_on = 0;
	for (int i = 0; i < left; i++) {
		if (training_dataset[*(tree + i)][FEATURE] != lab) {
			go_on = 1;
			break;
		}
	}
	forest->left = (TREE *)malloc(sizeof(TREE));
	forest->left->left = forest->left->right = NULL;
	if (go_on == 1) {
		construct(tree, forest->left, left);
	}
	else {
		forest->left->label = lab;
	}
	lab = training_dataset[*(tree + left)][FEATURE];
	go_on = 0;
	for (int i = left; i < size; i++) {
		if (training_dataset[*(tree + i)][FEATURE] != lab) {
			go_on = 1;
			break;
		}
	}
	forest->right = (TREE *)malloc(sizeof(TREE));
	forest->right->left = forest->right->right = NULL;
	if (go_on == 1) {
		construct(tree + size - right, forest->right, right);
	}
	else {
		forest->right->label = lab;
	}
}

void sort(int *tree, int feature, int size)
{
	for (int i = 1; i < size; i++) {
		double key = training_dataset[*(tree + i)][feature];
		int key_p = *(tree + i);
		int j = i - 1;
		while (j >= 0 && training_dataset[*(tree + j)][feature] > key) {
			*(tree + j + 1) = *(tree + j);
			j--;
		}
		*(tree + j + 1) = key_p;
	}
}

void testing(TREE *forest)
{
	TREE *tree = forest;
	if (tree->right == NULL && tree->left == NULL) {
		if (tree->label == 0) ans++;
		else ans--;
		return;
	}
	if (testing_dataset[tree->judge] < tree->num) testing(tree->left);
	else testing(tree->right);
}

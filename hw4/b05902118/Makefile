all: hw4_2.c
	gcc hw4_2.c -o hw4 -pthread

clean:
	rm -f hw4 submission.csv

run:
	./hw4 -data ../data -output submission.csv -tree 2 -thread 2

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
// gettimeofday()
#include <sys/time.h>

#define ROCK 0
#define PAPER 1
#define SCISSORS 2

pthread_mutex_t mutex;
pthread_cond_t empty, fill;

int RPS[2];
int count[2] = {0, 0};

struct player {
	int playerNum;
	int rounds;
};

void *play(void *player) {

	int rounds = ((struct player*)player)->rounds;
	int playerNum = ((struct player*)player)->playerNum;

	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_sec + tv.tv_usec + (int)pthread_self());

	for (int i = 0; i < rounds; i++) {
		int random = rand() % 3;

		pthread_mutex_lock(&mutex);

		// if the referee hasn't taken it out, wait
		while(count[playerNum] == 1)
			pthread_cond_wait(&empty, &mutex);

		RPS[playerNum] = random;
		count[playerNum] = 1;

		// signal to referee only when both players finished playing
		if (count[0] == 1 && count[1] == 1)
			pthread_cond_signal(&fill);

		pthread_mutex_unlock(&mutex);
	}

	// on x86_64, void* is 64 bit & int is 32 bits
	return (void*)(size_t)playerNum;
}

int main (int argc, char** argv) {

	int rounds = atoi(argv[1]);

	pthread_t p_thread[2];
	int thr_id;
	struct player player1, player2;
	player1.playerNum = 0;
	player2.playerNum = 1;
	player1.rounds = player2.rounds = rounds;
	int tie = 2;

	if (pthread_mutex_init(&mutex, NULL) != 0) {
		perror("mutex init");
		exit(1);
	}
	if (pthread_cond_init(&empty, NULL) != 0) {
		perror("cond init");
		exit(1);
	}
	if (pthread_cond_init(&fill, NULL) != 0) {
		perror("cond init");
		exit(1);
	}

	if((thr_id = pthread_create(&p_thread[0], NULL, play, (void *) &player1)) < 0) {
		perror("pthread_create");
		exit(1);
	}

	if((thr_id = pthread_create(&p_thread[1], NULL, play, (void *) &player2)) < 0) {
		perror("pthread_create");
		exit(1);
	}

	printf("Child 1 TID: %lu\n", p_thread[0]);
	printf("Child 2 TID: %lu\n", p_thread[1]);
	printf("Beginning %d Rounds...\n", rounds);
	printf("Fight!\n");
	printf("--------------------------\n");

	int currentR = 0;
	int rpsP1, rpsP2;
	int scores[3] = {0, 0, 0};
	for (int i = 0; i < rounds; i++) {
		currentR++;
		printf("Round %d:\n", currentR);

		pthread_mutex_lock(&mutex);

		while (count[player1.playerNum] == 0 || count[player2.playerNum] == 0)
			pthread_cond_wait(&fill, &mutex);

		rpsP1 = RPS[player1.playerNum];
		rpsP2 = RPS[player2.playerNum];
		count[player1.playerNum] = 0;
		count[player2.playerNum] = 0;

		// signal to both players
		pthread_cond_broadcast(&empty);
		pthread_mutex_unlock(&mutex);

		switch(rpsP1)
		{
			case ROCK:
				printf("Child 1 throws Rock!\n");
				break;
			case PAPER:
				printf("Child 1 throws Paper!\n");
				break;
			case SCISSORS:
				printf("Child 1 throws Scissors!\n");
				break;
		}

		switch(rpsP2)
		{
			case ROCK:
				printf("Child 2 throws Rock!\n");
				break;
			case PAPER:
				printf("Child 2 throws Paper!\n");
				break;
			case SCISSORS:
				printf("Child 2 throws Scissors!\n");
				break;
		}

		if (rpsP1 == ROCK) {
			if (rpsP2 == ROCK){
				scores[tie]++;
				printf("Game is a Tie!\n");
			}
			else if (rpsP2 == PAPER) {
				scores[player2.playerNum]++;
				printf("Child 2 Wins!\n");
			}
			else if (rpsP2 == SCISSORS) {
				scores[player1.playerNum]++;
				printf("Child 1 Wins!\n");
			}
		}
		else if (rpsP1 == PAPER) {
			if (rpsP2 == ROCK) {
				scores[player1.playerNum]++;
				printf("Child 1 Wins!\n");
			}
			else if (rpsP2 == PAPER) {
				scores[tie]++;
				printf("Game is a Tie!\n");
			}
			else if (rpsP2 == SCISSORS) {
				scores[player2.playerNum]++;
				printf("Child 2 Wins!\n");
			}
		}
		else if (rpsP1 == SCISSORS) {
			if (rpsP2 == ROCK) {
				scores[player2.playerNum]++;
				printf("Child 2 Wins!\n");
			}
			else if (rpsP2 == PAPER) {
				scores[player1.playerNum]++;
				printf("Child 1 Wins!\n");
			}
			else if (rpsP2 == SCISSORS) {
				scores[tie]++;
				printf("Game is a Tie!\n");
			}
		}


		printf("--------------------------\n");
	}

	int thread_r;
	// waits until the child thread returns / calls pthread_exit()
	// if the thread is detached (pthread_detach()) then it can't wait on join()
	// call join() on every joinable threads to free resources
	if (pthread_join(p_thread[player1.playerNum], (void**)(size_t)thread_r) != 0) {
		perror("thread join");
		exit(1);
	}
	if (pthread_join(p_thread[player2.playerNum], (void**)(size_t)thread_r) != 0) {
		perror("thread join");
		exit(1);
	}

	// if initialized by using PTHREAD_MUTEX_INITIALIZER, don't have to destroy
	if (pthread_mutex_destroy(&mutex) != 0) {
		perror("mutex destroy");
		exit(1);
	}
	if (pthread_cond_destroy(&fill) != 0) {
		perror("cvar destroy");
		exit(1);
	}
	if (pthread_cond_destroy(&empty) != 0) {
		perror("cvar destroy");
		exit(1);
	}

	printf("--------------------------\n");
	printf("Results:\n");
	printf("Child 1: %d\n", scores[player1.playerNum]);
	printf("Child 2: %d\n", scores[player2.playerNum]);
	printf("Ties   : %d\n", scores[tie]);

	if (scores[player1.playerNum] > scores[player2.playerNum])
		printf("Child 1 Wins!\n");
	else if (scores[player1.playerNum] == scores[player2.playerNum])
		printf("Game is a Tie!\n");
	else
		printf("Child 2 Wins!\n");

	return 0;
}

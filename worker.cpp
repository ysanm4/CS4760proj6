//Written by Yosef Alqufidi
//Date 5/15/25
//updated from project 5

#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <ctime>

using namespace std;

#define PAGES_PER_PROCESS 32
#define PAGE_SIZE 1024
#define BOUND_NS 1000000000

enum MsgType { 
	REQUEST_MEMORY = 1, 
	TERMINATE = 2, 

};

//Shared memory clock structure
struct ClockDigi{
	int sysClockS;
	int sysClockNano;
};

struct Message{
	long mtype;
	pid_t pid;
	int address;
	bool write;
};


//logic for shared memory
int main(){
//shared memory key
key_t key= 6321;

//access to shared memory
int shmid = shmget(key, sizeof(ClockDigi), 0666);
ClockDigi* clockVal = (ClockDigi*) shmat(shmid, NULL, 0);

int msgid = msgget(key, 0666);
pid_t pid = getpid();
srand(pid ^ time(NULL));

int refs = 0,
toTerminate = rand()% 201+900;

//generate reference
while(true){
	int page = rand()%PAGES_PER_PROCESS;
	int offset = rand()%PAGE_SIZE;
	bool write = (rand()%100)<20;
	int addr = page*PAGE_SIZE + offset;
	Message req = {
		REQUEST_MEMORY,
		pid,
		addr,
		write
	};

	msgsnd(msgid, &req, sizeof(req) - sizeof(long), 0);

//wait for reply
Message rep;
msgrcv(msgid, &rep, sizeof(rep) - sizeof(long), pid, 0);
	refs++;
	usleep(1000);

	if(refs >= toTerminate){

//termination
		Message term = {
			TERMINATE,
			pid,
			0,
			false
		};

		msgsnd(msgid, &term, sizeof(term) - sizeof(long), 0);
		break;
	}
}

	shmdt(clockVal);
	return 0;
	}


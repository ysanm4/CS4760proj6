//Written by Yosef Alqufidi
//Date 4/24/25
//updated from project 4

#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <cstring>

using namespace std;

#define MAX_RESOURCES 5
#define INSTANCES_PER_RESOURCE 10
#define BOUND_NS 1000000000

enum RequestType { 
	REQUEST = 0, 
	RELEASE = 1, 
	RELEASE_ALL = 2
};

//Shared memory clock structure
struct ClockDigi{
	int sysClockS;
	int sysClockNano;
};

struct Message{
	long mtype;
	pid_t pid;
	int type;
	int resID;
};


//logic for shared memory
int main(){
//shared memory key
key_t shmKey= 6321;

//access to shared memory
int shmid = shmget(shmKey, sizeof(ClockDigi), 0666);
if(shmid < 0){
	perror("shmget");
	return EXIT_FAILURE;
}
ClockDigi* clockVal = (ClockDigi*) shmat(shmid, nullptr, 0);

if(clockVal == (void*)-1){
	perror("shmat");
	return EXIT_FAILURE;
}
int msgid = msgget(shmKey, 0666);
if(msgid < 0){
	perror("msgget");
	return EXIT_FAILURE;
}
pid_t pid = getpid();
srand(pid);
int alloc[MAX_RESOURCES] = {0};

long startTime = (long)clockVal->sysClockS*1000000000LL + clockVal->sysClockNano;
long lastAction = startTime;

while(true){
	long now = (long)clockVal->sysClockS *1000000000LL + clockVal->sysClockNano;
	long delta = now - lastAction;
	long waitBound = rand() % BOUND_NS;
	if(delta >= waitBound){
		lastAction = now;
		if(now - startTime >= 1000000000LL && (rand()%100)<10){
			for(int r=0;r<MAX_RESOURCES;r++){
				if(alloc[r]>0){ 
					Message rel{1,pid,RELEASE, r}; 
					msgsnd(msgid,&rel,sizeof(rel) - sizeof(long),0); 
				}
                }
			Message relAll{1,pid,RELEASE_ALL,0}; 
			msgsnd(msgid,&relAll,sizeof(relAll)-sizeof(long),0);
			break;
		}

		if((rand()%100)<70){
			int r = rand()%MAX_RESOURCES;
			if(alloc[r] < INSTANCES_PER_RESOURCE){
				Message req{1,pid,REQUEST, r}; 
				msgsnd(msgid,&req,sizeof(req)-sizeof(long),0);
//grant request
				Message grant;
				msgrcv(msgid, &grant, sizeof(grant) - sizeof(long), pid, 0);
				alloc[r]++;
				}
			}else{
//release
				int nonzero = 0;
				for(int r = 0; r<MAX_RESOURCES;r++) if(alloc[r]>0) nonzero++;
				if(nonzero > 0){
					int r;
					do{ r = rand() %MAX_RESOURCES; }while(alloc[r]==0);
				Message rel{1,pid,RELEASE, r}; 
				msgsnd(msgid,&rel,sizeof(rel)-sizeof(long),0);
			alloc[r]--;
				}
			}
		}
		usleep(1000);
	}
	shmdt(clockVal);
	return 0;
	}


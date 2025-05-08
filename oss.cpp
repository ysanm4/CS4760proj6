//Written by Yosef Alqufidi
//Date 5/15/25
//updated from project 5


#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
#include <csignal>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctime>
#include <string>
#include <vector>
#include <sys/msg.h>
#include <algorithm>

using namespace std;

//PCB tracking children

#define PROCESS_TABLE 20
#define MAX_PROCESSES 18

//32k per process
#define PAGES_PER_PROCESS 32

//frame table
#define FRAME_COUNT 256
#define PAGE_SIZE 1024

//14ms
#define DISK_TIME_NS (14LL * 1000000LL)

enum MsgType { 
	REQUEST_MEMORY = 1, 
	TERMINATE = 2
};

//struct for PCB
struct PCB{
	BOOL occupied;
	pid_t pid;
	int startSeconds;
	int startNano;
	int accesses;
	int faults;
	int pageTable[PAGES_PER_PROCESS];
};

//struct for clock
struct ClockDigi{
	int sysClockS;
	int sysClockNano;
};

//struct for messages
struct Message{
	long mtype;
	pid_t pid;
	int address;
	bool write;
};

struct Frame{
	bool occupied;
	pid_t pid;
	int page;
	bool dirty;
	int lastRfSec;
	int lastRefNano;
};

struct Fault{
	pid_t pid;
	int page;
	bool write;
	int reqSec;
	int reqNano;
};

static PCB processTable[PROCESS_TABLE];
static Frame frameTable[FRAME_COUNT];
static queue<Fault> faultQueue;
//clock ID
static int shmid;
//shared memory ptr
static ClockDigi* clockVal;
//message ID
static int msgid;

static ofstream logFile;

void advanceClock(long long deltaNs){
	long long total = (long long) clockVal->sysClockS * 1000000000LL + clockVal->sysClockNano + deltaNs;
	clockVal->sysClockS = total / 1000000000LL;
	clockVal->sysClockNano = total % 1000000000LL;
}

int allocateFrame(pid_t pid, int page){
	for(int i=0; i<FRAME_COUNT; i++){
		if(!frameTabel[i].occupied){
			frameTable[i] ={
				true,
				pid,
				page,
				false,
				clockVal->sysClockS,
				clockVal->sysClockNano
			};
			returni;
		}
	}

//LRU	
int lruIdx = 0;
long long lruTime = (long long) frameTable[0].lastRefSec * 1000000000LL + frameTable[0].lastRefNano;
for(int i = 1; <FRAME_COUNT; i++){
       long long t = (long long) frameTable[i].lastRefSec * 10000000000LL + frameTable[i].lastRefNano;
	if(t < lruTime){
	 lruTime = t;
	 lruIdx = i;
	}
}



//print process table to screen and logfile
void printProcessTable(){
	cout<<"Process Table:-------------------------------------------------------------------------------\n";
	cout<<"Entry\tOccupied\tPID\tStartS\tStartN\tMessagesSent\n";
	logFile<<"Process Table::-------------------------------------------------------------------------------\n";
	logFile << "Entry\tOccupied\tPID\tStartS\tStartN\tMessagesSent\n";
    for (int i = 0; i < PROCESS_TABLE; i++) {
	    if(!processTable[i].occupied) continue;
        cout << i << "\t" << processTable[i].occupied << "\t\t"
             << processTable[i].pid << "\t"
             << processTable[i].startSeconds << "\t"
             << processTable[i].startNano << "\t"
             << processTable[i].messagesSent << "\n";
        logFile << i << "\t" << processTable[i].occupied << "\t\t"
                << processTable[i].pid << "\t"
                << processTable[i].startSeconds << "\t"
                << processTable[i].startNano << "\t"
                << processTable[i].messagesSent << "\n";
	for(int r = 0; r < MAX_RESOURCES; r++){
		cout << " " << processTable[i].alloc[r];
		logFile << " " << processTable[i].alloc[r];

    }
    cout << "\n";
    logFile << "\n";
}
}

void printResourceTable(){
	cout << "Resource Table:----------------------------------------------------------------------------------\n";
	cout << "ResID available WaitingCount\n";
    logFile << "Resource Table:------------------------------------------------------------------------------------\n";
    logFile << "ResID available WaitingCount\n";
    for(int r = 0; r < MAX_RESOURCES; r++) {
        cout << "R" << r << ": available=" << resources[r].available
             << " waiting=" << resources[r].waitCount << "\n";
        logFile << "R" << r << ": available=" << resources[r].available
                << " waiting=" << resources[r].waitCount << "\n";
    }
}

//cleanup
void cleanup(int) {
    //send kill signal to all children based on their PIDs in process table
for(int i = 0; i < PROCESS_TABLE; i++){
	if(processTable[i].occupied) 
		kill(processTable[i].pid, SIGKILL);
}

//free up shared memory
if(clockVal) shmdt(clockVal);
shmctl(shmid, IPC_RMID, NULL);
msgctl(msgid, IPC_RMID, NULL);
if(logFile.is_open()) logFile.close();
    exit(1);
}

// Deadlock detection
vector<int> detectDeadlock() {
    
    vector<vector<int>> graph(PROCESS_TABLE);
    for(int i = 0; i < PROCESS_TABLE; i++) {
        if(processTable[i].occupied && waitingFor[i] != -1) {
            int res = waitingFor[i];
            for(int j = 0; j < PROCESS_TABLE; j++) {
                if(processTable[j].occupied && processTable[j].alloc[res] > 0) {
                    graph[i].push_back(j);
                }
            }
        }
    }
    vector<bool> visited(PROCESS_TABLE), rec(PROCESS_TABLE);
    vector<int> cycle;
    function<bool(int)> dfs = [&](int u) {
        visited[u] = rec[u] = true;
        for(int v : graph[u]) {
            if(!visited[v]) {
                if(dfs(v)) { 
			if(cycle.empty()) cycle.push_back(u); 
			return true; 
		}
            } else if(rec[v]) {
                cycle.push_back(u);
                return true;
            
        
	}
    }
        rec[u] = false;
        return false;
    };
    for(int i = 0; i < PROCESS_TABLE; i++) {
        if(processTable[i].occupied && waitingFor[i] != -1) {
            fill(visited.begin(), visited.end(), false);
            fill(rec.begin(), rec.end(), false);
            cycle.clear();
            if(dfs(i)) return cycle;
        }
    }
    return {};
}


//adding command line args for parse
int main( int argc, char *argv[]){
	
	int n_case = 0;
	int s_case = 0;
//	int t_case = 0;
	int i_case = 0;
	string logFileName;
//	bool n_var = false, s_var = false, i_var = false, f_var = false;
	int opt;

//setting up the parameters for h,n,s,t,i, and f
	while ((opt = getopt(argc, argv, "hn:s:i:f:")) != -1) {
		switch (opt){
			case 'h':
			cout<< "This is the help menu\n";
			cout<< "-h: shows help\n";
			cout<< "-n: processes\n";
			cout<< "-s: simultaneous\n";
//			cout<< "-t: iterations\n";
			cout<< "-f: logfile\n";
			cout<< "To run try ./oss -n 1 -s 1 -i 100 -f logfile.txt\n";

	return EXIT_SUCCESS;

			case 'n':
				n_case = atoi(optarg);
//				n_var = true;
				break;

			case 's':
				s_case = atoi(optarg);
//				s_var = true;
				break;

//			case 't':
//				t_case = atoi(optarg);
//				t_var = true;
//				break;

			case 'i':
				i_case = atoi(optarg);
//				i_var = true;
				break;
			case 'f':
                		logFileName = optarg;
//             			f_var = true;
                		break;

			default:
				cout<< "invalid";

			return EXIT_FAILURE;
		}
	}

//only allow all three to be used together and not by itself 	
//	if(!n_var || !s_var || !i_var || !f_var){
//		cout<<"ERROR: You cannot do one alone please do -n, -s,-i and -f together.\n";
//			return EXIT_FAILURE;
//	}
//error checking for logfile
	logFile.open(logFileName);
	if(!logFile){
		cout<<"ERROR: could not open" << logFileName << "\n";
		return EXIT_FAILURE;
	}

//shared memory for clock using key to verify

key_t key = 6321;

shmid = shmget(key, sizeof(ClockDigi), IPC_CREAT | 0666);
	clockVal = (ClockDigi*)shmat(shmid, NULL, 0);
	clockVal->sysClockS = 0;
	clockVal->sysClockNano = 0;

for(int i = 0; i < PROCESS_TABLE; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = 0;
        processTable[i].startSeconds = 0;
        processTable[i].startNano = 0;
        processTable[i].messagesSent = 0;
	waitingFor[i] = -1;
        fill(begin(processTable[i].alloc),
		       	end(processTable[i].alloc),
		       	0);
    }
//init resources
    for(int r = 0; r < MAX_RESOURCES; r++) {
        resources[r].available = INSTANCES_PER_RESOURCE;
        resources[r].waitCount = 0;
    }
//message queue
    msgid = msgget(key, IPC_CREAT | 0666);

    signal(SIGINT, cleanup);
    signal(SIGALRM, cleanup);
    alarm(5);

    int launched = 0;
    int running = 0;
    long long lastLaunch = 0;
    long long lastPrint = 0;
    long long lastDetect = 0;
    
    const long long incNs = 10LL * 1000000LL;     
    const long long halfSec = 500LL * 1000000LL;  
    const long long oneSec = 1000LL * 1000000LL;  

//main loop
while(launched < n_case || running > 0){
    long long totalNs = (long long)clockVal->sysClockS * 1000000000LL + clockVal->sysClockNano + incNs;
    clockVal->sysClockS = totalNs / 1000000000LL;
    clockVal->sysClockNano = totalNs % 1000000000LL;
    long long now = (long long)clockVal->sysClockS * 1000000000LL + clockVal->sysClockNano;
	
	pid_t pid;
        while((pid = waitpid(-1, nullptr, WNOHANG)) > 0) {
            int idx = findIndex(pid);
            if(idx >= 0) {
//free
                for(int r = 0; r < MAX_RESOURCES; r++) {
                    resources[r].available += processTable[idx].alloc[r];
                    processTable[idx].alloc[r] = 0;
                }
                processTable[idx].occupied = 0;
                waitingFor[idx] = -1;
                running--;
            }
        }

//fork and exec	
	if(launched < n_case && running < s_case && now - lastLaunch >= (long long)i_case * 1000000LL) {
            pid = fork();
            if(pid < 0) {
                perror("fork"); cleanup(0);
            } else if(pid == 0) {
                execlp("./worker", "worker", nullptr);
                perror("execlp"); exit(EXIT_FAILURE);
            } else {
//register pcb
                for(int i = 0; i < PROCESS_TABLE; i++) {
                    if(!processTable[i].occupied) {
                        processTable[i].occupied = 1;
                        processTable[i].pid = pid;
                        processTable[i].startSeconds = clockVal->sysClockS;
                        processTable[i].startNano = clockVal->sysClockNano;
                        break;
                    }
                }
                launched++;
                running++;
                lastLaunch = now;
            }
        }

//message handleing
	Message msg;
        while(true){
		ssize_t sz = msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT);

		if(sz < 0){
			if (errno == ENOMSG) break;
			perror("msgrcv");
			break;
		}
		int idx = findIndex(msg.pid);
		if(idx < 0) continue;

            if(msg.type == REQUEST) {
                if(resources[msg.resID].available > 0) {
                    resources[msg.resID].available--;
                    processTable[idx].alloc[msg.resID]++;
                    waitingFor[idx] = -1;
//grant
			Message rep{
	    msg.pid, msg.pid, REQUEST, msg.resID};
	    
	   		msgsnd(msgid, &rep, sizeof(rep) - sizeof(long), 0);
			processTable[idx].messagesSent++;
		}else{
			int q = resources[msg.resID].waitCount++;
			resources[msg.resID].waitQueue[q] = msg.pid;
			waitingFor[idx] = msg.resID;
		}
	    }else if(msg.type == RELEASE){
    resources[msg.resID].available++;
    processTable[idx].alloc[msg.resID]--;
	    
	   }else if(msg.type == RELEASE_ALL){
	       for(int r = 0; r < MAX_RESOURCES; ++r){
	    resources[r].available += processTable[idx].alloc[r];
	    processTable[idx].alloc[r] = 0;
	       }
   waitingFor[idx] = -1;
	   }
	}   

	for(int r = 0; r < MAX_RESOURCES; r++){
		Resource &res = resources[r];
		while(res.waitCount > 0 && res.available > 0){
			pid_t wpid = res.waitQueue[0];

			for(int k = 1; k < res.waitCount; k++)
				res.waitQueue[k-1] = res.waitQueue[k];
			res.waitCount--;
			res.available--;
			int widx = findIndex(wpid);
			if(widx >= 0){
			processTable[widx].alloc[r]++;
			Message rep{wpid, wpid, REQUEST, r};
			msgsnd(msgid, &rep, sizeof(rep) - sizeof(long), 0);
			processTable[widx].messagesSent++;
			}
		}
	}

	if(now - lastPrint >= halfSec){
		cout << "OSS PID " << getpid() << " SysClock "
                 << clockVal->sysClockS << ":" << clockVal->sysClockNano << "\n";
            logFile << "OSS PID " << getpid() << " SysClock "
                    << clockVal->sysClockS << ":" << clockVal->sysClockNano << "\n";
            printProcessTable();
            printResourceTable();
            lastPrint = now;
        }

	if(now - lastDetect >= oneSec) {
            vector<int> cycle = detectDeadlock();
            if(!cycle.empty()) {
//choose pid 
                int victim = cycle.front();
                pid_t vpid = processTable[victim].pid;
//log and kill
                cout << "Deadlock detected, killing process " << vpid << "\n";
                logFile << "Deadlock detected, killing process " << vpid << "\n";
                kill(vpid, SIGKILL);
//cleanup
                for(int r = 0; r < MAX_RESOURCES; r++) {
                    resources[r].available += processTable[victim].alloc[r];
                    processTable[victim].alloc[r] = 0;
                }
                processTable[victim].occupied = 0;
                waitingFor[victim] = -1;
                running--;
            }
            lastDetect = now;
        }
    }

  
    cout << "Simulation complete.\n";
    logFile << "Simulation complete.\n";
    cleanup(0);
    return 0;
}

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
#include <queue>
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
	bool occupied;
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

//first fit
int allocateFrame(pid_t pid, int page){
	for(int i=0; i < FRAME_COUNT; i++){
		if(!frameTable[i].occupied){
			frameTable[i] ={
				true,
				pid,
				page,
				false,
				clockVal->sysClockS,
				clockVal->sysClockNano
			};
			return i;
		}
	}

//LRU	
int lruIdx = 0;
long long lruTime = (long long) frameTable[0].lastRfSec * 1000000000LL + frameTable[0].lastRefNano;
for(int i = 1; i < FRAME_COUNT; i++){
       long long t = (long long) frameTable[i].lastRfSec * 1000000000LL + frameTable[i].lastRefNano;
	if(t < lruTime){
	 lruTime = t;
	 lruIdx = i;
	}
}

Frame victim = frameTable[lruIdx];
//dirty bit
long long extra = victim.dirty ? DISK_TIME_NS : 0;

cout<< "oss: claring fram" << lruIdx 
<< "of Pid" << victim.pid
<< "page" << victim.page <<"\n";
logFile<< "oss: claring fram" << lruIdx 
<< "of Pid" << victim.pid
<< "page" << victim.page <<"\n";
advanceClock(DISK_TIME_NS + extra);

for( auto &p: processTable){
	if(p.occupied && p.pid == victim.pid){
		p.pageTable[victim.page] = -1;
		break;
	}
}

frameTable[lruIdx] = {
	true,
	pid,
	page,
	false,
	clockVal->sysClockS,
	clockVal->sysClockNano
};
return lruIdx;
}

//frame/process table
void printMemoryMap(){
	cout<< "Memory map" << clockVal->sysClockS << ":" 
		<< clockVal->sysClockNano <<"\n";
       	logFile<< "Memory map" << clockVal->sysClockS << ":" 
		<< clockVal->sysClockNano <<"\n";
	cout<< "Frame num of dirty pid\n";
	logFile<< "Frame num of dirty pid\n";

	for(int i = 0; i < FRAME_COUNT; i++){
		cout<< i <<" "
			<<(frameTable[i].occupied?"y":"n")<<" "
			<<(frameTable[i].dirty?"1":"0")<<" "
			<<frameTable[i].lastRfSec<<" "
			<<frameTable[i].lastRefNano<<" "
			<<frameTable[i].pid<<" "
			<<frameTable[i].page<<"\n";
		logFile<< i <<" "
                        <<(frameTable[i].occupied?"y":"n")<<" "
                        <<(frameTable[i].dirty?"1":"0")<<" "
                        <<frameTable[i].lastRfSec<<" "
                        <<frameTable[i].lastRefNano<<" "
                        <<frameTable[i].pid<<" "
                        <<frameTable[i].page<<"\n";
	}

	for(int i = 0; i<PROCESS_TABLE; i++){
		if(processTable[i].occupied){
			cout<<"pid"<<processTable[i].pid<<" process table:";
			logFile<<"pid"<<processTable[i].pid<<" process table:";
			for(int pg = 0; pg<PAGES_PER_PROCESS; pg++){
				cout<<processTable[i].pageTable[pg] <<(pg + 1<PAGES_PER_PROCESS?" ":"\n");
				logFile<<processTable[i].pageTable[pg] <<(pg + 1<PAGES_PER_PROCESS?" ":"\n");
			}
		}
	}
}


//print process table to screen and logfile
void printProcessTable(){
	cout<<"Process Table:-------------------------------------------------------------------------------\n";
	cout<<"Entry\tOccupied\tPID\tStartS\tStartN\tAccesses\tFaults\n";
	logFile<<"Process Table::-------------------------------------------------------------------------------\n";
	logFile << "Entry\tOccupied\tPID\tStartS\tStartN\tAccesses\tFaults\n";
    for (int i = 0; i < PROCESS_TABLE; i++) {
	    if(processTable[i].occupied){
//		    auto &p = processTable[i];
        cout << i << "\t" 
	     << processTable[i].occupied << "\t\t"
             << processTable[i].pid << "\t"
             << processTable[i].startSeconds << "\t"
             << processTable[i].startNano << "\t"
             << processTable[i].accesses << "\t"
	     << processTable[i].faults << "\n";
        logFile << i << "\t" 
		<< processTable[i].occupied << "\t\t"
                << processTable[i].pid << "\t"
                << processTable[i].startSeconds << "\t"
                << processTable[i].startNano << "\t"
                << processTable[i].accesses << "\t"
		<< processTable[i].faults << "\n";
    cout << "\n";
    logFile << "\n";

	    }
    }
}


//cleanup
void cleanup(int) {
    //send kill signal to all children based on their PIDs in process table
for(auto &p:processTable)
	if(p.occupied) kill(p.pid,SIGKILL);
//free up shared memory
if(clockVal) shmdt(clockVal);
shmctl(shmid, IPC_RMID, NULL);
msgctl(msgid, IPC_RMID, NULL);
if(logFile.is_open()) logFile.close();
    exit(0);
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

//message queue
    msgid = msgget(key, IPC_CREAT | 0666);

    signal(SIGINT, cleanup);
    signal(SIGALRM, cleanup);
    alarm(5);

    for (int j = 0; j < PROCESS_TABLE; j++) {
        processTable[j].occupied     = false;
        processTable[j].pid          = 0;
        processTable[j].startSeconds = 0;
        processTable[j].startNano    = 0;
        processTable[j].accesses     = 0;
        processTable[j].faults       = 0;
        for (int pg = 0; pg < PAGES_PER_PROCESS; pg++) {
            processTable[j].pageTable[pg] = -1; 
        }
    }
    

    // initialize frame table
    for (int j = 0; j < FRAME_COUNT; j++) {
        frameTable[j] = { false, 0, 0, false, 0, 0 };
    }

    int launched = 0;
    int running = 0;
    long long lastLaunch = 0;
    long long lastPrint = 0; 

const long long incNs = 10LL * 1000000LL;

//main loop
while(launched < n_case || running > 0){
	advanceClock(incNs);
    long long now = (long long)clockVal->sysClockS * 1000000000LL + clockVal->sysClockNano;
	pid_t pid;
        while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
//free
                for(int k = 0; k < PROCESS_TABLE; k++) {
			if(processTable[k].occupied && processTable[k].pid == pid){
				long long totalTime = now - ((long long)processTable[k].startSeconds * 1000000000LL + processTable[k].startNano);
				cout<< "pid" << pid << "done runnitime = " << totalTime << " ns accesses = " << processTable[k].accesses << " faults = " << processTable[k].faults << "\n";
				logFile<< "pid" << pid << "done runnitime = " << totalTime << " ns accesses = " << processTable[k].accesses << " faults = " << processTable[k].faults << "\n";
				
                processTable[k].occupied = false;
                running--;
		break;
            }
        }
	}

//fork and exec	
	if(launched < n_case && running < s_case && now - lastLaunch >= (long long)i_case * 1000000LL) {
            pid = fork();
            if(pid == 0) {
                execlp("./worker", "worker", NULL);
                perror("exec"); exit(1);
            } else {
//register pcb
                for (int k = 0; k < PROCESS_TABLE; k++) {
                    if (!processTable[k].occupied) {
                        processTable[k].occupied     = true;
                        processTable[k].pid          = pid;
                        processTable[k].startSeconds = clockVal->sysClockS;
                        processTable[k].startNano    = clockVal->sysClockNano;
                        processTable[k].accesses     = 0;
                        processTable[k].faults       = 0;
                        for (int pg = 0; pg < PAGES_PER_PROCESS; pg++) {
                            processTable[k].pageTable[pg] = -1; 
                        }
			launched++;
			running++;
			lastLaunch = now;
                        break;
                    }
                }
            }
        }

	if(!faultQueue.empty()){
		Fault f = faultQueue.front();
		long long elapsed = now - ((long long) f.reqSec * 1000000000LL + f.reqNano);
		if(elapsed >= DISK_TIME_NS + (frameTable[f.pid % FRAME_COUNT].dirty? DISK_TIME_NS:0)){
			faultQueue.pop();
			int frameIdx = allocateFrame(f.pid, f.page);
			for(auto &p : processTable){
				if(p.occupied && p.pid == f.pid){
					p.pageTable[f.page] = frameIdx;
					break;
				}
			}
			Message wake = {
				f.pid,
				f.pid, 
				0,
				false
			};
			msgsnd(msgid,&wake, sizeof(wake) - sizeof(long), 0);
		}
	}	

			
//message handleing
Message msg;
while(msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), REQUEST_MEMORY, IPC_NOWAIT) > 0){


	cout << "oss: P" << msg.pid
             << (msg.write ? " requesting write of address " : " requesting read of address ")
             << msg.address
             << " at time " << clockVal->sysClockS << ":" << clockVal->sysClockNano << "\n";
        logFile << "oss: P" << msg.pid
                << (msg.write ? " requesting write of address " : " requesting read of address ")
                << msg.address
                << " at time " << clockVal->sysClockS << ":" << clockVal->sysClockNano << "\n";

        for (auto &p : processTable) {
            if (p.occupied && p.pid == msg.pid) {
                p.accesses++;
                int page = msg.address / PAGE_SIZE;
                int frm  = p.pageTable[page];
			if(frm >= 0){
	cout << "oss: Address " << msg.address
                         << " in frame " << frm
                         << (msg.write ? ", writing" : ", giving data")
                         << " to P" << msg.pid
                         << " at time " << clockVal->sysClockS << ":" << clockVal->sysClockNano << "\n";
                    logFile << "oss: Address " << msg.address
                            << " in frame " << frm
                            << (msg.write ? ", writing" : ", giving data")
                            << " to P" << msg.pid
                            << " at time " << clockVal->sysClockS << ":" << clockVal->sysClockNano << "\n";


				frameTable[frm].lastRfSec = clockVal->sysClockS;
				frameTable[frm].lastRefNano = clockVal->sysClockNano;
				if(msg.write) frameTable[frm].dirty = true;
				advanceClock(100);
				Message ack = {
					msg.pid,
					msg.pid,
					0,
					false
				};
				msgsnd(msgid, &ack, sizeof(ack) - sizeof(long),0);
			}else{


			cout << "oss: Address " << msg.address
                         << " not in a frame, pagefault at time "
                         << clockVal->sysClockS << ":" << clockVal->sysClockNano << "\n";
                    logFile << "oss: Address " << msg.address
                            << " not in a frame, pagefault at time "
                            << clockVal->sysClockS << ":" << clockVal->sysClockNano << "\n";

				p.faults++;
				faultQueue.push(Fault{
						msg.pid,
						page,
						msg.write,
						clockVal->sysClockS,
						clockVal->sysClockNano
						});
			}
			break;
		}
	}
}
//cleanup
if(now - lastPrint >= 1000000000LL){
	printMemoryMap();
	printProcessTable();
	lastPrint = now;
}
}

cleanup(0);
return 0;

}


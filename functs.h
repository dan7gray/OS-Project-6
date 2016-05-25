#ifndef FUNCTS_H
#define FUNCTS_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>//Shared Mem
#include <sys/ipc.h>
#include <sys/wait.h>//wait()
#include <signal.h>
#include <unistd.h> //fork()
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <sys/sem.h>
#include <stdbool.h>

#define HARD_LIMIT 18
#define SOFT_LIMIT 12

struct PCB{//In shared mem
	int pid[18];
	int running[18];
	double cpuTimeUsed[18];//total CPU time used
	double totalSystemTime[18];//total time in the system
	double idle[18];//total time in the system
	double idleTotal[18];//total time in the system
	double turnAround[18];
	int processDone;
	int processSignaling;
	int processSignal[18];
	int action[18];//Determine if process will read or write. 0 = read, 1 = write
	int pageWant[18];
	int readWriteDone;
	int memoryAccessCount[18];
	int pageFaults[18];
};

struct timing{//In shared mem
	//1 sec = 1000 milli = 1000000 micro = 1000000000 nano
	unsigned int clockSecs; 
	unsigned int clockNanos;
};

struct pageTable{//1 page table per process //Page table holds pg number and frame number pointing to
	int PTBR[18];//Page Table Base Register - Hold 1st frame id
	int PTLR[18];//Page Table Length Register - Size of page table, so don't access memory outside page table limit
	int frameID[18][32];//32KB/1KB = 32 pages. Process #, page #, holds frame #
	double referenced[18][32];//32KB/1KB = 32 pages. Process #, page #, holds frame #
	char dirty[256];//If page modified or not
	char valid[256];//If page in memory or not
	// int permissions[18][32];//32KB/1KB = 32 pages.
	int suspended[18];//Queue of processes suspended for requesting memory.
	int suspTail;
};

//Frame # gets a timestamp when brought in, to use for LRU alg too
int get_page(int processId, struct pageTable* PT, struct PCB* processBlock, int RAM[], struct timing* timer);
void runDaemon(int freeFrames, struct pageTable* PT, int RAM[]);
void printPageTable(struct pageTable* PT, int processId);
 
//suspended Queues' manipulation/query functs
int addQueue(int processId, int suspended[], int *tail);
void printQueue(int suspended[]);
int inQueue(int queue[], int processId);
int queueEmpty(int suspended[]);
int selectFromQueue(int suspended[], int *tail, int processId);
int popQueue(int theQueue[], int *tail);

//Timing
void advanceClock(int *clockSecs, int *clockNanos, int amount);
double getTotalTime(int *clockSecs, int *clockNanos, double *time);
double genRandomDouble();//For comparing to time when creating processes

//Bit Vector
void printBit(int bv[], int size, char valid[]);
void set(int bv[], int i);//Set value in bitVector
int member(int bv[], int i);//Check if value in bitVector
void clearBit(int bv[], int i);//Clear the process from the process table
int bvFull(int bv[], int numProcesses);//Check if process table full
int bvEmpty(int bv[], int numProcesses);//Check if process table empty
int countEmptyFrames(int bv2[], int size);//Count total empty frames, for daemon
int getIndex(int framesNeed, int taken[]);//Get next available empty frame address

//Shared Memory:
extern int shmidVal;
extern int shmidVal2;
extern int shmidVal3;
extern key_t key; //For shared memory
extern key_t key2; //For shared memory
extern key_t key3; //For shared memory

//Signaling:
void sigHandler(int mysignal);
void initSigHandler();

//Semaphores:
extern int semKey, semId;//for waitClock(), signalClock()
extern int semKeyR, semIdR;//For processes requesting resources
extern int semKeyCond, semIdCond;//For processes that are ending
extern int semKeyReq, semIdReq;//For requesting memory
struct sembuf operation;
struct sembuf removeP;
struct sembuf condition;
struct sembuf request;
void waitClock();
void signalClock();
void waitRequest(int processRequesting, int running);//For when processes requesting resources
void signalRequest();
void waitSem();
void signalSem();
void waitSem2();
void signalSem2();
void waitRemove();
void signalRemove();

#endif
/*$Author: o2-gray $
*$Date: 2016/04/20 03:20:51 $
*$Log: functs.h,v $
*Revision 1.1  2016/04/20 03:20:51  o2-gray
*Initial revision
*
*/

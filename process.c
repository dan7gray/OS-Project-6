#include "functs.h"

int semKey = 8675309, semId;
int semKeyR = 3333333, semIdR;//For Removing processes
int semKeyReq = 7777777, semIdReq;//For Requesting memory in process.c
int semKeyCond = 90210, semIdCond;//For removing process
int key = 4444444;
int key2 = 5555555;
int key3 = 6666666;
int shmidVal;
int shmidVal2;
int shmidVal3;

int main(int argc, char * argv[]){

char *p;
int processId = strtol(argv[0],&p,10);
shmidVal = strtol(argv[1],&p,10);
shmidVal2 = strtol(argv[2],&p,10);
shmidVal3 = strtol(argv[3],&p,10);

double currTime, arrivalTime, startTime, endTime, currRunTime;
srand(time(NULL));

int terminate = 0, accessCount = 0, readWrite = 0, checkTerminate = rand() % 200 + 900;// [900-1100]
int r;//For for loops
double genRandomDouble(int processId);//No funct overloading in C, this oss & process funct different

struct PCB *processBlock;
struct timing *timer;
struct pageTable *PT;

//Attach to shared mem
if((processBlock = (struct PCB *)shmat(shmidVal,(void *)0, 0)) == (void *)-1){ //Returns void*
        fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
        perror("shmat");
        exit(EXIT_FAILURE);
}
if((timer = (struct timing *)shmat(shmidVal2, 0, 0)) == (void *)-1){ //Returns void*
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("shmat");
	exit(EXIT_FAILURE);
}
if((PT = (struct pageTable*)shmat(shmidVal3, 0, 0)) == (void *)-1){ //Returns void*
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("shmat");
	exit(EXIT_FAILURE);
}

//Get/Create semaphores
if((semId = semget(semKey,1,0)) == -1){//For clock
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror(" semget");
	exit(EXIT_FAILURE);
}
if((semIdCond = semget(semKeyCond,1,0)) == -1){//For memory requests
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror(" semget");
	exit(EXIT_FAILURE);
}
if((semIdReq = semget(semKeyReq,1,0)) == -1){//For resource requests
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror(" semget");
	exit(EXIT_FAILURE);
}
if((semIdR = semget(semKeyR,1,0)) == -1){//For when no longer suspended
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror(" semget");
	exit(EXIT_FAILURE);
}

arrivalTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &arrivalTime);//Get arrival time
processBlock->turnAround[processId] = arrivalTime;
startTime = arrivalTime;//Get start time
checkTerminate = rand() % 200 + 900;//New terminate time 900-1100

while(true){
	advanceClock(&timer->clockSecs, &timer->clockNanos, 999);//advance clock to simulate overhead
	currRunTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &currRunTime);//Get current time

if(processBlock->memoryAccessCount[processId] >= checkTerminate){//Check if process chould terminate at random times between 0 and 250ms
	accessCount =  accessCount + processBlock->memoryAccessCount[processId];
	terminate = rand() % (processId + 10);// + processId so concurrent processes don't get same num since seeded with time, +10 so large enough to % again
	terminate = terminate % 2;//Check if process will terminate
	if(terminate == 1){//Deallocate all resources and end
		waitRemove();
		/*****************************************************************
		Signal OSS to Deallocate/Release Memory & Remove for Process Table
		*****************************************************************/
		processBlock->processDone = processId;//Signal OSS process done
		
		processBlock->memoryAccessCount[processId] = accessCount;
		endTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &endTime);//Get time ended at
		processBlock->turnAround[processId] = endTime - processBlock->turnAround[processId];// turnAround = endTime - arrivalTime;
		processBlock->cpuTimeUsed[processId] = processBlock->cpuTimeUsed[processId] + processBlock->turnAround[processId] - processBlock->idleTotal[processId];
		processBlock->totalSystemTime[processId] = endTime - processBlock->totalSystemTime[processId];//endTime - Creation time
		// fprintf(stdout,"FINAL %d: arrivalTime %.9f, endTime %.9f, turnAround: %.9f, cpuTimeUsed: %.9f, memoryAccessCount %d\n", processId, arrivalTime, endTime, processBlock->turnAround[processId], processBlock->cpuTimeUsed[processId], processBlock->memoryAccessCount[processId]);
		return 0;
	}
	// fprintf(stdout,"Process %d NOT TERMINATING: memoryAccessCount = %d\n", processId, processBlock->memoryAccessCount[processId]);
	fprintf(stdout,"Process %d Not Terminating...\n", processId);
	processBlock->memoryAccessCount[processId] = 0;
	checkTerminate = rand() % 200 + 900;//New terminate time 900-1100
}
 
//Add processId below to to make more random between processes
processBlock->action[processId] = (rand() + processId) % 2;//Determine if process will read or write. 0 = read, 1 = write.
if(PT->PTLR[processId] != 1){//If have more than one page
	processBlock->pageWant[processId] = rand() % PT->PTLR[processId];//Pick a random page from PT to use
}else{//rand() % 1 = rand().  If only have 1 page in table
	processBlock->pageWant[processId] = 0;
}

currTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &currTime);//Get current time
processBlock->processSignal[processId] = processId;
while(processBlock->processSignal[processId] == processId || processBlock->running[processId] == 0){
	waitRequest(processBlock->processSignal[processId], processBlock->running[processId]);//Only one can deallocate at a time
	if(processBlock->processSignal[processId] == processId){//Signal if OSS hasn't got to this request yet
		processBlock->processSignaling = processId;//Signal variable
	}
	if(processBlock->running[processId] == 0){
		currRunTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &currRunTime);//Get current time
		if(currRunTime - currTime > 0.015 && processBlock->readWriteDone == -1 && processBlock->running[processId] == 0){
			processBlock->readWriteDone = processId;
		}
	}
}
// fprintf(stdout,"Process %d DONE with request\n", processId);

}//End While(true)
return 0;
}

void sigHandler(int mysignal){
	pid_t childId;
	struct PCB *processBlock;
	struct timing *timer;
	struct pageTable *PT;

    //Attach to shared memory
	if((processBlock = (struct PCB *)shmat(shmidVal, (void *)0, 0)) == (void *)-1){ //Returns void*
		fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
		perror("shmat");
		exit(EXIT_FAILURE);
	}
	if((timer = (struct timing *)shmat(shmidVal2, 0, 0)) == (void *)-1){ //Returns void*
		fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
		perror("shmat");
		exit(EXIT_FAILURE);
	}
	if((PT = (struct pageTable *)shmat(shmidVal3, 0, 0)) == (void *)-1){ //Returns void*
		fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
		perror("shmat");
		exit(EXIT_FAILURE);
	}
        printf("SLAVE process id %d  signal %d\n", getpid(),mysignal);
	switch(mysignal){
			case SIGINT:
				fprintf(stderr,"Child %d dying because of an interrupt!\n",childId);
				//Detach and Remove shared mem
				if(shmdt(processBlock) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
					perror("shmdt");
				   exit(EXIT_FAILURE);
				}
				if (shmctl(shmidVal,IPC_RMID,NULL) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
					perror("shmctl");
					exit(EXIT_FAILURE);
				}
				if(shmdt(timer) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
						perror("shmdt");
					   exit(EXIT_FAILURE);
				}
				if (shmctl(shmidVal2,IPC_RMID,NULL) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
					perror("shmctl");
					exit(EXIT_FAILURE);
				}
				if(shmdt(PT) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
						perror("shmdt");
					   exit(EXIT_FAILURE);
				}
				if (shmctl(shmidVal3,IPC_RMID,NULL) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
					perror("shmctl");
					exit(EXIT_FAILURE);
				}
				
				//Delete Semaphore.  IPC_RMID Remove the specified semaphore set
				if(semctl(semId, 0, IPC_RMID) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
					perror("semctl");
					exit(EXIT_FAILURE);
				}
				if(semctl(semIdCond, 0, IPC_RMID) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
					perror("semctl");
					exit(EXIT_FAILURE);
				}
				if(semctl(semIdReq, 0, IPC_RMID) == -1){
					fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
					perror("semctl");
					exit(EXIT_FAILURE);
				}
				//raise(SIGTERM);
				exit(1);
				break;
		   default:
				printf("Default signal in slave %d\n", mysignal);
				break;
	}
	return;
}

void initSigHandler(){
	struct sigaction signalAct;
	signalAct.sa_handler = &sigHandler;//Setup sigHandler
	signalAct.sa_flags = SA_RESTART|SA_SIGINFO;
	sigfillset(&signalAct.sa_mask);//Block all other signals

	if(sigaction(SIGINT, &signalAct, NULL) == -1){
			perror("SIGINT receipt failed");
	}
}

int inQueue(int suspended[], int processId){
	int i;
	for(i = 0; suspended[i] != -1; i++){//Move elements forward in the queue
		if(suspended[i] == processId){
			return 1;//Process already in queue
		}
	}
	return 0;//Process not in queue
}

int addQueue(int processId, int suspended[18], int *tail){
	// fprintf(stdout,"Added Process %d to suspended Queue\n", processId);
	if(*tail == HARD_LIMIT){ // Check to see if the Queue is full
		fprintf(stdout,"%d QUEUE IS FULL\n",processId);
		return 0;
	}
	*tail = *tail + 1;
	suspended[*tail % HARD_LIMIT] = processId;// Add the item to the Queue
	// fprintf(stdout,"suspended[%d] = %d = %d, tail = %d \n", *tail % HARD_LIMIT, processId,suspended[*tail % HARD_LIMIT], *tail);
	return 1;
}

int selectFromQueue(int suspended[], int *tail, int processId){
// int selectFromQueue(int r, int p, int suspended[r][p], int resType, int *tail, int processId){
	if(*tail == -1){// Check for empty Queue
		fprintf(stderr,"SUSPENDED QUEUE IS EMPTY \n");
		return -1;  // Return 0 if queue is empty
	}else{
		*tail = *tail - 1;
		int i = 0;
		while(suspended[i] != processId){//Find element in queue
			i++;
		}
		// fprintf(stdout,"Id Found suspended[%d] = %d\n",i, suspended[i]);
		processId = suspended[i];// Get Id to return
		suspended[i] = -1;
		
		while(suspended[i + 1] != -1){//Move elements forward in the queue
			suspended[i] = suspended[i + 1];
			i++; // fprintf(stdout,"Shift suspended[%d] = %d\n",i , suspended[i + 1]);
		}
		// fprintf(stdout,"suspended[%d] = 0\n",i);
		suspended[i] = -1;//Set last spot that was filled to -1 (nothing) since process moved up in queue
		// fprintf(stdout,"Process: Select queue return: %d\n",processId);
		return processId;// Return popped Id
	}
}

void advanceClock(int *clockSecs, int *clockNanos, int amount){//1 sec = 1,000 milli = 1,000,000 micro = 1,000,000,000 nano
	waitClock();
	int randTime;
	randTime = rand() % amount + 1;//simulate overhead activity for each iteration by 0.0001s
	// fprintf(stdout,"ENTER clockSecs %d, clockNanos %d, randTime %d, %d += %d\n",*clockSecs,*clockNanos,randTime,*clockNanos,randTime);
	*clockNanos += randTime;
	if(*clockNanos > 999999999){
		*clockSecs = *clockSecs + 1;
		*clockNanos -= 1000000000;
	}
	// fprintf(stdout,"LEAVE clockSecs %d, clockNanos %d\n",*clockSecs,*clockNanos);
	signalClock();
	return;
}

double getTotalTime(int *clockSecs, int *clockNanos, double *time){
	*time = (double)*clockSecs + ((double)*clockNanos/1000000000);
	return *time;
}

double genRandomDouble(int processId){//72500000ns == 72.5 millisecs == .725s
	// fprintf(stdout,"returning %.9f\n", ((double)rand() * ( 0.5 - 0 ) ) / (double)RAND_MAX + 0);
	double randNum = (double)rand() + (double)processId;// + processId so concurrent processes don't get same num since seeded with time
	return (randNum * ( 0.25 - 0 ) ) / (double)RAND_MAX + 0;
	// return ((double)rand() * ( 0.25 - 0 ) ) / (double)RAND_MAX + 0;
}

void waitClock(){
	operation.sem_num = 0;/* Which semaphore in the semaphore array*/
    operation.sem_op = -1;/* Subtract 1 from semaphore value*/
    operation.sem_flg = 0;/* Set the flag so we will wait*/
    if(semop(semId, &operation, 1) == -1){
        exit(EXIT_FAILURE);
    }
}

void signalClock(){
	operation.sem_num = 0;/* Which semaphore in the semaphore array*/
    operation.sem_op = 1;/* Add 1 to semaphore value*/
    operation.sem_flg = 0;/* Set the flag so we will wait*/
	if(semop(semId, &operation, 1) == -1){
        exit(EXIT_FAILURE);
    }
}

void waitRequest(int processRequesting, int running){
	if(processRequesting == -1 && running == 1){
		request.sem_num = 0;/* Which semaphore in the semaphore array*/
		request.sem_op = -1;/* Subtract 1 from semaphore value*/
		request.sem_flg = 0;/* Set the flag so we will wait*/
		if(semop(semIdReq, &request, 1) == -1){
			exit(EXIT_FAILURE);
		}
	}
}

void waitSem(){
	condition.sem_num = 0;/* Which semaphore in the semaphore array*/
    condition.sem_op = -1;/* Subtract 1 from semaphore value*/
    condition.sem_flg = 0;/* Set the flag so we will wait*/
    if(semop(semIdCond, &condition, 1) == -1){
        exit(EXIT_FAILURE);
    }
}

void signalSem(){
	condition.sem_num = 0;/* Which semaphore in the semaphore array*/
    condition.sem_op = 1;/* Add 1 to semaphore value*/
    condition.sem_flg = 0;/* Set the flag so we will wait*/
	if(semop(semIdCond, &condition, 1) == -1){
        exit(EXIT_FAILURE);
    }
}

void waitRemove(){
	removeP.sem_num = 0;/* Which semaphore in the semaphore array*/
    removeP.sem_op = -1;/* Subtract 1 from semaphore value*/
    removeP.sem_flg = 0;/* Set the flag so we will wait*/
    if(semop(semIdR, &removeP, 1) == -1){
        exit(EXIT_FAILURE);
    }
}

/*$Author: o2-gray $
*$Date: 2016/04/26 15:04:25 $
*$Log: process.c,v $
*Revision 1.5  2016/04/26 15:04:25  o2-gray
*Processes communicating well with OSS now
*
*Revision 1.4  2016/04/24 03:16:34  o2-gray
*Just need a little more testing with multiple processes then need to do statistics.
*Already writing to log.txt when process ends.
*
*Revision 1.3  2016/04/23 21:16:51  o2-gray
*Nothing new really, changed a semaphore around with process.c and oss.c
*to make it work properly.  Clearly wasn't thinking straight before bed.
*
*Revision 1.2  2016/04/20 03:20:08  o2-gray
*Set up and putting in Page Requests to oss.
*
*Revision 1.1  2016/04/17 02:43:50  o2-gray
*Initial revision
*
*/

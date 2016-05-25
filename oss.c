#include "functs.h"

int semKey = 8675309, semId;//For clock
int semKeyR = 3333333, semIdR;//For Removing processes
int semKeyReq = 7777777, semIdReq;//For Requesting memory in process.c
int semKeyCond = 90210, semIdCond;//For processes requesting resources
int key = 4444444;
int key2 = 5555555;
int key3 = 6666666;
int shmidVal;
int shmidVal2;
int shmidVal3;

int main(int argc, char * argv[]){
/*******************************************************************************************
Initialize vars.  totalNumAllowed, numAllowedConcurrently, & runTime can be modified safely
*******************************************************************************************/
int p, r;//for loop vars
double runTime = 6;//Time the program can run until in stops (seconds)
double total = 0, hitRatio = 0, EAT = 0;//For statistics at end
int numProc = 0;//For statistics at end
bool resources = true;
int totalSuspended = 0;//Keep track of number of page faults
int pageFaults = 0, memoryAccesses = 0;//Keep track of number of page faults
int numberCompleted = 0;//Keep track of number of processes complete for calculations at the end
int processRemoved = -1, id, signaling, pageReturned, endIndex;
int processId = 0, processCounter = 0, processToRunId = -1, processRequestId = -1, nextIndex = 0, freeFrames = 0;
pid_t procPid, waitingPid;
int status;
double timePassed = 0, totalTime = 0, prevTotalTime = 0, daemonTime = 0, printTime = 0;
double randTime;//Timing for when to generate next process
srand(time(NULL));
char writeString[100];//For writing to log file
char tempProcessId[sizeof(int)];//For sending processId through execl 
char tempShmidVal[sizeof(int)];//For sending shmidVal through execl 
char tempShmidVal2[sizeof(int)];//For sending shmidVal2 through execl tempPriority
char tempShmidVal3[sizeof(int)];//For sending shmidVal2 through execl tempPriority

initSigHandler();//Initialize signal handler

struct PCB *processBlock;
struct timing *timer;
struct pageTable *PT;

unsigned int bv[1] = {0};//bv = bitVector. keep track of PCBs taken. Process table
unsigned int RAM[8] = {0};//256KB available in system memory. Bit vector
//Bit vector below to keep track of free frames.  Could have used the other
//processes PTBR's and PTLR's but was taking too long and this is last day to work on this
unsigned int framesTaken[8] = {0};

//Vars for getopts
int totalNumAllowed;//Number of processes allowing to be created
long count2;
char *count, *printTimeEntered, *q;
int hflag = 0, sflag = 0, tflag = 0, vflag = 0, pflag = 0, c = 0, err = 0;
extern char *optarg; //used when parsing options that take a name, stores parameters
extern int optind; //current index into main functions argument list
//Used to find arguments after all the option processing is done
//command line options get suffixed by a : if they require a parameter

while((c = getopt(argc, argv, "hs:p:tv")) != -1){//-1 when no more options found
	switch(c){
	case 'h':
		hflag = 1;
		break;
	case 's':
		sflag = 1;
		count = optarg;
		break;
	case 'p':
		pflag = 1;
		printTimeEntered = optarg;
		break;
	case 't':
		tflag = 1;
		break;
	case 'v':
		vflag = 1;
		break;
	case '?'://Option not in the list
		err = 1;
		break;
	}
}

if(hflag == 1){//Display info
	fprintf(stdout,"The program will spawn a number of children and let them take turns requesting memory.\n");
	fprintf(stdout,"If the time runs out, they will all DIE!\n");
	fprintf(stdout,"Optional arguments:\n");
	fprintf(stdout,"	-s (num)     Sets the total number of processes to run concurrently\n");
	fprintf(stdout,"	-p (num)     How often the Memory Map will print in seconds.  Recommend 0.5 or below for more feedback.\n");
	fprintf(stdout,"	-t           Daemon mode, to guarantee seeing Daemon in action. fork() is not timed, all Process PTLR's = 32 to fill Memory fast.\n");
	fprintf(stdout,"	             Memory Map will print a lot, when you see different colored 1's you know their valid bit is off.\n");
	fprintf(stdout,"	             CTRL-C to stop the program to look at the bits or run til' the end and scroll back to see.\n");
	fprintf(stdout,"	             Do Not use in conjunction with -s argument.\n");
	fprintf(stdout,"	-v           Sets the program to Verbose mode.  This will print the Memory Map on every memory access\n");
	fprintf(stdout,"	             and print each Processes Page Table (In a different color) on every Page Fault.\n");
	fprintf(stdout,"	             **WARNING** This prints A LOT.  Please make sure your computer can handle it before taking the plunge!\n");
	fprintf(stdout,"This programs handles signals from CTRL-C\n");
	fprintf(stdout,"Memory Map shows current memory:\n");
	fprintf(stdout,"	\033[22;31m0\033[0m = Frame Not in Memory\n");
	fprintf(stdout,"	1 = Frame In Memory\n");
	fprintf(stdout,"	\033[22;34m1\033[0m = Frame Marked for Replacement\n");
	fprintf(stdout,"	\033[22;31mDid\033[0m this \"Cuz \033[22;34mMurica\033[0m\033[22;31m!\033[0m\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"******** It may \"stall\", just give it a second.  It does run! *******\n");
	fprintf(stdout,"PRESS ANY KEY TO CONTINUE\n");
	getchar();
}

if(sflag == 1){//Set the number of Processes
	count2 = strtol(count,&q,10);
	if(count2 > 18){
		fprintf(stdout,"Let's not go too crazy here.  How about 18 children instead :D\n");
		totalNumAllowed = HARD_LIMIT;
	}else{
		totalNumAllowed = count2;
	}
}else{
	totalNumAllowed = SOFT_LIMIT;//+1 to take account of master process
}

if(pflag == 1){//Set the number of Processes
	if(printTimeEntered){
		printTime = atof(printTimeEntered);
	}else{
		fprintf(stderr,"Invalid Input for argument -p, Memory Map will print every 1s.\n");
		printTime = 1;//1 second
		fprintf(stdout,"PRESS ANY KEY TO CONTINUE\n");
		getchar();
	}
}else{
	printTime = 1;//1 second
}

if(vflag == 1 || tflag){//Verbose or Daemon mode
	fprintf(stdout,"***WARNING*** This prints A LOT.  Ready?\n");
	fprintf(stdout,"PRESS ANY KEY TO CONTINUE\n");
	getchar();
}

//Create & Attach to Shared Memory: shared memory segment specified by shmidVal to the address space
if((shmidVal = shmget(key, sizeof(struct PCB), IPC_CREAT | 0666)) < 0){
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("shmget");
	exit(EXIT_FAILURE);
}
if((processBlock = (struct PCB*)shmat(shmidVal, 0, 0)) == (void *)-1){ //Returns void*
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("shmat");
	exit(EXIT_FAILURE);
}
if((shmidVal2 = shmget(key2, sizeof(struct timing), IPC_CREAT | 0666)) < 0){
	fprintf(stderr,"%s %d ",__FILE__, __LINE__);
	perror("shmget");
	exit(EXIT_FAILURE);
}
if((timer = (struct timing*)shmat(shmidVal2, 0, 0)) == (void *)-1){ //Returns void*
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("shmat");
	exit(EXIT_FAILURE);
}
if((shmidVal3 = shmget(key3, sizeof(struct pageTable), IPC_CREAT | 0666)) < 0){
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("shmget");
	exit(EXIT_FAILURE);
}
if((PT = (struct pageTable*)shmat(shmidVal3, 0, 0)) == (void *)-1){ //Returns void*
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("shmat");
	exit(EXIT_FAILURE);
}

//Create Semaphores
if((semId = semget(semKey,1,IPC_CREAT | 0666)) == -1){//For advancing clock
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror(" semget");
	exit(EXIT_FAILURE);
}
semctl(semId, 0, SETVAL, 1);//Initialize binary semaphore for clock, unlocked from start
if((semIdCond = semget(semKeyCond,1,IPC_CREAT | 0666)) == -1){//For processes requesting resources
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror(" semget");
	exit(EXIT_FAILURE);
}
semctl(semIdCond, 0, SETVAL, 1);//Initialize binary semaphore for requests, unlocked from start
if((semIdR = semget(semKeyR,1,IPC_CREAT | 0666)) == -1){//For removing resources in process.c
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror(" semget");
	exit(EXIT_FAILURE);
}
semctl(semIdR, 0, SETVAL, 1);//Initialize binary semaphore
if((semIdReq = semget(semKeyReq,1,IPC_CREAT | 0666)) == -1){
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror(" semget");
	exit(EXIT_FAILURE);
}
semctl(semIdReq, 0, SETVAL, 1);//Initialize binary semaphore

for(p = 0; p < 18; p++){
	processBlock->pid[p] = -1;
	processBlock->memoryAccessCount[p] = 0;
	processBlock->processSignal[p] = -1;
	PT->PTLR[p] = -1;//Page Table Length Register
	PT->suspended[p] = -1;
	PT->PTBR[p] = -1;
	for(r = 0; r < 32; r++){
		PT->frameID[p][r] = -1;
		PT->referenced[p][r] = 0;
	}
}
PT->suspTail = -1;
for(r = 0; r < 256; r++){
	PT->dirty[r] = 0;//If page modified or not
	PT->valid[r] = 0;//If page in memory or not
}

//Initialize clock items
timer->clockSecs = 0;  timer->clockNanos = 0;

processBlock->processDone = -1;//Hold Id of process ending
processBlock->processSignaling = -1;//Hold Id of process requesting
processBlock->readWriteDone = -1;//Hold Id of process done with a read/write

randTime = genRandomDouble();//Timing for when to generate next process
timePassed = getTotalTime(&timer->clockSecs, &timer->clockNanos, &timePassed);//For timing printing memory map
daemonTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &daemonTime);//For timing printing memory map

int t = 0;

while(true){
	//If Time passed && Total processes < Amount chosen && Frames available && not too many processes OR Daemon mode	
	if(((totalTime - prevTotalTime) > randTime) && processCounter < totalNumAllowed && resources && processId < HARD_LIMIT || tflag && processCounter < totalNumAllowed && resources && processId < HARD_LIMIT){
	// if(processCounter < totalNumAllowed && resources && processId < HARD_LIMIT){
		prevTotalTime = totalTime;//Set so we can use to determine when next process can be created
		totalTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &totalTime);
		
		if(!tflag){
			PT->PTLR[processId] = rand() % 32 + 1;
		}else{//If Daemon Mode enabled
			PT->PTLR[processId] = 32;
		}
		
		nextIndex = getIndex(PT->PTLR[processId], framesTaken);//int getIndex(int bv2[], int framesNeed)
		if(nextIndex == -1){//If not enough Frames available
			fprintf(stderr,"*** Tried to fork a new process: %d, Out of RAM ***\n", processId);
			resources = false;
			continue;
		}
		
		if(bvFull(bv, totalNumAllowed) == 1){//Make sure bitVector/Process table not full. If is, don't generate new process
			fprintf(stdout,"Process Table full.\n", processId);
			resources = false;
			continue;//If full, don't create newe process
		}
		if(member(bv, processId) == 0){//Insert process into Process Table
			set(bv, processId);//Put in bitvector
		}else{
			fprintf(stdout,"Couldn't insert %d in Process Table.\n", processId);
		}
		
		//Put in Page Table. Start out with at least 1 page so can start running
		PT->frameID[processId][r] = nextIndex;//Process #, page #, holds frame #
		PT->PTBR[processId] = nextIndex;
		endIndex = PT->PTBR[processId] + PT->PTLR[processId] - 1;
		for(r = PT->PTBR[processId]; r <= endIndex; r++){
			if(member(framesTaken, r) == 0){//Insert process into bitVector.  Stop creating processes once bitVector full
				set(framesTaken, r);//Put in bitvector
			}else{
				fprintf(stdout,"Couldn't insert %d in Taken Table.\n", r);
			}
		}
		
		if((procPid = fork()) == 0){//is child, new process receives a copy of the address space of the parent
			fprintf(stdout,"New child %d has pid %d\n",processId, getpid());
			// fprintf(stdout,"New child %d is %d, time: %.9f, PTBR = %d, PTLR = %d, Ends at %d\n",processId, getpid(),prevTotalTime, PT->PTBR[processId], PT->PTLR[processId], PT->PTBR[processId] + PT->PTLR[processId]);
			processBlock->running[processId] = 1;
			processBlock->pid[processId] = getpid();
			processBlock->cpuTimeUsed[processId] = 0;//total CPU time used
			processBlock->totalSystemTime[processId] = totalTime;//total CPU time used
			processBlock->turnAround[processId] = 0;
			processBlock->idle[processId] = 0;
			processBlock->idleTotal[processId] = 0;
			
			sprintf(tempProcessId,"%d", processId);//For passing the ID through exec 
			sprintf(tempShmidVal,"%d", shmidVal);//For passing the shared mem ID through exec 
			sprintf(tempShmidVal2,"%d", shmidVal2);//For passing the shared mem ID through exec 
			sprintf(tempShmidVal3,"%d", shmidVal3);//For passing the shared mem ID through exec 
 
			execl("./process",tempProcessId,tempShmidVal,tempShmidVal2,tempShmidVal3,NULL);//Execute executible
			perror("Child failed to execl ");
			exit(EXIT_FAILURE);
		}else if(procPid < 0){//fork() fails with -1
			fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
			perror("fork() error");
			exit(EXIT_FAILURE);
		}
		randTime = genRandomDouble();//Create new random time to wait before making new process
		processId++;
		processCounter++;
	}
	if(totalTime - timePassed > printTime){
		fprintf(stdout,"Printing Memory Map: Time left: %.9f\n", (double)runTime - totalTime);
		printBit(RAM, 32, PT->valid);
		timePassed = getTotalTime(&timer->clockSecs, &timer->clockNanos, &timePassed);
	}

	if(processBlock->processDone != -1){//A process is done, remove from bitVector
		// fprintf(stdout,"Process %d Starting to be removed by OSS\n", processBlock->processDone);
		numberCompleted++;
		processBlock->running[processBlock->processDone] = 0;
		waitingPid = waitpid(processBlock->pid[processBlock->processDone], &status, WUNTRACED);
		if(waitingPid == -1){//Wait for this process to end before decr processCounter and creating new process
			fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
			perror("waitpid");
			exit(EXIT_FAILURE);
		}
		for(r = 0; r < PT->PTLR[processBlock->processDone]; r++){//Remove from memory
			if(PT->frameID[processBlock->processDone][r] != -1){
				clearBit(RAM, PT->frameID[processBlock->processDone][r]);
				clearBit(framesTaken, PT->frameID[processBlock->processDone][r]);
			}
		}
		/*************************************************
			Write to log file now that process gone
		*************************************************/
		//Do calculations before opening file.  Figure keep file open for least amount of time as possible
		totalTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &totalTime);
		hitRatio = (double)processBlock->pageFaults[processBlock->processDone]/(double)processBlock->memoryAccessCount[processBlock->processDone];
		EAT = ((1 - hitRatio) * 10) + (hitRatio * 15000000);//Effective Access Time
		FILE *fp;
		if((fp = fopen("log.txt","a")) == NULL){
			fprintf(stderr,"%s",__FILE__);
			perror("Failed to open file ");
			return -1;//Failed
		}

		writeString[100];
		// fprintf(stdout,"((1 - %.9f) * 10) = %.9f, %.9f * 15000000 = %.9f\n", hitRatio, (1 - hitRatio) * 10, hitRatio, hitRatio * 15000000);
		// fprintf(stdout,"%.9f + %.9f = %.9f\n", (1 - hitRatio) * 10, hitRatio * 15000000, EAT);
		snprintf(writeString, sizeof(writeString),"%d ended at time %.6f, Effective Access Time = %.8fs = %.7fms = %.4fns",processBlock->processDone, totalTime, EAT/1000000000, EAT/1000000, EAT);
		writeString[strlen(writeString)-1]= '\0';//Remove \n from asctime function
		fprintf(fp,"%s\n",writeString);
		fclose(fp);
		processBlock->pid[processBlock->processDone] = -1;
		fprintf(stdout,"Process %d terminating...  Clear %d from Proccess table: \n", processBlock->processDone, processBlock->processDone);
		clearBit(bv, processBlock->processDone);//Remove from Process Table
		printBit(bv, 4, PT->valid);//Show Process Table
		processCounter--;
		resources = true;
		processBlock->processDone = -1;//Wait til next process done
		signalRemove();
	}
	
	if(totalTime - timePassed > printTime){
		fprintf(stdout,"Printing Memory Map: Time left: %.9f\n", (double)runTime - totalTime);
		printBit(RAM, 32, PT->valid);
		timePassed = getTotalTime(&timer->clockSecs, &timer->clockNanos, &timePassed);
	}
	
	if(processBlock->processSignaling != -1){//If processes in blocked queue waiting for resources
		signaling = processBlock->processSignaling;
		// fprintf(stdout,"OSS: signaling %d = %d\n", signaling, processBlock->processSignal[signaling]);
		freeFrames = countEmptyFrames(RAM, 32);
		if(freeFrames < 25 && totalTime - daemonTime > 0.001){//Free frames < 10% total frames. 256/10 = 26
			runDaemon(freeFrames, PT, RAM);
			printBit(RAM, 32, PT->valid);//Show Memory Map after Daemon, to display off valid bits
			daemonTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &daemonTime);//For timing printing memory map
		}
		pageReturned = get_page(signaling, PT, processBlock, RAM, timer);
		if(pageReturned != -1){//Not a Page Fault
			processBlock->processSignal[signaling] = -1;
			processBlock->pageWant[signaling] = -1;
			totalTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &totalTime);
			PT->referenced[signaling][pageReturned] = totalTime;
		}else{//Page Fault
			processBlock->processSignal[signaling] = -1;
			processBlock->idle[signaling] = getTotalTime(&timer->clockSecs, &timer->clockNanos, &totalTime);
		}
		/********************************************************************************************************************
		OPTIONAL: Print Memory Map on every memory reference.  Uncomment function below. Try it at least once, great feedback
		********************************************************************************************************************/
		if(tflag || vflag){//Verbose or Daemon mode
			printBit(RAM, 32, PT->valid);
		}
		// fprintf(stdout,"OSS DONE signaling: %d, memoryAccesses %d\n", signaling, processBlock->memoryAccessCount[signaling]);
		processBlock->processSignaling = -1;
		signaling = -1;
	}
	
	if(totalTime - timePassed > printTime){
		fprintf(stdout,"Printing Memory Map: Time left: %.9f\n", (double)runTime - totalTime);
		printBit(RAM, 32, PT->valid);
		timePassed = getTotalTime(&timer->clockSecs, &timer->clockNanos, &timePassed);
	}
	
	//Check if ALL suspended.  ProcessId holds current total num processes in system & starts @ 0
	totalSuspended = countArray(PT->suspended);
	if(totalSuspended == processId + 1 - numberCompleted && totalSuspended != 0){
		advanceClock(&timer->clockSecs, &timer->clockNanos, 15000000);//advance clock 15ms
		processRemoved = popQueue(PT->suspended, &PT->suspTail);
		PT->valid[PT->frameID[processRemoved][processBlock->pageWant[processRemoved]]] = 1;
		
		if(processBlock->action[processRemoved] == 1){//0 = read, 1 = write
			PT->dirty[PT->frameID[processRemoved][processBlock->pageWant[processRemoved]]] = 1;//If page modified or not
		}
		processBlock->pageWant[processRemoved] = -1;
		processBlock->running[processRemoved] = 1;
	}
	
	if(totalTime - timePassed > printTime){
		fprintf(stdout,"Printing Memory Map: Time left: %.9f\n", (double)runTime - totalTime);
		printBit(RAM, 32, PT->valid);
		timePassed = getTotalTime(&timer->clockSecs, &timer->clockNanos, &timePassed);
	}
	
	if(processBlock->readWriteDone != -1){
		// fprintf(stdout,"OSS %d readWriteDone = %d\n", processBlock->readWriteDone, processBlock->action[processBlock->readWriteDone]);
		processRemoved = selectFromQueue(PT->suspended, &PT->suspTail, processBlock->readWriteDone);
		if(processRemoved != -1){//-1 if was removed previously, because all processes were suspended
			if(processBlock->action[processRemoved] == 1){//0 = read, 1 = write
				PT->dirty[PT->frameID[processRemoved][processBlock->pageWant[processRemoved]]] = 1;//If page modified or not
			}
			totalTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &totalTime);
			processBlock->idleTotal[processRemoved] = processBlock->idleTotal[processRemoved] + totalTime - processBlock->idle[processRemoved];
			PT->valid[PT->frameID[processRemoved][processBlock->pageWant[processRemoved]]] = 1;
			processBlock->pageWant[processRemoved] = -1;
			processBlock->running[processRemoved] = 1;
			processBlock->readWriteDone = -1;
			/************************************************************************************************************
			 The function directly below prints the page table of a process once it's page fault is serviced
			*************************************************************************************************************/
			if(vflag){//Enabled in Verbose mode
				printPageTable(PT, processRemoved);//Print out this Process' Page Table since request Fulfilled
			}
			signalRequest();
		}
	}
	
	advanceClock(&timer->clockSecs, &timer->clockNanos, 999);//advance clock to simulate overhead
	totalTime = getTotalTime(&timer->clockSecs, &timer->clockNanos, &totalTime);
	
	if(totalTime - timePassed > printTime){//Print Memory Map every second
		fprintf(stdout,"Printing Memory Map: Time left: %.9f\n", (double)runTime - totalTime);
		printBit(RAM, 32, PT->valid);
		timePassed = getTotalTime(&timer->clockSecs, &timer->clockNanos, &timePassed);
	}
	t++;
	if(t > 500000){//So get some feedback if oss has free time
		fprintf(stdout,"OSS Running...\n");
		t = 0;
	}

	// if(totalTime > runTime && bvEmpty(bv, numberCompleted)){
	if(totalTime > runTime){
		fprintf(stdout,"Time is up!\n");
		break;
	}
}
sleep(1);
for(p = 0; p < 18; p++){//Total up memory accesses
	memoryAccesses = memoryAccesses + processBlock->memoryAccessCount[p];
}
for(p = 0; p < 18; p++){//Total up page faults
	pageFaults = pageFaults + processBlock->pageFaults[p];
}

/************************************************************************************************************
Statistics: Average CPU time, Average turnaround time, Average waiting time, Average Idle times for processes
*************************************************************************************************************/

numProc = processId;

fprintf(stdout,"\n***************************** CPU Utilization Times ********************************\n");
fprintf(stdout,"Total CPU Time	  Idle Time    CPU Time Used	 Cummulative CPU Time Used\n");
for(p = 0; p < 18; p++){
	if(processBlock->cpuTimeUsed[p] == 0){
		numProc = p;
		break;
	}
	if(processBlock->idleTotal[p] > 0){
		fprintf(stdout,"%d: %.9f - %.9f = %.9f	  ",p,processBlock->cpuTimeUsed[p] + processBlock->idleTotal[p], processBlock->idleTotal[p], processBlock->cpuTimeUsed[p]);
	}else{
		fprintf(stdout,"%d: %.9f			    		  ",p,processBlock->cpuTimeUsed[p]);
	}
	total += processBlock->cpuTimeUsed[p];//Idle time already subtracted for cpuTimeUsed
	fprintf(stdout,"total: %.9f\n",total);
}
fprintf(stdout,"\nCummulative CPU Time Used between All processes: %.9f\n",total);
fprintf(stdout,"Average CPU Time Used: %.9f/%d = %.9f\n", total, numProc, (total/(double)numProc));
fprintf(stdout,"Logical Clock ran for: %.9fs\n",totalTime);
/************************************************************************************************************
CPU idle time left out.  Unlike a previous assignment, a process or oss will always be running since all are 
running concurrently.  We aren't waiting for processes/oss to finish before oss/another process can run, so 
the CPU is always running.
*************************************************************************************************************/
fprintf(stdout,"\n***************************** Idle Times **********************************\n");
total = 0;
for(p = 0; p < numProc; p++){
	if(processBlock->idleTotal[p] > 0){
		fprintf(stdout,"%d: %.9f	  ", p, processBlock->idleTotal[p]);
		total = total + processBlock->idleTotal[p];
	}
}
fprintf(stdout,"\nAverage idle time: %.9f/%d = %.9f\n", total, numProc, total/(double)numProc);

fprintf(stdout,"\n***************************** Turnaround Times **********************************\n");
total = 0;
for(p = 0; p < numProc; p++){
	fprintf(stdout,"%d: %.9f	  ", p, processBlock->turnAround[p]);
	total = total + processBlock->turnAround[p];
	if(p % 3 == 0){
		fprintf(stdout,"\n");
	}
}
fprintf(stdout,"\nAverage turnaround time: %.9f/%d = %.9f\n", total, numProc, total/(double)numProc);

fprintf(stdout,"\n***************************** Waiting Times ********************************\n");
total = 0;
//Waiting time of a process = finish time of that process - execution time - arrival time = turnAround - CPU time used
for(p = 0; p < numProc; p++){//Waiting time = endTime - creation time - turnaround
	fprintf(stdout,"%d: %.9f	  ",p,processBlock->totalSystemTime[p] - processBlock->turnAround[p]);
	total = total + processBlock->totalSystemTime[p] - processBlock->turnAround[p];
	if(p % 3 == 0){
		fprintf(stdout,"\n");
	}
}
fprintf(stdout,"\nAverage Waiting time: %.9f/%d = %.9f\n",total, numProc, total/(double)numProc);
/************************************************************************************************************
No scheduling in this project, so waiting time is relatively short since upon creation, the process will
arrive in a short period of time and start executing immediately
*************************************************************************************************************/
fprintf(stdout,"\n***************************** Throughput ********************************\n");
fprintf(stdout,"Number of Processes Completed = %d, Total Time %.9f\n", numProc, totalTime);
fprintf(stdout,"Throughput: %d/%.9f = %.9f\n",numProc, totalTime, totalTime/(double)numProc);
fprintf(stdout,"A process is created at a rate approximately every %.9fs\n", totalTime/(double)numProc);

fprintf(stdout,"\n***************************** Memory Access ********************************\n");
fprintf(stdout,"Page Faults: %d\n",pageFaults);
fprintf(stdout,"Memory Accesses: %d\n",memoryAccesses);
hitRatio = (double)pageFaults/(double)memoryAccesses;
fprintf(stdout,"Page Fault Rate: %d/%d = %.9f\n", pageFaults, memoryAccesses, hitRatio);
EAT = ((1 - hitRatio) * 10) + (hitRatio * 15000000);//Effective Access Time
fprintf(stdout,"Overall EAT: %.9fns = %.9fms = %.9fs\n", EAT, EAT/1000000, EAT/1000000000);
fprintf(stdout,"\n");
for(p = 0; p < 18; p++){
	if(PT->PTBR[p] != -1){
		fprintf(stdout,"%d's Frames = [%d-%d],  ", p, PT->PTBR[p], PT->PTBR[p] + PT->PTLR[p] - 1);
	}
	if(p % 5 == 4){
		fprintf(stdout,"\n");
	}
}
fprintf(stdout,"\n");

for(p = 0; p < 18; p++){
	for(r = 0; r < 32; r++){
		if(PT->frameID[p][r] != -1){
			clearBit(RAM, PT->frameID[p][r]);
		}
	}
}

if(procPid != 0){//child's pid returned to parent
	// printf("Master ID %d\n",getpid());
   bool childrenAlive = false;
   for(p = 0; p < 18; p++){
		if(processBlock->pid[p] != -1){//If > 0 processes running, not deadlocked
			// fprintf(stdout,"********************** processBlock->pid[%d] = %d ********************************\n", p, processBlock->pid[p]);
			childrenAlive = true;
			break;
		}
	}

   if(childrenAlive){//If processes still alive, kill them
		fprintf(stdout,"Time is up, but children remain and must be eradicated...\n");
		fprintf(stdout,"     _.--''--._\n");
		fprintf(stdout,"    /  _    _  \\ \n");
		fprintf(stdout," _  ( (_\\  /_) )  _\n");
		fprintf(stdout,"{ \\._\\   /\\   /_./ }\n");
		fprintf(stdout,"/_'=-.}______{.-='_\\ \n");
		fprintf(stdout," _  _.=('''')=._  _\n");
		fprintf(stdout,"(_''_.-'`~~`'-._''_)\n");
		fprintf(stdout," {_'            '_}\n");
		fprintf(stderr,"\n");

		kill(-getpid(),SIGINT);
	}
}

//Detach from shared mem and Remove shared mem when all processes done
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
if(semctl(semId, 0, IPC_RMID) == -1){//For clock
    fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("semctl");
	exit(EXIT_FAILURE);
}
if(semctl(semIdCond, 0, IPC_RMID) == -1){//For processes requesting resources
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("semctl");
	exit(EXIT_FAILURE);
}
if(semctl(semIdR, 0, IPC_RMID) == -1){//For clock
    fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("semctl");
	exit(EXIT_FAILURE);
}
if(semctl(semIdReq, 0, IPC_RMID) == -1){
	fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
	perror("semctl");
	exit(EXIT_FAILURE);
}

fprintf(stdout,"Master process is complete\n");
return 0;
}//END main

void sigHandler(int mysignal){
	// printf("MASTER process id %d  signal %d\n", getpid(),mysignal);
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

	switch(mysignal){
		case SIGINT:
			fprintf(stderr,"The Parent killed all of the children because something interrupted it (probably the children)!\n");
			// kill(-getpid(),SIGTERM);
			//Detach & Remove shared mem when all processes done
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
			if(semctl(semIdR, 0, IPC_RMID) == -1){//For clock
				fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
				perror("semctl");
				exit(EXIT_FAILURE);
			}
			if(semctl(semIdReq, 0, IPC_RMID) == -1){
				fprintf(stderr,"%s Line %d ",__FILE__,__LINE__);
				perror("semctl");
				exit(EXIT_FAILURE);
			}

			exit(1);
			break;
		default:
			printf("Default signal %d\n", mysignal);
			break;
	}
	return;
}

void initSigHandler(){
	struct sigaction signalAct;
	signalAct.sa_handler = &sigHandler;//Setup sigHandler
	signalAct.sa_flags = SA_RESTART|SA_SIGINFO;
	// sigfillset(&signalAct.sa_mask);//Block all other signals

	if(sigaction(SIGINT, &signalAct, NULL) == -1){
		perror("SIGINT receipt failed");
	}
}

int get_page(int processId, struct pageTable* PT, struct PCB* processBlock, int RAM[], struct timing* timer){
	// fprintf(stdout,"get_page process %d wants page %d = frame %d\n", processId, processBlock->pageWant[processId], PT->frameID[processId][processBlock->pageWant[processId]]);
	int p;
	int frameWant = PT->PTBR[processId] + processBlock->pageWant[processId];;
	// if(processBlock->pageWant[processId] > PT->PTLR[processId] - 1){
	if(frameWant > PT->PTBR[processId] + PT->PTLR[processId] - 1){
		perror("Accessing outside memory: Segmentation Fault\n");
		exit(0);
	}
	// fprintf(stdout,"get_page process %d wants page %d = frame %d\n", processId, processBlock->pageWant[processId], frameWant);
	processBlock->memoryAccessCount[processId] = processBlock->memoryAccessCount[processId] + 1;
	
	if(member(RAM, frameWant) == 0){//Check if frame not in memory
	// if(PT->valid[frameWant] == 0){//Check if frame not in memory
		// fprintf(stdout,"%d Page Faulted on Page %d = frame %d\n", processId, processBlock->pageWant[processId], frameWant);
		set(RAM, frameWant);//Put frame in memory
		PT->frameID[processId][processBlock->pageWant[processId]] = frameWant;//Put Frame Id in Page Table
		PT->valid[frameWant] = 1;
		processBlock->running[processId] = 0;//Suspended now
		addQueue(processId, PT->suspended, &PT->suspTail);//Now in Suspended Queue
		processBlock->pageFaults[processId] = processBlock->pageFaults[processId] + 1;
		return -1;
	}else{//Frame IS in memory
		// fprintf(stdout,"Frame %d available for: %d\n", frameWant, processId);
		PT->valid[frameWant] = 1;
		if(processBlock->action[processId] == 1){
			PT->dirty[frameWant] = 1;
		}
		processBlock->action[processId] = -1;
		advanceClock(&timer->clockSecs, &timer->clockNanos, 10);//advance clock 10ns on success
		signalRequest();
		return processBlock->pageWant[processId];
	}
}

void runDaemon(int freeFrames, struct pageTable* PT, int RAM[]){
	// fprintf(stdout,"RUNNING DAEMON\n");
	int tempId = -1;
	int tempPage = -1;
	int numTurnOff = 256 * 0.05;
	int LRU[numTurnOff];//Hold all frame addresses of oldest numTurnOff residents
	int pid[numTurnOff];//Hold pid that has the frame removing
	int pageId[numTurnOff];//Hold page Id of frame removing
	double times[numTurnOff];//Hold all frame addresses of oldest numTurnOff residents
	double frameLRUTime = 9999;
	int frameLRU = -1;
	fprintf(stdout,"freeFrames %d numTurnOff %d\n", freeFrames, numTurnOff);
	int f, p, r;
	for(f = 0; f < numTurnOff; f++){
		LRU[f] = -1;
	}
	
	for(f = 0; f < numTurnOff; f++){//Find LRU Frames
		for(p = 0; p < 18; p++){
			// for(r = 0; r < 32; r++){
			for(r = 0; r < PT->PTLR[p]; r++){
				// fprintf(stdout,"PT->referenced[%d][%d] %.9f, PT->frameID[%d][%d] = %d\n", p, r, PT->referenced[p][r], p, r, PT->frameID[p][r]);
				// if(PT->referenced[p][r] < frameLRUTime && PT->referenced[p][r] != 0){
				// if(PT->referenced[p][r] < frameLRUTime && PT->referenced[p][r] != 0 && inQueue(LRU, PT->frameID[p][r]) == 0){
				if(PT->referenced[p][r] < frameLRUTime && PT->referenced[p][r] > (double)0 && member(RAM, PT->frameID[p][r]) == 1 && inQueue(LRU, PT->frameID[p][r]) == 0){
					frameLRUTime = PT->referenced[p][r];
					frameLRU = PT->frameID[p][r];
					tempId = p;
					tempPage = r;
					// fprintf(stdout,"PT->referenced[%d][%d] %.9f, PT->frameID[%d][%d] = %d, inQueue %d\n", p, r, PT->referenced[p][r], p, r, PT->frameID[p][r], inQueue(LRU, PT->frameID[p][r]));
				}else if(PT->referenced[p][r] == 0){
					// fprintf(stdout,"NOT EXIST: PT->referenced[%d][%d] %.9f, PT->frameID[%d][%d] = %d, inQueue %d, member %d\n", p, r, PT->referenced[p][r], p, r, PT->frameID[p][r], inQueue(LRU, PT->frameID[p][r]), member(RAM, PT->frameID[p][r]));
				}
			}
		}
		pid[f] = tempId;
		pageId[f] = tempPage;
		LRU[f] = frameLRU;
		times[f] = frameLRUTime;
		// fprintf(stdout,"tempId %d, tempPage %d, frameLRU %d, frameLRUTime %.9f\n", tempId, tempPage, frameLRU, frameLRUTime);
		frameLRUTime = 9999;
		frameLRU = -1;
	}
	
	for(f = 0; f < numTurnOff; f++){
		// fprintf(stdout,"LRU[%d] = %.9f   frame %d\n", f, times[f], LRU[f]);
		if(PT->valid[LRU[f]] == 1 ){//Turn off valid bit
			// fprintf(stdout,"Turning of valid bit frame %d, valid bit was %d\n", LRU[f], PT->valid[LRU[f]]);
			PT->valid[LRU[f]] = 0;
		}else if(PT->valid[LRU[f]] == 0){//Remove from memory
			if(PT->dirty[LRU[f]] == 1){
				/**************************************************************************
				 Save to Disk before Removing Frame since has been wrote to (dirty bit set)
				**************************************************************************/
			}
			// fprintf(stdout,"Removing frame %d == %d, is member %d, valid bit was %d, pid %d, pageId %d\n", LRU[f], PT->frameID[pid[f]][pageId[f]], member(RAM, LRU[f]), PT->valid[LRU[f]], pid[f], pageId[f]);
			PT->dirty[LRU[f]] = 0;
			PT->referenced[pid[f]][pageId[f]] = 0;
			clearBit(RAM, LRU[f]);//Remove from Memory
		}else{
			fprintf(stdout,"ERROR frame %d, valid bit %d\n", LRU[f], PT->valid[LRU[f]]);
		}
	}
	
	return;
}

int getIndex(int framesNeed, int taken[]){
	
int isset = 0; 
int p;
int emptyFrames = 0;

for(p=0; p < 256; p++){
	if(member(taken, p) == 0){//Insert process into bitVector.  Stop creating processes once bitVector full
		emptyFrames++;
		// fprintf(stdout,"%d: emptyFrames %d, framesNeed %d\n", p, emptyFrames, framesNeed);
		if(emptyFrames >= framesNeed){
			// fprintf(stdout, "Return Start Index: %d, emptyFrames %d, End Index %d\n", p + 1 - emptyFrames, emptyFrames, p);
			return p + 1 - emptyFrames;
		}
	}else{
		emptyFrames = 0;
		// fprintf(stdout,"Filled. p %d, emptyFrames %d, framesNeed %d\n", p, emptyFrames, framesNeed);
	}
}

return -1;//Can't fit this process into memory
}

void printPageTable(struct pageTable* PT, int processId){
	fprintf(stdout,"\033[22;32m%d's Page Table (Base Register = %d, PTLR = %d): ", processId, PT->PTBR[processId], PT->PTLR[processId]);
	int p;
	for(p = 0; p < PT->PTLR[processId]; p++){
		if(PT->frameID[processId][p] != -1){
			fprintf(stdout,"%d ", PT->frameID[processId][p]);
		}else{
			fprintf(stdout,". ");
		}
	}
	fprintf(stdout,"\033[0m\n");
	return;
}

int addQueue(int processId, int queue[18], int *tail){
	// fprintf(stdout,"Added Process %d to Queue\n", processId);
	if(*tail == HARD_LIMIT){ // Check to see if the Queue is full
		fprintf(stdout,"%d QUEUE IS FULL\n",processId);
		return 0;
	}
	*tail = *tail + 1;
	queue[*tail % HARD_LIMIT] = processId;// Add the item to the Queue
	// fprintf(stdout,"queue[%d] = %d = %d, tail = %d \n", *tail % HARD_LIMIT, processId,queue[*tail % HARD_LIMIT], *tail);
	return 1;
}

void printQueue(int queue[]){
	fprintf(stdout,"Queue: [");
	int x;
	for(x = 0; queue[x] != -1; x++){
	// for(x = 0; x < 18; x++){
		fprintf(stdout,"%d ",queue[x]);
	}
	fprintf(stdout,"]\n");
	return;
}

int inQueue(int queue[], int processId){
	int i;
	for(i = 0; queue[i] != -1; i++){//Move elements forward in the queue
		if(queue[i] == processId){
			return 1;//Process already in queue
		}
	}
	return 0;//Process not in queue
}

int queueEmpty(int queue[]){
	int n = 0, x;
	for(x = 0; queue[x] != -1; x++){
		n++;
		return n;
	}
	return n;//If 0 is empty
}

int selectFromQueue(int queue[], int *tail, int processId){
	// fprintf(stderr,"tail: %d, processId: %d \n", *tail, processId);
	if(*tail == -1){// Check for empty Queue
		// fprintf(stderr,"QUEUE IS EMPTY \n");
		return -1;  // Return 0 if queue is empty
	}else{
		*tail = *tail - 1;
		// fprintf(stdout,"processId Want = %d, tail = %d\n",processId, *tail);
		int i = 0;
		while(queue[i] != processId){//Find element in queue
			// fprintf(stdout,"Id search queue[%d] = %d != %d\n",i, queue[i], processId);
			i++;
		}
		// fprintf(stdout,"Id Found queue[%d] = %d\n",i, queue[i]);
		processId = queue[i];// Get Id to return
		queue[i] = -1;
		// int i = 1;
		while(queue[i + 1] != -1){//Move elements forward in the queue
			// fprintf(stdout,"Shift queue[%d] = %d\n",i , queue[i + 1]);
			queue[i] = queue[i + 1];
			i++;
		}
		// fprintf(stdout,"queue[%d] = 0\n",i);
		queue[i] = -1;//Set last spot that was filled to 0 since process moved up in queue
		// fprintf(stdout,"Oss: Select queue return: %d\n",processId);
		return processId;// Return popped Id
	}
}

int popQueue(int theQueue[], int *tail){
	int processId;
	if(*tail == -1){// Check for empty Queue
		// fprintf(stderr,"POP QUEUE IS EMPTY \n");
		return 0;  // Return null character if queue is empty
	}
	*tail = *tail - 1;
	// fprintf(stdout,"POP theQueue[0] = %d, tail = %d\n",theQueue[0],*tail);
	processId = theQueue[0];	// Get character to return
	int i = 1;
	while(theQueue[i] != -1){//Move elements forward in the queue
		// fprintf(stdout,"POP theQueue[%d] = %d\n",i - 1, theQueue[i]);
		theQueue[i - 1] = theQueue[i];
		i++;
	}
	// fprintf(stdout,"POP theQueue[%d] = 0\n",i - 1);
	theQueue[i - 1] = -1;//Set last spot that was filled to 0 since process moved up in queue
	return processId;// Return popped character
}

int countArray(int array[]){
	int n = 0, x;
	for(x = 0; x < 18; x++){
		if(array[x] != -1){
			n++;
		}
	}
	return n;
}

void advanceClock(int *clockSecs, int *clockNanos, int amount){//1 sec = 1,000 milli = 1,000,000 micro = 1,000,000,000 nano
	waitClock();
	int randTime;
	if(amount == 10){
		randTime = amount;//incr 10 ns
	}else if(amount == 15000000){
		randTime = amount;//incr 15000000ns, 15ms
	}else{
		randTime = rand() % amount + 1;//simulate overhead activity for each iteration by 0.0001s
	}
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
	// double time = (double)*clockSecs + ((double)*clockNanos/1000000000);
	*time = 0;
	*time = (double)*clockSecs + ((double)*clockNanos/1000000000);
	return *time;
}

void clearBit(int bv[], int i){//Clear the process from the process table
  int j = i/32;
  int pos = i%32;//2 % 32 = 2.  33 % 32 = 1.
  unsigned int flag = 1;  // flag = 0000.....00001
  flag = flag << pos;     // flag = 0000...010...000   (shifted i positions)
  flag = ~flag;           // flag = 1111...101..111 - Reverse the bits
  // fprintf(stdout,"Clear Frame %d: byte %d, pos %d, = %d \n", i, j, pos, bv[j] & flag);
  bv[j] = bv[j] & flag;   // RESET the bit at the i-th position in bv[i]
  //OR this is all the above consolidated: bv[i/32] &= ~(1 << (i%32));
  return;
}

void set(int bv[], int i){//set value in bv.  i is value to set in bv array
  int j = i/32;//Get the array index/position
  int pos = i%32;//Get the bit position
  unsigned int flag = 1;   // flag = 0000.....00001
  
  flag = flag << pos;      // flag = 0000...010...000   (shifted i positions)
	
  bv[j] = bv[j] | flag;    // Set the bit at the i-th position in bv[i]
  // fprintf(stdout,"Setting Frame %d: byte %d, pos %d, = %d \n", i, j, pos, bv[j] | flag);
  //OR this is all the above consolidated: bv[i/32] |= 1 << (i%32);
  return;
}

void printBit(int bv[], int size, char valid[]){//set value in bv.  i is value to set in bv array
int i = 0, p;
int numBits = size * 8;
if(size == 32){ // fprintf(stdout,"Printing Memory Map\n");
	fprintf(stdout,"    \033[01;35m0 1 2 3 4 5 6 7 8 9          15  16                           31\033[0m\n");
}else{
	fprintf(stdout,"\033[01;35m0 1 2 3 4 5 6 7 8 9          15  16                           31\033[0m\n");
}
if(size == 32){
	for(p=0; p < numBits; p++){
		if(i == 0){
			if(p == 0){
				fprintf(stdout,"\033[01;35m%d   \033[0m", p);
			}else if(p == 32 || p == 64 || p == 96){
				fprintf(stdout,"\033[01;35m%d  \033[0m", p);
			}else{
				fprintf(stdout,"\033[01;35m%d \033[0m", p);
			}
		}
		 if(member(bv, p)){
			 if(valid[p] == 0){
				fprintf(stdout, "\033[22;34m1 \033[0m");
			 }else{
				 fprintf(stdout, "1 ");
			 }
		 }else{
			fprintf(stdout, "\033[22;31m0 \033[0m"); 
		 }
	
		i++;
		if(i == 16){
			fprintf(stdout," ");
		}else if(i == 32){
			fprintf(stdout,"\033[01;35m%d \033[0m\n", p);
			i = 0;
		}
	}
}else{
	for(p=0; p < numBits; p++){
		 if(member(bv, p)){
			fprintf(stdout, "1 ");
		 }else{
			fprintf(stdout, "\033[22;31m0 \033[0m"); 
		 }
		i++;
		if(i == 16){
			fprintf(stdout," ");
		}else if(i == 32){
			fprintf(stdout,"\n");
			i = 0;
		}
	}
}

return;
}

int countEmptyFrames(int bv2[], int size){
	
int j, p, i = 0, count = 0, numBits = size * 8;
int maxPow = 1<<(size*8-1);
// fprintf(stdout, "COUNTING FREE FRAMES\n");
// for(p=0; p < 256; p++){
for(p=0; p < numBits; p++){
	if(!member(bv2, p)){
		count++;
		// fprintf(stdout, "%d ", member(bv2, p));
	 }else{
		 // fprintf(stdout, "%d ", member(bv2, p));
	 }
	if(p % 32 == 31){
	 // fprintf(stdout, "\n");
	}
}
// fprintf(stdout, "DONE COUNTING FREE FRAMES %d\n", count);
return count;
}

int member(int bv[], int i){//check if i in bv
  int j = i/32;//Get the array position
  int pos = i%32;//Get the bit position
  unsigned int flag = 1;  // flag = 0000.....00001
  flag = flag << pos;     // flag = 0000...010...000   (shifted i positions)
  int p;
	// fprintf(stdout,"member flag: %d, number looking for: %d\n", flag, i);
  // fprintf(stdout,"member bv[%d]: %d\n", j, bv[j]);
  if (bv[j] & flag)// Test the bit at the k-th position in bv[i]
	 return 1;
  else
	 return 0;
 //OR this is all the above consolidated: return ((bv[i/32] & (1 << (i%32) )) != 0) ;
}

int bvFull(int bv[], int numProcesses){//check if i in bv
	int i;
	for(i = 0; i <= numProcesses; i++){
		  int j = i/32;//Get the array position
		  int pos = i%32;//Get the bit position
		  unsigned int flag = 1;  // flag = 0000.....00001
		  flag = flag << pos;     // flag = 0000...010...000   (shifted i positions)
		  int p;
		  if (bv[j] & flag){// Test the bit at the k-th position in bv[i]
			//Spot taken
		  }else{
			  return 0;//Spot open
		  }
	}
	return 1;//Is full
}

int bvEmpty(int bv[], int numProcesses){//check if i in bv
	int i;
	for(i = 1; i <= numProcesses; i++){
		  int j = i/32;//Get the array position
		  int pos = i%32;//Get the bit position
		  unsigned int flag = 1;  // flag = 0000.....00001
		  flag = flag << pos;     // flag = 0000...010...000   (shifted i positions)
		  int p;
		  if (bv[j] & flag){// Test the bit at the k-th position in bv[i]
			return 0;//Not empty
		  }else{
		  }
	}
	return 1;//Is empty
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

void waitRemove(){
	removeP.sem_num = 0;/* Which semaphore in the semaphore array*/
    removeP.sem_op = -1;/* Subtract 1 from semaphore value*/
    removeP.sem_flg = 0;/* Set the flag so we will wait*/
    if(semop(semIdR, &removeP, 1) == -1){
        exit(EXIT_FAILURE);
    }
}

void signalRemove(){
	removeP.sem_num = 0;/* Which semaphore in the semaphore array*/
    removeP.sem_op = 1;/* Add 1 to semaphore value*/
    removeP.sem_flg = 0;/* Set the flag so we will wait*/
	if(semop(semIdR, &removeP, 1) == -1){
        exit(EXIT_FAILURE);
    }
}

void signalRequest(){
	request.sem_num = 0;/* Which semaphore in the semaphore array*/
	request.sem_op = 1;/* Add 1 to semaphore value*/
	request.sem_flg = 0;/* Set the flag so we will wait*/
	if(semop(semIdReq, &request, 1) == -1){
		exit(EXIT_FAILURE);
	}
}

double genRandomDouble(){
	// fprintf(stdout,"returning %.9f\n", ((double)rand() * ( 0.5 - 0 ) ) / (double)RAND_MAX + 0);
	// return ((double)rand() * (max - min)) / (double)RAND_MAX + min;
	return ((double)rand() * ( 0.5 - 0 ) ) / (double)RAND_MAX + 0;//[0-500ms]
}

/*$Author: o2-gray $
*$Date: 2016/04/26 15:02:34 $
*$Log: oss.c,v $
*Revision 1.5  2016/04/26 15:02:34  o2-gray
*Have OSS and Processes communicating good now.  Need to make sure nextIndex
*function that searches the RAM for an empty spot to hold a new process
*with it's given amount of frames is working.  Then EAT for each process 
*when they write to the log, then done!
*
*Revision 1.4  2016/04/24 03:16:06  o2-gray
*About done.  Need more testing with multiple processes and then do statistics
*
*Revision 1.3  2016/04/23 21:14:26  o2-gray
*This is a late check-in.  Worked on it all yesterday and didn't remember to
*check it in until I layed down.  Set an alarm to do it today and got distracted.
*Anyways...
*getPage is working good with multiple processes now.  Had to change a few things
*around since not testing it so much now.  Started to create the Daemon function.
*
*Revision 1.2  2016/04/20 03:18:00  o2-gray
*RAM, Process Table, Suspended Queue, Page Tables, PCBs, and timing all set up
*Doing memory request with 1 process right now.  Need to finish get_page then
*test with more processes.
*
*Revision 1.1  2016/04/17 02:37:48  o2-gray
*Initial revision
*
*/

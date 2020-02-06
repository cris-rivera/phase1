/* ------------------------------------------------------------------------
   phase1.c

   CSCV 452

   ------------------------------------------------------------------------ */

//Comment

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();
void RdyList_Insert(proc_ptr process);
int getpid();
void dump_processes();
int zap(int pid);


/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lists  */
proc_ptr ReadyList;
proc_ptr BlockedList;
proc_ptr ZapperList;

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	     Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
   int i;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   for(i = 0; i < MAXPROC; i++)
   {
      ProcTable[i].next_proc_ptr = NULL;
      ProcTable[i].child_proc_ptr = NULL;
      ProcTable[i].next_sibling_ptr = NULL;
      memset(ProcTable[i].name, 0, sizeof(ProcTable[i].name));
      memset(ProcTable[i].start_arg, 0, sizeof(ProcTable[i].start_arg));
      ProcTable[i].state.start = NULL;
      ProcTable[i].state.initial_psr = 0;
      ProcTable[i].pid = -1;
      ProcTable[i].priority = 0;
      ProcTable[i].start_func = NULL;
      ProcTable[i].stack = NULL;
      ProcTable[i].stacksize = 0;
      ProcTable[i].status = EMPTY;
      ProcTable[i].exit_status = -1;
      ProcTable[i].z_status = NONE;
   }  
   
   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");
   ReadyList = NULL;
   BlockedList = NULL;
   ZapperList = NULL;
   Current = NULL;

   /* Initialize the clock interrupt handler */
   //int vec[CLOCK_DEV] = clock_handler;

   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                   SENTINELPRIORITY);
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\n");
      halt(1);
   }
  
   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");
   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
   if (result < 0) {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
   if (DEBUG && debugflag)
      console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
112    Name - RdyList_Insert
113    Purpose - Inserts newly forked process into the ready table based on priority.
114    Parameters - A pointer to the newl forked process called 'process'.
115    Returns - nothing
116    Side Effects - ReadyList has a new addition in a sorted place.
117    ----------------------------------------------------------------------- */
void RdyList_Insert(proc_ptr process)
{
  
  proc_ptr walker = NULL;

  if(ReadyList == NULL)
  {
     ReadyList = process;
  }

  if(process->priority < ReadyList->priority)
  {
     process->next_proc_ptr = ReadyList;
     ReadyList = process;
  }
  else
  {
     walker = ReadyList->next_proc_ptr;
     while(walker != NULL)
     {
       if(process->priority < walker->priority)
       {
         process->next_proc_ptr = walker;
         walker = ReadyList;
       }

       if(walker->next_proc_ptr == process->next_proc_ptr)
       {
         walker->next_proc_ptr = process;
       }
       else
       {
         walker = walker->next_proc_ptr;
       }

     }
  }

}

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*f)(char *), char *arg, int stacksize, int priority)
{
   int proc_slot = next_pid % MAXPROC;
   int pid_count = 0;
   proc_ptr proc_tbl_ptr = NULL;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0){
      console("fork1(): not in kernel mode");
      halt(1);
   }
   /* Return if stack size is too small */
   if(stacksize < USLOSS_MIN_STACK){
      console("fork1(): stack size too small");
      halt(1);
   }

   /* find an empty slot in the process table */
   while(pid_count < MAXPROC && ProcTable[proc_slot].status != EMPTY)
   {
      next_pid++;
      proc_slot = next_pid % MAXPROC;
      pid_count++;
   }

   /* Return if process table is full */
   if(pid_count >= MAXPROC)
   {
      //enableInterrupts();
      if(DEBUG && debugflag)
      {
        console("fork1(): process table full\n");
      }
      return -1;
   }

   /* fill-in entry in process table */
   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }
   strcpy(ProcTable[proc_slot].name, name);
   ProcTable[proc_slot].pid = next_pid++;
   ProcTable[proc_slot].start_func = f;
   ProcTable[proc_slot].stack = malloc(stacksize);
   ProcTable[proc_slot].stacksize = stacksize;
   ProcTable[proc_slot].priority = priority;
   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);

   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC)
    */
   context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);

   proc_tbl_ptr = &ProcTable[proc_slot];
   RdyList_Insert(proc_tbl_ptr);
   ProcTable[proc_slot].status = READY;
   
   /* for future phase(s) */
   //p1_fork(ProcTable[proc_slot].pid);

  if(Current != NULL)
  {
    Current->child_proc_ptr = &ProcTable[proc_slot];
  }

  // Avoid calling sentinel
  if(strcmp(ProcTable[proc_slot].name, "sentinel"))
  { 
    dispatcher();
  }

  enableInterrupts();
  console("IN FORK\n");
  return ProcTable[proc_slot].pid;

} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
   int result;

   if (DEBUG && debugflag)
      console("launch(): started\n");

   /* Enable interrupts */
   enableInterrupts();

   /* Call the function passed to fork1, and capture its return value */
   result = Current->start_func(Current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", Current->pid);

   quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{
  if(Current->child_proc_ptr == EMPTY)
     return -2;
   
   //Check if parent is zapped 
   // If yes, return -1

  while(Current->child_proc_ptr->exit_status == -1)
  {
     //sleep? 
  }

  *code = Current->child_proc_ptr->exit_status;

  console("join(): Should not see this!");
  return 0;
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
   if(Current->child_proc_ptr != EMPTY)
   {
      console("quit(): Child processes are active");
      halt(1);
   }
   else{
      Current->exit_status = code;
   }

   console("QUIT\n");

   p1_quit(Current->pid);
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
   proc_ptr next_process = NULL;
   proc_ptr walker = NULL;
   
  /* checks if there is a process currently running */
  if(Current == NULL)
  {
    next_process = ReadyList;
    ReadyList = ReadyList->next_proc_ptr;
    next_process->next_proc_ptr = NULL;
    
    Current = next_process;
    Current->status = RUNNING;
    context_switch(NULL, &Current->state);
  }
  else if(Current->status == BLOCKED)
  {
    next_process = ReadyList;
    ReadyList = ReadyList->next_proc_ptr;
    next_process->next_proc_ptr = NULL;

    walker = BlockedList;
    while(walker->next_proc_ptr != NULL)
    {
      walker = walker->next_proc_ptr;
    }

    walker->next_proc_ptr = Current;
    walker = Current;
    Current = next_process;
    Current->status = RUNNING;
    context_switch(&walker->state, &Current->state);

  }
  else
  {
   /* Sets top of ready list as next runnable process.
    * Sets the top of ready list to the next ready process.
    * disconnects next runnable process from ready list*/
    next_process = ReadyList;
    ReadyList = ReadyList->next_proc_ptr;
    next_process->next_proc_ptr = NULL;

    walker = ReadyList;
    while(walker->next_proc_ptr != NULL)
    {
      walker = walker->next_proc_ptr;
    }

    walker->next_proc_ptr = Current;
    
    Current->status = READY;
    walker = Current;

    Current = next_process;
    Current->status = RUNNING;
    context_switch(&walker->state, &Current->state);

  }
   p1_switch(Current->pid, next_process->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
	     processes are blocked.  The other is to detect and report
	     simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
		   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char * dummy)
{
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void check_deadlock()
{
   /* Gotta figure out where check_io is 

   if(check_io() == 1)
      return;

   */
  
   //Check the number of living processes
   int process_count = 0;
   for(int i = 0; i < MAXPROC; i++)
   {
      if(ProcTable[i].status != EMPTY)
         process_count++;
   }

   if(process_count > 1)
      halt(1);
   
   if(process_count == 1)
      halt(0);

} /* check_deadlock */

int zap(int pid)
{
  int proc_slot = 0;
  proc_ptr walker = NULL;

  //for loop to iterate through proc_list to find PID to zap
  for(int i = 0; i < MAXPROC; i++)
  {
    if(ProcTable[i].pid == pid)
      proc_slot = i;
  }

  //If statement to assure zapping only occurs if pid is actually found in the
  //process table
  if(proc_slot != 0)
  {
    //change z_status of newly found pid process to zapped
    ProcTable[proc_slot].z_status = ZAPPED;
    //change z_status of current process to zapper
    Current->z_status = ZAPPER;
    Current->z_pid = ProcTable[proc_slot].pid;
    //add current process to ZapperList
    if(ZapperList == NULL)
      ZapperList = Current;
    else
    {
      //iterate through ZapperList and add itself to the end
      walker = ZapperList;
      while(walker->next_proc_ptr != NULL)
      {
        walker = walker->next_proc_ptr;
      }

      walker->next_proc_ptr = Current;
    }
    
    //change status of current process to BLOCKED
    Current->status = BLOCKED;
    //call dispatcher
    dispatcher();
  }

  //to supress warning for now
  return 0;

}

/* ------------------------------------------------------------------------
 * Name - enableInterrupets
 * Purpose - Enables all interrupts on vector table, however for phase 1
 * it will only enable the clock interrupt.
 * Parameters - none
 * Side Effects: none yet
 * ----------------------------------------------------------------------- */
void enableInterrupts()
{
  
  //if not in kernel mode...
  if((PSR_CURRENT_MODE & psr_get()) == 0){
    console("Kernel Error: Not in kernel mode. may not enable interrupts\n");
    halt(1);
  //else is in kernel mode...
  } else
   /* We ARE in kernel mode */
   psr_set( psr_get() | PSR_CURRENT_INT );
} /* enablebleInterrupts */ 


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */ 

int getpid()
{
  return Current->pid;
}

void dump_processes()
{
  int i = 0;
  proc_ptr walker = &ProcTable[i];

  while(walker->pid != -1)
  {
    console("Name: \n");
    console("PID: %d\n", walker->pid);
    console("Priority: %d\n", walker->priority);
    console("Status: \n");
    console("CPU Time: \n");
    console("Parent's PID: \n");
    console("Children: \n\n");
    i += i;
    walker = &ProcTable[i];
  }
}

/* Unifished Clock handler

void clock_handler()
{
	if(Current->start_time == NULL)
		Current->start_time = read_time();
	time_slice();

}

int read_time(void)
{
	// Return system time in microseconds
	return sys_clock()/1000;
}

int read_cur_start_time(void)
{
	return Current->start_time;
}

void time_slice(void)
{
	if(((read_time() - read_cur_start_time()) * 1000) >= 80))
		dispatcher();
	else
		return; 
}

*/

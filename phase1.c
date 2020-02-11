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
static void RdyList_Insert(proc_ptr process);
int getpid();
void dump_processes();
int zap(int pid);
int is_zapped(void);
int check_io();
void test_kernel_mode(char *str);
int block_me(int new_status);
int unblock_proc(int pid);
void clock_handler();
int read_time(void);
int read_cur_start_time(void);
void time_slice(void);
static void BlkList_Delete(proc_ptr process);
static void ZprList_Delete(proc_ptr process);


/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 0;

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
   char *func_str = "startup()";
   test_kernel_mode(func_str);

   /* initialize the process table */
   for(i = 0; i < MAXPROC; i++)
   {
      ProcTable[i].next_proc_ptr = NULL;
      ProcTable[i].next_zapper_ptr = NULL;
      ProcTable[i].child_proc_ptr = NULL;
      ProcTable[i].next_sibling_ptr = NULL;
      ProcTable[i].parent_proc_ptr = NULL;
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
      ProcTable[i].zapped_status = NONE;
      ProcTable[i].zapper_status = NONE;
      ProcTable[i].z_pid = -1;
      ProcTable[i].start_time = 0;
   }  
   
   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");
   ReadyList = NULL;
   BlockedList = NULL;
   ZapperList = NULL;
   Current = NULL;

   /* Initialize the clock interrupt handler */
   int_vec[CLOCK_DEV] = clock_handler;

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

   char *func_str = "finish()";
   test_kernel_mode(func_str);
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
static void RdyList_Insert(proc_ptr process)
{
  char *func_str = "RdyList_Insert()";
  test_kernel_mode(func_str);

  proc_ptr walker, previous;
  previous = NULL;


  if(ReadyList == NULL)
    ReadyList = process;
  else
  {
    walker = ReadyList;
    
    // "walker" steps to link in ReadyList with greater priority than "process"
    // "previous" points to ReadyList link before the greater-priority process.
    while(walker != NULL && walker->priority <= process->priority)
    {
      previous = walker;
      walker = walker->next_proc_ptr;
    }

    // "process" goes to the front of ReadyList if it has a lower priority
    // than any other process in ReadyList. 
    if(previous == NULL)
    {
      process->next_proc_ptr = ReadyList;
      ReadyList = process;
    }

    // Insert "process" into ReadyList immeadiately before the next
    // link which has a greater priority
    else
    {
      previous->next_proc_ptr = process;
      process->next_proc_ptr = walker;
    }
  }
  //return;
}/* RdyList_Insert*/

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
   char *func_str = "fork1()";
   int proc_slot = next_pid % MAXPROC;
   int pid_count = 0;
   proc_ptr proc_tbl_ptr = NULL;
   proc_ptr walker = NULL;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   test_kernel_mode(func_str);

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
      enableInterrupts();
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

   
   //Insert forked process into ReadyList
   ProcTable[proc_slot].status = READY;
   proc_tbl_ptr = &ProcTable[proc_slot];
   RdyList_Insert(proc_tbl_ptr);
   
   /* for future phase(s) */
   //p1_fork(ProcTable[proc_slot].pid);

  //sets up Parent-Child relationship.

  if(Current != NULL)
  {
    // Current has no children yet, establish child-parent relationship
    if(Current->child_proc_ptr == NULL) 
      Current->child_proc_ptr = &ProcTable[proc_slot];

    // Current has one child, establish sibling relationship between 
    // Current's child and process being forked
    else if(Current->child_proc_ptr->next_sibling_ptr == NULL)
        Current->child_proc_ptr->next_sibling_ptr = &ProcTable[proc_slot];
    
    // Current has multiple children. Insert the process currently being 
    // forked at the end of the next_sibiling linked list.
    else
    {
      walker = Current->child_proc_ptr;
      while(walker->next_sibling_ptr != NULL)
      {
        walker = walker->next_sibling_ptr;
      }
      walker->next_sibling_ptr = &ProcTable[proc_slot];
    }

    // establish parent-child relationship
    ProcTable[proc_slot].parent_proc_ptr = Current;
  }

  // Avoid calling sentinel
  if(strcmp(ProcTable[proc_slot].name, "sentinel"))
  { 
    dispatcher();
  }

  enableInterrupts();
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
   char *func_str = "launch()";
   test_kernel_mode(func_str);
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

  if(DEBUG && debugflag)
    console("in join()\n");

  char *func_str = "join()";
  test_kernel_mode(func_str);
  proc_ptr temp = NULL;
  int temp_pid = 0;

  // Current process has no children
  if(Current->child_proc_ptr == NULL)
     return -2;

  // Continuously call dispatcher until child process quits
  while(Current->child_proc_ptr->status != DEAD)
  {
    Current->status = BLOCKED;
    dispatcher();
  }
  
  //check if parent is zapped
 //if yes, return -1
 if(Current->zapped_status == ZAPPED)
   return -1;

  // Child has quit
  if(Current->child_proc_ptr->status == DEAD)
  {
    *code = Current->child_proc_ptr->exit_status;
    temp_pid = Current->child_proc_ptr->pid;
    temp = Current->child_proc_ptr;

    // Make Current's next child the previous child's next sibling
    Current->child_proc_ptr = temp->next_sibling_ptr;
    
    //Clear out PCB block
    temp->next_proc_ptr = NULL;
    temp->next_zapper_ptr = NULL;
    temp->child_proc_ptr = NULL;
    temp->next_sibling_ptr = NULL;
    temp->parent_proc_ptr = NULL;
    memset(temp->name, 0, sizeof(temp->name));
    memset(temp->start_arg, 0, sizeof(temp->start_arg));
    temp->state.start = NULL;
    temp->state.initial_psr = 0;
    temp->pid = -1;
    temp->priority = 0;
    temp->start_func = NULL;
    temp->stack = NULL;
    temp->stacksize = 0;
    temp->exit_status = -1;
    temp->zapped_status = NONE;
    temp->zapper_status = NONE;
    temp->z_pid = -1;
    temp->start_time = 0;
    temp->status = EMPTY;
   
    
    return temp_pid;
  }

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

   if(DEBUG && debugflag)
      console("in quit()\n");

   char *func_str = "quit()";
   test_kernel_mode(func_str);
   proc_ptr child_ptr = Current->child_proc_ptr;
   proc_ptr parent_ptr = Current->parent_proc_ptr;
   proc_ptr walker = NULL;
   proc_ptr temp = NULL;
   
   //added this if statement because not every process will have a child, and
   //it was causing a segmentation fault to not check because it was trying to
   //check the status of a structure held within a NULL address, which
   //obviously does not exist.
   if(child_ptr != NULL)
   {
    //This should be checking the status of the child process not the value of
    //the pointer. The status is held in its own variable, but the
    //child_proc_ptr only hold the address of the child process.
    if(child_ptr->status != DEAD)
    {
      console("quit(): Child processes are active!\n");
      halt(1);
    }
   }

    //else statement was unnecessary, if there is a running child process
    //active it will halt and never reach any of the following code. If there
    //is not a running process, then it can reach this code and execute it
    //without problem.
    //Changed this to DEAD to signify that the process is dead instead of
    //empty. 
    Current->status = DEAD;
    Current->exit_status = code;

    if(Current->zapped_status == ZAPPED)
    {
      walker = ZapperList;
      temp = BlockedList;
      while(walker != NULL && temp != NULL)
      {
        if(walker->z_pid == Current->pid)
        {
          temp = walker;
          walker = walker->next_zapper_ptr;
          temp->z_pid = -1;
          temp->zapper_status = NONE;
          ZprList_Delete(temp);
          BlkList_Delete(temp);
          temp->status = READY;
          RdyList_Insert(temp);
        }
        else
          walker = walker->next_zapper_ptr;

      }
    }
     
      //If parent is blocked, unblock it.
      if(parent_ptr != NULL)
      {
       parent_ptr->status = READY;
       RdyList_Insert(parent_ptr);
      }
    

    dispatcher();

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
   if(DEBUG && debugflag)
    console("in dispatcher()\n");
   char *func_str = "dispatcher()";
   test_kernel_mode(func_str);
   proc_ptr next_process = NULL;
   proc_ptr walker = NULL;
   
  // No currently running process. This code will intiate
  // the first process to run. ReadyList should only contain
  // one process
  if(Current == NULL)
  {
    
    // next_process points to the singular process in ReadyList
    next_process = ReadyList;
    ReadyList = ReadyList->next_proc_ptr;
    next_process->next_proc_ptr = NULL;
    
    // Execute Current
    Current = next_process;
    Current->status = RUNNING;
    context_switch(NULL, &Current->state);
  }

  // Current process has been blocked
  else if(Current->status == BLOCKED)
  {
    next_process = ReadyList;
    ReadyList = ReadyList->next_proc_ptr;
    next_process->next_proc_ptr = NULL;
    
    // There are no other blocked processes.
    // Add Current process to the BlockedList and execute next process in ReadyList
    if(BlockedList == NULL)
    {
      BlockedList = Current;
      Current = next_process;
      Current->status = RUNNING;
      context_switch(&BlockedList->state, &Current->state);
    }

    // There are at least one blocked processes
    else
    {
      // Add Current to the end of BlockedList
      walker = BlockedList;
      while(walker->next_proc_ptr != NULL)
      {
        walker = walker->next_proc_ptr;
      }
      walker->next_proc_ptr = Current;

      // Execute next process in ReadyList
      // Walker is used as a temporary holder for previously running process
      walker = Current;
      Current = next_process;
      Current->status = RUNNING;
      context_switch(&walker->state, &Current->state);
    }

  }


  else if(Current->status == DEAD)
  { 
    // Run next process in ReadyList
    if(ReadyList != NULL)
    { 
      next_process = ReadyList;
      ReadyList = ReadyList->next_proc_ptr;
      next_process->next_proc_ptr = NULL;

      walker = Current;

      Current = next_process;
      Current->status = RUNNING;
      context_switch(&walker->state, &Current->state);
    }

    // else call Sentinel here? Current process is dead and ReadyList is NULL
    //ReadyList should never be NULL, sentinel is always the last element in
    //ReadyList. Once all processes are dead, the context switch will switch to
    //sentinel from the top of ReadyList.
  }

  // Under which circumstances will this code execute?
  // When clock_handler switches processes?
  else
  {
   /* Sets top of ready list as next runnable process.
    * Sets the top of ready list to the next ready process.
    * disconnects next runnable process from ready list*/
    next_process = ReadyList;
    ReadyList = ReadyList->next_proc_ptr;
    next_process->next_proc_ptr = NULL;
    
    RdyList_Insert(Current);
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
   char *func_str = "sentinel()";
   test_kernel_mode(func_str);
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
   char *func_str = "check_deadlock()";
   test_kernel_mode(func_str);

   //check_io() is a dummy function that just returns 0 during this phase.
   if(check_io() == 1)
      return;
  
   //Check the number of living processes
   int process_count = 0;
   for(int i = 0; i < MAXPROC; i++)
   {
      if(ProcTable[i].status != EMPTY)
         process_count++;
   }

   if(process_count > 1){
      if(DEBUG && debugflag)
        console("check_deadlock(): process_count = %d\n", process_count);
      halt(1);
   }
   
   if(process_count == 1){
      if(DEBUG && debugflag)
        console("check_deadlock(): process_count = 0\n");
      halt(0);
   }

} /* check_deadlock */

int zap(int pid)
{
  if(DEBUG && debugflag)
    console("in zap()\n");
   
  char *func_str = "zap()";
  test_kernel_mode(func_str);
  int proc_slot = -1;
  proc_ptr walker = NULL;

  if(pid == Current->pid)
  {
    console("Error: Cannot process cannot zap itself!\n");
    halt(1);
  }

  //for loop to iterate through proc_list to find PID to zap
  for(int i = 0; i < MAXPROC; i++)
  {
    if(ProcTable[i].pid == pid)
      proc_slot = i;
  }
  
  if(proc_slot == -1)
  {
    console("Error: pid does not exist!\n");
    halt(1);
  }

  //If statement to assure zapping only occurs if pid is actually found in the
  //process table
  if(proc_slot != 0)
  {
    
    //change z_status of newly found pid process to ZAPPED
    ProcTable[proc_slot].zapped_status = ZAPPED;

    //change z_status of current process to zapper
    Current->zapper_status = ZAPPER;
    Current->z_pid = ProcTable[proc_slot].pid;

    //add current process to ZapperList
    if(ZapperList == NULL)
      ZapperList = Current;
    else
    {
      //iterate through ZapperList and add itself to the end
      walker = ZapperList;
      while(walker->next_zapper_ptr != NULL)
      {
        walker = walker->next_zapper_ptr;
      }

      walker->next_zapper_ptr = Current;
    }
   
   // ZAPPED process is not DEAD
   while(ProcTable[proc_slot].status != DEAD && ProcTable[proc_slot].status != EMPTY)
   { 
    //change status of current process to BLOCKED
    Current->status = BLOCKED;
    dispatcher();
   }
  }
  
  if(Current->zapped_status == ZAPPED)
    return -1;

  // to supress warning for now
  // ZAPPED process is DEAD
  if(ProcTable[proc_slot].status == DEAD || ProcTable[proc_slot].status == EMPTY)
    return 0;

  console("zap(): should never see this.\n");
  return -2;
}

int is_zapped(void)
{
  if(DEBUG && debugflag)
    console("in is_zapped()\n");

  char *func_str = "is_zapped()";
  test_kernel_mode(func_str);
  if(Current->zapped_status == ZAPPED)
    return 1;
  else
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
  char *func_str = "enableInterrupts()";
  test_kernel_mode(func_str);
  psr_set( psr_get() | PSR_CURRENT_INT );
} /* enablebleInterrupts */ 


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
  char *func_str = "disableInterrupts()";
  test_kernel_mode(func_str);
  psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */ 

int getpid()
{
  char *func_str = "getpid()";
  test_kernel_mode(func_str);
  return Current->pid;
}

void dump_processes()
{
  char *func_str = "dump_processes()";
  test_kernel_mode(func_str);
  int i;
  proc_ptr parent = NULL;
  proc_ptr child = NULL;

  //just to make it look aesthetically pleasing.
  console("\n");

  for(i = 0; i < MAXPROC; i++)
  {
    parent = ProcTable[i].parent_proc_ptr;
    child = ProcTable[i].child_proc_ptr;
     
    console("Name: %s\n", ProcTable[i].name);
    console("PID: %d\n", ProcTable[i].pid);
    console("Priority: %d\n", ProcTable[i].priority);
    console("CPU Time: \n");

    if(parent == NULL)
       console("Parent's PID: No parent\n");
    else
      console("Parent PID: %lu\n", parent->pid);
    if(child == NULL)
      console("Child: No children\n");
    else
      console("Child: %s\n", child->name);

    if(ProcTable[i].status == EMPTY)
      console("Status: Empty\n\n");
    else if(ProcTable[i].status == DEAD)
      console("Status: Dead\n\n");
    else if(ProcTable[i].status == BLOCKED)
      console("Status: Blocked\n\n");
    else if(ProcTable[i].status == READY)
      console("Status: Ready\n\n");
    else if(ProcTable[i].status == RUNNING)
      console("Status: Running\n\n");        
  }
}

//This is a dummy function that we must include just so check_deadlock() works.
//We will implement it at a later phase, but Professor Xu told us to include it 
//now as a dummy function.
int check_io()
{
  char *func_str = "check_io()";
  test_kernel_mode(func_str);
  return 0;
}

void test_kernel_mode(char *str)
{
  if((PSR_CURRENT_MODE & psr_get()) == 0)
  {
       console("%s: not in kernel mode\n", str);
       halt(1);
  }
}

int block_me(int new_status)
{
  char *func_str = "block_me()";
  test_kernel_mode(func_str);

  proc_ptr walker = BlockedList;


  if(new_status <= 10)
  {
    console("Error: invalid status.\n");
    halt(1);
  }

  if(Current->zapped_status == ZAPPED)
    return -1;

  // Insert Current to end of BlockedList
  Current->status = BLOCKED;
  while(walker->next_proc_ptr != NULL)
  {
    walker = walker->next_proc_ptr;
  }
  walker->next_proc_ptr = Current;

  dispatcher();

  return 0;
}

int unblock_proc(int pid)
{
  char *func_str = "unblock_proc()";
  test_kernel_mode(func_str);
  proc_ptr walker = BlockedList;

  // Find process in BlockedList with designated PID
  while(walker->pid != pid && walker->next_proc_ptr != NULL)
  {
    walker = walker->next_proc_ptr;
  }

  if(walker->status != BLOCKED || walker == Current || walker->status <= 10 || walker->pid != pid)
    return -2;

  if(Current->zapped_status == ZAPPED)
    return -1;

  //TODO delete unblocked process from BlockedList
  
  walker->status = READY;
  RdyList_Insert(walker);
  dispatcher();

  return 0;
}  


void clock_handler()
{
  char *func_str = "clock_handler()";
  test_kernel_mode(func_str);

  // The first interrupt for the Current process
	if(Current->start_time == 0)
		Current->start_time = read_time();
	time_slice();

}

int read_time(void)
{
  char *func_str = "read_time()";
  test_kernel_mode(func_str);

	// Return system time in microseconds
	return sys_clock()/1000;
}

int read_cur_start_time(void)
{
  char *func_str = "read_cur_start_time()";
  test_kernel_mode(func_str);

	return Current->start_time;
}


void time_slice(void)
{
  char *func_str = "time_slice()";
  test_kernel_mode(func_str);

  // Current process has exceeded 79ms of runtime 
	if(((read_time() - read_cur_start_time()) * 1000) >= 80)
  {
    Current->start_time = 0;
		dispatcher();
  }
	else
		return; 
}



static void BlkList_Delete(proc_ptr process)
{
  if(DEBUG && debugflag)
    console("in BlkList_Delete()\n");

   char *func_str = "BlkList_Delete()";
   test_kernel_mode(func_str);

   proc_ptr walker, previous;
   previous = NULL;
   walker = BlockedList;

   if(walker == NULL)
   {
     console("BlkList_Delete(): BlockedList is empty. Halting...\n");
     halt(1);
   }

   while(walker->next_proc_ptr != NULL && walker->pid != process->pid)
   {
     previous = walker;
     walker = walker->next_proc_ptr;
   }

   if(previous == NULL)
   {
     //BlockedList points to next item in the list.
     //The selected item's next process pointer is made NULL, disconnecting it
     //from the blocked list.
     BlockedList = walker->next_proc_ptr;
     walker->next_proc_ptr = NULL;
   }
   else
   {
     if(walker->next_proc_ptr == NULL)
       previous->next_proc_ptr = NULL;
     else
     {
       previous->next_proc_ptr = walker->next_proc_ptr;
       walker->next_proc_ptr = NULL;
     }
   }
   return;
}

static void ZprList_Delete(proc_ptr process)
{
  char *func_str = "ZprList_delete()";
  test_kernel_mode(func_str);

  proc_ptr walker, previous;
  previous = NULL;
  walker = ZapperList;

  while(walker != NULL && walker->z_pid != process->z_pid)
  {
    previous = walker;
    walker = walker->next_zapper_ptr;
  }

  if(previous == NULL)
  {
    ZapperList = walker->next_zapper_ptr;
    walker->next_zapper_ptr = NULL;
  }
  else
  {
    if(walker->next_zapper_ptr == NULL)
      previous->next_zapper_ptr = NULL;
    else
    {
      previous->next_zapper_ptr = walker->next_zapper_ptr;
      walker->next_zapper_ptr = NULL;
    }
  }
}

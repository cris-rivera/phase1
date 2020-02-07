#define DEBUG 0
#define TRUE 1
#define FALSE 0

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

struct proc_struct {
   proc_ptr       next_proc_ptr;
   proc_ptr       child_proc_ptr;
   proc_ptr       next_sibling_ptr;
   char           name[MAXNAME];     /* process's name */
   char           start_arg[MAXARG]; /* args passed to process */
   context        state;             /* current context for process */
   short          pid;               /* process id */
   int            priority;
   int (* start_func) (char *);   /* function where process begins -- launch */
   char          *stack;
   unsigned int   stacksize;
   int            status;         /* READY, BLOCKED, QUIT, etc. */
   int            exit_status;   
   int            z_status;       /* NONE, ZAPPED, ZAPPER. */
   short          z_pid;          /* pid of process zapped by this process */
   /* other fields as needed... */
};

struct psr_bits {
        unsigned int cur_mode:1;
       unsigned int cur_int_enable:1;
        unsigned int prev_mode:1;
        unsigned int prev_int_enable:1;
    unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};

enum {
  EMPTY,
  DEAD,
  BLOCKED,
  READY,
  RUNNING
} status_code;

enum {
  NONE,
  ZAPPED,
  ZAPPER
} z_status_code;

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY


#include "usloss.h"
#define DEBUG 0
extern int debugflag;

void
p1_fork(int pid)
{
  if(DEBUG && debugflag)
    console("p1_fork() called: pid = %d\n", pid);
}/* p1_switch */

void 
p1_switch(int old, int new)
{
  if(DEBUG && debugflag)
    console("p1_switch() called: pid = %d\n", pid);
}/* p1_switch */

void
p1_quit(int pid)
{
  if(DEBUG && debugflag)
    console("p1_quit() called: pid = %d\n", pid);
}/* p1_quite  */

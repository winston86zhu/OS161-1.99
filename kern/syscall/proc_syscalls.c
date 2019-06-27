#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include <array.h>
#include <synch.h>
#include <mips/trapframe.h>


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */
//Cause the current process to exit. The exit code exitcode is reported back to other process(es) 
// via the waitpid() call. The process id of the exiting process should not be reused until 
// all processes interested in collecting the exit code with waitpid have done so. 
//(What "interested" means is intentionally left vague; you should design this.)

void sys__exit(int exitcode) {
  struct addrspace *as;
  struct proc *p = curproc;
  (void) p;
  (void) exitcode;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
 


  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */

#if OPT_A2
  KASSERT(lk_proc);
  KASSERT( p != NULL);

  //lock_acquire(p->proc_lock);
  
  // If parent is waiting on this, then wake it up
  //lock_release(p->proc_lock);
 
  lock_acquire(lk_proc);
  for(int i = array_num(all_process) - 1; i > 0; i--){
  	struct proc * get_proc = array_get(all_process, i);
  	if(get_proc->parent_pid == p->pid){//I am dying, telling my alive children& deadchildren
  		if(get_proc->exit_status == true){
  			//Died Chidlren
  			proc_destroy(get_proc);
  		} else if (get_proc->exit_status == false){
  			//Orphan 
  			get_proc->parent_alive = true;
  		}
  	}
  }
  
  if(p->parent_alive == false){
  	//lock_acquire(lk_proc);
  	p->exitcode = _MKWAIT_EXIT(exitcode);
  	
  	p->exit_status = true;
  	//
  	lock_acquire(p->proc_lock);
  	cv_broadcast(p->proc_cv, p->proc_lock);
  	lock_release(p->proc_lock);
  	//lock_release(lk_proc);
  } else {
  	proc_destroy(p);
  }
  lock_release(lk_proc);
  
  
  #else
  proc_destroy(p);
  
  #endif
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}

#if OPT_A2

int sys_fork(struct trapframe *trp, pid_t * ret){
  KASSERT(curproc != NULL);
  struct proc* new_proc = proc_create_runprogram(curproc->p_name);
  struct addrspace * new_as;


  lock_acquire(lk_proc);
  int succeed = as_copy(curproc->p_addrspace, &new_as);
  lock_release(lk_proc);

  if(succeed){
    proc_destroy(new_proc);
    return succeed;
  }
  
  new_proc->p_addrspace = new_as;

  lock_acquire(lk_proc);
  /*Children trapframe => Copy from parent*/
  struct trapframe *new_trp = kmalloc(sizeof(struct trapframe));

  if (!new_trp) {
      proc_destroy(new_proc);
      return ENOMEM;
   }
   *new_trp = *trp;
  //memcpy(new_trp, trp, sizeof(struct trapframe));
  //kprintf("SSSS");
  new_proc->parent_p = curproc;
  new_proc->parent_pid = curproc->pid;

  //new_proc->alive = true;
  //new_proc->parent_alive = true;
  

  succeed = thread_fork(curthread->t_name, new_proc,(void *) enter_forked_process, 
  	(struct trapframe *)new_trp, 0);

  if(succeed){
    kfree(new_as);
    proc_destroy(new_proc);
    return EMPROC;//ENOMEM
  }

  *ret = new_proc->pid;
  lock_release(lk_proc);
  return 0;

}

#endif




/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
    KASSERT(curproc);
    *retval = curproc->pid;
#else
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }


#if OPT_A2
  /*Only a parent can call waitpid on its children*/
  
  struct proc * my_child = array_get(all_process, proc_search(all_process, pid));
  if(my_child == NULL){
  	*retval = -1;
    return ECHILD;
  }
  lock_acquire(my_child->proc_lock);
  if(!my_child->exit_status){
    cv_wait(my_child->proc_cv, my_child->proc_lock);
  }
  lock_release(my_child->proc_lock);
  //After calling MKWAITEXIT (exitcode -> exit_status)
  // exits_status is alive 
  exitstatus = my_child->exitcode;
#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);

}


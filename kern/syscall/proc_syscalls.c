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
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  #if OPT_A2
  KASSERT(lk_proc);
  KASSERT( p != NULL );
  lock_acquire(lk_proc);
  cv_broadcast(p->proc_cv, lk_proc);
  lock_release(lk_proc);

  for(unsigned int i=0; i < array_num(p->p_children); i++){
    struct proc *get_p = array_get(p->p_children,i);
    if(get_p != NULL){
      if(get_p->exit_status == false){
        get_p->parent_alive = false;
      } else {
        proc_destroy(get_p);
      }
    }

      
  }

  if(p->parent_p == NULL || p->parent_alive == false){
    proc_destroy(p); // parent already dead
  } else {
    lock_acquire(lk_proc);
    p->exit_status = true;
    p->exitcode = _MKWAIT_EXIT(exitcode);
    /*for(unsigned int i=0; i < array_num(p->parent_p->p_children); i++){

    }*/
    lock_release(lk_proc);

  }




  #else
  (void)exitcode;
  #endif



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
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}

#if OPT_A2

int sys_fork(struct trapframe *trp, pid_t * ret){
  KASSERT(curproc != NULL);

  struct proc* new_proc = proc_create_runprogram(curproc->p_name);
  struct addrspace * new_as;

  if(new_proc->pid == -1){
    proc_destroy(new_proc);
    return -1;
  }

  lock_acquire(lk_proc);
  int succeed = as_copy(curproc_getas(), &new_as);
  lock_release(lk_proc);

  if(succeed){
    proc_destroy(new_proc);
    return -1;
  }

  new_proc->p_addrspace = new_as;

  lock_acquire(lk_proc);
  /*Children trapframe => Copy from parent*/
  struct trapframe *new_trp = kmalloc(sizeof(struct trapframe));
  memcpy(new_trp, trp, sizeof(struct trapframe));
  new_proc->parent_p = curproc;
  new_proc->parent_pid = curproc->pid;
  array_add(curproc->p_children,new_proc,NULL);
  new_proc->alive = true;
  new_proc->parent_alive = true;
  lock_release(lk_proc);

  succeed = thread_fork(curthread->t_name, new_proc,(void *) enter_forked_process, new_trp, 0);
  if(succeed){
    kfree(new_as);
    proc_destroy(new_proc);
    return -1;//ENOMEM
  }

  *ret = new_proc->pid;
  return true;

}

#endif




/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
    KASSERT(curproc != NULL);
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
  int ret_val = -1;
  for(unsigned int i=0; i < array_num(curproc->p_children); i++){
    struct proc * p_iter = array_get(curproc->p_children, i);
    if(p_iter->pid == pid){
      ret_val = i;
      break;
    }   
  }
  if(ret_val == -1){
    *retval = -1;
    return ECHILD; //?
  }
  struct proc *proc_child = array_get(curproc->p_children,ret_val);
  lock_acquire(lk_proc);
  while(proc_child->exit_status == false){
    cv_wait(proc_child->proc_cv, lk_proc);
  }
  exitstatus = proc_child->exitcode;

  lock_release(lk_proc);

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


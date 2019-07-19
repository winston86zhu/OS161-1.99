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
#include <vfs.h>
#include <test.h>
#include <kern/fcntl.h>
#include "opt-A3.h"




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

#if OPT_A3
void kill_thread(int exitcode){
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
    p->exitcode = _MKWAIT_SIG(exitcode);
    
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
  

  //proc_destroy(p);
  

  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in kill_thread\n");
}

#endif

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

  lock_acquire(new_proc->proc_lock);
  /*Children trapframe => Copy from parent*/
  struct trapframe *new_trp = kmalloc(sizeof(struct trapframe));

  if (!new_trp) {
      proc_destroy(new_proc);
      return ENOMEM;
   }
   *new_trp = *trp;
  //memcpy(new_trp, trp, sizeof(struct trapframe));
  new_proc->parent_p = curproc;
  new_proc->parent_pid = curproc->pid;

  new_proc->alive = true;
  //new_proc->parent_alive = true;
  

  succeed = thread_fork(curthread->t_name, new_proc,(void *) enter_forked_process, 
  	(struct trapframe *)new_trp, 0);

  if(succeed){
    kfree(new_as);
    proc_destroy(new_proc);
    return EMPROC;//ENOMEM
  }

  *ret = new_proc->pid;
  lock_release(new_proc->proc_lock);
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

  if (options != 0) {
    return(EINVAL);
  }


#if OPT_A2
  /*Only a parent can call waitpid on its children*/
  KASSERT(all_process);
  
  struct proc * my_child = array_get(all_process, proc_search(all_process, pid));
  if(my_child == NULL){
  	*retval = -1;
    return ECHILD;
  } else {
    lock_acquire(my_child->proc_lock);
    if(!my_child->exit_status){
      cv_wait(my_child->proc_cv, my_child->proc_lock);
    }
    lock_release(my_child->proc_lock);
    //After calling MKWAITEXIT (exitcode -> exit_status)
    // exits_status is alive 
    exitstatus = my_child->exitcode;
  }
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

#if OPT_A2
int sys_execv(userptr_t pname, userptr_t in_args){

  char * name = (char *) pname;
  char ** args = (char **) in_args;
  (void) args;

  //Count the number of argument and allocate space
  int i = 0;
  while(args[i] != NULL){
    i++;
  }
  unsigned int counter = i + 1;
  unsigned int ori_counter = i;

  // Copy argument space from user args 
  char ** arg_space = kmalloc(sizeof(char *) * (counter));
  
  // Copy indivdiaul string from arg 
  for(unsigned int i = 0; i < counter - 1; i++){
    arg_space[i] = kmalloc(sizeof(char) * strlen(args[i]) + 1);
    int error = copyinstr((const_userptr_t) args[i], arg_space[i], (strlen(args[i]) + 1), NULL);
    if(error){
      for(unsigned int j = 0; j < i; j++){
        kfree(arg_space[j]);
      }
      kfree(arg_space);
      return error;
    }
  }
  arg_space[counter - 1] = NULL;

  /*
  * Copy Prog name
  */
  char * name_space = kmalloc(sizeof(char) * (strlen(name) + 1));
  int error_p = copyinstr(pname, name_space, (strlen(name) + 1), NULL);
  if(error_p){
    for(unsigned int j = 0; j < counter - 1; j++){
        kfree(arg_space[j]);
      }
    kfree(arg_space);
    kfree(name_space);
    return error_p;
  }


/*  Copy from Runprogram  */

	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

  /* Open the file. */
  result = vfs_open(name, O_RDONLY, 0, &v);
  if (result) {
    return result;
  }

  /* Create a new address space. */
  as = curproc_getas();
  as_destroy(as);
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. */
  curproc_setas(as);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    return result;
  }


  char * all_args [counter];
  (void) ori_counter;
  if(all_args == NULL){
    return ENOMEM;
  }
  all_args[counter] = NULL;
  /* Copy from user stack*/ 
  for(int i = counter - 2; i >= 0; i--){
    stackptr -= ROUNDUP(strlen(arg_space[i]) + 1, 8);
    result = copyoutstr(arg_space[i], (userptr_t)stackptr, strlen(arg_space[i]) + 1, NULL);
    if(result){
      return result;
    }
    all_args[i] = (char *) stackptr;
  }
  all_args[counter - 1] = NULL;

  stackptr -= ROUNDUP((counter) * sizeof(char *), 8);
  result = copyout(all_args, (userptr_t) stackptr, ROUNDUP((counter) * sizeof(char *), 8));
  if(result){
    for(unsigned int i = 0; i < counter - 1; i++){
      kfree(arg_space[i]);
    }
    kfree(arg_space);
    kfree(name_space);
    return ENOMEM;
  }


  for(unsigned int i = 0; i < counter - 1; i++){
    kfree(arg_space[i]);
  }
  kfree(arg_space);
  kfree(name_space);


  /* Warp to user mode. */
  enter_new_process(counter - 1 /*argc*/, (userptr_t) stackptr /*userspace addr of argv*/,
        stackptr, entrypoint);

  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;

}



#endif



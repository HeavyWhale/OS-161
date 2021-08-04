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
#ifdef OPT_A2
#include <mips/trapframe.h> // for struct trapframe
#include <synch.h>
#endif // OPT_A2

#ifdef OPT_A2
extern struct array* pid_arr;
extern struct array* pid_info_arr;
// sys_fork(tf, retval) creates a new child process with caller's running 
//   states exactly via copying caller's trapframe. This function returns
//   an exit code and modifies retval as the new child process's pid if 
//   succeed
int
sys_fork
(struct trapframe *tf, pid_t *retval) 
{
    //check_cil(curproc, "BEFORE sys_fork");
    int exitcode; // for various functions

    // Step 1: Create a new process struct for the child process
    struct proc* child_proc;
    char* child_name = kmalloc(sizeof(char) * (strlen(curproc->p_name)+3));
    if (child_name == NULL) return ENOMEM; // out of memory
    strcpy(child_name, curproc->p_name);
    strcat(child_name, "_c");
    child_proc = proc_create_runprogram(child_name); // PID is initialized inside
    if (child_proc == NULL) {
        kfree(child_name);
        return ENOMEM;
    }


    // Step 2: Create and copy AS (and data) from the parent to the child
    // Note: Child's AS will be implicitly associated inside `as_copy`
    struct addrspace* child_as;
    exitcode = as_copy(curproc_getas(), &child_as); // both creating and copying can be done at as_copy
    if (exitcode) {
        kfree(child_name);
        proc_destroy(child_proc);
        return exitcode; // pass the exitcode
    }


    // Step 3 & 4:
    //   - Associate duplicated AS with new child process
    //   - Assign a PID and create parent/child relationship
    // Note: PID assignment is done at `proc_create_runprogram` via `proc_create` from `proc.c`
    spinlock_acquire(&child_proc->p_lock); // protect update on fields of child_proc 
    child_proc->p_addrspace = child_as;
    child_proc->p_parent = curproc;
    exitcode = add_child(curproc, child_proc);
    spinlock_release(&child_proc->p_lock);
    if (exitcode) {
        as_destroy(child_proc->p_addrspace);
        kfree(child_name);
        proc_destroy(child_proc);
        return exitcode;
    }
    

    // Step 5: Create a thread for child process
    char* thread_name = kstrdup(child_name);
    if (thread_name == NULL) {
        kfree(child_name);
        as_destroy(child_proc->p_addrspace);
        proc_destroy(child_proc);
        return ENOMEM;
    }


    // Step 6: Duplicate trapframe from parent, put it onto child's heap and modify it
    struct trapframe* kern_tf = kmalloc(sizeof(struct trapframe));
    if (kern_tf == NULL) {
        kfree(child_name);
        kfree(thread_name);
        as_destroy(child_proc->p_addrspace);
        spinlock_acquire(&curproc->p_lock2);
        remove_child(curproc, child_proc);
        spinlock_release(&curproc->p_lock2);
        proc_destroy(child_proc);
        return ENOMEM;
    }
    *kern_tf = *tf; // do the copying
    exitcode = thread_fork(thread_name,
                           child_proc,
                           enter_forked_process,
                           (void*)kern_tf, 0);
    // kfree(kern_tf); ///////////////////////////////////////////////////
    if (exitcode) {
        kfree(child_name);
        kfree(thread_name);
        as_destroy(child_proc->p_addrspace);
        spinlock_acquire(&curproc->p_lock);
        remove_child(curproc, child_proc);
        spinlock_release(&curproc->p_lock);
        proc_destroy(child_proc);
        return exitcode;
    }

    //check_cil(curproc, "AFTER sys_fork");
    // return child's pid
    *retval = child_proc->pid;
    return 0;
}

#endif // OPT_A2: sys_fork 

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

#ifdef OPT_A2
    if (curproc->p_parent != NULL) {
        lock_acquire(curproc->p_parent->p_lk);
        struct proc* parent = curproc->p_parent;
        struct pinfo* pi = find_child_byAddr(parent, curproc, NULL);
        if (pi == NULL) {
            panic("sys__exit: cannot find child's info by searching parent's pil");
        }
        pi->exitcode = _MKWAIT_EXIT(exitcode);
        pi->alive = false;
        cv_broadcast(parent->p_cv, parent->p_lk); // signal the parent proc
        lock_release(curproc->p_parent->p_lk);
    }
    if (curproc->p_cil != NULL) {
        // set all children's parent to NULL
        unsigned int size = pinfolist_num(curproc->p_cil);
        for (unsigned int i = 0 ; i < size; i++) {
            struct pinfo* pi = pinfolist_get(curproc->p_cil, i);
            pi->proc->p_parent = NULL;
        }
    }
#endif // OPT_A2

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

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
  
//    check_cil(curproc, "AFTER sys__exit");

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
#ifdef OPT_A2
    *retval = curproc->pid;
#else
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
#endif // OPT_A2
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
#ifdef OPT_A2
    //check_cil(curproc, "BEFORE sys_waitpid");
    int exitcode; // for various functions
    int child_exitstatus; // for child's exit status 
    unsigned int index;
    if (options != 0) return EINVAL;

    // Step 0: Verify such child with pid exists
    struct pinfo* cinfo = find_child_byPID(curproc, pid, &index);
    if (cinfo == NULL) return ECHILD; // parent has no such child with given pid
    struct proc* child_proc = cinfo->proc;

    
    // Step 1: Validate child-parent relationship
    // Note: This should be already verified in step 0 from the parent's side,
    //       we will check from child's side again.
    if (child_proc->p_parent != curproc) {
        panic("sys_waitpid: Fetched child from curproc's cil\
               but child does not have such parent");
    }

    // Step 2: Check if child is alive or not
    if (cinfo->alive == false) {
        // child is dead, gracefully delete it
        spinlock_acquire(&curproc->p_lock2);
        child_exitstatus = cinfo->exitcode; // retrieve exitstatus from child
        pinfolist_remove(curproc->p_cil, index); // we can safely remove this child from cil
        spinlock_release(&curproc->p_lock2);
    } else {
        // child is alive, let parent wait
        lock_acquire(curproc->p_lk);
        while (cinfo->alive == true) {
            cv_wait(curproc->p_cv, curproc->p_lk);
        }
        child_exitstatus = cinfo->exitcode;
        lock_release(curproc->p_lk);
        // now child is dead (finished running)
        spinlock_acquire(&curproc->p_lock2);
        remove_child(curproc, child_proc);
        //pinfolist_remove(curproc->p_cil, index); // we can safely remove this child from cil
        if (pinfolist_num(curproc->p_cil) == 0) {
            // cil is empty
            pinfolist_destroy(curproc->p_cil);
            curproc->p_cil = NULL;
        }
        spinlock_release(&curproc->p_lock2);
    }
    exitcode = copyout((void *)&child_exitstatus,status,sizeof(int));
    if (exitcode) {
        return (exitcode);
    }
    //check_cil(curproc, "AFTER sys_waitpid");
    *retval = pid;
    return (0);
    
#else
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
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
#endif
}

void check_cil(struct proc* parent, const char* info) {
    if (!parent) return;

    kprintf("%s\n", info);
    kprintf(">>> parent:%s with pid=%d\n", parent->p_name, parent->pid);
    if (!parent->p_cil) return;
    unsigned int size = pinfolist_num(parent->p_cil);
    for (unsigned int i = 0; i < size; i++) {
        struct pinfo* cur = pinfolist_get(parent->p_cil, i);
        kprintf(">>>>>> p_cil[%d]: proc=%s, pid=%d\n", i, cur->proc->p_name, cur->proc->pid);
    }
    kprintf(">>>>>>>>>>>>>>>>>>>>>>>>>>\n");
}

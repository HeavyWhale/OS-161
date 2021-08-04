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
#if OPT_A2
#include <synch.h>
#include <mips/trapframe.h>
#include <limits.h>
#include <vfs.h>
#include <kern/fcntl.h>

void wrapper(void *data1, unsigned long data2){
  curproc->info->parent = data2;
  enter_forked_process(data1);
}

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */
int sys_fork(struct trapframe *tf, pid_t *retval){
  struct proc* np= proc_create_runprogram("thread");
  if(np == NULL){
    return ENPROC;
  }
    //struct addrspace *as = as_create();
    //if (as == NULL) {
    //  proc_destroy(np);
    //  return ENOMEM;
    //}
  as_copy(curproc_getas(), &(np->p_addrspace));
    //curproc_setas(as);
  struct trapframe *ttf = kmalloc(sizeof(struct trapframe));
  if (ttf == NULL){
    proc_destroy(np);
    return ENOMEM;      
  }
  *ttf = *tf;
  int err = thread_fork("thread", np, wrapper, ttf, curproc->pid);
  if(err) {
    proc_destroy(np);
    kfree(ttf);
    return err;
  }
  *retval = np->pid;
  return 0;
}

void cleanArr(char** arr, int len){
  for(int i = 0; i < len; i++){
    kfree(arr[i]);
  }
  kfree(arr);
}
int sys_execv(char* pname, char**argv){
  if(pname == NULL){
    return EFAULT;
  }

  char* kpname = kmalloc(sizeof(char)*(strlen(pname)+1));
  if(kpname == NULL){
    return ENOMEM;
  }
  copyinstr((userptr_t)pname, kpname, strlen(pname)+1, NULL);

  int argc = 0;
  int total = 0;
  while(argv[argc]){
    total += strlen(argv[argc]);
    argc++;
  }
  if(total > ARG_MAX){
    return E2BIG;
  }
  char** kargv = (char **)kmalloc((argc+1) * sizeof(char*));
  if(kargv == NULL){
    return ENOMEM;
  }
  for(int i = 0; i < argc; i++){
    kargv[i] = kmalloc(sizeof(char)*(strlen(argv[i])+1));
    if(kargv[i] == NULL){
      cleanArr(kargv,i);
      return ENOMEM;
    }
    copyinstr((userptr_t)argv[i], kargv[i], strlen(argv[i])+1, NULL);
  }
  kargv[argc] = NULL;


  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;
  


/* Open the file. */
  result = vfs_open(kpname, O_RDONLY, 0, &v);
  kfree(kpname);
  if (result) {
    cleanArr(kargv,argc);
    return result;
  }

  /* We should be a new process. */
  //KASSERT(curproc_getas() == NULL);

/* Create a new address space. */
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    cleanArr(kargv,argc);
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
    cleanArr(kargv,argc);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    cleanArr(kargv,argc);
    return result;
  }

  //------------------------
  while((stackptr % 8) != 0){
    stackptr--;
  }

  vaddr_t stk[argc + 1];
  for (int i = argc - 1; i >= 0; i--)
  {
    stackptr -= strlen(kargv[i]) + 1;
    copyoutstr(kargv[i], (userptr_t)stackptr, strlen(kargv[i]) + 1, NULL);
    stk[i] = stackptr;
  }

  while((stackptr % 4) != 0){
    stackptr--;
  }

  stk[argc]=0;

  for (int i = argc; i >= 0; i--)
  {
    stackptr -= ROUNDUP(sizeof(vaddr_t), 4);
    copyout(&stk[i], (userptr_t)stackptr, sizeof(vaddr_t));
  }
  cleanArr(kargv,argc);

//?as_destroy(as);
  /* Warp to user mode. */
enter_new_process(argc /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,stackptr, entrypoint);

  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;
}

#endif
void sys__exit(int exitcode) {

#if OPT_A2
  lock_acquire(lk);
  //parent info
  struct Info* pinfo = findByPid(curproc->info->parent);
  // parent not existed, parent is dead
  curproc->info->alive = false;
  if(pinfo == NULL || !pinfo->alive){
    //we can delete pid//
    clearInfo(curproc->pid);
  }else{
    //keep my info
  }
  clearZombia(curproc->pid);
  curproc->info->code = _MKWAIT_EXIT(exitcode);  
  cv_signal(cv, lk);
  lock_release(lk);
#endif

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

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
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

#if OPT_A2
  if(status == NULL){
    return EFAULT;
  }
  if(pid < 0){
    return ESRCH;
  }
  lock_acquire(lk);
  struct Info* info = findByPid(pid);
  if(info == NULL){
    result =  ESRCH;
  }else if(info->parent != curproc->pid){
    result = ECHILD;
  }else{
    if(info->alive){
      cv_wait(cv, lk);
    }
    exitstatus = info->code;
    result = 0;
  }
  lock_release(lk);
  if(result){
    return result;
  }
  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  result = copyout((void *)&exitstatus,status,sizeof(int));
  *retval = pid;
  return result;

#else

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


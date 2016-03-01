/*
 * Process-related syscalls.
 * New for ASST1.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <pid.h>
#include <machine/trapframe.h>
#include <syscall.h>
#include <kern/wait.h>

/*
 * sys_fork
 * 
 * create a new process, which begins executing in md_forkentry().
 */


int
sys_fork(struct trapframe *tf, pid_t *retval)
{
	struct trapframe *ntf; /* new trapframe, copy of tf */
	int result;

	/*
	 * Copy the trapframe to the heap, because we might return to
	 * userlevel and make another syscall (changing the trapframe)
	 * before the child runs. The child will free the copy.
	 */

	ntf = kmalloc(sizeof(struct trapframe));
	if (ntf==NULL) {
		return ENOMEM;
	}
	*ntf = *tf; /* copy the trapframe */

	result = thread_fork(curthread->t_name, enter_forked_process, 
			     ntf, 0, retval);
	if (result) {
		kfree(ntf);
		return result;
	}

	return 0;
}

/*
 * sys_getpid
 * Placeholder to remind you to implement this.
 */

pid_t
sys_getpid()
{
	return curthread->t_pid; 
}
	

/*
 * sys_waitpid
 * Placeholder comment to remind you to implement this.
 */

pid_t 
sys_waitpid(pid_t pid, int *status, int options)
{
    if (options != 0 && options != WNOHANG) {
	return -EINVAL;
    }
    if (status == NULL) {
	return -EFAULT;
    }
	// ESRCH handled in pid_join
	/*
	if (!pi_get(pid)->pi_ppid == sys_getpid()) {
		return ECHILD;
    }
    */
    // checks if pid is a child of current process
    if (!is_parent(pid, curthread->t_pid)) {
        return -ECHILD;
    }

    // cannot wait for ourselves
    if (pid == curthread->t_pid) {
        return -ECHILD;
    }
    // WNOHANG to not wait for process, options to wait
    int ret = pid_join(pid, status, options);
    
    //kprintf("status: %d\n", *status);
    //kprintf("ret: %d\n", ret);
    return ret;
}

/*
 * sys_kill
 * Placeholder comment to remind you to implement this.
 */
int
sys_kill(pid_t pid, int sig)
{
	// implement 1, 2, 9, 15, 17, 19, 28, 29
	// if sig is 0, then no signal is sent but error checking still occurs
	// returns 0 on success, -1 on error and errno is set appropriately
	// check for EINVAL, EUNIMP, ESRCH
	int implemented[9] = {0, 1, 2, 9, 15, 17, 19, 28, 29};
	bool valid = false;
	if (sig < 0 || sig > 31) {
		return EINVAL;
	}
	for (int i = 0; i < sizeof(implemented) / sizeof(implemented[0]; i++) {
		if (implemented[i] == sig) 
			valid = true;
	}
	if (!valid) {
		return EUNIMP;
	}

	if (!in_table(pid)) {
		return ESRCH;
	}
	// needs to set the flag of pid's pidinfo struct to sig
	set_flag(pid, sig);
}

	

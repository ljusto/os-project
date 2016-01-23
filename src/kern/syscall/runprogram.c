/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <copyinout.h>
/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, unsigned long nargs, char **args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}
	/* We should be a new thread. */
	KASSERT(curthread->t_addrspace == NULL);

	/* Create a new address space. */
	curthread->t_addrspace = as_create();
	if (curthread->t_addrspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_addrspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		return result;
	}
	int i;
	int total_len = 0;
	int size_args = (nargs + 1) * 4;
	/* Copy args into the user stack */
	kprintf("before \n");
	for (i = nargs; i > 0; i--) {
	  /* Calulate length of arg[i] */
	  int len_arg = sizeof(args[i - 1]);
	  kprintf("%d length of arg \n", len_arg);
	  /* calulate the amount of padding we need */
	  int remainder = (4 - (len_arg % 4)) % 4;
	  kprintf("%d remainder \n", remainder);
	  int j = 0;
	  char end[remainder];
	  while (j < remainder) {
            end[j] = '\0';
	    j++;
	  }
	  total_len += len_arg + remainder;
	  /*args[i - 1] = (char *) (stackptr - total_len);*/
	  copyout((void *)args[i - 1], (userptr_t)(stackptr - total_len), len_arg);
	  copyout((void *)end, (userptr_t)(stackptr - total_len + len_arg), remainder);
	}
	kprintf("after \n");
	/* need to do the padding for the args aswell */
   	
   	/* trying to align the last item on the stack */
    int len_array = sizeof(args);
    int array_remainder = (4 - (len_array % 4)) % 4;
    char arrgs_end[array_remainder];
    int r_counter = 0;
    while (r_counter < array_remainder) {
   		arrgs_end[r_counter] = '\0';
	    	r_counter++;
	  }
	  

 
	copyout(args, (userptr_t) (stackptr - total_len - len_array - array_remainder), size_args); 
   
	copyout(arrgs_end, (userptr_t) (stackptr - total_len - len_array), array_remainder);


	kprintf("add \n"); 
	/* Warp to user mode. */
	enter_new_process(nargs /*argc*/, (userptr_t) (stackptr - total_len - len_array - array_remainder) /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}


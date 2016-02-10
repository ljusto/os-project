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

	/* getting the memory space for number of arguments including the null space at the end */
	char **argv = (char **) kmalloc(sizeof(char *) * (nargs + 1));
        unsigned int i;
	for (i = 0; i < nargs; i++) 
	{
  		char * arg;
  	  	/* Calulate length of arg[i] */
	  	int len_arg = strlen(args[i]) + 1;
  		/* calculate number of byte we need to pad to the end of each argument "multiple of 4" */
  		int remainder = (4 - (len_arg % 4)) % 4;
  		/* setting up the memory for the argument (after padding) */
  		arg = kmalloc((len_arg + remainder) * sizeof(char));
  		int j = 0;
  		/* case we need add null for padding */
  		while (j < len_arg + remainder) {
			if (j >= len_arg) {
				arg[j] = '\0';
				j++;
			}
			/* other wise just add the character to the argument*/
			else {
				arg[j] = args[i][j];
				j++;
			}
		}
    	/* every time we add the argument to stack, we set the stackptr to store that argument*/
  		stackptr -= len_arg + remainder;
  		/* copy the argument to the userspace using stackptr*/
  		result = copyout((const void *)arg, (userptr_t) stackptr,
		(size_t) (len_arg + remainder));
    	if (result) {
	    	kprintf("runprogram - copyout error %d\n",result);
	    	return result;
		}
        kfree(arg);
        argv[i] = (char *) stackptr;
    }


	argv[nargs] = NULL;
	if(args[nargs] == NULL){
		stackptr -= 4 * sizeof(char);
	}
	/* set the stackptr to store the list of arguments */
		stackptr -= sizeof(char *) * (nargs + 1);
		/* copy the list  of arguments to the userspace using stackptr*/
		result = copyout((const void *) (argv), (userptr_t) stackptr,
		sizeof(char *) * (nargs + 1));
		if(result) {
			kprintf("runprogram - copyout error %d\n",result);
			return result;
		}
		kfree(argv);
	/* Warp to user mode. */
	/* after stackptr is setup in the user space, enter the new process*/
	enter_new_process((int) nargs /*argc*/, (userptr_t) stackptr /*userspace addr of argv*/,
			  		stackptr, entrypoint);
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}


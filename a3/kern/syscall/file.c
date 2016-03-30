/* BEGIN A3 SETUP */
/*
 * File handles and file tables.
 * New for ASST3
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/unistd.h>
#include <file.h>
#include <syscall.h>

/*** openfile functions ***/

/*
 * file_open
 * opens a file, places it in the filetable, sets RETFD to the file
 * descriptor. the pointer arguments must be kernel pointers.
 * NOTE -- the passed in filename must be a mutable string.
 * 
 * A3: As per the OS/161 man page for open(), you do not need 
 * to do anything with the "mode" argument.
 */
int
file_open(char *filename, int flags, int mode, int *retfd)
{
	// open file, vnode will be in vn
	if (filename || filename == NULL) {
		printf("null pointer");
		return -1;
	}
	if (flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR) {
		printf("invalid flag");
		return -1;
	}

	// find the first available index in file table
	int i = 0;
	struct filetable *ft = curthread->t_filetable;
	while (ft[i] != NULL && i < 10 /*size of file table*/) {
		i++;
	}
	struct vnode vn;
	vfs_open(filename, flags, mode, &vn);
	// create file table entry for this file
	struct fd_entry new;
	new->flags = 0;
	new->fd_lock = // make lock?
	new->vnode = vn;
	new->num_connected = 1;
	new->file_offset = 0;
	curthread->t_filetable[i] = vn;
	return 0;


	// place file table entry into the file table
	// update retfd to have the index of the filetable entry in the file table 

}


int file_dup2(int oldfd, int newfd, int *retval){
	struct filetable *ftable = curthread->t_filetable;
	// oldfd is not a valid file handle, or newfd is a value that 
	// cannot be a valid file handle.
	if (newfd >= __OPEN_MAX || newfd < 0 ||oldfd >= __OPEN_MAX || oldfd < 0){
		return EBADF;
	}
	// if oldfd is equal to newfd then there is no need to change it
	if (oldfd==newfd){
		*retval = newfd;
		return 0;
	}
	// if file is already open, close it
	if (ftable->entries[newfd] != NULL){
		file_close(newfd);
	}
	// if oldfd is not a valid file handle
	if (ftable->entries[oldfd] == NULL){
		return EBADF;
	}
	lock_acquire(ftable->entries[oldfd]->fd_lock);
	// one more process is linked
	ftable->entries[oldfd]->num_connected++;
	// dup
	ftable->entries[newfd] = ftable->entries[oldfd];
	lock_release(ftable->entries[oldfd]->fd_lock);
	*retval = newfd;
	return 0;
	}
/* 
 * file_close
 * Called when a process closes a file descriptor.  Think about how you plan
 * to handle fork, and what (if anything) is shared between parent/child after
 * fork.  Your design decisions will affect what you should do for close.
 */
int
file_close(int fd)
{
	filetable_entry *fte = curthread->t_filetable[fd];
	if (!lock_do_i_hold(fte->sync_lock)) {
		lock_acquire(fte->sync_lock);
	}
	if (fte->num_connected == 1) {
		vfs_close(fte->file);
		curthread->t_filetable[fd] = NULL;
	} else {
		fte->num_connected = fte->num_connected - 1;
	}
	return 0;
}


/*** filetable functions ***/

/* 
 * filetable_init
 * pretty straightforward -- allocate the space, set up 
 * first 3 file descriptors for stdin, stdout and stderr,
 * and initialize all other entries to NULL.
 * 
 * Should set curthread->t_filetable to point to the
 * newly-initialized filetable.
 * 
 * Should return non-zero error code on failure.  Currently
 * does nothing but returns success so that loading a user
 * program will succeed even if you haven't written the
 * filetable initialization yet.
 */

int
filetable_init(void)
{
	int result;
	int fd;
	int MAX_FILENAME_SIZE = 64;
	char file_name[MAX_FILENAME_SIZE]; 

	// allocate memory for file table (for this thread)
	curthread->t_filetable = kmalloc(sizeof(struct filetable));

	// if no memory, return ENOMEM
	if (curthread->t_filetable == NULL){
		return ENOMEM;
	}

	// init the filetable, setting each element to null pointer
	for(int i = 0; i < __OPEN_MAX; i++){
		curthread->t_filetable->entries[i]=NULL;
	}
	// open file descriptor 0 -- stdin
	strcpy(file_name, "con:");
	if (result = file_open(file_name, O_RDONLY, 0, &fd)){
		return result;
	}

	// open file descriptor 1 -- stdout
	strcpy(file_name, "con:");
	if (result = file_open(file_name, O_WRONLY, 0, &fd)){
		return result;
	}

	// open file descriptor 2 -- stderr
	strcpy(file_name, "con:");
	if (result = file_open(file_name, O_WRONLY, 0, &fd)){
		return result;
	}
	return 0;
}
		

/*
 * filetable_destroy
 * closes the files in the file table, frees the table.
 * This should be called as part of cleaning up a process (after kill
 * or exit).
 */
void
filetable_destroy(struct filetable *ft)
{
    int result;
   for (size_t i = 0;i < __OPEN_MAX; i++) {
		/* code */
		if (ft->entries[i]) {
			result = file_close(i);
			KASSERT(result == 0);
			}
	}
	kfree(ft);
}	


/* 
 * You should add additional filetable utility functions here as needed
 * to support the system calls.  For example, given a file descriptor
 * you will want some sort of lookup function that will check if the fd is 
 * valid and return the associated vnode (and possibly other information like
 * the current file position) associated with that open file.
 */


/* END A3 SETUP */

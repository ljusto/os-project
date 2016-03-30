/* BEGIN A3 SETUP */
/*
 * Declarations for file handle and file table management.
 * New for A3.
 */

#ifndef _FILE_H_
#define _FILE_H_

#include <kern/limits.h>

struct vnode;

// fd object stores all the info
struct fd_entry {
	const char *fname; /* filename */
	struct vnode *vn; /* file in question */
	int flags; /* ??? */
	int num_connected; /* number of processes linked for R/W to this fd_entry */
	struct lock *fd_lock; /* for file synchronization */
	off_t file_offset;  /* so when we do another read, we know where we left off */
};
/*
 * filetable struct
 * just an array, nice and simple.  
 * It is up to you to design what goes into the array.  The current
 * array of ints is just intended to make the compiler happy.
 */

struct filetable {
struct fd_entry *entries[__OPEN_MAX];
};
/* these all have an implicit arg of the curthread's filetable */
int filetable_init(void);
void filetable_destroy(struct filetable *ft);


/* opens a file (must be kernel pointers in the args) */
int file_open(char *filename, int flags, int mode, int *retfd);
/* dup2 clones the file handle oldfd onto the file handle newfd. 
   If newfd names an already-open file, that file is closed.
*/ 
int file_dup2(int oldfd, int newfd, int *retval);
/* closes a file */
int file_close(int fd);

/* A3: You should add additional functions that operate on
 * the filetable to help implement some of the filetable-related
 * system calls.
 */

#endif /* _FILE_H_ */

/* END A3 SETUP */

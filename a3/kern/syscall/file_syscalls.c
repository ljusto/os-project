/* BEGIN A3 SETUP */
/* This file existed for A1 and A2, but has been completely replaced for A3.
 * We have kept the dumb versions of sys_read and sys_write to support early
 * testing, but they should be replaced with proper implementations that 
 * use your open file table to find the correct vnode given a file descriptor
 * number.  All the "dumb console I/O" code should be deleted.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <copyinout.h>
#include <synch.h>
#include <file.h> 

/* This special-case global variable for the console vnode should be deleted 
 * when you have a proper open file table implementation.
 */
struct vnode *cons_vnode=NULL; 

/* This function should be deleted, including the call in main.c, when you
 * have proper initialization of the first 3 file descriptors in your 
 * open file table implementation.
 * You may find it useful as an example of how to get a vnode for the 
 * console device.
 */
void dumb_consoleIO_bootstrap()
{
  int result;
  char path[5];

  /* The path passed to vfs_open must be mutable.
   * vfs_open may modify it.
   */

  strcpy(path, "con:");
  result = vfs_open(path, O_RDWR, 0, &cons_vnode);

  if (result) {
     // Tough one... if there's no console, there's not
     // * much point printing a warning...
     // * but maybe the bootstrap was just called in the wrong place
     
    kprintf("Warning: could not initialize console vnode\n");
    kprintf("User programs will not be able to read/write\n");
    cons_vnode = NULL;
  }
}

/*
 * mk_useruio
 * sets up the uio for a USERSPACE transfer. 
 */
static
void
mk_useruio(struct iovec *iov, struct uio *u, userptr_t buf, 
	   size_t len, off_t offset, enum uio_rw rw)
{

	iov->iov_ubase = buf;
	iov->iov_len = len;
	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = offset;
	u->uio_resid = len;
	u->uio_segflg = UIO_USERSPACE;
	u->uio_rw = rw;
	u->uio_space = curthread->t_addrspace;
}

/*
 * sys_open
 * just copies in the filename, then passes work to file_open.
 * You have to write file_open.
 * 
 */
int
sys_open(userptr_t filename, int flags, int mode, int *retval)
{
	char *fname;
	int result;

	if ( (fname = (char *)kmalloc(__PATH_MAX)) == NULL) {
		return ENOMEM;
	}

	result = copyinstr(filename, fname, __PATH_MAX, NULL);
	if (result) {
		kfree(fname);
		return result;
	}

	result =  file_open(fname, flags, mode, retval);
	kfree(fname);
	return result;
}

/* 
 * sys_close
 * You have to write file_close.
 */
int
sys_close(int fd)
{
	return file_close(fd);
}

/* 
 * sys_dup2
 * 
 */
int
sys_dup2(int oldfd, int newfd, int *retval)
{
        int result;
		result = file_dup2(oldfd, newfd, retval);
	if (result) {

		return result;
		}
	return 0;
}

/*
 * sys_read
 * calls VOP_READ.
 * 
 * A3: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
	struct uio user_uio;
	struct iovec user_iov;
	struct fd_entry *file;	
	int result;
	int offset = 0;

	/* Make sure we were able to init the cons_vnode */
	if (cons_vnode == NULL) {
	  return ENODEV;
	}

	/* better be a valid file descriptor */
	/* Right now, only stdin (0), stdout (1) and stderr (2)
	 * are supported, and they can't be redirected to a file
	 */
	if (fd < 0 || fd > __OPEN_MAX - 1) {
	  return EBADF;
	}

	file = curthread->t_filetable->entries[fd];
	if (file == NULL){
		return EBADF;
  	}
	lock_acquire(curthread->t_filetable->entries[fd]->fd_lock);
	/* set up a uio with the buffer, its size, and the current offset */
	mk_useruio(&user_iov, &user_uio, buf, size, offset, UIO_READ);

	/* does the read */
	result = VOP_READ(cons_vnode, &user_uio);
	if (result) {
		lock_release(file->fd_lock);
		return result;
	}
	file->file_offset = user_uio.uio_offset;
	lock_release(curthread->t_filetable->entries[fd]->fd_lock);
	/*
	 * The amount read is the size of the buffer originally, minus
	 * how much is left in it.
	 */
	*retval = size - user_uio.uio_resid;

	return 0;
}

/*
 * sys_write
 * calls VOP_WRITE.
 *
 * A3: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */

int
sys_write(int fd, userptr_t buf, size_t len, int *retval) 
{
        struct uio user_uio;
        struct iovec user_iov;
        int result;
        int offset = 0;
        struct fd_entry *file;	
        /* Make sure we were able to init the cons_vnode */
        if (cons_vnode == NULL) {
          return ENODEV;
        }
        file = curthread->t_filetable->entries[fd];

        lock_acquire(file->fd_lock);
        /* Right now, only stdin (0), stdout (1) and stderr (2)
         * are supported, and they can't be redirected to a file
         */
        if (fd < 0 || fd > __OPEN_MAX) {
          return EBADF;
        }
       
        /* set up a uio with the buffer, its size, and the current offset */
        mk_useruio(&user_iov, &user_uio, buf, len, offset, UIO_WRITE);

        /* does the write */
        result = VOP_WRITE(file->vn, &user_uio);
        if (result) {
        	lock_release(file->fd_lock);
            return result;
        }
        	file->file_offset = user_uio.uio_offset;
 			lock_release(file->fd_lock);

        /*
         * the amount written is the size of the buffer originally,
         * minus how much is left in it.
         */
        *retval = len - user_uio.uio_resid;

        return 0;
}

/*
 * sys_lseek
 * 
 */
int
sys_lseek(int fd, off_t offset, int whence, off_t *retval)
{
	struct fd_entry *file;
	struct stat stat_info;
	int result;
	// Not valid
	if (fd < 0 || fd >= __OPEN_MAX) {
		return EBADF;
	}

	file = curthread->t_filetable->entries[fd];
	if (file) {
		return EBADF;
	}
	lock_acquire(file->fd_lock);
	switch(whence) {
		// Case SEEK_SET: the new position is offset. 
		case SEEK_SET:
			*retval = offset;
			break;
		// Case SEEK_CUR: the new position is the current position plus offset. 
		case SEEK_CUR:
			*retval = offset + file->file_offset;
			break;
		// Case SEEK_END: the new position is the position of end-of-file plus offset
		case SEEK_END:
			result = VOP_STAT(file->vn, &stat_info);
			if (result) {
				lock_release(file->fd_lock);
				return result;
			}
			*retval = offset + stat_info.st_size;
			break;
	// DEFAULT: lseek fails 
	default:
		lock_release(file->fd_lock);
		return EINVAL;
	}
	// Positions less than 0 are invalid
	if (*retval < 0) {
		lock_release(file->fd_lock);
		return EINVAL;
	}
	// Does the seeking
	result = VOP_TRYSEEK(file->vn, *retval);
	if (result) {
		lock_release(file->fd_lock);
		return result;
	}
	file->file_offset = *retval;
	lock_release(file->fd_lock);
	return 0;
}




/* really not "file" calls, per se, but might as well put it here */

/*
 * sys_chdir
 * 
 */
int
sys_chdir(userptr_t path)
{
	char kpath[__PATH_MAX];
	int result;
	// Copy in the path 
	result = copyinstr(path, kpath, __PATH_MAX, NULL);
	if (result) {
		return result;
	}
	// Send it to vfs_chdir
	return vfs_chdir(kpath);
}

/*
 * sys___getcwd
 * 
 */
int
sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{
	struct uio userio;
	struct iovec iov;
	int result;
	mk_useruio(&iov, &userio, buf, buflen, 0, UIO_READ);
	result = vfs_getcwd(&userio);
	if (result) {
		return result;
	}
	*retval = buflen - userio.uio_resid;
	return 0;
}

/*
 * sys_fstat
 */
int
sys_fstat(int fd, userptr_t statptr)
{
    struct stat kstat;
	struct filetable *ft;
	ft = curthread->t_filetable;
	int result;
	// Validate the fd 
	if (fd <0 || fd >= __OPEN_MAX) {
		return EBADF;
	}
	// Cannot find file 
	if (ft->entries[fd] == NULL) {
		return EBADF;
	}
	// Invalid StatPtr
	if (!statptr) {
		return EFAULT;
	}
	result = VOP_STAT(ft->entries[fd]->vn, &kstat);
	if (result) {
		return result;
	}
	result = copyout(&kstat, statptr, sizeof(struct stat));
	return result;
}

/*
 * sys_getdirentry
 */
int
sys_getdirentry(int fd, userptr_t buf, size_t buflen, int *retval)
{      
	int result;
	struct uio Uio;
	struct iovec user_iov;
	if (fd < 0 || fd > __OPEN_MAX-1){
		return EBADF;
	}
	//check if the fd is valid
	if (curthread->t_filetable->entries[fd] == NULL){
		return EBADF;
	}
	//doing the reading
	lock_acquire(curthread->t_filetable->entries[fd]->fd_lock);
	/* set up a uio with the buffer, its size, and the current offset */
	mk_useruio(&user_iov, &Uio, buf, buflen, curthread->t_filetable->entries[fd]->file_offset, UIO_READ);
	//does the reading
	result = VOP_GETDIRENTRY(curthread->t_filetable->entries[fd]->vn, &Uio);
	if (result) {
		lock_release(curthread->t_filetable->entries[fd]->fd_lock);
		return result;
	}
	//get the valid result, then returned from file_read, return it
	curthread->t_filetable->entries[fd]->file_offset = Uio.uio_offset;
	lock_release(curthread->t_filetable->entries[fd]->fd_lock);
	*retval = buflen - Uio.uio_resid;
	return 0;
	}

/* END A3 SETUP */





/* This file use for NCTU OSDI course */


// It's handel the file system APIs 
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <fs.h>
#include <kernel/fs/fat/ff.h>

extern struct fs_fd fd_table[FS_FD_MAX];

/*TODO: Lab7, file I/O system call interface.*/
/*Note: Here you need handle the file system call from user.
 *       1. When user open a new file, you can use the fd_new() to alloc a file object(struct fs_fd)
 *       2. When user R/W or seek the file, use the fd_get() to get file object.
 *       3. After get file object call file_* functions into VFS level
 *       4. Update the file objet's position or size when user R/W or seek the file.
 *          (You can find the useful marco in ff.h)
 *       5. Remember to use fd_put() to put file object back after user R/W, seek or close the file.
 *       6. Handle the error code, for example, if user call open() but no fd slot can be use,
 *          sys_open should return -STATUS_ENOSPC.
 *
 *  Call flow example:
 *        ┌──────────────┐
 *        │     open     │
 *        └──────────────┘
 *               ↓
 *        ╔══════════════╗
 *   ==>  ║   sys_open   ║  file I/O system call interface
 *        ╚══════════════╝
 *               ↓
 *        ┌──────────────┐
 *        │  file_open   │  VFS level file API
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │   fat_open   │  fat level file operator
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │    f_open    │  FAT File System Module
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │    diskio    │  low level file operator
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │     disk     │  simple ATA disk dirver
 *        └──────────────┘
 */

// Below is POSIX like I/O system call 
int sys_open(const char *file, int flags, int mode)
{
	//We dont care the mode.
	/* TODO */
	int fd = fd_new();
	if(fd==-1) return -1;
	int retVal = file_open(&fd_table[fd],file,flags);
	if( retVal<0 ) {
		sys_close(fd);
		return retVal;
	}
	fd_table[fd].size = f_size((FIL*)fd_table[fd].data);
	fd_table[fd].pos = 0;
	return fd;
}

int sys_close(int fd)
{
	/* TODO */
	int ret = 0;

	if ( fd<0 || fd>=FS_FD_MAX )
		return -STATUS_EINVAL;

	if(fd_table[fd].ref_count==1){
		fd_table[fd].size=0;
		fd_table[fd].pos=0;
		ret=file_close(&fd_table[fd]);
	}
	fd_put(&fd_table[fd]);
	return ret;
}
int sys_read(int fd, void *buf, size_t len)
{
	/* TODO */
	struct fs_fd *filedes;
	int ret,count;

	if( len<0 || !buf )
		return -STATUS_EINVAL;

	if( !(filedes=fd_get(fd)) )
		return -STATUS_EBADF;
	
	count=filedes->size-filedes->pos;
	if( count>len ) count=len;
	ret=file_read(filedes, buf, count);
	if( ret>0 ) filedes->pos+=ret;
	
	fd_put(filedes);

	return ret;
}
int sys_write(int fd, const void *buf, size_t len)
{
	/* TODO */
	struct fs_fd *filedes;
	int ret;

	if( len<0 || !buf )
		return -STATUS_EINVAL;

	if( !(filedes=fd_get(fd)) )
		return -STATUS_EBADF;	

	ret=file_write(filedes, buf, len);
	if(ret>0) filedes->pos+=ret;
	filedes->size=f_size((FIL*)filedes->data);
	
	fd_put(filedes);
	
	return ret;

}
/* Note: Check the whence parameter and calcuate the new offset value before do file_seek() */
off_t sys_lseek(int fd, off_t offset, int whence)
{
	/* TODO */
	struct fs_fd *filedes;
	int ret,new_pos;

	if( offset<0 )
		return -STATUS_EINVAL;

	if( !(filedes=fd_get(fd)) )
		return -STATUS_EBADF;

	switch(whence){
		case SEEK_SET:
			new_pos = offset;
			break;
		case SEEK_CUR:
			new_pos = filedes->pos+offset;
			break;
		case SEEK_END:
			new_pos = filedes->size+offset;
			break;
		default:
			fd_put(filedes);
			return -STATUS_EINVAL;
	}
	filedes->pos = new_pos;
	if(!(ret=file_lseek(&fd_table[fd], new_pos)))
		ret=new_pos;
	fd_put(filedes);
	
	return ret;
}

int sys_unlink(const char *pathname)
{
	/* TODO */
	return file_unlink(pathname);
}

int sys_list(const char *pathname)
{
	return file_list(pathname);
}

int sys_mkdir(const char *pathname)
{
	return file_mkdir(pathname);
}

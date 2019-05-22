/* This file use for NCTU OSDI course */
/* It's contants file operator's wapper API */
#include <fs.h>
#include <fat/ff.h>
#include <inc/string.h>
#include <inc/stdio.h>

/* Static file objects */
FIL file_objs[FS_FD_MAX];

/* Static file system object */
FATFS fat;

/* It file object table */
struct fs_fd fd_table[FS_FD_MAX];

/* File system operator, define in fs_ops.c */
extern struct fs_ops elmfat_ops; //We use only one file system...

/* File system object, it record the operator and file system object(FATFS) */
struct fs_dev fat_fs = {
    .dev_id = 1, //In this lab we only use second IDE disk
    .path = {0}, // Not yet mount to any path
    .ops = &elmfat_ops,
    .data = &fat
};

/*TODO: Lab7, VFS level file API.
 *  This is a virtualize layer. Please use the function pointer
 *  under struct fs_ops to call next level functions.
 *
 *  Call flow example:
 *        ┌──────────────┐
 *        │     open     │
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │   sys_open   │  file I/O system call interface
 *        └──────────────┘
 *               ↓
 *        ╔══════════════╗
 *   ==>  ║  file_open   ║  VFS level file API
 *        ╚══════════════╝
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

int fr2status(int fret){
	int ret = 1;
	switch(fret){
		/* (0) Succeeded */
		case FR_OK:
			ret = STATUS_OK;
			break;

		/* (1) A hard error occurred in the low level disk I/O layer */
		case FR_DISK_ERR:
			break;

		/* (2) Assertion failed */
		case FR_INT_ERR:
			break;

		/* (3) The physical drive cannot work */
		case FR_NOT_READY:
			ret = STATUS_EIO;
			break;

		/* (4) Could not find the file */
		case FR_NO_FILE:
			ret = STATUS_ENOENT;
			break;

		/* (5) Could not find the path */
		case FR_NO_PATH:
			break;

		/* (6) The path name format is invalid */
		case FR_INVALID_NAME:
			ret = STATUS_ENOENT;
			break;

		/* (7) Access denied due to prohibited access or directory full */
		case FR_DENIED:
			break;

		/* (8) Access denied due to prohibited access */
		case FR_EXIST:
			ret = STATUS_EEXIST;
			break;

		/* (9) The file/directory object is invalid */
		case FR_INVALID_OBJECT:
			ret = STATUS_ENXIO;
			break;

		/* (10) The physical drive is write protected */
		case FR_WRITE_PROTECTED:
			ret = STATUS_EROFS;
			break;

		/* (11) The logical drive number is invalid */
		case FR_INVALID_DRIVE:
			ret = STATUS_EBADF;
			break;

		/* (12) The volume has no work area */
		case FR_NOT_ENABLED:
			ret = STATUS_ENOSPC;
			break;

		/* (13) There is no valid FAT volume */
		case FR_NO_FILESYSTEM:
			ret = STATUS_ENODEV;
			break;

		/* (14) The f_mkfs() aborted due to any parameter error */
		case FR_MKFS_ABORTED:
			break;

		/* (15) Could not get a grant to access the volume within defined period */
		case FR_TIMEOUT:
			break;

		/* (16) The operation is rejected according to the file sharing policy */
		case FR_LOCKED:
			break;

		/* (17) LFN working buffer could not be allocated */
		case FR_NOT_ENOUGH_CORE:
			break;

		/* (18) Number of open files > _FS_LOCK */
		case FR_TOO_MANY_OPEN_FILES:
			ret = STATUS_EBUSY;
			break;

		/* (19) Given parameter is invalid */
		case FR_INVALID_PARAMETER:
			ret = STATUS_EINVAL;
			break;
	}
	return ret;
}

int fs_init()
{
    int res, i;

    /* Initial fd_tables */
    for (i = 0; i < FS_FD_MAX; i++)
    {
        fd_table[i].flags = 0;
        fd_table[i].size = 0;
        fd_table[i].pos = 0;
        fd_table[i].type = 0;
        fd_table[i].ref_count = 0;
        fd_table[i].data = &file_objs[i];
        fd_table[i].fs = &fat_fs;
    }

    /* Mount fat file system at "/" */
    /* Check need mkfs or not */
    if ((res = fs_mount("elmfat", "/", NULL)) != 0)
    {
        fat_fs.ops->mkfs("elmfat");
        res = fs_mount("elmfat", "/", NULL);
        return res;
    }
    return -STATUS_EIO;
}

/** Mount a file system by path 
*  Note: You need compare the device_name with fat_fs.ops->dev_name and find the file system operator
*        then call ops->mount().
*
*  @param data: If you have mutilple file system it can be use for pass the file system object pointer save in fat_fs->data
*/
int fs_mount(const char* device_name, const char* path, const void* data)
{
	if(strcmp(device_name,fat_fs.ops->dev_name)) return -STATUS_EIO;
	strcpy(fat_fs.path, path);
	int ret = fat_fs.ops->mount(&fat_fs, data);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}

/* Note: Before call ops->open() you may copy the path and flags parameters into fd object structure */
int file_open(struct fs_fd* fd, const char *path, int flags)
{
	fd->flags = flags;
	strcpy(fd->path,path);
	int ret = fat_fs.ops->open(fd);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}

int file_read(struct fs_fd* fd, void *buf, size_t len)
{
	int ret = fat_fs.ops->read(fd,buf,len);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}

int file_write(struct fs_fd* fd, const void *buf, size_t len)
{
	int ret = fat_fs.ops->write(fd,buf,len);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}

int file_close(struct fs_fd* fd)
{
	int ret = fat_fs.ops->close(fd);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}

int file_lseek(struct fs_fd* fd, off_t offset)
{
	int ret = fat_fs.ops->lseek(fd, offset);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}
int file_unlink(const char *path)
{
	int ret = fat_fs.ops->unlink(0,path);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}
int file_list(const char *path)
{
	int ret = fat_fs.ops->list(path);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}
int file_mkdir(const char *path)
{
	int ret = fat_fs.ops->mkdir(path);
	if( ret<0 ) return -fr2status(-ret);
	return ret;
}
/**
 * @ingroup Fd
 * This function will allocate a file descriptor.
 *
 * @return -1 on failed or the allocated file descriptor.
 */
int fd_new(void)
{
	struct fs_fd* d;
	int idx;

	/* find an empty fd entry */
	for (idx = 0; idx < FS_FD_MAX && fd_table[idx].ref_count > 0; idx++);


	/* can't find an empty fd entry */
	if (idx == FS_FD_MAX)
	{
		idx = -1;
		goto __result;
	}

	d = &(fd_table[idx]);
	d->ref_count = 1;

__result:
	return idx;
}

/**
 * @ingroup Fd
 *
 * This function will return a file descriptor structure according to file
 * descriptor.
 *
 * @return NULL on on this file descriptor or the file descriptor structure
 * pointer.
 */
struct fs_fd* fd_get(int fd)
{
	struct fs_fd* d;

	if ( fd < 0 || fd > FS_FD_MAX ) return NULL;

	d = &fd_table[fd];

	/* increase the reference count */
	d->ref_count ++;

	return d;
}

/**
 * @ingroup Fd
 *
 * This function will put the file descriptor.
 */
void fd_put(struct fs_fd* fd)
{

	fd->ref_count --;

	/* clear this fd entry */
	if ( fd->ref_count == 0 )
	{
		//memset(fd, 0, sizeof(struct fs_fd));
		memset(fd->data, 0, sizeof(FIL));
	}
}



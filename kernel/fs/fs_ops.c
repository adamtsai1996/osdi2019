/* This file use for NCTU OSDI course */
/* It's contants fat file system operators */

#include <inc/stdio.h>
#include <fs.h>
#include <fat/ff.h>
#include <diskio.h>

extern struct fs_dev fat_fs;

/*TODO: Lab7, fat level file operator.
 *       Implement below functions to support basic file system operators by using the elmfat's API(f_xxx).
 *       Reference: http://elm-chan.org/fsw/ff/00index_e.html (or under doc directory (doc/00index_e.html))
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
 *        ┌──────────────┐
 *        │  file_open   │  VFS level file API
 *        └──────────────┘
 *               ↓
 *        ╔══════════════╗
 *   ==>  ║   fat_open   ║  fat level file operator
 *        ╚══════════════╝
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

/* Note: 1. Get FATFS object from fs->data
*        2. Check fs->path parameter then call f_mount.
*/
int fat_mount(struct fs_dev *fs, const void* data)
{
	return -f_mount(fs->data, fs->path, 1);
}

/* Note: Just call f_mkfs at root path '/' */
int fat_mkfs(const char* device_name)
{
	return -f_mkfs("/", 0, 0);
}

/* Note: Convert the POSIX's open flag to elmfat's flag.
*        Example: if file->flags == O_RDONLY then open_mode = FA_READ
*                 if file->flags & O_APPEND then f_seek the file to end after f_open
*/
int fat_open(struct fs_fd* file)
{
	int open_mode=0;
	
	if( (file->flags&O_ACCMODE)==O_RDONLY ) open_mode|=FA_READ;
	else if( (file->flags&O_ACCMODE)==O_WRONLY ) open_mode|=FA_WRITE;
	else if( (file->flags&O_ACCMODE)==O_RDWR ) open_mode|=(FA_READ|FA_WRITE);
	
	if( (file->flags&O_CREAT)&&!(file->flags&O_TRUNC) ) open_mode|=FA_CREATE_NEW;
	if( file->flags&O_TRUNC ) open_mode|=FA_CREATE_ALWAYS;
	
	int ret = f_open(file->data,file->path,open_mode);
	if(file->flags&O_APPEND) f_lseek(file->data,f_size((FIL *)file->data));
	return -ret;
}

int fat_close(struct fs_fd* file)
{
	return -f_close(file->data);
}
int fat_read(struct fs_fd* file, void* buf, size_t count)
{
	UINT br;
	int ret = f_read(file->data, buf, count, &br);
	if( ret ) return -ret;
	return br;
}
int fat_write(struct fs_fd* file, const void* buf, size_t count)
{
	UINT br;
	int ret = f_write(file->data, buf, count, &br);
	if( ret ) return -ret;
	return br;
}
int fat_lseek(struct fs_fd* file, off_t offset)
{
	return -f_lseek(file->data, offset);
}
int fat_unlink(struct fs_fd* file, const char *pathname)
{
	return -f_unlink(pathname);
}
int fat_list(const char *pathname)
{
	FILINFO f_info;
	DIR dir;
	int ret = f_stat(pathname,&f_info);

	if( ret==0 && !(f_info.fattrib&AM_DIR) ){
		printk("File %8d\t%s\n", f_info.fsize, f_info.fname);
		return -FR_OK;
	}

	if( (ret=f_opendir(&dir, pathname))!=FR_OK ){
		printk("%s : File or path not exit.\n", pathname);
		return -ret;
	}

	while(1){
		ret = f_readdir(&dir, &f_info);
		if( ret!=FR_OK ){
			f_closedir(&dir);
			return -ret;
		}

		if( f_info.fname[0]==0 )
			return -f_closedir(&dir);

		if( f_info.fattrib&AM_DIR )
			printk("DIR  %8d\t%s\n", f_info.fsize, f_info.fname);
		else
			printk("File %8d\t%s\n", f_info.fsize, f_info.fname);
	}
}

int fat_mkdir(const char *pathname)
{
	return -f_mkdir(pathname);
}


struct fs_ops elmfat_ops = {
    .dev_name = "elmfat",
    .mount = fat_mount,
    .mkfs = fat_mkfs,
    .open = fat_open,
    .close = fat_close,
    .read = fat_read,
    .write = fat_write,
    .lseek = fat_lseek,
    .unlink = fat_unlink,
	.list = fat_list,
	.mkdir = fat_mkdir
};




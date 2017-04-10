#include "vfs.h"
#include "fat.h"
#include "ramdisk.h"
#include "fnctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern DISK disk;
extern FD fd_table[MAX_FILE_OPEN];

char txt[] = "The FAT occupies one or more blocks immediately following the boot block. Commonly, part of its last block will remain unused, since it is unlikely that the required number of entries will exactly fill a complete number of blocks. If there is a second FAT, this immediately follows the first (but starting in a new block). This is repeated for any further FATs.Note that multiple FATs are used particularly on floppy disks,because of the higher likelihood of errors when reading the disk. If the FAT is unreadable, files cannot be accessed and another copy of the FAT must be used. On hard disks, there is often only one FAT.In the case of the 16-bit FAT file system, each entry in the FAT is two bytes in length (i.e. 16 bits). The disk data area is divided into clusters, which are the same thing as allocation units, but numbered differently (instead of being numbered from the start of the disk, they are numbered from the start of the disk data area). So, the cluster number is the allocation unit number, minus a constant value which is the size of the areas in between the start of the disk and the start of the data area.";

int txt_len = 1135;

char hello_txt[] = "HELLO WORLDHELLO WORLDHELLO WORLDHELLO WORLDHELLO WORLD";

int hello_txt_len = 56;


static int file_open_demo();
static int dir_create_demo();
static int format_demo();
static int file_create_demo();
static int file_delete_demo();
static int dir_delete_demo();

int  main()
{
	int result = 0;
	printf("\n************\n");
	printf("\nDemo started\n");
	printf("\n************\n");
		
	result |= file_open_demo();
	result |= dir_create_demo();
	result |= mount_demo();
	result |= format_demo();
	result |= file_create_demo();
	result |= file_delete_demo();
	result |= dir_delete_demo();

	if (result == 0)
	{
		printf("\n***************************\n");
		printf("\nDemo completed successfully\n");
		printf("\n***************************\n");
	}
	
}

static int file_create_demo()
{
	char cpath[] = "foo.txt";
	char pathb[] = "sam"; 
	char *rpath;
	
	printf("\n*********\n");
	printf("\nFILE DEMO\n"); 
	printf("\n*********\n"); 	

	int device_id =  disk.init( BLOCK_SIZE * 1000); // 1000 blocks
	
	format(device_id);

	fat_mount(device_id);  //vfs specific function

	set_cwd(device_id, pathb);

	rpath = (char*) malloc(40);
	
	get_cwd(device_id, rpath);

	printf("return path:%s\n",rpath);	

	free(rpath);

	int fd = fat_create(device_id,cpath, 0xFF);

	printf("fd:%d\n",fd);

	printf("write:%d\n",fat_write(fd, txt, 1135));
	
	char *buffer = (char*)malloc(1135);

	printf("read:%d\n",fat_read(fd,buffer, 1135));

	if ( strcmp(buffer,txt) != 0)
	{	
		free(buffer);
		disk.deinit();
		return -1;
	}
	free(buffer);
	disk.deinit();
	return 0;
}

static int file_delete_demo()
{
	char path[] = "FOO";
	int fd;
	char *buffer;

	char *pos;

	
	printf("\n***************\n"); 
	printf("\nFILE OPEN DEMO \n"); 
	printf("\n***************\n"); 
	
	int device_id =  disk.init( BLOCK_SIZE * 1000); // 1000 blocks
	
	format(device_id);

	fat_mount(device_id);  //vfs specific function

	fd = fat_open(device_id, path ,O_CREAT|O_RDWR, 0x21);	
	
	printf("fd:%d\n", fd);

	/* close file */
	fat_close(fd);

	fat_del(device_id, path);

	search_free_dir(device_id, 0);

	fd = fat_open(device_id, path , O_RDWR, 0x21);

	printf("fd:%d\n", fd);

	fat_close(fd);

	disk.deinit();

	return fd == -1 ? 0 : 1;


}

static int file_open_demo()
{
	char path[] = "FOO.TXT";
	int fd;
	char *buffer;

	printf("\n***************\n"); 
	printf("\nFILE OPEN DEMO \n"); 
	printf("\n***************\n"); 
	
	int device_id =  disk.init( BLOCK_SIZE * 1000); // 1000 blocks
	
	format(device_id);

	fat_mount(device_id);  //vfs specific function

	fd = fat_open(device_id, path ,O_CREAT|O_RDWR, 0x00);	
	
	printf("fd:%d\n", fd);

	/* close file */
	fat_close(fd);

	fd = fat_open(device_id, path ,O_RDWR, 0x00);	
	
	printf("fd:%d\n",fd);

	printf("write:%d\n",fat_write(fd, txt, txt_len));
		
	buffer = (char*)malloc(txt_len);

	printf("read:%d\n",fat_read(fd,buffer, txt_len));

	if (strcmp(buffer,txt) != 0)
	{
		free(buffer);
		disk.deinit();
		return -1;
	}

	free(buffer);

	printf("\n***************\n"); 
	printf("\nFILE SEEK DEMO \n"); 
	printf("\n***************\n"); 

	buffer = (char*) malloc(56);

	fat_seek(fd, 56, SEEK_SET);
	
	printf("write:%d\n",fat_write(fd, hello_txt, hello_txt_len));

	/* file seek */

	fat_seek(fd,0,SEEK_SET);

	printf("read:%d\n",fat_read(fd, buffer, hello_txt_len));

	if (strncmp(txt,buffer,hello_txt_len) != 0)
	{
		free(buffer);
		disk.deinit();
		return -1;
	}

	printf("read:%d\n",fat_read(fd, buffer, hello_txt_len));

	if (strcmp(buffer, hello_txt) != 0)
	{
		free(buffer);
		disk.deinit();
		return -1;
	}

	free(buffer);
	
	disk.deinit();
	
	return 0;
}


int mount_demo()
{

	char path[] = "MOO.TXT";
	int  fd;
	int  device_id;
	char *rpath = (char*) malloc(40);

	printf("\n**********\n");
	printf("\nMOUNT DEMO\n"); 
	printf("\n**********\n"); 

	device_id =  disk.init( BLOCK_SIZE * 1000); // 1000 blocks

	format(device_id);

	fat_mount(device_id);  //vfs specific function

	set_cwd(device_id, "");

	fd = fat_open(device_id, path ,O_CREAT|O_RDWR, 0x00);	
	
	printf("fd:%d\n", fd);

	/* close file */
	fat_close(fd);

	fat_umount(device_id);
	
	fat_mount(device_id);  //vfs specific function

	fd = fat_open(device_id, path , O_RDWR, 0x00);	
	
	printf("fd:%d\n", fd);

	disk.deinit();

	return 0;

}

static int format_demo()
{
	char path[] = "MOO.TXT";
	int  fd;
	int  device_id;
	char *rpath = (char*) malloc(40);
	int  status = 0;

	printf("\n***********\n");
	printf("\nFORMAT DEMO\n"); 
	printf("\n***********\n"); 

	device_id =  disk.init( BLOCK_SIZE * 1000); // 1000 blocks

	format(device_id);

	fat_mount(device_id);  //vfs specific function

	set_cwd(device_id, "");

	fd = fat_open(device_id, path ,O_CREAT|O_RDWR, 0x00);	
	
	printf("fd:%d\n", fd);

	/* close file */
	fat_close(fd);

	fat_umount(device_id);
	
	fat_mount(device_id);  //vfs specific function

	status = format(device_id);

	printf("status:%d\n", status);
	
	fd = fat_open(device_id, path , O_RDWR, 0x00);	
	
	printf("fd:%d\n", fd);
	
	disk.deinit();
	
	return fd == -1 ? 0 : 1;

}


static int dir_create_demo()
{
	char file_path[] =  "sam/dam/ram/foo.txt";	
	char patha[] = "dam"; 	
	char pathb[] = "sam"; 
	char pathc[] = "sam/dam"; 
	char pathd[] = "sam/dam/ram"; 
	char pathe[] = "sam/dam/ram/cam"; 
	char pathh[] = "sam/dam/ram/jam"; 
	char pathi[] = "sam/dam/ram/tam";
	int fd;

	printf("\n***************\n");
	printf("\nCREATE DIR DEMO\n"); 
	printf("\n***************\n"); 
	
	int device_id =  disk.init( BLOCK_SIZE * 1000); // 1000 blocks

	format(device_id);

	fat_mount(device_id);  //vfs specific function

	set_cwd(device_id, "");
	
	fat_mkdir(device_id,patha);

	fat_mkdir(device_id,pathb);

	fat_mkdir(device_id,pathc);

	fat_mkdir(device_id,pathd);

	fat_mkdir(device_id,pathe);
	
	fat_mkdir(device_id,pathh);

	fd = fat_open(device_id, file_path ,O_CREAT|O_RDWR, 0x00);

	printf("fd:%d\n", fd);

	fat_close(fd);
	
	fd = fat_open(device_id, file_path ,O_RDWR, 0x00);

	printf("fd:%d\n", fd);

	fat_mkdir(device_id,pathi);
		
	disk.deinit();

	return 0;

}


static int dir_delete_demo()
{
	char file_path[] =  "sam/foo.txt";	
	char patha[] = "dam"; 	
	char pathb[] = "sam"; 
	char pathc[] = "sam/dam"; 
	int fd, status;
	char *rpath;

	printf("\n***************\n");
	printf("\nDELETE DIR DEMO\n"); 
	printf("\n***************\n"); 
	
	int device_id =  disk.init( BLOCK_SIZE * 1000); // 1000 blocks

	format(device_id);

	fat_mount(device_id);  //vfs specific function

	set_cwd(device_id, "");
	
	fat_mkdir(device_id,patha);

	fat_mkdir(device_id,pathb);

	fat_mkdir(device_id,pathc);

	/*fd = fat_open(device_id, file_path ,O_CREAT|O_RDWR, 0x00);

	printf("fd:%d\n", fd);

	fat_close(fd);
	
	fd = fat_open(device_id, file_path ,O_RDWR, 0x00);

	printf("fds:%d\n", fd);
*/
	status = fat_rmdir(device_id, pathb);
		
	set_cwd(device_id, pathc);

	rpath = (char*)malloc (20);

	memset(rpath,0,20);

	get_cwd(device_id, rpath);

	
	printf("rpath:%s", rpath);
	free(rpath);	
	disk.deinit();

	return status == -1 ? 0 : status  ;

}


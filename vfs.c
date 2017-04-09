#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fnctl.h"
#include "ramdisk.h"
#include "vfs.h"

extern DISK disk;

FD fd_table[MAX_FILE_OPEN] = {0,0};

int readDisk(unsigned char dev_id, unsigned char *ptr, int offset, int size)
{
	if (disk.dev_id == dev_id)
	{
		return disk.read(ptr, offset, size);
	}
	else
	{
		printf("[DEVICE]:Error no disk device found\n");
		return -1;
	}

}

int writeDisk(unsigned char dev_id, unsigned char *ptr, int offset, int size)
{
	if (disk.dev_id == dev_id)
	{
		//printf("%c\n", *ptr); 
		return disk.write(ptr, offset, size);
	}
	else
	{
		printf("Error not disk device found");
		return -1;
	}

}



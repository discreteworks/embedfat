#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fnctl.h"
#include "ramdisk.h"
#include "vfs.h"

DISK disk = {
  .dev_id = 0xFF,
  .data = NULL,
  .size = 0,
  .write = writeRamdisk,
  .read = readRamdisk,
  .init = createRamdiskDevice,
  .deinit = removeRamdiskDevice
};

int removeRamdiskDevice()
{
  free(disk.data); //free disk
  disk.data = NULL;
  disk.size = 0;
}

int createRamdiskDevice(unsigned int diskSize)
{
  if (disk.data == NULL) {
    disk.data = (unsigned char *)malloc(diskSize);  //Size in bytes
    memset(disk.data, 0x00, diskSize);
    printf("fat init\n");
  } else {
    printf("Ram disk allocated");
    free(disk.data); //free disk
    disk.data = (unsigned char *)malloc(diskSize);  //Size in bytes
    memset(disk.data, 0x00, diskSize);
  }
  return disk.dev_id;
}

int getRamDiskSize()
{
  return disk.size;
}

int writeRamdisk(unsigned char *ptr, int offset, int size)
{
  memcpy(disk.data + offset, ptr, size);
  return size;

}

int readRamdisk(unsigned char *ptr, int offset, int size)
{
  memcpy(ptr, disk.data + offset, size);
  return size;
}

#ifndef __ramdisk_h
#define __ramdisk_h

/* Ram disk definitions */
int createRamdiskDevice(unsigned int diskSize);
int writeRamdisk(unsigned char *ptr, int offset, int size);
int readRamdisk(unsigned char *ptr, int offset, int size);
int removeRamdiskDevice();
int getRamDiskSize();

#endif

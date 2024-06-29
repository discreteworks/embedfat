[![Build Status](https://travis-ci.com/discreteworks/embedfat.svg?branch=master)](https://travis-ci.com/discreteworks/embedfat)

embedfat
--------
FAT file system was developed for personal computers, FAT comes in difference flavors 12/16/32. Each flavors is from a specific era and the only difference between them is the allocation table.

FAT16, short for File Allocation Table 16-bit, is a file system format commonly used in older computer systems and storage devices.

Introduced in the early 1980s by Microsoft, FAT16 was a significant advancement in organizing and managing data on storage media such as floppy disks and early hard drives. With its 16-bit file allocation table, FAT16 provided a reliable and efficient method for storing and retrieving files, paving the way for the widespread adoption of personal computing.

The FAT16 file system is characterized by its simplicity and compatibility, making it suitable for a wide range of devices and operating systems. Despite its age, FAT16 remains relevant today, particularly in embedded systems and legacy devices where modern file systems may not be practical or compatible.
Architecture

<img src="https://discreteworks.com/assets/img/fat16.png" class="figure-img img-fluid rounded" alt="...">

Reserved Area (Boot Sector): The FAT16 file system starts with a boot sector, also known as the Master Boot Record (MBR). This sector contains essential information for bootstrapping the system and locating the file system structures.

File Allocation Table (FAT): FAT16 uses a 16-bit file allocation table to keep track of the allocation status of each cluster on the disk. This table is the heart of the file system and is essential for locating files and managing disk space.

Root Directory: Following the FAT is the root directory, which stores file entries and directory entries at the root level of the file system.

Data Area (Data Clusters): The actual data on the disk is stored in data clusters. Each cluster typically consists of one or more sectors, with the cluster size being determined by the FAT16 format parameters.

## Support
- Fat 16


## Build
```
make
```

## Run
```
./fat-demo
```

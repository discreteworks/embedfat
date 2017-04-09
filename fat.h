#ifndef __fat_h
#define __fat_h

/* FAT 16 */
typedef struct
{
	unsigned short  b_strap;	// 
	unsigned char   man_desc[8]; // manufacturer description
	unsigned short  b_per_blocks;// bytes per block
	unsigned char 	b_per_alloc;// blocks per allocation unit
	unsigned short 	r_blocks; 	// reserved blocks
	unsigned char	fats;		// total fats
	unsigned short	r_dirs;   	// root blocks;
	unsigned short	t2_blocks;  // total blocks if fits in 2 bytes else 0;
	unsigned char	m_desc;		// media descriptor
	unsigned short	b_per_fat;	// blocks per fat
	unsigned short	b_per_track;// blocks per tracks
	unsigned short	no_heads;	// number of heads
	unsigned int	h_blocks;	// hidden blocks
	unsigned int 	t4_blocks;	// total block greater than 2 bytes;
	unsigned short	p_dno;		// physical drive number
	unsigned char	extd_brd_sig;// extended boot record signature
	unsigned int	v_serial_no;// volume serial number
	unsigned char	v_label[11];// volume label
	unsigned char	fs_id[8];	// file system identifier
	unsigned char	bt[0x1c0];	// bootstrap program;
	unsigned short	b_sig;		// boot signature;
} boot_block;


typedef struct  
{
	unsigned char	filename[8];
	unsigned char	ext[3];
	unsigned char	file_attr;
	unsigned char	res[10];
	unsigned short	time;
	unsigned short	date;
	unsigned short	start_cluster;
	unsigned int	f_size;
} dir_ent;


/* Fat internals */
int find(unsigned char dev_id, char* filename, unsigned short start_cluster);
int find_n(unsigned char dev_id, char* filename, unsigned short *start_cluster);
int search_free_dir(unsigned char dev_id, unsigned short start_cluster);
unsigned short search_free_space(unsigned char dev_id);
int del_chain(unsigned short start_cluster);


/* Fat16 api */
int fat_open(int dev_id, char* filename, int flags, int mode);
int fat_create(int dev_id, char* filename, int mode);
int fat_close(int fd);
int fat_seek(int fd, int offset, int whence);
int fat_write(int fd, char *buffer, int size);
int fat_read(int fd, char *buffer, int size);
int fat_mkdir(int dev_id, char *path);
int fat_del(int dev_id, char *path);
int fat_rmdir(int dev_id, char *path);
int fat_first(int dev_id, char *path);
int fat_next(int dev_id, char *path);



#define ROOT_DIR			64 			// number of root directories
#define BLOCK_SIZE 			512			// sector size
#define FAT16 				"FAT16" 	// FAT16
#define FAT32 				"FAT32" 	// FAT32
#define FILE_SEPARATOR 		'/'			// File separator


#ifdef DEBUG
	#define DB_PRINTF(f_, ...) printf((f_), __VA_ARGS__)
#else
	#define DB_PRINTF(f_, ...)
#endif


/* Conversion macro little endian and big endian */

#define swap32(integer)	 		((unsigned char *)&integer)[0] <<  24 |\
							 	((unsigned char *)&integer)[1] << 16 |\
							 	((unsigned char *)&integer)[2] << 8 |\
							 	((unsigned char *)&integer)[3]

#define swap16(s_integer)	 	((unsigned char*)&s_integer)[0] << 8 |\
					 		 	((unsigned char*)&s_integer)[1]		


#define b_endian32(integer)	 	swap32(integer)

#define l_endian32(integer)	 	swap32(integer)


#define b_endian16(s_integer)	swap16(s_integer)

#define l_endian16(s_integer)	swap16(s_integer)

#endif

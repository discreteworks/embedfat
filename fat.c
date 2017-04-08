/*
* SIMPLE FAT 16
* This is free software, you can utilize it or modify for free and commmercial products
* while keeping the license information headers intact. This product as sub system or complete is not warranted in any
* condition. You can use or modify at your own risk and author is not resposible for
* any loss or damage that be a result of utilizing the product as system/sub system or complete software.
*/

//#define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fnctl.h"
#include "fat.h"
#include "vfs.h"


extern DISK disk;
extern FD fd_table[MAX_FILE_OPEN];

unsigned short cwd;

int read(unsigned dev_id, int block_no,unsigned char *buffer, int offset, int bytes)
{
	return readDisk(dev_id, buffer, block_no * BLOCK_SIZE + offset, bytes);
} 

int write(unsigned dev_id, int block_no,unsigned char *buffer, int offset, int bytes)
{
	return writeDisk(dev_id, buffer, block_no * BLOCK_SIZE + offset, bytes);
} 

unsigned short search_free_space(unsigned char dev_id)
{
	
	boot_block boot;
	char fat[BLOCK_SIZE];
	int i, j, fat_blocks, status = -1;
	
	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

	fat_blocks = l_endian16(boot.b_per_fat);

	/* just searching the first fat available  */
	for (i = 0; i < fat_blocks; i++)
	{	
		status = read(dev_id, i + 1, fat, i * BLOCK_SIZE, BLOCK_SIZE);
		
		if (status  == -1)
			return -1;
		
		for (j = 0; j < BLOCK_SIZE; j += 2)
		{	
			DB_PRINTF("\nfat:%d\n",(unsigned short)fat[j]);
			if (*(unsigned short*)&fat[j] == 0x0000)
			{	
				DB_PRINTF("free:%d\n",  i * BLOCK_SIZE / 2 + j);
				return i * BLOCK_SIZE / 2 + j;   // each fat entry is 2 bytes, so 256 entries per block. 
			}
		}
	}	
	return -1;
}

int search_free_dir(unsigned char dev_id, unsigned short start_cluster)
{
	boot_block boot;
	dir_ent dent;
	char fat[BLOCK_SIZE];
	int i, j, fat_blocks, status = -1;
	int root_blocks;
	int total_dir, directory_start;
	int chain;
	
	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;

	DB_PRINTF("search free :%d",start_cluster);
	if ( start_cluster == 0) // Root block
	{		
		for(i = 0; i < root_blocks; i++)
		{
			for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
			{				
				read(dev_id, directory_start + i, (unsigned char*)&dent, j, sizeof(dent));
				//DB_PRINTF("Filename:%d\n",BLOCK_SIZE/sizeof(dent));
				if (dent.filename[0] == 0x00 || dent.filename[0] == 0xe5)
				{
					return  i * BLOCK_SIZE / sizeof(dent) + j ;	
				}
			}	
		}
	}
	else
	{
		chain = start_cluster; 
		//DB_PRINTF("chain ok:%d", chain);
		while (chain != 0xFFFF)
		{
		//	DB_PRINTF("\nchain in ok:%d\n", chain);
			for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
			{
				read(dev_id, directory_start + root_blocks - 2 + chain / 2, (unsigned char*)&dent, j, sizeof(dent));
				//DB_PRINTF("Filename:%d\n",BLOCK_SIZE/sizeof(dent));
				if (dent.filename[0] == 0x00 || dent.filename[0] == 0xe5)
				{
					return  i * BLOCK_SIZE / sizeof(dent) + j ;	
				}
			}	
			read(dev_id, 1, (unsigned char*)&chain, chain, sizeof(short)); 
			chain =  l_endian16(chain); // little endian
			
		}	
	}
	return -1;
}

int search_path(int dev_id, char *path, unsigned short *start_cluster)
{
	unsigned short entry_cluster = 0;
	unsigned short chain_val;
	unsigned short chain_last = 0xFFFF;
	int found = -1;
	int i;
	int o_length = strlen(path);
	int offset_length = 0;
	int root_blocks;
	int fat_blocks = 0;
	dir_ent dent;
	boot_block boot;
	
	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * 32 / 512 ;

	for(i = 0; i < o_length + 1 ; i++)
	{
		if (path[i] == FILE_SEPARATOR || path[i] == '\0')
		{
			path[i] = '\0';
			DB_PRINTF("Search Path:%s\n",&path[offset_length]);	
			found = find_n(dev_id, &path[offset_length], start_cluster);
			
			offset_length= i + 1;
			DB_PRINTF("File found: %d\n",found);	
	
			if (found == -1)
			{
				return found;
			}
			else	
			{
				if (o_length == strlen(path))  // there are no sub directories
				{
					return -1;
				}
		
			}
			path[i]	= FILE_SEPARATOR;	
		}
	}
	return found;
}

int find_n(unsigned char dev_id, char* filename, unsigned short *start_cluster)
{
	boot_block boot;
	dir_ent dent;
	int i, j;
	int fat_blocks = 0, root_blocks;
	int total_dir, directory_start;
	char file[13];
	char *pos;
	int chain;

	DB_PRINTF("\nfind name:%s, %d\n", filename, *start_cluster);

	/* read the root block to get directory entries */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));

	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;

	/* root directory is statically allocated so routines follow two paths. */
	if (*start_cluster == 0) /* root dir */
	{	
		for(i=0; i < root_blocks; i++)
		{
			for(j=0; j < BLOCK_SIZE; j+=sizeof(dent))
			{
				read(dev_id, directory_start + i, (unsigned char*)&dent, j, sizeof(dent));

				if (*dent.filename == 0x2e || *dent.filename == 0xe5) continue;  // deleted or directory
				if (dent.ext[0] != 0x20)
				{
					pos = strchr(filename,'.');
				
					if (memcmp(dent.filename,filename, pos -filename) == 0 && memcmp(dent.ext,&filename[ pos - filename + 1], pos - filename + 1))
					{
						//DB_PRINTF("\nfilepos:%d", i * BLOCK_SIZE / sizeof(dent) + j);
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
						break; 
					}
				}
				else
				{
					if (memcmp(dent.filename,filename,strlen(filename)) == 0)
					{
						//DB_PRINTF("\nfilepos:%d\n", i * BLOCK_SIZE / sizeof(dent) + j);
						*start_cluster =  b_endian16(dent.start_cluster);						
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
						break; 
					}
				}
			}
		}
	}
	else /* dynamically allocated user directories */
	{
		chain = *start_cluster; 
		DB_PRINTF("subdir:%d",directory_start + root_blocks - 2 + chain / 2);
		while	(chain != 0xFFFF)
		{
			for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
			{
				read(dev_id, directory_start + root_blocks - 2 + chain / 2 , (unsigned char*)&dent, j, sizeof(dent));
				/* deleted or directory */
				DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
				if (*dent.filename == 0x2e || 
					*dent.filename == 0xe5 ||
					dent.filename[0] == '.'||
					(dent.filename [0] == '.' && dent.filename[1] == '.')) continue; 
				if (dent.ext[0] != 0x20)
				{
					pos = strchr(filename,'.');

					if (memcmp(dent.filename,filename, pos -filename) == 0 &&
					    memcmp(dent.ext,&filename[ pos - filename + 1], pos - filename + 1))
					{
						//DB_PRINTF("\nfilepos:%d", i * BLOCK_SIZE / sizeof(dent) + j);
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
						break; 
					}
				}
				else
				{
					if (memcmp(dent.filename,filename,strlen(filename)) == 0)
					{
						//DB_PRINTF("\nfilepos:%d\n", i * BLOCK_SIZE / sizeof(dent) + j);						
						*start_cluster =  b_endian16(dent.start_cluster);
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
						break; 
					}
				}
			}	
			read(dev_id, 1, (unsigned char*)&chain, chain, sizeof(short)); 
			chain =  b_endian16(chain); // big endian
		}	
	}
	return -1;

}

int set_cwd(int dev_id, char *path)
{
	unsigned short start_cluster = 0;
	
	if (*path == '\0')
	{
		cwd = 0;
	}
	else
	{
		search_path(dev_id, path, &start_cluster);
		cwd = start_cluster;
	}
	return cwd;

}

int get_cwd(int dev_id, char *path)
{
	
	char *path_ptr = path;	
	char *temp;
	unsigned short entry_cluster = 0;
	unsigned short chain_val;
	unsigned short chain_last = 0xFFFF;	
	unsigned short start_cluster = 0;
	unsigned short l_cwd = cwd;
	int root_blocks;
	int fat_blocks = 0;
	int found = -1;	
	int chain;
	int directory_start;
	int i, j;
	int next;

	dir_ent dent;
	boot_block boot;
	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = b_endian16(boot.b_per_fat) ;
	root_blocks = b_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;

	while( l_cwd != 0)
	{	
		read(dev_id, directory_start + root_blocks - 2 + cwd / 2, (unsigned char*)&dent, sizeof(dent), sizeof(dent));
		
		//DB_PRINTF("\n%c%c%c cwd: %d\n",dent.filename[0],dent.filename[1],dent.filename[2], directory_start + root_blocks - 2 + 8 / 2);
		chain = b_endian16(dent.start_cluster);
		DB_PRINTF("chain:%d", chain); 
				
		if (chain == 0)
		{
			for(i=0; i < root_blocks; i++)
			{	
				for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
				{
					read(dev_id, directory_start , (unsigned char*)&dent, j, sizeof(dent));
					// deleted or directory
					//DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
					if (*dent.filename == 0x2e || 
						*dent.filename == 0xe5 ||
						dent.filename[0] == '.'||
						(dent.filename [0] == '.' && dent.filename[1] == '.')) continue; 
					if (dent.ext[0] == 0x20 && b_endian16(dent.start_cluster) == l_cwd)
					{
						 DB_PRINTF("\nfound file:%c\n",dent.filename[0]);
						 for(i=0;i<8;i++)
						 if(dent.filename[i]!=0x20)			
						 	*path_ptr++ = dent.filename[i];
						 l_cwd = chain;
					}
				}
			}
		}
		else
		{
			while (chain != 0xFFFF)
			{
				for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
				{
					read(dev_id, directory_start + root_blocks - 2 + chain / 2 , (unsigned char*)&dent, j, sizeof(dent));
					// deleted or directory
					//DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
					if (*dent.filename == 0x2e || 
						*dent.filename == 0xe5 ||
						dent.filename[0] == '.'||
						(dent.filename [0] == '.' && dent.filename[1] == '.')) continue; 
					if (dent.ext[0] == 0x20 && b_endian16(dent.start_cluster) == l_cwd)
					{
						 DB_PRINTF("\nfound file:%c\n",dent.filename[0]);
						 for(i=0;i<8;i++)
						 if(dent.filename[i]!=0x20)			
						 	*path_ptr++ = dent.filename[i];
						 l_cwd = chain;
					}
				}	
				DB_PRINTF("\nchain inside:%d", chain); 
				read(dev_id, 1, (unsigned char*)&chain, chain, sizeof(short)); 
				chain =  b_endian16(chain); // big endian
			}
		
		}	

		*path_ptr++ = '/';
		DB_PRINTF("cwds:%d", l_cwd);
	}
	temp = malloc(strlen(path)+1);
	memcpy(temp,path,strlen(path));
	next = strlen(temp) - 1;
	for(i = strlen(temp) - 1 ; i>=0; i--)	
	{
		if (temp[i] == '/' || i == 0)
		{
			DB_PRINTF("%d\n",i);
			if ( i !=0 )
			memcpy(path,temp + i + 1 , next - i + 1);
			else
			memcpy(path,temp + i , next - i);	
			path = path + next - i;
			
			next = i;
		}	
	}
	*path++='\0';
	free (temp);
	return cwd;		
}
		
int fat_mkdir(int dev_id, char *path)
{
	dir_ent dent;
	boot_block boot;
	int root_blocks;
	int fat_blocks = 0;
	unsigned short entry_cluster = 0;
	int found = -1;
	unsigned short chain_val;
	unsigned short chain_last = 0xFFFF;	
	unsigned short start_cluster = 0;
	int chain;
	int total_dir, directory_start;
	int i;
	i = strlen(path);
	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;
	
	start_cluster = cwd;  // Set the start cluster to current working directory	
	
	found = search_path(dev_id, path, &start_cluster); 

	//if (found == -1)
	start_cluster = cwd;
	
	DB_PRINTF("\ncwd:%d", cwd);

	if (found != -1)
		return -1;

	found = search_free_dir(dev_id, start_cluster);
	
	DB_PRINTF("\nnew start cluster:%d", start_cluster);

	DB_PRINTF("\nfound here:%d", found);

	if (start_cluster > 0)
	{
		if (found ==-1)
		{
			chain_val = search_free_space (dev_id);
			chain_val = l_endian16(chain_val);
			write(dev_id, 1, (unsigned char*)&chain_val, start_cluster, sizeof(unsigned short));
			found = search_free_dir(dev_id, b_endian16(chain_val));
			chain = b_endian16(chain_val);
		}
		else
		{
			chain = start_cluster;
		}
		directory_start = directory_start + root_blocks - 2 + chain / 2;
	}
		
	/* create directory on root with two directory in directory own cluster.*/
	memset(dent.filename, 0x20, 8);
	memset(dent.ext, 0x20, 3);
	
	while(path[i]!=FILE_SEPARATOR&&i>=0) i--;	
	i++;
	DB_PRINTF("\nor:%s,path:%s i:%d\n",path,&path[i], i);

	/* copy the new filename to create */
	memcpy(dent.filename,&path[i],strlen(&path[i]));
	dent.file_attr = 0x28;
	dent.f_size = (unsigned int)0x0;
	
	entry_cluster = search_free_space (dev_id);

	DB_PRINTF("\nentry_cluster:%d\n",entry_cluster);

	if (entry_cluster > 0)
	{	
		dent.start_cluster = l_endian16(entry_cluster); // little endian
				
		if(write(dev_id, 1, (unsigned char*)&chain_last, entry_cluster, sizeof(unsigned short)))
		/* writing currently only on first fat... see later */
		{
			DB_PRINTF("\nwrite loc:%d %d\n",directory_start, chain);			
			write(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));	
			
			/* create two directories for navigation */
			DB_PRINTF("entry cluster:%d", entry_cluster);
			memset(dent.filename, 0x20, 8);
			*dent.filename = '.';
			write(dev_id,  boot.fats * fat_blocks + 1 + root_blocks - 2 + entry_cluster / 2, (unsigned char*)&dent, 0, sizeof(dent));	
			*(dent.filename + 1) = '.';	
			dent.start_cluster = l_endian16(chain);
			write(dev_id,  boot.fats * fat_blocks + 1 + root_blocks - 2 + entry_cluster / 2, (unsigned char*)&dent, sizeof(dent), sizeof(dent));	
		}
		
	}
	return 0;		

}
int fat_create(int dev_id, char* filename, int flags, int mode)
{
	char *pos;
	unsigned short entry_cluster = 0;
	unsigned short start_cluster = 0;
	unsigned short chain_val;
	unsigned short chain_last = 0xFFFF;		
	int found = -1;
	int chain;
	int directory_start;
	int fd =  -1;
	int root_blocks;
	int fat_blocks = 0;
	dir_ent    dent;
	boot_block boot;

	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;
	
	start_cluster = cwd;  /* set the start cluster to current working directory	*/
	
	found = search_path(dev_id, filename, &start_cluster);
				
	if (found == -1)
	{	
		/* create a new file  entry */
		found = search_free_dir(dev_id, start_cluster);
		DB_PRINTF("New free dir found:%d\n", found);
		if (start_cluster > 0)
		{
			if (found == -1)
			{
				chain_val = search_free_space (dev_id);
				chain_val = l_endian16(chain_val);
				write(dev_id, 1, (unsigned char*)&chain_val, start_cluster, sizeof(unsigned short));
				found = search_free_dir(dev_id, b_endian16(chain_val));
				chain = b_endian16(chain_val);
			}
			else
			{
				chain = start_cluster;
			}
			directory_start = directory_start + root_blocks - 2 + chain / 2;
		}

		/* write file entry */
		pos = strchr(filename,'.');
		if(pos)
		{			
			pos++;
		}

		/* init the filename and ext with 0x20, required.	*/
		memset(dent.filename, 0x20, 8);
		memset(dent.ext, 0x20, 3);		

		/* copy the new filename to create */
		memcpy(dent.filename,filename, pos - filename + 1);
		memcpy(dent.ext,pos, strlen(pos));
		dent.file_attr = 0x20;
		dent.f_size = (unsigned int)0x0;
		
		entry_cluster = search_free_space (dev_id);

		DB_PRINTF("File start_cluster:%d\n",entry_cluster);

		if (entry_cluster > 0)
		{	
			dent.start_cluster = l_endian16(entry_cluster); /* little endian */
			if (write(dev_id, 1, (unsigned char*)&chain_last, entry_cluster, sizeof(unsigned short)))
			/* writing currently on on first fat... see later */
			{
				if (chain == 0)
				{
					write(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));	
				}
				else
				{
					write(dev_id, directory_start  , (unsigned char*)&dent, found, sizeof(dent));	
				}
			}
		}
		if (found > 0)
		{
			fd = get_free_fd();
			fd_table[fd].dev_id = dev_id;
			fd_table[fd].dir_cluster = chain;
			fd_table[fd].id = found;
			fd_table[fd].flags = flags;
			
		}
	}
	return fd;
}

int fat_open(int dev_id, char* filename, int flags, int mode)
{	
	char *pos;
	unsigned short entry_cluster = 0;
	unsigned short start_cluster = 0;
	unsigned short chain_val;
	unsigned short chain_last = 0xFFFF;	
	unsigned short chain;
	int root_blocks;
	int found = -1;
	int fat_blocks = 0;
	int directory_start;
	int fd = -1;	
	dir_ent dent;
	boot_block boot;
    
	fd_table[fd].flags = flags;
	/* set the directory start_cluster
	 * to current working directory 
     */
	start_cluster = cwd;

	switch (flags)
	{
		case O_RDWR:
		case O_RDONLY:
		case O_WRONLY:
			found = search_path(dev_id, filename, &start_cluster);
			break;
		case O_RDWR | O_CREAT:
		case O_RDONLY | O_CREAT:
		case O_RDWR | O_TRUNC | O_CREAT:
		case O_RDONLY | O_TRUNC | O_CREAT:
		
			found = search_path(dev_id, filename, &start_cluster);	
			DB_PRINTF("File offset: %d",found);
			if (found == -1)
			{	
				/* read the root block */
				read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

				fat_blocks = l_endian16(boot.b_per_fat) ;
				root_blocks = l_endian16(boot.r_dirs) * 32 / 512 ;
				directory_start = boot.fats * fat_blocks + 1;

				/* create a new file  entry */
				found = search_free_dir(dev_id, start_cluster);
				DB_PRINTF("New free dir found:%d\n", found);
				if (start_cluster > 0)
				{
					if (found == -1)
					{
						chain_val = search_free_space (dev_id);
						chain_val = l_endian16(chain_val);
						write(dev_id, 1, (unsigned char*)&chain_val, start_cluster, sizeof(unsigned short));
						found = search_free_dir(dev_id, b_endian16(chain_val));
						chain = b_endian16(chain_val);
					}
					else
					{
						chain = start_cluster;
					}
					directory_start = directory_start + root_blocks - 2 + chain / 2;
				}
				/* write file entry */
				pos = strchr(filename,'.');
				if(pos)
				{			
					pos++;
				}
				/* init the filename and ext with 0x20, required. */
				memset(dent.filename, 0x20, 8);
				memset(dent.ext, 0x20, 3);		

				/* copy the new filename to create */
				memcpy(dent.filename,filename, pos - filename + 1);
				memcpy(dent.ext,pos, strlen(pos));
				dent.file_attr = 0x20;
				dent.f_size = (unsigned int)0x0;
		
				entry_cluster = search_free_space (dev_id);

				DB_PRINTF("File start cluster:%d\n",entry_cluster);

				if (entry_cluster > 0)
				{	
					dent.start_cluster = l_endian16(entry_cluster); /* little endian */
						
					if(write(dev_id, 1, (unsigned char*)&chain_last, entry_cluster, sizeof(unsigned short)))
					/* writing currently on on first fat... see later */
					{
						if (chain == 0)
						{
							write(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));	
						}
						else
						{
							write(dev_id, directory_start  , (unsigned char*)&dent, found, sizeof(dent));	
						}
					}			
				}
			}
			else
			{
				fd = EEXIST;
				found = -1;
			}
			break;
		default:
			fd = EINVAL;
			found = -1;
			break;

	}
	if (found > 0)
	{
		fd = get_free_fd();
		fd_table[fd].dev_id = dev_id;
		fd_table[fd].id = found;
		fd_table[fd].flags = flags;
		
	}
	return fd;
}

int fat_close(int fd)
{
	memset(&fd_table[fd],0x00, sizeof(fd_table[fd])); /* free the table entry */	
	fd_table[fd].id = -1;
	fd_table[fd].dev_id = -1;	
	return fd_table[fd].id; /* success on closure */
}


int fat_seek(int fd, int offset, int whence)
{
	
	int root_blocks;
	int fat_blocks = 0;
	int file_size = 0;
	int directory_start = 0;
	dir_ent    dent;
	boot_block boot;

	if (fd_table[fd].dev_id <= 0)
		return EBADF;	

	/* read the root block */
	read(fd_table[fd].dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

	fat_blocks = b_endian16(boot.b_per_fat) ;
	root_blocks = b_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;
	
	if (fd_table[fd].dir_cluster == 0)
		read(fd_table[fd].dev_id, directory_start, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));
	else
		read(fd_table[fd].dev_id, directory_start + root_blocks - 2 + fd_table[fd].dir_cluster / 2, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));

	file_size =  b_endian32(dent.f_size);

	switch (whence)
	{
		case SEEK_SET:
			if ( offset < 0) 
				return EINVAL;
			if (offset > file_size)
				return EOVERFLOW;
			fd_table[fd].cur_r_of = offset;
			fd_table[fd].cur_w_of = offset;		
			break;
		
		case SEEK_CUR:
			if (fd_table[fd].cur_r_of + offset < 0 || fd_table[fd].cur_w_of + offset < 0)
				return EINVAL;
			if (fd_table[fd].cur_w_of + offset > file_size  || fd_table[fd].cur_w_of + offset > file_size)		
				return EOVERFLOW;
			fd_table[fd].cur_r_of += offset;
			fd_table[fd].cur_w_of += offset;
			break;

		case SEEK_END:
			fd_table[fd].cur_r_of = fd_table[fd].cur_w_of =  file_size;
			break;
	
	}
	return fd_table[fd].cur_r_of;
}

int fat_write(int fd, char *buffer, int size)
{
	unsigned short start_cluster;
	unsigned short chain,chain_val = 0;
	unsigned int file_size;
	unsigned short free_val = 0xFFFF;
	unsigned short free_l;
	int allocation_unit; /* allocation units  = cluster size = sectors * sector size */
	int root_blocks;
	int fat_blocks = 0;		
	int free;
	int directory_start;
	int written = 0;
	int block_no;
	int w_size = 0;	
	dir_ent dent;
	boot_block boot;
		
	if (fd_table[fd].id < 0)
	{
		return -1; // file close
	}

	if (buffer == NULL)
	{
		return -1; // empty buffer 
	}
	
	/* read the root block */
	read(fd_table[fd].dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

	fat_blocks = b_endian16(boot.b_per_fat) ;
	root_blocks = b_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;
	
	DB_PRINTF("\ndir cluster:%d\n", fd_table[fd].dir_cluster);

	if (fd_table[fd].dir_cluster == 0)
		read(fd_table[fd].dev_id, directory_start, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));			
	else
		read(fd_table[fd].dev_id, directory_start + root_blocks -2 + fd_table[fd].dir_cluster / 2, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));
	
	start_cluster = b_endian16(dent.start_cluster); // big endian
	
	DB_PRINTF("\nstart cluster:%d:%d\n", start_cluster,fd_table[fd].id );	
		
	read(fd_table[fd].dev_id, 1, (unsigned char*)&chain, start_cluster, sizeof(unsigned short)); // we read in short 2 bytes;

	/* calculate cluster size in bytes */

	allocation_unit = b_endian16(boot.b_per_blocks) * boot.b_per_alloc;
 		
	DB_PRINTF ("\nallocation_unit: %d\n",allocation_unit);

	chain = start_cluster;
	
	file_size =  b_endian32(dent.f_size);	

	if (fd_table[fd].flags & O_TRUNC) //for truncat set file_size 0
	{
		fd_table[fd].cur_w_of = 0;

		while (chain_val != 0xFFFF)
		{	
			DB_PRINTF("\nchain:%d\n", chain);
			DB_PRINTF("trunc:%d\n", chain);
			read(fd_table[fd].dev_id, 1, (unsigned char*)&chain_val, chain, sizeof(short)); 

			write(fd_table[fd].dev_id, 1, (unsigned char*)&free_val, chain, sizeof(short)); 
			
			chain = ((char*)&chain_val)[0] << 8 | ((char*)&chain_val)[1]; // little endian
		}
		chain = start_cluster;
	}
	else
	{
		/* seek */

		block_no = (fd_table[fd].cur_w_of / allocation_unit);

		if (fd_table[fd].flags & O_APPEND) //append mode // 
		{
			fd_table[fd].cur_w_of = file_size;
		
			block_no = (fd_table[fd].cur_w_of / allocation_unit);
		}
	
	
		DB_PRINTF("\nblock_no:%d\n", fd_table[fd].flags & O_TRUNC );
	
		while (block_no > 0 && chain!=0xFFFF)
		{
		
			read(fd_table[fd].dev_id, 1, (unsigned char*)&chain, chain, sizeof(short)); 

			chain = ((char*)&chain)[0]<< 8 | ((char*)&chain)[1]; // little endian				
		
			if ( chain == 0xFFFF)
			{		
				break;
			}
		
			block_no--;	
		}

	}
	
	DB_PRINTF("\nchain:%d\n", chain);	
	DB_PRINTF("fseek p:%d\n", fd_table[fd].cur_w_of);

	DB_PRINTF("fseek s:%d\n", (fd_table[fd].cur_w_of + size)% BLOCK_SIZE);
		
	//DB_PRINTF("Write loc:%d\n",file_size);
	
	while (size > 0)
	{	
		if ((fd_table[fd].cur_w_of % allocation_unit + size) >= allocation_unit)
		{
			w_size =  allocation_unit - fd_table[fd].cur_w_of % allocation_unit;
		}
		else
		{		
			w_size =  (size) % allocation_unit;
		}
	
		DB_PRINTF("\nw_size:%d\n",w_size);

		// start writing on offset
		write(fd_table[fd].dev_id, directory_start + root_blocks - 2 + chain / 2, 
			  (unsigned char*)buffer + written, fd_table[fd].cur_w_of % allocation_unit , w_size);
		
		size -= w_size;
		file_size += w_size;
		written += w_size;
		
		if (size > 0)
		{	
			read(fd_table[fd].dev_id, 1, (unsigned char*)&chain_val, chain, sizeof(short)); 
			DB_PRINTF("chain_val:%d, chain:%d \n", chain_val, chain);
			if(chain_val == 0xFFFF)
			{	
				free = search_free_space(fd_table[fd].dev_id);

				//DB_PRINTF("sz in:%d \n", free);		
				if (free)
				{	
						
					free_l = l_endian16(free); // little endian	
					DB_PRINTF("val:%d",free_l);		
			
					write(fd_table[fd].dev_id, 1, (unsigned char*)&free_l, chain, sizeof(unsigned short));
					write(fd_table[fd].dev_id, 1, (unsigned char*)&free_val, free, sizeof(unsigned short));
					chain = free;
				
				}			
				else
				{
					//DB_PRINTF("no fat entry found");
				 	return written;
					
				}
			}
			else
			{
				chain = l_endian16(chain_val);
			}
		}
		fd_table[fd].cur_w_of += written;
		
	}
	dent.f_size = l_endian32(file_size);
	
	DB_PRINTF("\nafter:dent.f_size:%u\n",dent.f_size);	
	if (fd_table[fd].dir_cluster == 0)
		write(fd_table[fd].dev_id, directory_start, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));	
	else
		write(fd_table[fd].dev_id, directory_start + root_blocks -2 + fd_table[fd].dir_cluster / 2, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));
	return written;
}


int fat_read(int fd, char *buffer, int size)
{
	dir_ent dent;
	boot_block boot;
	int root_blocks;
	int fat_blocks = 0;
	unsigned short start_cluster;
	int directory_start;
	int reads = 0;
	int block_no;
	int w_size;
	unsigned short chain;
	unsigned int file_size;
	unsigned int allocation_unit;
		
	if (fd_table[fd].id < 0)
	{
		return -1; // file close
	}

	if (buffer == NULL)
	{
		return -1; // empty buffer 
	}
	
	/* read the root block */
	read(fd_table[fd].dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

	fat_blocks = b_endian16(boot.b_per_fat) ;
	root_blocks = b_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;

	if (fd_table[fd].dir_cluster == 0)
		read(fd_table[fd].dev_id, directory_start, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));			
	else
		read(fd_table[fd].dev_id, directory_start + root_blocks -2 + fd_table[fd].dir_cluster / 2, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));			
	
	start_cluster = b_endian16(dent.start_cluster); // little endian	
	
	chain = start_cluster;	
	
	/* calculate cluster size in bytes */

	allocation_unit = b_endian16(boot.b_per_blocks) * boot.b_per_alloc;
	
	file_size =  b_endian32(dent.f_size);
	
	block_no = (fd_table[fd].cur_r_of / allocation_unit);

	DB_PRINTF("\nbno:%d\n",fd_table[fd].cur_r_of );	

	while (block_no >0 && chain!=0xFFFF)
	{
	
		read(fd_table[fd].dev_id, 1, (unsigned char*)&chain, chain, sizeof(short)); 

		chain = ((char*)&chain)[0]<< 8 | ((char*)&chain)[1]; // little endian

		if ( chain == 0xFFFF)
		{		
			break;
		}
	
		block_no--;	
	}
	
	DB_PRINTF("\nchain:%d\n",chain);

	if (size + fd_table[fd].cur_r_of > file_size)
		return EOF;

	while (size > 0 )
	{	
		
		if ((fd_table[fd].cur_r_of % BLOCK_SIZE + size) >= allocation_unit)
		{
			w_size =  BLOCK_SIZE - fd_table[fd].cur_r_of % allocation_unit;
		}
		else
		{		
			w_size =  (size) % BLOCK_SIZE;
		}		
		DB_PRINTF("\nread size:%d\n", w_size);
		// start reading on offset
		
		read(fd_table[fd].dev_id, directory_start + root_blocks - 2 + chain / 2, (unsigned char*)buffer+reads,
			 fd_table[fd].cur_r_of % allocation_unit , w_size);
		
		
		size -= w_size;
		reads += w_size;
	
		if (size > 0 && reads >= allocation_unit)
		{
			DB_PRINTF("\nchain:%d\n",chain);
			read(fd_table[fd].dev_id, 1, (unsigned char*)&chain, chain, sizeof(short));
			chain = l_endian16(chain); // little endian
			DB_PRINTF("\nchain:%d\n",chain);
			
		}
		fd_table[fd].cur_r_of += w_size;
	}
	
	return reads;
}

int get_free_fd()
{
	int i = 0;
	for(i = 0; i< MAX_FILE_OPEN; i++)
	{
		if (fd_table[i].id == 0)
			return i;
	}
	return -1;

}

int find(unsigned char dev_id, char* filename, unsigned short start_cluster)
{
	boot_block boot;
	dir_ent dent;
	int i, j;
	int fat_blocks = 0, root_blocks;
	int total_dir, directory_start;
	char file[13];
	char *pos;
	int chain;

	/* read the root block to get directory entries */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));

	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * 32 / 512 ;
	directory_start = boot.fats * fat_blocks + 1;

	//DB_PRINTF("fats:%d",boot.r_blocks);
	if ( start_cluster == 0) // Root block
	{	
		for(i=0; i < root_blocks; i++)
		{
			for(j=0; j < BLOCK_SIZE; j+=sizeof(dent))
			{
				read(dev_id, directory_start + i, (unsigned char*)&dent, j, sizeof(dent));

				if (*dent.filename == 0x2e || *dent.filename == 0xe5) continue;  // deleted or directory
				if (dent.ext[0] != 0x20)
				{
					pos = strchr(filename,'.');
				
					if (memcmp(dent.filename,filename, pos -filename) == 0 && memcmp(dent.ext,&filename[ pos - filename + 1], pos - filename + 1))
					{
						//DB_PRINTF("\nfilepos:%d", i * BLOCK_SIZE / sizeof(dent) + j);
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
						break; 
					}
				}
				else
				{
					if (memcmp(dent.filename,filename,strlen(filename)) == 0)
					{
						//DB_PRINTF("\nfilepos:%d\n", i * BLOCK_SIZE / sizeof(dent) + j);						
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
						break; 
					}

				}
			
			}
		}
	}
	else
	{
			chain = start_cluster; 
			chain =  l_endian16(chain); // little endian	
			while	(chain != 0xFFFF)
			{
				for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
				{
					read(dev_id, directory_start + root_blocks + chain / 2 , (unsigned char*)&dent, j, sizeof(dent));
					if (*dent.filename == 0x2e || *dent.filename == 0xe5) continue;  // deleted or directory
					if (dent.ext[0] != 0x20)
					{
						pos = strchr(filename,'.');

						if (memcmp(dent.filename,filename, pos -filename) == 0 && memcmp(dent.ext,&filename[ pos - filename + 1], pos - filename + 1))
						{
							//DB_PRINTF("\nfilepos:%d", i * BLOCK_SIZE / sizeof(dent) + j);
							return  i * BLOCK_SIZE / sizeof(dent) + j ;	
							break; 
						}
					}
					else
					{
						if (memcmp(dent.filename,filename,strlen(filename)) == 0)
						{
							//DB_PRINTF("\nfilepos:%d\n", i * BLOCK_SIZE / sizeof(dent) + j);						
							return  i * BLOCK_SIZE / sizeof(dent) + j ;	
							break; 
						}
					}
				}	
				read(dev_id, 1, (unsigned char*)&chain, chain, sizeof(short)); 
				chain =  l_endian16(chain); // little endian
			}	
			
	}
	return -1;

}

int mount(int dev_id)
{
	mount_list[0].dev_id = dev_id;
	mount_list[0].mount_point = '/';
	mount_list[0].fs[0]='F';
	mount_list[0].fs[1]='A';
	mount_list[0].fs[2]='T';
	mount_list[0].fs[3]='\0';
}

int format(int dev_id)
{
	int i = 0, j = 0;
	int fat_blocks;
	int status = 0;
	int root_blocks;
	unsigned short block_size 	= BLOCK_SIZE;
	unsigned short root_dir 	= ROOT_DIR;
	
	char fat[BLOCK_SIZE] = {0x00};
	
	/* boot block */
	boot_block boot;
	/* dir entry */
	dir_ent root;

	boot.b_strap = 0xeb3c;
	memcpy(boot.man_desc, "MSWIN4.1", sizeof("MSWIN4.1"));
	boot.b_per_blocks = l_endian16(block_size); // 512
	boot.b_per_alloc = 0x1; // 1 block
	boot.r_blocks = 0x0100; // 4 blocks
	boot.fats = 1;
	boot.r_dirs = l_endian16(root_dir); // 64;
	boot.t2_blocks = l_endian32(disk.size);
	boot.m_desc	   = 0xf8;
	boot.b_per_fat = 0x1400;
	boot.b_per_track = 0x0a00;
	boot.no_heads = 0x0100;
	boot.h_blocks = 0x00;
	boot.t4_blocks =0x00;
	boot.p_dno = 0x00;
	boot.extd_brd_sig = 0x29;
	boot.v_serial_no = 0x2323;
	memset(boot.v_label, 0x20, sizeof(boot.v_label));
	memcpy(boot.v_label,"NO NAME", strlen("NO NAME"));
	memset(boot.fs_id, 0x20, sizeof(boot.fs_id));	
	memcpy(boot.fs_id, FAT16, 5);	
	//boot.bt //boot loader
	boot.b_sig = 0x55aa;

	/* write boot block */	
	status = write(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));
	
	if (status  == -1)
		return -1;	
	
	/*  write FAT tables */
	fat_blocks = ((char*)&boot.b_per_fat)[0]<<8 | ((char*)&boot.b_per_fat)[1]  ;
	for (i = 1; i < boot.fats + 1 ;i++)
	{
		for (j = 0; j < fat_blocks; j+=2)
		{	
			if (j == 0)
			{
				fat[j] = boot.m_desc;
				fat[j+1] = 0xFF;
			}
			else if (j == 2)
			{
				fat[j] = 0xFF;
				fat[j+1] = 0xFF;
			}
			else
			{
				fat[j] = 0x00;
				fat[j+1] = 0x00;
				
			}
		}	
		status = write(dev_id, i, fat, 0, BLOCK_SIZE);
		if (status  == -1)
			return -1;
	}
	/* write root directory */
	root_blocks = ((char*)&boot.r_blocks)[0]<<8 | ((char*)&boot.r_blocks)[1] ;
	for(i = 0; i < root_blocks; i++)
	{
		for(j = 0; j < BLOCK_SIZE / 32; j++)
		{	
			if(i == 0)
			{	// static allocation of root directory
				memset(root.filename,0x20, 8);
				memcpy(root.filename,"ROOTDIR", sizeof("ROOTDIR"));
				memset(root.ext,0x20, 3);
				root.file_attr = 0x28;	
				write(dev_id, boot.fats * fat_blocks + 1, (unsigned char*)&root, 0, sizeof(root));
			}
			else
			{	// empty directory entries available for use
				memset(&root,0x00,sizeof(root));
				write(dev_id, boot.fats * fat_blocks + 1 + j, (unsigned char*)&root, 0, sizeof(root));
			}
		}
	}	
	if (status  == -1)
		return -1;

	return 0;

}

int  main()
{
	int device_id =  disk.init( 512 * 1000); // 1000 blocks

	format(device_id);

	mount(device_id);  //vfs specific function

	//int fd = fat_open("FOO.TXT",O_CREAT|O_RDWR, 0xFF);
	
	//DB_PRINTF("fd:%d", fd);

	//fat_close(fd);

	//int fd = fat_open("FOO.TXT",O_CREAT|O_RDWR, 0xFF);	

	//fd = fat_open("FOO.TXT",O_CREAT|O_RDWR, 0xFF);	
	
	//DB_PRINTF("fd:%d",fd);

	
	char pathb[] = "sam"; 
	fat_mkdir(device_id,pathb);

	printf("set CWD:%d\n",set_cwd(device_id, pathb));

	char *rpath = (char*) malloc(40);
	
	printf("get CWD:%d\n",get_cwd(device_id, rpath));
	
	printf("return Path:%s\n",rpath);	
	
	char pathc[] = "dam"; 
	fat_mkdir(device_id,pathc);

	//char pathd[] = "sam/dam/ram"; 
	//fat_mkdir(device_id,pathd);

	//char pathe[] = "cam"; 
	//fat_mkdir(device_id,pathe);
	
	//DB_PRINTF("\n set CWD:%d\n",set_cwd(device_id, ""));
	//char path[] = "sam/dam"; 
	//unsigned short start_cluster = 0;
	//DB_PRINTF("path found:%d, %d",search_path(device_id, path, &start_cluster ), start_cluster);
	; 
	
	char cpath[] = "foo.txt";
	int fd = fat_create(device_id,cpath,O_CREAT|O_RDWR, 0xFF);
	printf("fd:%d",fd);

	printf("\nwrite:%d",fat_write(fd,"The FAT occupies one or more blocks immediately following the boot block. Commonly, part of its last block will remain unused, since it is unlikely that the required number of entries will exactly fill a complete number of blocks. If there is a second FAT, this immediately follows the first (but starting in a new block). This is repeated for any further FATs.Note that multiple FATs are used particularly on floppy disks,because of the higher likelihood of errors when reading the disk. If the FAT is unreadable, files cannot be accessed and another copy of the FAT must be used. On hard disks, there is often only one FAT.In the case of the 16-bit FAT file system, each entry in the FAT is two bytes in length (i.e. 16 bits). The disk data area is divided into clusters, which are the same thing as allocation units, but numbered differently (instead of being numbered from the start of the disk, they are numbered from the start of the disk data area). So, the cluster number is the allocation unit number, minus a constant value which is the size of the areas in between the start of the disk and the start of the data area.", 1135));
	
	char *buffer = (char*)malloc(1135);

	printf("\nread:%d",fat_read(fd,buffer, 1135));

	printf("\nbuffer:%s\n", buffer);

	//DB_PRINTF("\ncwd:%d",set_cwd(device_id, pathb));

	
	//DB_PRINTF("\ncwd:%d",get_cwd(device_id, rpath));
	
	//DB_PRINTF("%s",rpath);
	//DB_PRINTF("free:%d",search_free_dir(device_id));

	//DB_PRINTF("\nLabel:%c\n", *((boot_block*)(disk.data))->v_label);

}





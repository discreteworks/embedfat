/*
* EMBED FAT 16
* This is free software, you can utilize it or modify for free and commmercial products
* while keeping the license information headers intact. This product as sub system or complete is not warranted in any
* condition. You can use or modify at your own risk and author is not resposible for
* any loss or damage that be a result of utilizing the product as system /sub system or complete software.
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

int delete_chain(int dev_id, unsigned short start_cluster)
{
	unsigned short chain = start_cluster;
	int i, j, fat_blocks;
	int root_blocks;
	int total_dir, directory_start;
	unsigned short free_val = 0x00;	
	boot_block boot;
	dir_ent dent;

	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	directory_start = boot.fats * fat_blocks + 1;	

	if (chain > 0)
	{
		while (chain != 0xFFFF)
		{
			read(dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain, chain %  BLOCK_SIZE, sizeof(short)); 
	
			write(dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&free_val, chain % BLOCK_SIZE, sizeof(short));
	
			chain =  b_endian16(chain); 
		}	
		write(dev_id, start_cluster / BLOCK_SIZE  + 1, (unsigned char*)&free_val, start_cluster % BLOCK_SIZE, sizeof(short));
	}

}
/*  Name
*						search_free_space
*   Description
*						search free space within fat.
*   Inputs
*		dev_id    		device id.
*		
*   Outputs 
*		fat cluster		fat entry for cluster.
*		
*/

unsigned short search_free_space(unsigned char dev_id)
{
	boot_block boot;
	int i, j, fat_blocks, status = -1;
	char fat[BLOCK_SIZE];
	
	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

	fat_blocks = b_endian16(boot.b_per_fat);   /* Total number of blocks for fat */

	/* just searching the first fat available  */
	for (i = 0; i < fat_blocks; i++)
	{	
		status = read(dev_id, i + 1, fat, i * BLOCK_SIZE, BLOCK_SIZE); /* Load the block one by one to search for free space */
		
		if (status  == -1)
			return -1;
		
		for (j = 0; j < BLOCK_SIZE; j += 2)
		{	
			DB_PRINTF("\nfat:%d\n",(unsigned short)fat[j]);
			if (*(unsigned short*)&fat[j] == 0x0000)
			{	
				DB_PRINTF("free:%d\n",  i * BLOCK_SIZE / 2 + j);
				return i * BLOCK_SIZE / 2 + j;   /* each fat entry is 2 bytes, so 256 entries per block. */ 
			}
		}
	}	
	return -1;
}

/*  Name
*						search_free_dir
*   Description
*						search free directory entry space within root directory and sub directory starting with
*						start cluster.
*   Inputs
*		dev_id    		device id.
*		path			path of file/directory to search.
*		start_cluster	start_cluster passed as directory entries cluster to begin.
*		
*   Outputs 
*		found 			current index of the directory entry in its start directory.
*		
*/
int search_free_dir(unsigned char dev_id, unsigned short start_cluster)
{
	boot_block boot;
	dir_ent dent;
	char fat[BLOCK_SIZE];
	int i, j, fat_blocks, status = -1;
	int root_blocks;
	int total_dir, directory_start;
	int chain;
	int b_per_alloc; /* sector per cluster or blocks per cluster */
	
	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = b_endian16(boot.b_per_fat) ;
	root_blocks = b_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	directory_start = boot.fats * fat_blocks + 1;
	b_per_alloc = boot.b_per_alloc

	DB_PRINTF("search free :%d\n",start_cluster);
	if ( start_cluster == 0) /* root directory */
	{		
		for(i = 0; i < root_blocks; i++)
		{
			for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
			{				
				read(dev_id, directory_start + i, (unsigned char*)&dent, j, sizeof(dent));
				DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
				//DB_PRINTF("Filename:%d\n",BLOCK_SIZE/sizeof(dent));
				if (dent.filename[0] == 0x00 || dent.filename[0] == 0xe5)
				{
					return  i * BLOCK_SIZE / sizeof(dent) + j ;	 /* offset per block number */
				}
			}	
		}
	}
	else /* sub directory */
	{
		chain = start_cluster; 
		while (chain != 0xFFFF)
		{
			for(i = 0; i < b_per_alloc; i++)
			{
				for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
				{
					DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
					read(dev_id, directory_start + root_blocks - 2 + chain / 2, (unsigned char*)&dent, j, sizeof(dent));
				
					if (dent.filename[0] == 0x00 || dent.filename[0] == 0xe5)
					{
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
					}
				}
			}	
			read(dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain, chain % BLOCK_SIZE, sizeof(short)); 
			chain =  b_endian16(chain);
			
		}	
	}
	return -1;
}

/*  Name
*						search_path
*   Description
*						search a path for current directory offset and for directory find its 
*						start cluster.
*   Inputs
*		dev_id    		device id.
*		path			path of file/directory to search.
*		start_cluster	start_cluster passed as reference contains current start cluster to begin.
*		
*   Outputs 
*		found 			current index of the directory entry in its parent.
*		start_cluster 	start cluster of the directory entry or current directory of file.
*/

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
	root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;

	for(i = 0; i < o_length + 1 ; i++)
	{
		if (path[i] == FILE_SEPARATOR || path[i] == '\0')
		{
			path[i] = '\0';
			DB_PRINTF("Search Path:%s\n",&path[offset_length]);	
			found = find_name(dev_id, &path[offset_length], start_cluster);
			
			offset_length= i + 1;
			DB_PRINTF("File found: %d, level: %d\n",found, i);	
	
			if (found == -1)
			{
				return found;
			}
			else	
			{
				if (o_length == strlen(path))  // there are no sub directories
				{
					return found;
				}
		
			}
			path[i]	= FILE_SEPARATOR;	
		}
	}
	return found;
}

/*  Name
*						find_name
*   Description
*						called iteratively to search a dir or file within a directory cluster.
*   Inputs
*		dev_id    		device id.
*		path			path of file/directory to search.
*		start_cluster	start_cluster passed as reference contains current start cluster to begin.
*		
*   Outputs 
*		found 			current index of the directory entry in its parent.
*		start_cluster 	start cluster of the directory entry or current directory of file.
*/

int find_name(unsigned char dev_id, char* filename, unsigned short *start_cluster)
{
	char file[13];
	char *pos;
	unsigned short chain;
	unsigned short chain_val;
	int i, j;
	int fat_blocks = 0, root_blocks;
	int total_dir, directory_start;
	int b_per_alloc;
	boot_block boot;
	dir_ent dent;	

	DB_PRINTF("\nfind name:%s, %d\n", filename, *start_cluster);
	/* read the root block to get directory entries */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));

	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	directory_start = boot.fats * fat_blocks + 1;
	b_per_alloc = boot.b_per_alloc;
	pos = strchr(filename,'.');
	
	/* root directory is statically allocated so routines follow two paths. */
	if (*start_cluster == 0) /* root dir */
	{	
		for(i = 0; i < root_blocks; i++)
		{
			for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
			{
				read(dev_id, directory_start + i, (unsigned char*)&dent, j, sizeof(dent));

				DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);

				if (*dent.filename == 0x2e || *dent.filename == 0xe5) continue;  // deleted or directory
				
				if (dent.ext[0] != 0x20 && pos)
				{
					if (memcmp(dent.filename,filename, strlen(filename) - strlen(pos)) == 0 && memcmp(dent.ext, ++pos, strlen(pos)) == 0)
					{
						//DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
					}
				}
				else /* file with no extension or sub directory */
				{
					if (memcmp(dent.filename,filename,strlen(filename)) == 0) 
					{
						if (dent.file_attr & 0x10)	/* sub directory return it start cluster */
						{
							*start_cluster =  b_endian16(dent.start_cluster);
						}								
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
					}
				}
			}
		}
	}
	else /* dynamically allocated user directories */
	{
		chain = *start_cluster; 

		DB_PRINTF("subdir:%d\n",directory_start + root_blocks - 2 + chain / 2);
		
		read(dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain_val, chain % BLOCK_SIZE, sizeof(short)); 
		
		DB_PRINTF("chain : %d ,chain_val :%d\n",chain, chain_val);

		if (chain_val == 0)
		{
			return -1;
		}

		while	(chain != 0xFFFF)
		{
			for(i = 0; i < b_per_alloc; i++)
			{			
				for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
				{
					read(dev_id, directory_start + root_blocks - 2 + chain / 2 , (unsigned char*)&dent, j, sizeof(dent));
					/* deleted or directory */
					DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
					if (*dent.filename == 0x2e || 
						*dent.filename == 0xe5 ||
						(dent.filename [0] == 0x2e && dent.filename[1] == 0x2e)) continue; 
					if (dent.ext[0] != 0x20 && pos)
					{
						if (memcmp(dent.filename,filename, strlen(filename)) == 0 && memcmp(dent.ext, ++pos, strlen(pos)) == 0)
						{
							//DB_PRINTF("\nfilepos:%d", i * BLOCK_SIZE / sizeof(dent) + j);
							return  i * BLOCK_SIZE / sizeof(dent) + j ;	
						}
					}
					else  /* file with no extension or sub directory */
					{
						if (memcmp(dent.filename,filename,strlen(filename)) == 0) 
						{
							if (dent.file_attr & 0x10) /* sub directory return it start cluster */
							{
								*start_cluster =  b_endian16(dent.start_cluster);
							}						
							return  i * BLOCK_SIZE / sizeof(dent) + j ;	
					
						}
					}
				}
			}
			/* read the fat table for next in chain */	
			read(dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain, chain % BLOCK_SIZE, sizeof(short)); 
			chain =  b_endian16(chain); /* big endian */
		}	
	}
	return -1;
}

int set_cwd(int dev_id, char *path)
{
	unsigned short start_cluster = 0;
	int found;
	
	if (*path == '\0')
	{
		cwd = 0;
	}
	else
	{
		if (search_path(dev_id, path, &start_cluster))
		{
			cwd = start_cluster;
			
		}
		else
		{
			cwd = 0;
		}
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
	unsigned short chain;
	int directory_start;
	int i, j, k;
	int next;
	int b_per_alloc;

	dir_ent dent;
	boot_block boot;
	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = b_endian16(boot.b_per_fat) ;
	root_blocks = b_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	directory_start = boot.fats * fat_blocks + 1;
	b_per_alloc = boot.b_per_alloc;	

	/* iterate the loop till all the path is not evaluated backwards */
	while( l_cwd != 0)
	{	
		/* Read the (..) directory entry of the current working directory */
		read(dev_id, directory_start + root_blocks - 2 + l_cwd / 2, (unsigned char*)&dent, sizeof(dent), sizeof(dent));
		
		DB_PRINTF("\n%c%c%c cwd: %d\n",dent.filename[0],dent.filename[1],dent.filename[2], directory_start + root_blocks - 2 + 8 / 2);
		chain = b_endian16(dent.start_cluster);
		DB_PRINTF("chain:%d", chain); 
		
		/* read the root directory to find the directory entry matching the path */	
		if (chain == 0)
		{
			for(i = 0; i < root_blocks; i++)
			{	
				for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
				{
					read(dev_id, directory_start , (unsigned char*)&dent, j, sizeof(dent));
					// deleted or directory
					DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
					if (j == 0 && i == 0	   ||
						*dent.filename == 0x2e || 
						*dent.filename == 0xe5 ||
						(dent.filename [0] == 0x2e && dent.filename[1] == 0x2e)) continue; 
					if (dent.ext[0] == 0x20 &&  b_endian16(dent.start_cluster) == l_cwd)
					{
						 DB_PRINTF("\nfound directory:%c\n",dent.filename[0]);
						 for(i = 0; i < 8; i++)
						 {
						 	if(dent.filename[i] != 0x20)			
						 		*path_ptr++ = dent.filename[i];
						 }	
						 l_cwd = chain;
					}
				}
			}
		}
		else /* read the sub directory to find the directory entry matching the path */	
		{			
			read(dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain_val, chain % BLOCK_SIZE, sizeof(unsigned short)); 
			
			/* check invalid path */ 
			if (chain_val == 0)
			{
				path = NULL;				
				return -1;
			
			}			
			while (chain != 0xFFFF)
			{
				for(i = 0; i < b_per_alloc; i++)
				{
					for(j = 0; j < BLOCK_SIZE; j+=sizeof(dent))
					{
						read(dev_id, directory_start + root_blocks - 2 + chain / 2 , (unsigned char*)&dent, j, sizeof(dent));
						/* deleted or directory */
						DB_PRINTF("\n%c%c%c\n",dent.filename[0],dent.filename[1],dent.filename[2]);
						if (j == 0 && i == 0	   ||
							*dent.filename == 0x2e || 
							*dent.filename == 0xe5 ||
							(dent.filename [0] == 0x2e && dent.filename[1] == 0x2e)) continue; 
						if (dent.ext[0] == 0x20 &&   b_endian16(dent.start_cluster) == l_cwd)
						{
							 DB_PRINTF("\nfound file:%c\n",dent.filename[0]);
							 for(k =0; k < 8; k++)
							 {
							 	if(dent.filename[k] != 0x20)
								{			
							 		*path_ptr++ = dent.filename[k];
								}
							 }
						 	 l_cwd = chain;
						}
					}
				}	
				DB_PRINTF("\nchain inside:%d", chain); 
				/* read fat */
				read(dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain, chain % BLOCK_SIZE, sizeof(short)); 
				chain =  b_endian16(chain); 
			}
		}	
		*path_ptr++ = '/';
		DB_PRINTF("cwds:%d\n", l_cwd);
	}
	//printf("path:%d\n",path[8]);
	*path_ptr++ = '\0';
	temp = malloc(strlen(path)+1);
	memset(temp,0,strlen(path)+1);
    memcpy(temp,path,strlen(path));
    next = strlen(temp) - 1;
	memset(path,0,strlen(path)+1);
	
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
	root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	directory_start = boot.fats * fat_blocks + 1;
	
	start_cluster = cwd;  // Set the start cluster to current working directory	

	found = search_path(dev_id, path, &start_cluster); 

	DB_PRINTF("cwd:%d\n", cwd);

	if (found != -1)
	{
		return -1; /* directory exists */
	}
	found = search_free_dir(dev_id, start_cluster);
	
	DB_PRINTF("new start cluster:%d\n", start_cluster);

	DB_PRINTF("found here:%d\n", found);

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
	
	while (path[i] != FILE_SEPARATOR && i >= 0) i--;	
	i++;
	DB_PRINTF("orginal:%s,path:%s separtor index:%d\n",path,&path[i], i);

	/* copy the new filename to create */
	memcpy(dent.filename,&path[i],strlen(&path[i]));
	dent.file_attr = 0x10;
	dent.f_size = (unsigned int)0x0;
	
	entry_cluster = search_free_space (dev_id);

	DB_PRINTF("\nentry_cluster:%d\n",entry_cluster);

	if (entry_cluster > 0)
	{	
		dent.start_cluster = l_endian16(entry_cluster); // little endian
				
		if(write(dev_id, entry_cluster / BLOCK_SIZE + 1, (unsigned char*)&chain_last, entry_cluster % BLOCK_SIZE, sizeof(unsigned short)))
		/* writing currently only on first fat... see later */
		{
			DB_PRINTF("\nwrite loc:%d %d\n",directory_start, chain);			
			write(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));	
			
			/* create two directories for navigation */
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

int fat_rmdir(int dev_id, char *path)
{
	
	char *pos;
	unsigned short parent_cluster = 0;
	unsigned short start_cluster = 0;
	unsigned short chain_val;
	unsigned short chain_last = 0xFFFF;		
	int found = -1;
	int chain = 0;
	int directory_start;
	int fd =  -1;
	int i;
	int root_blocks;
	int fat_blocks = 0;
	dir_ent    dent;
	boot_block boot;

	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	directory_start = boot.fats * fat_blocks + 1;

	start_cluster = cwd;  /* set the start cluster to current working directory	*/
	
	found = search_path(dev_id, path, &start_cluster);

	printf("found :%d  cluster: start_cluster: %d\n", found, start_cluster);
	
	found = search_free_dir(dev_id, start_cluster);

	if (found > 64) /* offset for two 0x2e entries . and .. */
	{
		return -1;  /* directory not empty */
	}
	/* delete the directory cha‌in */
	delete_chain(dev_id, start_cluster);

	/* we are in root directory */
	if (start_cluster == 0)
	{
		read(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));
	}
	else /* we are in sub directory */
	{
		read(dev_id, directory_start + root_blocks - 2 + start_cluster / 2 , (unsigned char*)&dent, sizeof(dent), sizeof(dent));
	}
	/* get the parent_cluster from sub directory .. directory entry */
	parent_cluster = b_endian16(dent.start_cluster);
	
	if (parent_cluster == 0)
	{
		read(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));
		dent.filename[0] = 0xE5;
		write(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));
	}
	else
	{
		write(dev_id, directory_start + root_blocks - 2 + parent_cluster / 2 , (unsigned char*)&dent, found, sizeof(dent));
		dent.filename[0] = 0xE5;
		write(dev_id, directory_start + root_blocks - 2 + parent_cluster / 2 , (unsigned char*)&dent, found, sizeof(dent));
	}

	return 0;	
}


int fat_first(int dev_id, char *path)
{

}
int fat_next(int dev_id, char *path)
{


}


int fat_del(int dev_id, char* filename)
{
	char *pos;
	unsigned short entry_cluster = 0;
	unsigned short start_cluster = 0;
	unsigned short chain_val;
	unsigned short chain_last = 0xFFFF;		
	int found = -1;
	int chain = 0;
	int directory_start;
	int fd =  -1;
	int i;
	int root_blocks;
	int fat_blocks = 0;
	dir_ent    dent;
	boot_block boot;

	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	directory_start = boot.fats * fat_blocks + 1;

	start_cluster = cwd;  /* set the start cluster to current working directory	*/
	
	found = search_path(dev_id, filename, &start_cluster);

	if (start_cluster == 0)
	{
		read(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));
	}
	else
	{
		read(dev_id, directory_start + root_blocks - 2 + start_cluster / 2 , (unsigned char*)&dent, found, sizeof(dent));	
	}
	entry_cluster = l_endian16(dent.start_cluster);
	
	
	if (dent.file_attr & 0x10)
	{
		return -1; /* sub directory entry cannot delete */
	} 
	
	delete_chain(dev_id, entry_cluster);

	dent.filename[0] = 0xE5;

	if (start_cluster == 0)
	{
		write(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));
	}
	else
	{
		write(dev_id, directory_start + root_blocks - 2 + start_cluster / 2 , (unsigned char*)&dent, found, sizeof(dent));
	}
	return 0;	

}

int fat_create(int dev_id, char* filename, int mode)
{
	char *pos;
	unsigned short entry_cluster = 0;
	unsigned short start_cluster = 0;
	unsigned short chain_val;
	unsigned short chain_last = 0xFFFF;		
	int found = -1;
	int chain = 0;
	int directory_start;
	int fd =  -1;
	int i;
	int root_blocks;
	int fat_blocks = 0;
	dir_ent    dent;
	boot_block boot;

	/* read the root block */
	read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	
	fat_blocks = l_endian16(boot.b_per_fat) ;
	root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
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
				write(dev_id, start_cluster / BLOCK_SIZE + 1, (unsigned char*)&chain_val, start_cluster % BLOCK_SIZE, sizeof(unsigned short));
				found = search_free_dir(dev_id, b_endian16(chain_val));
				chain = b_endian16(chain_val);
			}
			else
			{
				chain = start_cluster;
			}
			directory_start = directory_start + root_blocks + (chain / 2 - 2) * boot.b_per_alloc;
		}
		

		/* write file entry */
		/* init the filename and ext with 0x20, required. */
		memset(dent.filename, 0x20, 8);
		memset(dent.ext, 0x20, 3);		

		/* copy the new filename to create */
		pos = strchr(filename,'.');

		i = strlen(filename);

		while(i > 0 && filename[i] != FILE_SEPARATOR) i--;
		if (i > 0)				
			i++;

		if(pos)
		{			
			pos++;
			memcpy(dent.filename,filename + i  , pos - filename + i  + 1);
			memcpy(dent.ext,pos, strlen(pos));
		}
		else
		{
			memcpy(dent.filename, filename + i,  strlen(filename) - i);
		}
		dent.file_attr = mode;
		dent.f_size = (unsigned int)0x0;
		
		entry_cluster = search_free_space (dev_id);

		DB_PRINTF("File start_cluster:%d\n",entry_cluster);

		if (entry_cluster > 0)
		{	
			dent.start_cluster = l_endian16(entry_cluster); /* little endian */
			/* write fat */
			if (write(dev_id, entry_cluster /  BLOCK_SIZE + 1, (unsigned char*)&chain_last, entry_cluster % BLOCK_SIZE, sizeof(unsigned short)))
			/* writing currently on on first fat... see later */
			{
				if (chain == 0)
				{
					write(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));	
				}
				else
				{
					write(dev_id, directory_start, (unsigned char*)&dent, found, sizeof(dent));	
				}
			}
		}
		if (found > 0)
		{
			fd = get_free_fd();
			fd_table[fd].dev_id = dev_id;
			fd_table[fd].dir_cluster = chain;
			fd_table[fd].id = found;
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
	unsigned short chain = 0;
	int root_blocks;
	int found = -1;
	int fat_blocks = 0;
	int directory_start;
	int fd = -1;	
	int i;
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
			chain = start_cluster;
			break;
		case O_RDWR | O_CREAT:
		case O_RDONLY | O_CREAT:
		case O_RDWR | O_TRUNC | O_CREAT:
		case O_RDONLY | O_TRUNC | O_CREAT:
		
			found = search_path(dev_id, filename, &start_cluster);	
			DB_PRINTF("File offset: %d\n",found);
			if (found == -1)
			{	
				/* read the root block */
				read(dev_id, 0, (unsigned char*)&boot, 0, sizeof(boot));	

				fat_blocks = l_endian16(boot.b_per_fat) ;
				root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
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
						write(dev_id, start_cluster /  BLOCK_SIZE + 1, (unsigned char*)&chain_val, start_cluster %  BLOCK_SIZE, sizeof(unsigned short));
						found = search_free_dir(dev_id, b_endian16(chain_val));
						chain = b_endian16(chain_val);
					}
					else
					{
						chain = start_cluster;
					}
					directory_start = directory_start + root_blocks + (chain / 2 - 2) * boot.b_per_alloc;
				}

				/* write file entry */

				/* init the filename and ext with 0x20, required. */
				memset(dent.filename, 0x20, 8);
				memset(dent.ext, 0x20, 3);		

				/* copy the new filename to create */
				pos = strchr(filename,'.');

				i = strlen(filename);

				while(i > 0 && filename[i] != FILE_SEPARATOR) i--;
				if (i > 0)				
					i++;

				if(pos)
				{			
					pos++;
					memcpy(dent.filename,filename + i  , pos - filename + i  + 1);
					memcpy(dent.ext,pos, strlen(pos));
				}
				else
				{
					memcpy(dent.filename, filename + i,  strlen(filename) - i);
				}
				
				dent.file_attr = mode;
				dent.f_size = (unsigned int)0x0;
		
				entry_cluster = search_free_space (dev_id);

				DB_PRINTF("File start cluster:%d\n",entry_cluster);

				if (entry_cluster > 0)
				{	
					dent.start_cluster = l_endian16(entry_cluster); /* little endian */
						
					if(write(dev_id, entry_cluster / BLOCK_SIZE + 1, (unsigned char*)&chain_last, entry_cluster % BLOCK_SIZE, sizeof(unsigned short)))
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
		fd_table[fd].dir_cluster = chain;
		fd_table[fd].flags = flags;
		
	}
	return fd;
}

int fat_close(int fd)
{
	memset(&fd_table[fd],0x00, sizeof(fd_table[fd])); /* free the table entry */	
	fd_table[fd].id = 0;
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
	root_blocks = b_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
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
	root_blocks = b_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	directory_start = boot.fats * fat_blocks + 1;
	
	DB_PRINTF("dir cluster:%d\n", fd_table[fd].dir_cluster);

	if (fd_table[fd].dir_cluster == 0)
		read(fd_table[fd].dev_id, directory_start, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));			
	else
		read(fd_table[fd].dev_id, directory_start + root_blocks -2 + fd_table[fd].dir_cluster / 2, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));
	
	start_cluster = b_endian16(dent.start_cluster); // big endian
	
	DB_PRINTF("start cluster:%d:%d\n", start_cluster,fd_table[fd].id );	
		
	read(fd_table[fd].dev_id, start_cluster / BLOCK_SIZE + 1, (unsigned char*)&chain, start_cluster % BLOCK_SIZE, sizeof(unsigned short)); // we read in short 2 bytes;

	/* calculate cluster size in bytes */

	allocation_unit = b_endian16(boot.b_per_blocks);
 		
	allocation_unit *=  boot.b_per_alloc;

	chain = start_cluster;
	
	file_size =  b_endian32(dent.f_size);	

	if (fd_table[fd].flags & O_TRUNC) //for truncat set file_size 0
	{
		fd_table[fd].cur_w_of = 0;

		while (chain_val != 0xFFFF)
		{	
			DB_PRINTF("chain:%d\n", chain);
			DB_PRINTF("trunc:%d\n", chain);
			read(fd_table[fd].dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain_val, chain % BLOCK_SIZE, sizeof(short)); 

			write(fd_table[fd].dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&free_val, chain % BLOCK_SIZE, sizeof(short)); 
			
			chain = b_endian16(chain_val); // little endian
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
		
		DB_PRINTF("block_no:%d\n", fd_table[fd].flags & O_TRUNC );
	
		while (block_no > 0 && chain!=0xFFFF)
		{
			read(fd_table[fd].dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain, chain % BLOCK_SIZE, sizeof(short)); 

			chain = b_endian16(chain); 
		
			if ( chain == 0xFFFF)
			{		
				break;
			}
		
			block_no--;	
		}

	}
	
	DB_PRINTF("chain:%d\n", chain);	
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
	
		DB_PRINTF("w_size:%d\n",w_size);

		// start writing on offset
		write(fd_table[fd].dev_id, directory_start + root_blocks + (chain / 2 - 2) * boot.b_per_alloc, 
			  (unsigned char*)buffer + written, fd_table[fd].cur_w_of % allocation_unit , w_size);
		
		size -= w_size;
		file_size += w_size;
		written += w_size;
		
		if (size > 0)
		{	
 			/* read fat */ 
			read(fd_table[fd].dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain_val, chain % BLOCK_SIZE, sizeof(short)); 
			DB_PRINTF("chain_val:%d, chain:%d \n", chain_val, chain);
			if(chain_val == 0xFFFF)
			{	
				free = search_free_space(fd_table[fd].dev_id);

				//DB_PRINTF("sz in:%d \n", free);		
				if (free)
				{	
						
					free_l = b_endian16(free); 
					DB_PRINTF("val:%d",free_l);		
			
					write(fd_table[fd].dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&free_l, chain % BLOCK_SIZE, sizeof(unsigned short));
					write(fd_table[fd].dev_id, free / BLOCK_SIZE +  1, (unsigned char*)&free_val, free % BLOCK_SIZE, sizeof(unsigned short));
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
	
	DB_PRINTF("after:dent.f_size:%u\n",dent.f_size);	
	if (fd_table[fd].dir_cluster == 0)
		write(fd_table[fd].dev_id, directory_start, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));	
	else
		write(fd_table[fd].dev_id, directory_start + root_blocks + (fd_table[fd].dir_cluster / 2 - 2)  * boot.b_per_alloc, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));
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
	int cluster_no;
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
	root_blocks = b_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	root_blocks = root_blocks == 0 ? 1:root_blocks; /* root blocks less than block size */
	directory_start = boot.fats * fat_blocks + 1;

	if (fd_table[fd].dir_cluster == 0)
		read(fd_table[fd].dev_id, directory_start, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));			
	else
		read(fd_table[fd].dev_id, directory_start + root_blocks -2 + fd_table[fd].dir_cluster / 2, (unsigned char*)&dent, fd_table[fd].id, sizeof(dent));			
	
	start_cluster = b_endian16(dent.start_cluster); 
	
	chain = start_cluster;	
	
	/* calculate cluster size in bytes */

	allocation_unit = b_endian16(boot.b_per_blocks);
 		
	allocation_unit *=  boot.b_per_alloc;
	
	file_size =  b_endian32(dent.f_size);
	
	cluster_no = (fd_table[fd].cur_r_of / allocation_unit);

	DB_PRINTF("bno:%d\n",fd_table[fd].cur_r_of );	

	while (cluster_no > 0 && chain!=0xFFFF)
	{
	
		read(fd_table[fd].dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain, chain % BLOCK_SIZE, sizeof(short)); 

		chain = b_endian16(chain);

		if ( chain == 0xFFFF)
		{		
			break;
		}
	
		cluster_no--;	
	}
	
	DB_PRINTF("chain:%d\n",chain);

	if (size + fd_table[fd].cur_r_of > file_size)
	{
		return EOF;
	}

	while (size > 0 )
	{	
		
		if ((fd_table[fd].cur_r_of % allocation_unit + size) >= allocation_unit)
		{
			w_size =  allocation_unit - fd_table[fd].cur_r_of % allocation_unit;
		}
		else
		{		
			w_size =  (size) % allocation_unit;
		}		
		DB_PRINTF("read size:%d\n", w_size);
		// start reading on offset
		
		read(fd_table[fd].dev_id, directory_start + root_blocks + (chain / 2 - 2) * boot.b_per_alloc ,
			 (unsigned char*)buffer + reads,
			 fd_table[fd].cur_r_of % allocation_unit , w_size);
		
		
		size -= w_size;
		reads += w_size;
	
		if (size > 0 && reads >= allocation_unit)
		{
			DB_PRINTF("chain:%d\n",chain);
			read(fd_table[fd].dev_id, chain / BLOCK_SIZE + 1, (unsigned char*)&chain, chain % BLOCK_SIZE, sizeof(short));
			chain = l_endian16(chain); // little endian
			DB_PRINTF("chain:%d\n",chain);
			
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
	root_blocks = l_endian16(boot.r_dirs) * sizeof(dir_ent) / BLOCK_SIZE ;
	root_blocks = root_blocks == 0 ? 1:root_blocks; /* root blocks less than block size */
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
						//DB_PRINTF("filepos:%d\n", i * BLOCK_SIZE / sizeof(dent) + j);
						return  i * BLOCK_SIZE / sizeof(dent) + j ;	
						break; 
					}
				}
				else
				{
					if (memcmp(dent.filename,filename,strlen(filename)) == 0)
					{
						//DB_PRINTF("filepos:%d\n", i * BLOCK_SIZE / sizeof(dent) + j);						
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
							//DB_PRINTF("filepos:%d\n", i * BLOCK_SIZE / sizeof(dent) + j);
							return  i * BLOCK_SIZE / sizeof(dent) + j ;	
							break; 
						}
					}
					else
					{
						if (memcmp(dent.filename,filename,strlen(filename)) == 0)
						{
							//DB_PRINTF("filepos:%d\n", i * BLOCK_SIZE / sizeof(dent) + j);						
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

int fat_mount(int dev_id)
{
	mount_list[0].dev_id = dev_id;
	mount_list[0].mount_point = '/';
	mount_list[0].fs[0]='F';
	mount_list[0].fs[1]='A';
	mount_list[0].fs[2]='T';
	mount_list[0].fs[3]='\0';
}

int fat_umount(int dev_id)
{
	memset(&mount_list[0], 0, sizeof(mount_list));

}

int format(int dev_id)
{
	int i = 0, j = 0, k = 0;
	int status = 0;
	int root_blocks;
	int fat_blocks;
	unsigned short block_size 		= BLOCK_SIZE;
	unsigned short root_dir 		= ROOT_DIR;
	unsigned short block_per_fat 	= BLOCK_PER_FAT;
	
	char fat[BLOCK_SIZE * BLOCK_PER_FAT] = {0x00};
	
	/* boot block */
	boot_block boot;
	/* dir entry */
	dir_ent root;

	boot.b_strap = 0xeb3c;
	memcpy(boot.man_desc, "MSWIN4.1", sizeof("MSWIN4.1"));
	boot.b_per_blocks = l_endian16(block_size); // BLOCK_SIZE
	boot.b_per_alloc = 0x4; // 1 block  block per cluster
	boot.r_blocks = 0x0100; // 4 blocks
	boot.fats = 1;
	boot.r_dirs = l_endian16(root_dir); // 64;
	boot.t2_blocks = l_endian32(disk.size);
	boot.m_desc	   = 0xf8;
	boot.b_per_fat = l_endian16(block_per_fat);
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
	
	/*  write FAT tables for the first block only as it only has the 
	 *	two reserved blocks	
	 */
	fat_blocks = b_endian16(boot.b_per_fat);
	
	//printf("fat_blocks:%d\t", fat_blocks);
	for (i = 1; i < boot.fats + 1 ;i++)
	{
		for (j = 0; j < fat_blocks; j++)
		{
			for (k = 0; k < BLOCK_SIZE; k+=2)
			{	
				if (j == 0 && k == 0)
				{
					fat[k] = boot.m_desc;
					fat[k+1] = 0xFF;
				}
				else if (j == 0 && k == 2)
				{
					fat[k] = 0xFF;
					fat[k+1] = 0xFF;
				}
				else
				{
					fat[k] = 0x00;
					fat[k+1] = 0x00;
				
				}
			}
			
			status = write(dev_id, j, fat, k, BLOCK_SIZE);
		}	
		

		if (status  == -1)
			return -1;
	}
	
	
	/* write root directory */

	root_blocks =  root_dir * sizeof(dir_ent) / BLOCK_SIZE;
	
	root_blocks = root_blocks == 0 ? 1 : root_blocks; /* root blocks less than block size */
	
	for(i = 0; i < root_blocks; i++)
	{
		for(j = 0; j < BLOCK_SIZE; j+=sizeof(dir_ent))
		{	
			if(i == 0 && j == 0)
			{	/* static allocation of root directory */
				memset(root.filename,0x20, 8);
				memcpy(root.filename,"ROOTDIR", sizeof("ROOTDIR"));
				memset(root.ext,0x20, 3);
				root.file_attr = 0x28;	
				write(dev_id, boot.fats * fat_blocks + 1 + i, (unsigned char*)&root, j, sizeof(dir_ent));
			}
			else
			{	
				/* empty directory entries available for use */
				memset(&root, 0x00, sizeof(root));
				write(dev_id, boot.fats * fat_blocks + 1 + i, (unsigned char*)&root, j , sizeof(dir_ent));
			}
		}
	}	
	if (status  == -1)
		return -1;

	return 0;

}

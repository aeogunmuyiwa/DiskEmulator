#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <strings.h>

#define FALSE 0
#define TRUE 1

//super block
#define FS_MAGIC           0xf0f03410
typedef struct{
	int magic;
	int nblocks;
	int nfatblocks;
	char filler[DISK_BLOCK_SIZE-3*sizeof(int)];
} super_block;

super_block mb;

//directory
#define MAX_NAME_LEN 6
#define VALID 1
#define NON_VALID 0
typedef struct{
	unsigned char used;
	char name[MAX_NAME_LEN+1];
	unsigned int length;
	unsigned int first_block;
} dir_entry;
#define N_DIR_ENTRIES (DISK_BLOCK_SIZE / sizeof(dir_entry))
dir_entry dir[N_DIR_ENTRIES];

// file allocation table
#define N_ADDRESSES_PER_BLOCK (DISK_BLOCK_SIZE / sizeof(int))
#define FREE 0
#define BUSY 2
#define EOFF 1
unsigned int *fat;


int fs_format(){

  disk_read(0, (char *)&mb);
  if(mb.magic == FS_MAGIC){
    printf("Refusing to format a formatted disk!\n");
    return -1;
  }
  // ????????????
  return 0;
}


void fs_debug()
{
  // ????????

}

int fs_mount()
{
  
  if(mb.magic == FS_MAGIC){
    printf("disc already mounted!\n");
    return -1;
  }

  // ????????

  return 0;
}


int fs_create(char *name)
{

  if(mb.magic != FS_MAGIC){
    printf("disc not mounted\n");
    return -1;
  }
  
 // ????

  return 0;
}



int fs_delete( char *name )
{

  if(mb.magic != FS_MAGIC){
    printf("disc not mounted\n");
    return -1;
  }

  // ????
  return 0;
}

int fs_getsize( char *name ){

  if(mb.magic != FS_MAGIC){
    printf("disc not mounted\n");
    return -1;
  }

   // ??????
   
}


int fs_read( char *name, char *data, int length, int offset )
{

  if(mb.magic != FS_MAGIC){
    printf("disc not mounted\n");
    return -1;
  }

  // ????
}


int fs_write( char *name, const char *data, int length, int offset )
{
 

  if(mb.magic != FS_MAGIC){
    printf("disc not mounted\n");
    return -1;
  }
  
  // ???
}

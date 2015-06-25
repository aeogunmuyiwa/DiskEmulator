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
	unsigned int nfatblocks;
	char filler[DISK_BLOCK_SIZE - 3*sizeof(int)];
} super_block;

super_block mb;

//directory
#define MAX_NAME_LEN 6
#define VALID 1
#define NON_VALID 0
typedef struct{
	unsigned char valid;          //TODO: Change to valid??
	char name[MAX_NAME_LEN+1];
	unsigned int length;
	unsigned int first_block;
} dir_entry;
#define N_DIR_ENTRIES (DISK_BLOCK_SIZE / sizeof(dir_entry))
dir_entry dir[N_DIR_ENTRIES];

char dir_initialized; // flag set when dir is initialized

// file allocation table
#define N_ADDRESSES_PER_BLOCK (DISK_BLOCK_SIZE / sizeof(int))
#define FREE 0
#define BUSY 2
#define EOFF 1
unsigned int *fat;

static void read_fat(int nfatblocks)
{
  int i;
  if(fat != NULL) return; // read_fat has been called already
  fat = malloc(N_ADDRESSES_PER_BLOCK * nfatblocks);
  for(i = 0; i < nfatblocks; ++i)
    disk_read(2 + i, (char*)(fat + i * N_ADDRESSES_PER_BLOCK));
}

static void read_dir()
{
  if(dir_initialized) return;
  dir_initialized = 1;
  disk_read(1, (char*)dir);
}

static void init_super_block()
{
  mb.magic = FS_MAGIC;
  mb.nfatblocks = ceil((double)disk_size() / (double)N_ADDRESSES_PER_BLOCK);
  mb.nblocks = disk_size() - mb.nfatblocks - 2;
}

static void init_empty_dir()
{
  int i;
  dir_initialized = 1;
  for(i = 0; i < N_DIR_ENTRIES; ++i)
    dir[i].valid = 0;
}

int fs_format()
{
  if(mb.magic == FS_MAGIC) {
    printf("Refusing to format a mounted disk\n");
    return -1;
  }

  disk_read(0, (char *)&mb);
  if(mb.magic == FS_MAGIC){
    printf("Refusing to format a formatted disk!\n");
    mb.magic = 0;
    return -1;
  }

  //Format disk
  init_super_block();
  init_empty_dir();
  
  //Flush
  disk_write(0, (char*)&mb);
  disk_write(1, (char*)dir);

  mb.magic = 0; // mark disk as unmounted

  return 0;
}

// pre: sb is initialized and magic number is valid
static void list_super_block(super_block* sb)
{
  printf("supberblock:\n\tmagic number is valid\n"
        "\t%d blocks on disk\n"
        "\t%d blocks for file allocation table\n",
        sb->nblocks, sb->nfatblocks);
}

// pre: fat and dir are initialized
static void list_files()
{
  unsigned int i, block;
  for(i = 0; i < N_DIR_ENTRIES; ++i)
    if(dir[i].valid) {
      printf("File \"%s\":\n\tsize: %d bytes\n\tBlocks:",
              dir[i].name, dir[i].length);
      for(block = dir[i].first_block; block != EOFF; block = fat[block])
        printf(" %d", block);
      printf("\n");
    }
}

void fs_debug()
{
  super_block sb;
  if(mb.magic == FS_MAGIC) {
    sb = mb;
  } else {  // disk not mounted
    disk_read(0, (char*)&sb);
    if(sb.magic != FS_MAGIC) {
      printf("disk not formatted\n");
      return;
    }
    read_dir();
    read_fat(sb.nfatblocks);
  }
  list_super_block(&sb);
  list_files();
}

int fs_mount()
{
  
  if(mb.magic == FS_MAGIC){
    printf("disk already mounted!\n");
    return -1;
  }

  disk_read(0, (char*)&mb);
  if(mb.magic != FS_MAGIC) {
    printf("Invalid magic number\n");
    return -1; 
  }
  read_dir();
  read_fat(mb.nfatblocks);

  return 0;
}

static dir_entry* first_invalid_dir_entry()
{
  int i;
  for(i = 0; i < N_DIR_ENTRIES; ++i)
    if(!dir[i].valid)
      return &dir[i];
  return NULL;
}

int valid_name(const char* name)
{
  int i;
  for(i = 0; i <= MAX_NAME_LEN; i++){
    if(name[i] == '\0')
      return 1;
  }
  return 0;
}

int fs_create(char *name)
{
  dir_entry* entry;

  if(mb.magic != FS_MAGIC){
    printf("disk not mounted\n");
    return -1;
  }
  
  entry = first_invalid_dir_entry();
  if(entry == NULL) {
    printf("disk full\n");
    return -1;
  }

  if(!valid_name(name)){
    printf("invalid name\n");
    return -1;
  }

  //Init dir
  entry->valid = 1;
  strcpy(entry->name, name);
  entry->length = 0;
  entry->first_block = EOFF;

  return 0;
}

dir_entry* found_file(const char* name){
  int i;
  for(i = 0; i < N_DIR_ENTRIES; ++i){
    if(dir[i].valid == 1 && strcmp(name, dir[i].name) == 0)
        return dir + i;
  }

  return NULL;
}


int fs_delete( char *name )
{

  if(mb.magic != FS_MAGIC){
    printf("disk not mounted\n");
    return -1;
  }

  if(!valid_name(name)){
    printf("invalid name\n");
    return -1;
  }

  dir_entry* file_to_del = found_file(name);

  if(!file_to_del){
    printf("file not found\n");
    return -1;
  }

  file_to_del->valid = 0;

  unsigned int next_block;
  for(next_block = file_to_del->first_block; next_block != EOFF; next_block = fat[next_block])
    fat[next_block] = FREE;

  return 0;
}


int fs_getsize( char *name ){

  if(mb.magic != FS_MAGIC){
    printf("disk not mounted\n");
    return -1;
  }

  if(!valid_name(name)){
     printf("invalid name\n");
    return -1;
  }

  dir_entry* file = found_file(name);

  if(!file){
    printf("file not found\n");
    return -1;
  }

  return file->length;
}

unsigned get_block(dir_entry* file, int offset){
  unsigned first_block = file->first_block;
  unsigned block_n = offset / DISK_BLOCK_SIZE;

  unsigned int block;
  unsigned next_block;
  for(next_block = first_block, block = 0; next_block != EOFF; next_block = fat[next_block], ++block){
    if(block == block_n)
      return next_block;
  }

  return -1; 
}


int fs_read( char *name, char *data, int length, int offset )
{

  if(mb.magic != FS_MAGIC){
    printf("disk not mounted\n");
    return -1;
  }

  if(!valid_name(name)){
     printf("invalid name\n");
    return -1;
  }

  dir_entry* file = found_file(name);

  if(!file){
    printf("file not found\n");
    return -1;
  }

  unsigned first_block = get_block(file, offset);

  if(first_block == -1)
    return 0;

  int bytes_read = 0;
  unsigned block_offset = offset % DISK_BLOCK_SIZE;
  unsigned block = first_block;
  int valid_size = file->length - offset;

  char current_block[DISK_BLOCK_SIZE];
  while(bytes_read < length){
    if(block == EOFF)
      return bytes_read;

    disk_read(block, current_block);

    int to_read = (valid_size - bytes_read < DISK_BLOCK_SIZE) ? valid_size - bytes_read : DISK_BLOCK_SIZE;
    memcpy(data, current_block + block_offset, to_read);
    block_offset = 0;
    bytes_read += to_read;
    

    block = fat[block];
  }

  return bytes_read;
}


int fs_write( char *name, const char *data, int length, int offset )
{
 

  if(mb.magic != FS_MAGIC){
    printf("disk not mounted\n");
    return -1;
  }

  if(!valid_name(name)){
     printf("invalid name\n");
    return -1;
  }

  dir_entry* file = found_file(name);

  if(!file){
    printf("file not found\n");
    return -1;
  }

  unsigned first_block = get_block(file, offset);

  if(first_block == -1){
    printf("invalid offset");
    return -1;
  }

  int bytes_written = 0;
  unsigned block_offset = offset % DISK_BLOCK_SIZE;
  unsigned block = first_block;

  char current_block[DISK_BLOCK_SIZE];
  
  disk_read(first_block, current_block);

  int to_write = DISK_BLOCK_SIZE;
  while(bytes_written < length){
    if(block == EOFF) {
      printf("invalid length\n");
      return -1;
    }

    if(length - bytes_written < DISK_BLOCK_SIZE){
      to_write = length - bytes_written;
      disk_read(block, current_block);
    }

    memcpy(current_block + block_offset, data, to_write);
    
    disk_write(block, current_block);

    block_offset = 0;
    bytes_written += to_write;
    block = fat[block];
  }

  return bytes_written;
}

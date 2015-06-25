#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <strings.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

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

void read_fat(int nfatblocks)
{
  int i;
  if(fat != NULL) return; // read_fat has been called already
  fat = malloc(N_ADDRESSES_PER_BLOCK * nfatblocks);
  for(i = 0; i < nfatblocks; ++i)
    disk_read(2 + i, (char*)(fat + i * N_ADDRESSES_PER_BLOCK));
}

void read_dir()
{
  if(dir_initialized) return;
  dir_initialized = 1;
  disk_read(1, (char*)dir);
}

void init_super_block()
{
  mb.magic = FS_MAGIC;
  mb.nfatblocks = ceil((double)disk_size() / (double)N_ADDRESSES_PER_BLOCK);
  mb.nblocks = disk_size() - mb.nfatblocks - 2;
}

void init_empty_dir()
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
void list_super_block(super_block* sb)
{
  printf("supberblock:\n\tmagic number is valid\n"
        "\t%d blocks on disk\n"
        "\t%d blocks for file allocation table\n",
        sb->nblocks, sb->nfatblocks);
}

// pre: fat and dir are initialized
void list_files()
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

dir_entry* first_invalid_dir_entry()
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

  return EOFF; 
}

int read_bytes(unsigned block, int offset, int length, char* dest){
  char buffer[DISK_BLOCK_SIZE];
  disk_read(block, buffer);

  int bytes_to_read = min(DISK_BLOCK_SIZE - offset, length);

  memcpy(dest, buffer + offset, bytes_to_read);

  return bytes_to_read;
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

  if(first_block == EOFF)
    return 0;

  int bytes_read = 0;
  unsigned block_offset = offset % DISK_BLOCK_SIZE;
  unsigned block = first_block;
  int bytes_to_read = min(file->length - offset, length);

  for(block = first_block; bytes_read < bytes_to_read && block != EOFF; block = fat[block]) {
    bytes_read += read_bytes(block, block_offset, bytes_to_read - bytes_read, data + bytes_read);
    block_offset = 0;
  }

  return bytes_read;
}

int write_bytes(unsigned block, int offset, int length, const char* src){
  char buffer[DISK_BLOCK_SIZE];
  int bytes_to_write = DISK_BLOCK_SIZE;

  if(offset > 0 || length < DISK_BLOCK_SIZE){
    disk_read(block, buffer);
    bytes_to_write = min(DISK_BLOCK_SIZE - offset, length);
  }

  memcpy(buffer + offset, src, bytes_to_write);
  disk_write(block, buffer);

  return bytes_to_write;
}

unsigned first_free_block(int from)
{
  int i;
  int fat_entries = N_ADDRESSES_PER_BLOCK * mb.nfatblocks;
  for(i = from; i < fat_entries; i++)
    if(fat[i] == FREE)
      return i;
  return EOFF;
}

void allocate_blocks(dir_entry* file, int nblocks)
{
  unsigned* block = &(file->first_block);
  while(*block != EOFF)
    block = fat + *block;
  int i;
  int last_free_block = 0;

  for(i = 0; i < nblocks; i++) {
    *block = first_free_block(last_free_block);

    if(*block == EOFF)
      break;
    
    last_free_block = *block;
    block = fat + *block;
  }
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
    
  allocate_blocks(file, blocks_to_allocate);

  unsigned first_block = get_block(file, offset);

  if(first_block == EOFF)
    return 0;

  int bytes_written = 0;
  unsigned block_offset = offset % DISK_BLOCK_SIZE;
  unsigned block = first_block;

  for(block = first_block; bytes_written < length && block != EOFF; block = fat[block]) {
    bytes_written += write_bytes(block, block_offset, length - bytes_written, data + bytes_written);
    block_offset = 0;
  }

  return bytes_written;
}

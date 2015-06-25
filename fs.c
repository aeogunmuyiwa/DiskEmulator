#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <strings.h>
#include <math.h>

#define UNMOUNT_DISK_ERROR "Disc not mounted"
#define DISK_ALREADY_MOUNTED_ERROR "Disk already mounted"
#define NO_SUCH_FILE_ERROR "File not found"
#define FILE_ALREADY_EXISTS_ERROR "File already exists"
#define CANT_FORMAT_MOUNTED "Cannot format a mounted disk!"
#define DIR_FULL "Directory is full"
#define NO_SPACE "No space available"
#define NO_SPACE_FOR_FILE "No space available for the entire file"
#define INVALID_FILENAME "Name length to big"
#define MISMATCH_MAGICNO "Magic number on disk does not match"
#define MATCHING_MAGICNO "Magic number is valid"

#define SUPERBLOCK_NUM 0
#define DIRBLOCK_NUM 1

#define FREE_STRING "FREE"
#define BUSY_STRING "BUSY"
#define EOFF_STRING "EOFF"

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
int nblocks, nfatblocks;

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
unsigned int *fat = NULL;

#define maximum_value(x, y) (((x) > (y)) ? (x) : (y))
#define minimum_value(x, y) (((x) < (y)) ? (x) : (y))
#define up_rounded_division(x, y) ((x+y-1)/y)

/* Checks if the disk is mounted */
int is_mounted() {
	if(mb.magic == FS_MAGIC){
		return 1;
	}
	return 0;
}

/*Checks if the name is valid */
int is_name_valid(char* string) {
	if(strlen(string) <= MAX_NAME_LEN) {
		return 1;
	}

	return 0;
}

/* Writes fat to disk */
void write_fat_to_disk() {
	int i;
	for (i = 0; i < nfatblocks; i++) {
		disk_write(2 + i, &((char *)fat)[i * DISK_BLOCK_SIZE]);
	}
}

/* Writes the superblock to the disk */
void write_superblock_to_disk() {
	disk_write(SUPERBLOCK_NUM, (char *)&mb);
}

/* Writes the directory block to the disk */
void write_dir_to_disk() {
	disk_write(DIRBLOCK_NUM, (char *)&dir);
}

/* Reads the superblock from the disk */
void read_superblock_from_disk() {
	disk_read(SUPERBLOCK_NUM, (char*)&mb);
}

/* Reads the directory from the disk */
void read_dir_from_disk() {
	disk_read(DIRBLOCK_NUM, (char*)dir);
}

/* Reads the fat from the disk */
void read_fat_from_disk() {
	if (fat == NULL) {
		fat = (unsigned int *) malloc(nfatblocks * DISK_BLOCK_SIZE);
	}
	int i;
	for(i = 0; i < nfatblocks; i++) {
		disk_read(2 + i, ((char *)fat) + i * DISK_BLOCK_SIZE);
		//disk_read(2 + i, (char*)(fat + i * DISK_BLOCK_SIZE));
	}
}

/* Returns a pointer to the entry in the dir table that matches with given
   file name, if there is no file with such name returns NULL */
dir_entry * get_dir_entry(char * file_name) {
	int i;
	for(i = 0; i < N_DIR_ENTRIES; i++) {
		dir_entry * entry = &(dir[i]);
		if((strcmp(entry->name, file_name) == 0) && (entry->used)) {
			return entry;
		}
	}
	return NULL;
}

/* Returns a pointer an empty entry in the dir table, if there
  is none returns null */
dir_entry *get_empty_dir_entry() {
	int i;
	for(i = 0; i < N_DIR_ENTRIES; i++) {
		dir_entry *entry = &(dir[i]);
		if(entry->used == FALSE) {
			return entry;
		}
	}
	return NULL;
}

/* Returns an empty fat entry */
unsigned int * get_empty_fat_entry(int * starting_index) {
	int i;
	for(i = *starting_index; i < nblocks; i++) {
		unsigned int * entry = &(fat[i]);
		if(*entry == FREE) {
			* starting_index = i;
			return entry;
		}
	}
	
	* starting_index = i;
	return NULL;
}

/* Prints the fat blocks, only used for testing */
void print_fat() {
	int i;
	for(i = 0; i < nblocks; i++) {
		if(fat[i] == FREE) {
			printf("%d %s\n", i, FREE_STRING);
		} else if(fat[i] == BUSY) {
			printf("%d %s\n", i, BUSY_STRING);
		} else if(fat[i] == EOFF) {
			printf("%d %s\n", i, EOFF_STRING);
		} else {
			printf("%d %d\n", i, fat[i]);
		}
		fflush(stdout);
	}
}

/* Formats the superblock */
void format_superblock() {
	nblocks = disk_size();
	nfatblocks = up_rounded_division(nblocks, N_ADDRESSES_PER_BLOCK);

	mb.magic = FS_MAGIC;
	mb.nblocks = nblocks;
	mb.nfatblocks = nfatblocks;

	write_superblock_to_disk();
}

/* Formats the directory */
void format_directory() {
	int i;
	for (i = 0; i < N_DIR_ENTRIES; i++) {
		dir[i].used = 0;
	}

	write_dir_to_disk();
}

/* Formats the fat */
void format_fat() {
	fat = (unsigned int *) calloc(nblocks, sizeof(int));
	
	int num_busy_blocks = 2 + nfatblocks;
	int i;
	for (i = 0; i < num_busy_blocks; i++) {
		fat[i] = BUSY;
	}
	
	write_fat_to_disk();
}

/* Formats the disk */
int fs_format(){
	if(is_mounted()){
		printf("%s\n", CANT_FORMAT_MOUNTED);
		return -1;
	}
	
	read_superblock_from_disk();
	
	format_superblock();
	format_directory();
	format_fat();

	mb.magic = 0;
	return 0;
}

/* Prints a debug message */
void fs_debug() {
	
	int current_magic = mb.magic;
	
	if(!is_mounted()) {
		read_superblock_from_disk();
		nblocks = mb.nblocks;
		nfatblocks = mb.nfatblocks;
		read_dir_from_disk();
		read_fat_from_disk();
		if(!is_mounted()) {
			printf("%s\n", MISMATCH_MAGICNO);
			return;
		} else {
			printf("%s\n", "superblock:");
			printf("%s\n", UNMOUNT_DISK_ERROR);
		}
	} else {
		printf("%s\n", "superblock:");
		printf("%s\n", MATCHING_MAGICNO);
	}
	
	printf("%d%s\n", mb.nblocks, "blocks on disk");
	printf("%d%s\n", mb.nfatblocks ,"blocks for file allocation table");

	int i;
	for(i = 0; i < N_DIR_ENTRIES; i++) {
		if(dir[i].used) {
			printf("%s%s%s\n", "File \"", dir[i].name, "\":" );
			printf("%s%d%s\n", "\tsize:", dir[i].length, " bytes");
			int fat_index = dir[i].first_block;
			if (fat_index != EOFF) {
				printf("blocks: ");
				do {
					printf("%d ", fat_index);
					fat_index = fat[fat_index];
				} while (fat_index != EOFF);
				printf("\n");
			}
		}
	}
	
	mb.magic = current_magic;
	
//	print_fat();
}

/* Mounts the disk */
int fs_mount() {
	if (is_mounted()) {
		printf("%s\n", DISK_ALREADY_MOUNTED_ERROR);	
		return -1;
	}

	read_superblock_from_disk();
	
	if (!is_mounted()) {
		printf("%s\n", MISMATCH_MAGICNO);	
		return -1;
	}

	nblocks = mb.nblocks;
	nfatblocks = mb.nfatblocks;
	
	read_dir_from_disk();
	read_fat_from_disk();
		
	return 0;
}

/* Creates a file with filename file */
int fs_create(char *name) {
	if (!is_mounted()) {
		printf("%s\n", UNMOUNT_DISK_ERROR);	
		return -1;
	}
	
	if(!is_name_valid(name)) {
		printf("%s\n", INVALID_FILENAME);
		return -1;
	}
	
	if(get_dir_entry(name) != NULL) {
		printf("%s\n", FILE_ALREADY_EXISTS_ERROR);	
		return -1;
	}

	
	dir_entry *entry = get_empty_dir_entry();
	
	if(entry == NULL) {
		printf("%s\n", DIR_FULL);
		return -1;
	}
	
	entry->used = TRUE;
	strcpy(entry->name, name);
	entry->length = 0;
	entry->first_block = EOFF;
	
	write_dir_to_disk();

	return 0;
}

/* Deletes a file with filename name */
int fs_delete( char *name ) {

	if (!is_mounted()) {
		printf("%s\n", UNMOUNT_DISK_ERROR);
		return -1;
	}

	if(!is_name_valid(name)) {
		printf("%s\n", INVALID_FILENAME);
		return -1;
	}

	dir_entry *entry = get_dir_entry(name);
	
	if (entry == NULL) {
		printf("%s\n", NO_SUCH_FILE_ERROR);
		return -1;
	}
	
	int fat_index = entry->first_block;
	int temp_index;
	while(fat_index != EOFF) {
		temp_index = fat_index;
		fat_index = fat[fat_index];
		fat[temp_index] = FREE;
	}


	entry->used = FALSE;
	
	write_dir_to_disk();
	write_fat_to_disk();
	
	return 0;
}

/* Returns the size of the file with filename name */
int fs_getsize( char *name ){
	if (!is_mounted()) {
		printf("%s\n", UNMOUNT_DISK_ERROR);
		return -1;
	}
	
	if(!is_name_valid(name)) {
		printf("%s\n", INVALID_FILENAME);
		return -1;
	}	
	
	dir_entry * entry = get_dir_entry(name);
	
	if (entry == NULL) {
		printf("%s\n", NO_SUCH_FILE_ERROR);
		return -1;
	}
	
	return entry->length;
}

/* Gets the first block to use */
unsigned int get_offset_block(unsigned int first_block, int offset) {
	int current_fat_offset = 0;
	while(current_fat_offset < offset)  {
		if (first_block == EOFF) {
			return -1;
		}
		first_block = fat[first_block];
		current_fat_offset++;
	}
	
	return first_block;
}

/* Reads data from blocks */
int  read_from_blocks(char * data, int read_size, unsigned int block, int first_block_offset) {
	char * temp = (char *) malloc(DISK_BLOCK_SIZE);
	int current_read_size = 0;
	while((current_read_size < read_size) && (block != EOFF)) {
		disk_read(block, temp);
		int mem_to_copy =  minimum_value(DISK_BLOCK_SIZE - first_block_offset, read_size - current_read_size);
		memcpy(data, temp + first_block_offset, mem_to_copy);
		first_block_offset = 0;
		
		block = fat[block];
		data += mem_to_copy;
		current_read_size += mem_to_copy;
	}
	
	free(temp);
	return current_read_size;
}

/* Reads data */
int fs_read( char *name, char *data, int length, int offset) {
	
	if (!is_mounted()) {
		printf("%s\n", UNMOUNT_DISK_ERROR);
		return -1;
	}
	
	if(!is_name_valid(name)) {
		printf("%s\n", INVALID_FILENAME);
		return -1;
	}
	
	dir_entry * entry = get_dir_entry(name);
	
	if (entry == NULL) {
		printf("%s\n", NO_SUCH_FILE_ERROR);
		return -1;
	}
	
	int fat_offset = (offset / DISK_BLOCK_SIZE);
	int block_offset = offset % DISK_BLOCK_SIZE;
	int read_size = minimum_value(entry->length - fat_offset * DISK_BLOCK_SIZE - block_offset, length);
	
	unsigned int first_read_block = (get_offset_block(entry->first_block, fat_offset));
	
	if (first_read_block == -1) {
		return -1;
	}
	
	int result = read_from_blocks(data, read_size, first_read_block, block_offset);
	
	return result;
}

/* Allocate new blocks */
int find_more_blocks(unsigned int * original_block, int number_of_blocks) {

	int blocks_found = 0;
	unsigned int * block = original_block;
	while (blocks_found < number_of_blocks) {
		if (*block == EOFF) {
			break;
		}
		
		block = &fat[*block];
		blocks_found++;
	}
	
	int fat_index = 0;
	while (blocks_found < number_of_blocks) {
		unsigned int * fat_entry = get_empty_fat_entry(&fat_index);
		if (fat_entry == NULL ) {
			break;
		}
		
		* block = fat_index;
		block = fat_entry;
		blocks_found++;
		fat_index++;
	}
	
	if (*block == FREE) {
		* block = EOFF;
	}
	
	return blocks_found;
}

/* Writes to blocks */
int  write_to_blocks(const char * data, int write_size, unsigned int block, int first_block_offset) {
	char * temp = (char *) malloc(DISK_BLOCK_SIZE);
	
	int current_write_size = 0;
	while((current_write_size < write_size) && (block != EOFF)) {
		
		int mem_to_copy;
		if((first_block_offset > 0) || (write_size - current_write_size < DISK_BLOCK_SIZE)) {
			disk_read(block, temp);
			mem_to_copy = minimum_value(DISK_BLOCK_SIZE - first_block_offset, write_size - current_write_size);
		} else {
			mem_to_copy = DISK_BLOCK_SIZE;
		}

		memcpy(temp + first_block_offset, data, mem_to_copy);
		
		disk_write(block, data);
		first_block_offset = 0;
		
		block = fat[block];
		
		data += mem_to_copy;
		current_write_size += mem_to_copy;
	}
	
	return current_write_size;
}

/* Writes data */
int fs_write( char *name, const char *data, int length, int offset ) {

	if (!is_mounted()) {
		printf("%s\n", UNMOUNT_DISK_ERROR);
		return -1;
	}
	
	if(!is_name_valid(name)) {
		printf("%s \n", INVALID_FILENAME);
		return -1;
	}
	
	dir_entry * entry = get_dir_entry(name);
	
	if (entry == NULL) {
		printf("%s\n", NO_SUCH_FILE_ERROR);
		return -1;
	}
	
	int fat_offset = (offset / DISK_BLOCK_SIZE);
	int block_offset = offset % DISK_BLOCK_SIZE;
	
	int blocks_needed = up_rounded_division(length + offset, DISK_BLOCK_SIZE);
	int blocks_found = find_more_blocks(&(entry->first_block), blocks_needed);
	
	if (blocks_found == 0) {
		printf("%s\n", NO_SPACE);
		return -1;
	} else if (blocks_found < blocks_needed) {
		printf("%s\n", NO_SPACE_FOR_FILE);
	}
	
	unsigned int first_write_block = get_offset_block(entry->first_block, fat_offset);
	
	int result =  write_to_blocks(data, length, first_write_block, block_offset);
	
	entry->length = maximum_value(entry->length, result + offset);
	
	write_dir_to_disk();
	write_fat_to_disk();
	
	return result;
}
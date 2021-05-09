#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/* TODO: Phase 1 */

#define FAT_EOC 0xFFFF

/* 
*	structure of the file system:
* 	super_block | FAT | root_directory | DATA ...
*/

struct super_block {
	/* super block occupy [1] block */
	uint8_t signature[8];				/* [8 bytes] Signature (must be equal to "ECS150FS") */
	uint16_t total_virtual_blk;			/* [2 bytes] Total amount of blocks of virtual disk */
	uint16_t root_dir_index;			/* [2 bytes] Root directory block index */
	uint16_t data_index;				/* [2 bytes] Data block start index */
	uint16_t total_data_blk;			/* [2 bytes] Amount of data blocks */
	uint8_t total_FAT_blk;				/* [1 byte] Number of blocks for FAT */
	uint8_t padding[4079];				/* [4079 bytes] Unused/Padding */
}__attribute__((packed));

struct root_directory {
	/* root_directory occupy [1] block with 128 entries*/
	uint8_t file_name[FS_FILENAME_LEN];		/* [16 bytes] Filename (including NULL character) */
	uint32_t file_size;				/* [4 bytes] Size of the file (in bytes) */
	uint16_t ini_data_index;			/* [2 bytes] Index of the first data block */
	uint8_t padding[10];				/* [10 bytes] Unused/Padding */
}__attribute__((packed));

struct file_descriptor {
	/* one entry of the file_descriptor holds the file's directory */
	struct root_directory* file_dir_entry;
	int offset;
}__attribute__((packed));

/* FAT occupy [total_data_blk * 2 / BLOCK_SIZE] blocks*/
static uint16_t* file_alloc_table;			/* [2 bytes per entry] FAT */

/* pointers to both the super block and root directory */
static struct super_block* super_blk;
static struct root_directory* root_dir;

/* pointer to the fd list */
static struct file_descriptor* fd_table;

/* flag to see if a disk is mounted */
static int mount_flag = 0;

/* 
*	helpers
*/
/* get the number of free fat entries */
int get_fat_free(void) {
	int result = 0;

	for(int i = 0; i < super_blk->total_data_blk; i++) {
		if(file_alloc_table[i] == 0)
			result++;
	}

	return result;
}

/* get the free root_dir entries */
int get_root_dir_free(void) {
	int result = 0;

	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(root_dir[i].file_name[0] == '\0')
			result++;
	}

	return result;
}

/* get the number of free fd_table */
int get_free_fd(void) {
	int result = 0;

	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if(fd_table[i].file_dir_entry == NULL)
			result++;
	}

	return result;
}

/* get the number of block for holding the file */
int get_num_file_blk(int fd) {
	int result = 0;
	int current_blk_index = fd_table[fd].file_dir_entry->ini_data_index;

	while(file_alloc_table[current_blk_index] != FAT_EOC) {
		current_blk_index = file_alloc_table[current_blk_index];
		result++;
	}

	result++;

	return result;
}

/* convert count to num of block */
int get_count_to_blk(size_t count) {
	if(count % BLOCK_SIZE == 0) {
		return count / BLOCK_SIZE;
	} else {
		return count / BLOCK_SIZE + 1;
	}
}

/* get the list of free fat indexes */
int* get_free_fat_indexes(int num_blk) {
	int count = 0;
	int* free_indexes = malloc(sizeof(int) * num_blk);

	for(int i = 0; i < super_blk->total_data_blk && count < num_blk; i++) {
		if(file_alloc_table[i] == 0) {
			free_indexes[count] = i;
			count++;
		}
	}

	return free_indexes;
}

/* get the file's all indexes */
int* get_file_fat_indexes(int fd) {
	int file_require_blk = get_num_file_blk(fd);
	int current_index;
	int* file_fat_indexes = malloc(sizeof(int) * file_require_blk);

	file_fat_indexes[0] = fd_table[fd].file_dir_entry->ini_data_index;
	current_index = file_fat_indexes[0];
	for(int i = 1; i < file_require_blk; i++) {
		file_fat_indexes[i] = file_alloc_table[current_index];
		current_index = file_alloc_table[current_index];
	}

	return file_fat_indexes;
}

/* earse all allocated data structures */
void clean_FS(void) {
	free(super_blk);
	free(root_dir);
	free(file_alloc_table);
}

/* set all the FD's root_dir pointer to NULL */
void init_fd_table(void) {
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].file_dir_entry = NULL;
		fd_table[i].offset = 0;
	}
}

/* 
*	library functions
*/
int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	/* a temporary pointer to the signiture */
	uint8_t* sig_tmp;

	/* initialize the FD table */
	fd_table = malloc(sizeof(struct file_descriptor) * FS_OPEN_MAX_COUNT);
	init_fd_table();

	/* allocate memory for super block, FAT, and root directory */
	super_blk = malloc(sizeof(struct super_block));
	root_dir = malloc(sizeof(struct root_directory) * FS_FILE_MAX_COUNT);

	/* open up the virtual disk */
	/* return -1 if the disk cannot be open */
	if(block_disk_open(diskname) == -1)
		return -1;

	/* load the super block and root directory with the corresponding block in the virtual disk */
	/* ERROR CHECKING */
	if(block_read(0, super_blk) == -1)
		return -1;
	
	if(block_read(super_blk->root_dir_index, root_dir) == -1)
		return -1;

	/* initialize the FAT once we have the super block information*/
	file_alloc_table = malloc(super_blk->total_FAT_blk * BLOCK_SIZE);

	/* since the FAT spans couple blocks, we need to load each block into the FAT */
	/* FAT starts at the second block and ends before the root_dir_block */
	/* since each address increment increment the whole address by 2 bytes, */
	/* so if to increment by 1 byte, divide BLOCK_SIZE by 2*/
	for(int i = 1; i <= super_blk->total_FAT_blk; i++){
		if(block_read(i, file_alloc_table + (BLOCK_SIZE / 2 * (i - 1))) == -1)
			return -1;
	}

	/* ERROR CHECKING */
	/* 1. check for signiture */
	sig_tmp = super_blk->signature;
	if(
		sig_tmp[0] != 'E' || 
		sig_tmp[1] != 'C' ||
		sig_tmp[2] != 'S' ||
		sig_tmp[3] != '1' ||
		sig_tmp[4] != '5' ||
		sig_tmp[5] != '0' ||
		sig_tmp[6] != 'F' ||
		sig_tmp[7] != 'S'
	) return -1;

	/* 2. check for block_disk_count */
	if(super_blk->total_virtual_blk != block_disk_count())
		return -1;

	mount_flag = 1;

	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	/* if the disk is not mounted */
	if(!mount_flag)
		return -1;

	/* write back super block, FAT, and root directory*/
	/* ERROR CHECKING */
	if(block_write(0, super_blk) == -1)
		return -1;

	for(int i = 1; i <= super_blk->total_FAT_blk; i++){
		if(block_write(i, file_alloc_table + (BLOCK_SIZE / 2 * (i - 1))) == -1)
			return -1;
	}

	if(block_write(super_blk->root_dir_index, root_dir) == -1)
		return -1;

	/* deallocate the memeory */
	clean_FS();

	/* close out the virtual disk */
	if(block_disk_close() == -1)
		return -1;

	mount_flag = 0;

	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	if(!mount_flag)
		return -1;

	fprintf(stdout, "FS Info:\n");
	fprintf(stdout, "total_blk_count=%d\n", super_blk->total_virtual_blk);
	fprintf(stdout, "fat_blk_count=%d\n", super_blk->total_FAT_blk);
	fprintf(stdout, "rdir_blk=%d\n", super_blk->root_dir_index);
	fprintf(stdout, "data_blk=%d\n", super_blk->data_index);
	fprintf(stdout, "data_blk_count=%d\n", super_blk->total_data_blk);
	fprintf(stdout, "fat_free_ratio=%d/%d\n", get_fat_free(), super_blk->total_data_blk);
	fprintf(stdout, "rdir_free_ratio=%d/%d\n", get_root_dir_free(), FS_FILE_MAX_COUNT);

	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	/* ERROR CHECKING */
	/* if a NULL string is passed or the disk is not mounted*/
	if(filename == NULL || mount_flag == 0)
		return -1;

	/* there is no more empty slot for a new file */
	if(get_root_dir_free() == 0)
		return -1;

	/* the string cannot exceed 15 characters since last character is NULL */
	if(strlen(filename) >= FS_FILENAME_LEN)
		return -1;

	/* if the name matches with one of the file: nope! */
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(strcmp(filename, (char*)root_dir[i].file_name) == 0)
			return -1;
	}

	/* SAFE TO PROCEED */
	/* find empty slot in root_dir and throw all the information into it */
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(root_dir[i].file_name[0] == '\0') {
			strcpy((char*)root_dir[i].file_name, filename);

			root_dir[i].file_size = 0;

			root_dir[i].ini_data_index = FAT_EOC;

			break;
		}
	}

	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	struct root_directory* root_dir_entry = NULL;

	/* ERROR CHECKING */
	if(filename == NULL || mount_flag == 0)
		return -1;

	/* if the file is not closed */
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if(fd_table[i].file_dir_entry != NULL) {
			if(strcmp(filename, (char*)(fd_table[i].file_dir_entry->file_name)) == 0)
				return -1;
		}
	}

	/* find the matching name within the root_dir */
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(strcmp(filename, (char*)root_dir[i].file_name) == 0) {
			/* found it !!! */
			/* assign it to a temp pointer that hold the root_dir entry */
			root_dir_entry = &(root_dir[i]);

			break;
		}
	}

	/* ERROR CHECKING */
	/* cannot find the file */
	if(root_dir_entry == NULL)
		return -1;

	/* SAFE TO PROCEED */
	/* start dealing with the FAT deallocation */
	uint16_t cur_fat_entry = root_dir_entry->ini_data_index;
	
	if(cur_fat_entry != FAT_EOC) {
		/* making sure that the file is not an empty file */
		while(file_alloc_table[cur_fat_entry] != FAT_EOC) {
		
			uint16_t next_fat_entry = file_alloc_table[cur_fat_entry];

			file_alloc_table[cur_fat_entry] = 0;

			cur_fat_entry = next_fat_entry;
		}

		file_alloc_table[cur_fat_entry] = 0;
	}

	/* deal with the root_dir reset */
	memset(root_dir_entry->file_name, '\0', FS_FILENAME_LEN);
	root_dir_entry->file_size = 0;
	root_dir_entry->ini_data_index = 0;

	return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	/* ERROR CHECKING */
	if(mount_flag == 0)
		return -1;

	/* list out all the files in the root directory */
	fprintf(stdout, "FS Ls:\n");
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(root_dir[i].file_name[0] != '\0')
			fprintf(stdout, "file: %s, size: %u, data_blk: %u\n", root_dir[i].file_name, root_dir[i].file_size, root_dir[i].ini_data_index);
	}

	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	struct root_directory* root_dir_entry = NULL;
	int free_fd_index = -1;

	/* ERROR CHECKING */
	if(get_free_fd() == 0 || filename == NULL || mount_flag == 0)
		return -1;

	/* find the matching name within the root_dir */
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(strcmp(filename, (char*)root_dir[i].file_name) == 0) {
			/* found it !!! */
			/* assign it to a temp pointer that hold the root_dir entry */
			root_dir_entry = &(root_dir[i]);

			break;
		}
	}

	/* ERROR CHECKING */
	/* cannot find the file */
	if(root_dir_entry == NULL)
		return -1;

	/* SAFE TO PROCEED */
	/* find the first empty entry of the FD table and throw the root_dir_entry in there */
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if(fd_table[i].file_dir_entry == NULL) {
			free_fd_index = i;

			fd_table[i].file_dir_entry = root_dir_entry;

			break;
		}
	}

	return free_fd_index;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	/* ERROR CHECKING */
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT || fd_table[fd].file_dir_entry == NULL || mount_flag == 0)
		return -1;

	/* SAFE TO PROCEED */
	/* set the index of the fd_table to be null again */
	fd_table[fd].file_dir_entry = NULL;
	fd_table[fd].offset = 0;

	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	/* ERROR CHECKING */
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT || fd_table[fd].file_dir_entry == NULL || mount_flag == 0)
		return -1;

	return fd_table[fd].file_dir_entry->file_size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	/* ERROR CHECKING */
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT || fd_table[fd].file_dir_entry == NULL || mount_flag == 0)
		return -1;

	if(offset > fd_table[fd].file_dir_entry->file_size)
		return -1;

	/* SAFE TO PROCEED */
	fd_table[fd].offset = offset;
	
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	int writable_blk;
	int offset;
	int write_byte;
	int* free_fat_index_list;

	/* ERROR CHECKING */
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT || fd_table[fd].file_dir_entry == NULL || mount_flag == 0)
		return -1;

	/* SAFE TO PROCEED */
	write_byte = 0;
	offset = fd_table[fd].offset;

	/* if the file is an empty file */
	if(fd_table[fd].file_dir_entry->ini_data_index == FAT_EOC) {
		/* get how many blocks can write into the FS with count */
		if(get_fat_free() > get_count_to_blk(count)) {
			writable_blk = get_count_to_blk(count);
			write_byte = count;
		} else {
			writable_blk = get_fat_free();
			write_byte = get_fat_free() * BLOCK_SIZE;
		}

		/* if there is no empty block or the count is zero that I can write on: return 0 byte written */
		if(writable_blk == 0)
			return write_byte;

		/* get the list of empty slots for holding the count size */
		free_fat_index_list = get_free_fat_indexes(writable_blk);

		/* assign the write bytes into the fd_table and set to the correct offset*/
		fd_table[fd].file_dir_entry->file_size = write_byte;
		fd_table[fd].file_dir_entry->ini_data_index = free_fat_index_list[0];
		fs_lseek(fd, offset + write_byte);

		/* update the FAT */
		for(int i = 0; i < writable_blk; i++) {
			if(i != writable_blk - 1)
				file_alloc_table[free_fat_index_list[i]] = free_fat_index_list[i + 1];
			else
				file_alloc_table[free_fat_index_list[i]] = FAT_EOC;
		}

		/* let's write the file into the FS */
		for(int i = 0; i < writable_blk; i++) {
			if(block_write(super_blk->data_index + free_fat_index_list[i], buf + (BLOCK_SIZE * i)) == -1)
				return -1;
		}
	/* if the file is not a new file */
	} else {
		int ori_file_size = fd_table[fd].file_dir_entry->file_size;
		int total_file_require_blk;
		int single_file_require_blk;
		int new_file_size;
		int more_new_blk;
		int* file_fat_indexes;
		void* tmp_buf = NULL;

		if(offset + count <= (size_t)ori_file_size) {
			new_file_size = ori_file_size;
		} else {
			new_file_size = offset + count;
		}

		/* save how many blks require to write the new file, and how many blks require to write the current file */
		total_file_require_blk = get_count_to_blk(new_file_size);
		single_file_require_blk = get_num_file_blk(fd);

		/* if the new file total block is more than what we currently have */
		if(total_file_require_blk > single_file_require_blk) {
			/* any more blk do we need ? */
			more_new_blk = total_file_require_blk - single_file_require_blk;

			/* if we have more free blks than what we needed */
			if(get_fat_free() >= more_new_blk) {
				writable_blk = more_new_blk;
				write_byte = count;
				fd_table[fd].file_dir_entry->file_size = new_file_size;
			} else {
				writable_blk = get_fat_free();
				write_byte = (single_file_require_blk + get_fat_free()) * BLOCK_SIZE - offset;
				fd_table[fd].file_dir_entry->file_size = (single_file_require_blk + get_fat_free()) * BLOCK_SIZE;
			}

			tmp_buf = malloc(total_file_require_blk * BLOCK_SIZE);
			memset(tmp_buf, '\0', total_file_require_blk * BLOCK_SIZE);

			file_fat_indexes = get_file_fat_indexes(fd);
			free_fat_index_list = get_free_fat_indexes(writable_blk);

			/* read the current file into a tmp_buf */
			for(int i = 0; i < single_file_require_blk; i++) {
				if(block_read(super_blk->data_index + file_fat_indexes[i], tmp_buf + (BLOCK_SIZE * i)) == -1)
					return -1;
			}

			/* copy those info into tmp_buf */
			memcpy(tmp_buf + offset, buf, count);

			/* update the FAT */
			int current_index = file_fat_indexes[single_file_require_blk-1];
			for(int i = 0; i < writable_blk; i++) {
				file_alloc_table[current_index] = free_fat_index_list[i];
				current_index = free_fat_index_list[i];
			}
			file_alloc_table[current_index] = FAT_EOC;

			/* let's write the file into the FS */
			int current_FAT_index = fd_table[fd].file_dir_entry->ini_data_index;
			for(int i = 0; i < single_file_require_blk + writable_blk; i++) {
				if(block_write(super_blk->data_index + current_FAT_index, tmp_buf + (BLOCK_SIZE * i)) == -1)
					return -1;

				current_FAT_index = file_alloc_table[current_FAT_index];
			}

			fs_lseek(fd, offset + write_byte);

		} else {
			if(new_file_size > ori_file_size) {
				fd_table[fd].file_dir_entry->file_size = new_file_size;
			}

			tmp_buf = malloc(single_file_require_blk * BLOCK_SIZE);
			memset(tmp_buf, '\0', single_file_require_blk * BLOCK_SIZE);

			file_fat_indexes = get_file_fat_indexes(fd);

			/* read the old file into a tmp_buf */
			for(int i = 0; i < single_file_require_blk; i++) {
				if(block_read(super_blk->data_index + file_fat_indexes[i], tmp_buf + (BLOCK_SIZE * i)) == -1)
					return -1;
			}

			/* modify the file */
			memcpy(tmp_buf + offset, buf, count);

			/* write the new file into the data blocks */
			for(int i = 0; i < single_file_require_blk; i++) {
				if(block_write(super_blk->data_index + file_fat_indexes[i], tmp_buf + (BLOCK_SIZE * i)) == -1)
					return -1;
			}

			write_byte = count;

			fs_lseek(fd, offset + write_byte);
		}
	}

	return write_byte;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	void* tmp_buf;
	int file_require_blk;
	int offset;
	int after_offset_size;
	int current_FAT_index;
	int read_byte;

	/* ERROR CHECKING */
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT || fd_table[fd].file_dir_entry == NULL || mount_flag == 0)
		return -1;
	
	if(fd_table[fd].file_dir_entry->file_size == 0)
		return 0;

	/* SAFE TO PROCEED */
	/* find out the sizes before and after the offset sizes */
	file_require_blk = get_num_file_blk(fd);
	offset = fd_table[fd].offset;
	after_offset_size = fd_table[fd].file_dir_entry->file_size - offset;

	/* allocate enough for the file first */
	tmp_buf = malloc(BLOCK_SIZE * file_require_blk);
	memset(tmp_buf, '\0', BLOCK_SIZE * file_require_blk);

	/* let's read the block into tmp_buf */
	current_FAT_index = fd_table[fd].file_dir_entry->ini_data_index;
	for(int i = 0; i < file_require_blk; i++) {
		if(block_read(super_blk->data_index + current_FAT_index, tmp_buf + (BLOCK_SIZE * i)) == -1)
			return -1;

		current_FAT_index = file_alloc_table[current_FAT_index];
	}

	if(count <= (size_t)after_offset_size) {
		/* if count is less than or equal to what we have left, just copy them to buf */
		tmp_buf += offset;
		memcpy(buf, tmp_buf, count);
		read_byte = count;
		/* set the offset to what is not read */
		fs_lseek(fd, offset + read_byte);
	} else {
		/* if count is greater than what we have, then put what we have left in buf */
		tmp_buf += offset;
		memcpy(buf, tmp_buf, after_offset_size);
		read_byte = after_offset_size;
		/* set the offset to what is not read */
		fs_lseek(fd, offset + read_byte);
	}

	return read_byte;
}

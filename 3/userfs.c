#include "userfs.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

static struct block *
ufs_add_block(struct block *last_block)
{
    struct block *new_block = (struct block *) malloc(sizeof(struct block));
    if (new_block == NULL)
	{
        ufs_error_code = UFS_ERR_NO_MEM;
        return NULL;
    }
    new_block->prev = NULL;
    new_block->next = NULL;
	new_block->occupied = 0;
    new_block->memory = (char *) malloc(sizeof(char) * BLOCK_SIZE);
    if (new_block->memory == NULL)
	{
        free(new_block);
        ufs_error_code = UFS_ERR_NO_MEM;
        return NULL;
    }

    if (last_block != NULL)
	{
        new_block->prev = last_block;
        last_block->next = new_block;
    }

    return new_block;
}

static void
ufs_delete_blocks(struct block *start_block)
{
	struct block *current_block = start_block;
	struct block *next_block = NULL;
	while (current_block != NULL)
	{
		next_block = start_block->next;
		free(current_block->memory);
		free(current_block);
		current_block = next_block;
	}
}

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	bool marked_as_deleted;
	/* PUT HERE OTHER MEMBERS */
};



/** List of all files. */
static struct file *file_list = NULL;
static struct file *last_file = NULL;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

struct file*
ufs_find_file(const char *filename)
{
	struct file* current_file = file_list;

	while (current_file != NULL)
	{
		if (current_file->name == filename && !current_file->marked_as_deleted)
		{
			return current_file;
		}
	}
	return NULL;
}

struct file*
ufs_create_file(const char* filename)
{
	struct file *new_file = (struct file*)malloc(sizeof(struct file));
    if (new_file == NULL)
	{
        ufs_error_code = UFS_ERR_NO_MEM;
        return NULL;
    }
	new_file->refs = 0;

	new_file->prev = last_file;
	new_file->next = NULL;
	if (file_list == NULL)
	{
		file_list = new_file;
	}
	if (last_file != NULL)
	{
		last_file->next = new_file;
	}
	last_file = new_file;
	
	new_file->block_list = ufs_add_block(NULL);
	if (new_file->block_list == NULL)
	{
        ufs_error_code = UFS_ERR_NO_MEM;
		free(new_file);
        return NULL;
	}
	new_file->last_block = new_file->block_list;

    new_file->name = (char *)malloc(sizeof(char) * strlen(filename));
    if (new_file->name == NULL)
	{
        ufs_error_code = UFS_ERR_NO_MEM;
		free(new_file->block_list);
        free(new_file);
        return NULL;
    }
	strcpy(new_file->name, filename);

	return new_file;
}

void ufs_delete_file(struct file* file_to_delete)
{
    ufs_delete_blocks(file_to_delete->block_list);
    free(file_to_delete->name);

    if (file_to_delete->prev != NULL)
	{
		file_to_delete->prev->next = file_to_delete->next;
	}
    if (file_to_delete->next != NULL) {
        file_to_delete->next->prev = file_to_delete->prev;
    }  

    if (file_list == file_to_delete)
        file_list = file_to_delete->next;

    if (last_file == file_to_delete) {
        last_file = file_to_delete->prev;
    }

    free(file_to_delete);
}

struct filedesc {
	struct file *file;
	/* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptors_count = 0;
static int file_descriptors_capacity = 0;

int
ufs_create_file_descriptor(struct file *current_file)
{
	struct filedesc* file_descriptor = (struct filedesc*)malloc(sizeof(struct filedesc));
	if (file_descriptor == NULL)
	{
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }
	file_descriptor->file = current_file;

	current_file->refs++;

	if (file_descriptors == NULL)
	{
		file_descriptors = (struct filedesc **)malloc(sizeof(struct filedesc*) * 20);
		if (file_descriptors == NULL)
		{
			ufs_error_code = UFS_ERR_NO_MEM;
			free(file_descriptor);
			return -1;
		}
		file_descriptors_capacity = 20;
	}
	else if (file_descriptors_capacity == file_descriptors_count)
	{
		struct filedesc **new_file_descriptors = (struct filedesc **)malloc(sizeof(struct filedesc*) * file_descriptors_capacity * 2);
		if (new_file_descriptors == NULL)
		{
			ufs_error_code = UFS_ERR_NO_MEM;
			free(file_descriptor);
			return -1;
		}
		file_descriptors_capacity *= 2;
		memcpy(new_file_descriptors, file_descriptors, sizeof(struct filedesc*) * file_descriptors_count);
		free(file_descriptors);
		file_descriptors = new_file_descriptors;
	}
	file_descriptors[file_descriptors_count] = file_descriptor;
	file_descriptors_count++;

	return file_descriptors_count;
}

struct file*
ufs_delete_file_descriptor(int fd)
{
	int index = fd - 1;
    struct filedesc * file_descriptor = file_descriptors[index];
    file_descriptors[index] = NULL;

    struct file* file = file_descriptor->file;
    file->refs--;
    free(file_descriptor);
    file_descriptors_count--;

	return file;
}


int
ufs_open(const char *filename, int flags)
{
	struct file* current_file = ufs_find_file(filename);

	if (current_file == NULL)
	{
		if (flags == UFS_CREATE)
		{
			current_file = ufs_create_file(filename);
		}
		else
		{
			ufs_error_code = UFS_ERR_NO_FILE;
			return -1;
		}
	}

	return ufs_create_file_descriptor(current_file);
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	struct filedesc* file_desc = file_descriptors[fd - 1];

	size_t remain_size = size;
	const char* remain_buf = buf;
	struct block* last_block = file_desc->file->last_block;
	while (remain_size != 0)
	{
		size_t write_size = BLOCK_SIZE - last_block->occupied;
		if (remain_size < write_size)
		{
			write_size = remain_size < write_size;
		}
		memcpy(&last_block[last_block->occupied].memory, remain_buf, write_size);

		remain_buf += write_size;
		remain_size -= write_size;

		last_block->occupied += write_size;
		if (remain_size != 0 || last_block->occupied == BLOCK_SIZE)
		{
			last_block = ufs_add_block(last_block);
			if (last_block == NULL)
			{
				return -1;
			}
		}
	}
	return size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)buf;
	(void)size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

int
ufs_close(int fd)
{
	struct file* current_file = ufs_delete_file_descriptor(fd);

	if (current_file->refs == 0 &&  current_file->marked_as_deleted)
	{
		ufs_delete_file(current_file);
	}

	return 0;
}

int
ufs_delete(const char *filename)
{
	struct file* file_to_delete = ufs_find_file(filename);
	if (file_to_delete == NULL)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (file_to_delete->refs > 0)
	{
		file_to_delete->marked_as_deleted = true;
	}
	else
	{
		ufs_delete_file(file_to_delete);
	}
	return 0;
}

void
ufs_destroy(void)
{
	for (int i = 0; i < file_descriptors_count; i++) {
		ufs_close(i + 1);
	}

	struct file* current_file = file_list;
	struct file* next_file = NULL;
	while (current_file != NULL)
	{
		next_file = current_file->next;
		ufs_delete_file(current_file);
		current_file = next_file;
	}

	free(file_list);
	free(last_file);
	free(file_descriptors);
}

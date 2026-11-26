/*
 * This is neotunes, a terminal music player written in C.
 * Copyright (C) 2026  Alexander Loff
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "neotunes.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * This function sets music_files to point to an array. The length of that array is the return value of this function. That array points to strings in a dynamically allocated section of memory. Those strings are the names of all mp3 files in the target directory (specified by the directory_path parameter). The array itself is also dynamically allocated. Both the strings in their block and the array in its block must be freed by the calling function. This should be done like so:
 * free(music_files[0]) // This frees the strings in their block
 * free(music_files) // This frees the array in its block
 */
int list_directory_entries(char* directory_path, char*** music_files) {
    DIR *dir;
    struct dirent *entry;

	// Open the directory
    dir = opendir(directory_path);
    if (dir == NULL) {
		perror(DIR_OPEN_ERR);
		exit(1);
    }

	// Create a dynamic array
#	define initial_capacity 10000
#	define resize_multiplier 1.5
	int capacity = initial_capacity;
	int size = 0;
	char* str_ptr = malloc(initial_capacity * sizeof(char));
	if (str_ptr == NULL) {
		perror(MEM_ALLOC_ERR);
		exit(1);
	}

	// Begin enumerating each file
	int num_songs = 0;
    while ((entry = readdir(dir)) != NULL) {
		// Extract basic file information
		char* filename = entry->d_name;
		int filetype = entry->d_type;
		int filename_len = strlen(filename);

		// Skip directories, other non-regular files, hidden files, and non-mp3 files
		if(filetype != DT_REG || filename[0] == '.' || strcmp(filename + filename_len - 4, ".mp3")) {
            continue;
		}

		// The current entry is valid and will be included in our array
		num_songs++;

		// Reallocate to get more space if str_ptr points to a block of too little size.
		int next_size = size + filename_len + 1; // +1 is to account for null terminator
		if (next_size > capacity) {
			capacity *= resize_multiplier;
			str_ptr = realloc(str_ptr, capacity);
			if (str_ptr == NULL) {
				perror(MEM_ALLOC_ERR);
				exit(1);
			}
		}

		// Copy the filename to the allocated space
		str_ptr[size] = '\0';
		strcpy(str_ptr + size, filename);
		size = next_size;
	}

	// dir is no longer needed
    closedir(dir);

	// Free extra space that was dynamically allocated for the filename strings on the heap, but was not necessary.
	str_ptr = realloc(str_ptr, size); // Reallocate to free space after the end of the string pointer array
	if (str_ptr == NULL) {
		perror(MEM_ALLOC_ERR);
		exit(1);
	}

	// Create the array that points to each string in the big string block
	// Allocate memory
	*music_files = malloc(num_songs * sizeof(char*));
	if (*music_files == NULL) {
		perror(MEM_ALLOC_ERR);
		exit(1);
	}
	// Populate the array
	for (int i = 0; i < num_songs; i++) {
		(*music_files)[i] = str_ptr;
		str_ptr += strlen(str_ptr) + 1; // +1 for null terminator
	}

    return num_songs;
}

// The value returned by this function should be quite read-only
char* get_music_path_from_env() {
	return getenv("XDG_MUSIC_DIR");
}

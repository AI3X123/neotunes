// Alexander Loff
// 10/13/2026
// Simple array queue for the neotunes music queue
// Function naming convention is q<purpose>

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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "neotunes.h"

#define initial_capacity 500
#define resize_multiplier 1.5

// This chunk's commented out because it's in the header file. It's here because I feel that a copy of it belongs in this file for completeness of the queue.c module.
	// struct struct MusicQueue {
	// 	size_t capacity;
	// 	size_t num_added;
	// 	size_t skip;
	// 	char** allocated;
	// };
	// typedef struct struct MusicQueue struct MusicQueue;

// Space for a queue is dynamically allocated, a queue is created, and a pointer to that queue is returned.
// The queue returned by this function should be deleted with the qdel function
struct MusicQueue* qnew() {
	// Put a queue on the heap
	struct MusicQueue* queue = malloc(sizeof(struct MusicQueue));
	if (queue == NULL) {
		perror(MEM_ALLOC_ERR);
		exit(1);
	}

	// Allocate space for the queue's array
	char** allocated = malloc(initial_capacity * sizeof(char*));
	if (allocated == NULL) {
		perror(MEM_ALLOC_ERR);
		exit(1);
	}

	// Initialize the queue's data
	queue->capacity = initial_capacity;
	queue->num_added = 0;
	queue->skip = 0;
	queue->allocated = allocated;

	// Return a pointer to the queue
	return queue;
}

// Delete the queue and free its memory
void qdel(struct MusicQueue* queue) {
	free(queue->allocated);
	queue->allocated = NULL;
	free(queue);
	queue = NULL;
}

// Reallocates the space for the queue's songs to have a larger capacity
void qresize(struct MusicQueue *queue) {
	queue->capacity *= resize_multiplier;
	queue->allocated = realloc(queue->allocated, queue->capacity);
}

// Append a song to the queue
// "song" needs to point to a character array that will not be modified for the duration of the queue's use
void qadd(struct MusicQueue *queue, char* song) {
	// If the queue is full, resize it
	if (queue->num_added >= queue->capacity) {
		qresize(queue);
	}

	// Add the song
	queue->allocated[queue->num_added] = song;
	queue->num_added++;
}

// Add a song to the queue immediately after the currently playing song
// "song" needs to point to a character array that will not be modified for the duration of the queue's use
void qaddnext(struct MusicQueue *queue, char* song) {
	// If the queue is full, resize it
	if (queue->num_added >= queue->capacity) {
		qresize(queue);
	}

	// Shift entries down the array
	for (size_t i = queue->num_added - 1; i >= queue->skip ; i--) {
		queue->allocated[i + 1] = queue->allocated[i];
	}

	// Add the song
	queue->allocated[queue->skip] = song;
	queue->num_added++;
}

// Remove an item from the queue and return the value
char* qpop(struct MusicQueue *queue) {
	// Queue is empty; no item to pop
	if (queue->skip == queue->num_added) {
		return NULL;
	}

	// Pop it
	char* to_pop = queue->allocated[queue->skip];
	queue->skip++;
	return to_pop;
}

// Nothing after this line is guarenteed to work properly

void move_play_back(struct MusicQueue *queue) {
	if(queue->skip > 0) {
		queue->skip--;
	}
}
void move_play(struct MusicQueue *queue) {
	if (queue->skip < queue->num_added) {
		queue->skip++;
	}
}
void del_cur(struct MusicQueue *queue) {
	// Shift entries up the array
	for (size_t i = queue->skip - 1; i < queue->num_added - 1; i++) {
		queue->allocated[i] = queue->allocated[i + 1];
	}

	queue->num_added--;
}

// This will get clunky when pausing is implemented. I don't like that this requires queue.c to care about the state of neotunes.c in a specific, non-util-like way. Probably will rewrite later.
void back(struct MusicQueue *queue, int now_playing) {
	if (now_playing) {
		if (queue->skip >= 2) {
			queue->skip -= 2;
		} else if (queue->skip >= 1) {
			queue->skip -= 1;
		}
	} else {
		if (queue->skip >= 1) {
			queue->skip -= 1;
		}
	}
}

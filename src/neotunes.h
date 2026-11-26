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

#ifndef NEOTUNES_H
#define NEOTUNES_H

#include <stddef.h>
#include <curses.h>

// Number of windows and aliases for their indexes based on what they are. These should only be used in neotunes.c and curses.c
#define NUM_WINDOWS 5
#define HEADER 0
#define LIBRARY 1
#define NOW_PLAYING 2
#define QUEUE 3
#define COMMAND 4

#define PIPE_OPEN_ERR "Failed to open a pipe"
#define FILE_OPEN_ERR "Failed to open a file"
#define DIR_OPEN_ERR "Failed to open a directory"
#define FORK_ERR "Failed to fork off a process"
#define MEM_ALLOC_ERR "Failed to allocate memory"

// path_utils.c
int joinnpath(char* combined, const char* path1, const char* path2, int buffer_size);

// posix_utils.c
int list_directory_entries(char* directory_path, char*** music_files);
char* get_music_path_from_env();

// mpgctl.c
int play_song(int write_pipe_fd, char* song_path);

// queue.c
struct MusicQueue { // I may eventually typedef this, but it's not a big deal to include "struct" a lot.
	size_t capacity;
	size_t num_added;
	size_t skip;
	char** allocated;
};
struct MusicQueue* qnew();
void qdel(struct MusicQueue *queue);
void qresize(struct MusicQueue *queue);
void qadd(struct MusicQueue *queue, char* song);
void qaddnext(struct MusicQueue *queue, char* song);
char* qpop(struct MusicQueue *queue);
void back(struct MusicQueue *queue, int now_playing);
void move_play_back(struct MusicQueue *queue);
void move_play(struct MusicQueue *queue);
void del_cur(struct MusicQueue *queue);

// curses_utils.c
void create_outer_windows(WINDOW* outer_windows[NUM_WINDOWS]);
void box_all_outer_windows(WINDOW* outer_windows[NUM_WINDOWS]);
void create_inner_windows(WINDOW* windows[NUM_WINDOWS], WINDOW* outer_windows[NUM_WINDOWS]);
void write_header(WINDOW* header_window);
void list_songs(const int num_songs, char** songs, const int start_at, const int selected, WINDOW* win_to_print_on);
// void add_song_to_window(char* song_name, WINDOW* window); // TODO probably delete this function
void replace_window_text(WINDOW* window, char* text, size_t len);
void print_queue(struct MusicQueue* queue, WINDOW* win_to_print_on, short is_playing);

#endif

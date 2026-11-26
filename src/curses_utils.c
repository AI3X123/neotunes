// Alexander Loff
// 10/15/2026
// I'm moving a bunch of stuff from neotunes.c to here to reduce the bloat of the driver file
// This file contains a bunch of the functions that I use as wrappers around the ncurses library

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
#include <curses.h>
#include <stdlib.h>
#include <string.h>

// Sizes of windows
#define HEIGHT_HEADER_WIN 3
#define HEIGHT_COMMAND_WIN 10
// #define PERCENT_OF_WIDTH_FOR_LIBRARY_WIN 40
// #define PERCENT_OF_WIDTH_FOR_NOW_PLAYING_WIN 30
#define PERCENT_OF_WIDTH_FOR_LIBRARY_WIN 60
#define PERCENT_OF_WIDTH_FOR_NOW_PLAYING_WIN 00

// Message printed to the header window
#define HEADER_MSG "NEOTUNES"

// Creates the outer windows which contain the borders and the inner windows
void create_outer_windows(WINDOW* outer_windows[NUM_WINDOWS]) {
	int screen_height, screen_width;
	getmaxyx(stdscr, screen_height, screen_width);

	int height_library_win = screen_height - HEIGHT_HEADER_WIN - HEIGHT_COMMAND_WIN;
	int width_library_win = screen_width * PERCENT_OF_WIDTH_FOR_LIBRARY_WIN / 100;

	int height_now_playing_win = screen_height - HEIGHT_HEADER_WIN - HEIGHT_COMMAND_WIN;
	int width_now_playing_win = screen_width * PERCENT_OF_WIDTH_FOR_NOW_PLAYING_WIN / 100;

	int height_queue_win = screen_height - HEIGHT_HEADER_WIN;
	int width_queue_win = screen_width - width_library_win - width_now_playing_win;

	int width_command_win = width_library_win + width_now_playing_win;

	outer_windows[HEADER] = newwin(HEIGHT_HEADER_WIN, screen_width, 0, 0);
	outer_windows[LIBRARY] = newwin(height_library_win, width_library_win, HEIGHT_HEADER_WIN, 0);
	outer_windows[NOW_PLAYING] = newwin(height_now_playing_win, width_now_playing_win, HEIGHT_HEADER_WIN, width_library_win);
	outer_windows[QUEUE] = newwin(height_queue_win, width_queue_win, HEIGHT_HEADER_WIN, width_library_win + width_now_playing_win);
	outer_windows[COMMAND] = newwin(HEIGHT_COMMAND_WIN, width_command_win, HEIGHT_HEADER_WIN + height_library_win, 0);
}

// Puts the borders on the edges of the outer windows
void box_all_outer_windows(WINDOW* outer_windows[NUM_WINDOWS]) {
	for(int i = 0; i<NUM_WINDOWS; i++) {
		box(outer_windows[i], 0, 0);
		wrefresh(outer_windows[i]);
	}
}

// Initializes the inner windows which contain the content of the program
void create_inner_windows(WINDOW* windows[NUM_WINDOWS], WINDOW* outer_windows[NUM_WINDOWS]) {
	for(int i = 0; i<NUM_WINDOWS; i++) {
		int y, x;
		getmaxyx(outer_windows[i], y, x);
		windows[i] = derwin(outer_windows[i], y - 2, x - 2, 1, 1);
	}
}

// Writes the HEADER_MSG string to the header window
void write_header(WINDOW* header_window) {
	mvwaddstr(header_window, 0, 0, HEADER_MSG);
	wrefresh(header_window);
}

// List the songs in the library window. (This may be used for listing songs in other windows at some point)
// TODO This function (and probably others) break when non-ascii chars are used, because non-ascii chars register as 1 character by strlen, but they can misprint and take up more than one column of space. This can occasionally result in things being a wee bit borked.
void list_songs(const int num_songs, char** songs, const int start_at, const int selected, WINDOW* win_to_print_on) {
	// Clear the window
	wclear(win_to_print_on);

	// See how much window space there is to work with
	int win_height, max_line_width;
	getmaxyx(win_to_print_on, win_height, max_line_width);

	// Print each song
	for(int i = start_at; i < num_songs && i - start_at < win_height; i++) {
		// Cut off ".mp3"
		int dot_len = strlen(songs[i]) - 4;
		songs[i][dot_len] = '\0';

		// Print the entry
		if (i == selected) {
			// Cut the song to fit the window
			char temp;
#			define extra_stuff_of_line_selected_with_nums 8
			if (dot_len + extra_stuff_of_line_selected_with_nums > max_line_width) {
				temp = songs[i][max_line_width - extra_stuff_of_line_selected_with_nums];
				songs[i][max_line_width - extra_stuff_of_line_selected_with_nums] = '\0';
			}

			// For the selected song, put an absolute line number and an indicator
			wprintw(win_to_print_on, ">  %3d: %s", i, songs[i]);

			// Return the song name to it's former state
			if (dot_len + extra_stuff_of_line_selected_with_nums > max_line_width) {
				songs[i][max_line_width - extra_stuff_of_line_selected_with_nums] = temp;
			} else if (dot_len + extra_stuff_of_line_selected_with_nums < max_line_width) {
				wprintw(win_to_print_on, "\n");
			}
		} else {
			// Cut the song to fit the window
			char temp;
#			define extra_stuff_of_line_unselected_with_nums 7
			if (dot_len + extra_stuff_of_line_unselected_with_nums > max_line_width) {
				temp = songs[i][max_line_width - extra_stuff_of_line_unselected_with_nums];
				songs[i][max_line_width - extra_stuff_of_line_unselected_with_nums] = '\0';
			}

			// Relative line numbers for all other songs
			wprintw(win_to_print_on, "  %3d: %s", abs(selected - i), songs[i]);

			// Return the song name to it's former state
			if (dot_len + extra_stuff_of_line_unselected_with_nums > max_line_width) {
				songs[i][max_line_width - extra_stuff_of_line_unselected_with_nums] = temp;
			} else if (dot_len + extra_stuff_of_line_unselected_with_nums < max_line_width) {
				wprintw(win_to_print_on, "\n");
			}
		}

		// Restore ".mp3"
		songs[i][dot_len] = '.';
	}

	// Refresh the window to display the changes
	wrefresh(win_to_print_on);
}

void print_queue(struct MusicQueue* queue, WINDOW* win_to_print_on, short is_playing) {
	// Clear the window
	wclear(win_to_print_on);

	// See how much window space there is to work with
	int win_height, max_line_width;
	getmaxyx(win_to_print_on, win_height, max_line_width);

	size_t skip = queue->skip - 1; // current song playing was already qpopped
	int start_at = 0 > ((int)(skip) - (win_height / 4)) ? 0 : (skip - (win_height / 4));

	char** songs = queue->allocated;

	// Print each song
	for(size_t i = start_at; i < queue->num_added && (int)(i - start_at) < win_height; i++) {
		// Cut off ".mp3"
		int dot_len = strlen(songs[i]) - 4;
		songs[i][dot_len] = '\0';

		// Print the entry
		if (i == skip && is_playing) {
			// Cut the song to fit the window
			char temp;
#			define extra_stuff_of_line_selected 3
			if (dot_len + extra_stuff_of_line_selected > max_line_width) {
				temp = songs[i][max_line_width - extra_stuff_of_line_selected];
				songs[i][max_line_width - extra_stuff_of_line_selected] = '\0';
			}

			// Absolute line number and an indicator for the selected song
			wprintw(win_to_print_on, ">  %s", songs[i]);

			// Return the song name to it's former state
			if (dot_len + extra_stuff_of_line_selected > max_line_width) {
				songs[i][max_line_width - extra_stuff_of_line_selected] = temp;
			} else if (dot_len + extra_stuff_of_line_selected < max_line_width) {
				wprintw(win_to_print_on, "\n");
			}
		} else {
			// Cut the song to fit the window
			char temp;
#			define extra_stuff_of_line_unselected 2
			if (dot_len + extra_stuff_of_line_unselected > max_line_width) {
				temp = songs[i][max_line_width - extra_stuff_of_line_unselected];
				songs[i][max_line_width - extra_stuff_of_line_unselected] = '\0';
			}

			// Relative line numbers for all other songs
			wprintw(win_to_print_on, "  %s", songs[i]);

			// Return the song name to it's former state
			if (dot_len + extra_stuff_of_line_unselected > max_line_width) {
				songs[i][max_line_width - extra_stuff_of_line_unselected] = temp;
			} else if (dot_len + extra_stuff_of_line_unselected < max_line_width) {
				wprintw(win_to_print_on, "\n");
			}
		}

		// Restore ".mp3"
		songs[i][dot_len] = '.';
	}

	// Refresh the window to display the changes
	wrefresh(win_to_print_on);
}

void add_song_to_window(char* song_name, WINDOW* window) {
	int dot_len = strlen(song_name) - 4;
	song_name[dot_len] = '\0'; // remove ".mp3"
	wprintw(window, "%s\n", song_name);
	song_name[dot_len] = '.'; // add ".mp3" back
	wrefresh(window);
}

void replace_window_text(WINDOW* window, char* text, size_t len) {
	wclear(window);
	waddnstr(window, text, len);
	wrefresh(window);
}

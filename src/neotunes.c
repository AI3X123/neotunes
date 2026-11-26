// I am Alexander Loff. This is neotunes. Development started on 6 September 2026

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

#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "neotunes.h"

// For keybinds:
#define key_esc 27 // Quit search without playing
#define key_enter 10 // Play or quit search and play
#define backspace 8 // Backspace for search mode
#define backspace2 127 // Backspace for search mode (I don't remember why I made two of these)
#define ctrl(x) ((x) & 0x1f) // Matches what ncurses getch returns when a key (x) and ctrl are hit together (allegedly)

#define query_str_max_len 150 // For search mode

#include "bluetooth.h" // This contains the MAC address of my earbuds

// Determine how deeply scrolled a window (LIBRARY) should be when a selected value changes
int determine_scroll_val(int target_select, int scroll_val, int window_height) {
	if (window_height < 5) {
		fprintf(stderr, "The window does not have sufficient height. Exiting now.\n");
		exit(1);
	}

	// Selection is edging down
	int forecasted_visually_selected = target_select - scroll_val;
	if (forecasted_visually_selected >= window_height - 2) {
		return target_select + 3 - window_height;
	}

	// Selection is at the very top of the list
	if (target_select <= 2) {
		return 0;
	}

	// Selection is edging up
	if (target_select < scroll_val + 2) {
		return target_select - 2;
	}

	// The selected value is not approaching an edge (it is bing chilling)
	return scroll_val;
}

int str_contains_case_insensitive(const char* str, const char* substr) {
	// Get lengths
	int str_len = strlen(str);
	int substr_len = strlen(substr);

	// Go through each possible position of str
	for (int i = 0; i <= str_len - substr_len; i++) {

		// Check if the next substr_len chars are substr
		int j;
		for (j = 0; j < substr_len; j++) {
			if (tolower(str[i + j]) != tolower(substr[j])) {
				break;
			}
		}

		// There's a match
		if (j == substr_len) {
			return 1;
		}
	}

	// There's no match
	return 0;
}

// I don't like using global state, but I can't think of a better way to hold this information such that exit_handler can access it
int mpg_listener_pid = 0;
int getcher_pid = 0;

// Handle exit stuff
void exit_handler(int signum) {
	// Restore terminal state and close ncurses
	// TODO when the terminal is resized, cleaning up ncurses fails. Then the music playing process is never terminated.
	keypad(stdscr, FALSE);
	echo();
	nocbreak();
	endwin();

	// Terminate child processes
	if (mpg_listener_pid != 0) {
		kill(mpg_listener_pid, SIGINT);
		waitpid(mpg_listener_pid, 0, 0);
	}

	if (getcher_pid != 0) {
		kill(getcher_pid, SIGINT);
		waitpid(getcher_pid, 0, 0);
	}

	_Exit(signum);
}
void normal_exit_handler() {
	exit_handler(0);
}

int main() {
	// Initialize some ncurses stuff
	// Setup the terminal and initscr (this is properly undone in the exit_handler)
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	// Create the pretty boxes around the windows
	WINDOW* outer_windows[NUM_WINDOWS];
	create_outer_windows(outer_windows);
	box_all_outer_windows(outer_windows);
	// Create the inner windows, which are the primary way of displaying information from this point forward
	WINDOW* windows[5];
	create_inner_windows(windows, outer_windows);
	// Write the title of the application in the header window
	write_header(windows[HEADER]);

	// Create a pipe for reading the ncurses user input and for receiving notifications when a song finishes
	int super_pipe[2];
	if ((pipe(super_pipe)) == -1) {
		perror(PIPE_OPEN_ERR);
		exit(1);
	}

	// For goofy reasons, it is necessary to fork off a "getcher" for ncurses. This is so that the main control loop contained in this function can read values from both keypresses and mpg_completion. When a key is pressed, or when an mpg123 process exits (meaning a song has finished), a message is sent though the "super_pipe," which is then read by the main control loop. Then the control loop behaves according to the input.
	int pid = fork();
	if (pid == -1) {
		perror(FORK_ERR);
		exit(1);
	}

	// Actions for the child process (the getcher) that reads from ncurses
	if (pid == 0) {
		// Close the read end of the pipe
		close(super_pipe[0]);

		// This is necessary for a clean exit
		signal(SIGINT, _Exit);

		// Loop for reading key presses
		char key[2];
		key[0] = 'k'; // 'k' for "Key press"
		while((key[1] = wgetch(windows[COMMAND])) != ERR) {
			write(super_pipe[1], key, 2);
		}

		// Handle improper getch parsing
		write(super_pipe[1], "f", 1); // 'f' for "Failure"
		fprintf(stderr, "There was an error reading a key with wgetch. The getcher is exiting now.");
		_Exit(0);
	}

	// The parent needs to store the pid after the fork
	getcher_pid = pid;

	// Setup the signal and exit handlers, which clean up child processes and such
	signal(SIGINT, exit_handler);
	atexit(normal_exit_handler);

	// Create data structures to represent the songs in XDG_MUSIC_DIR
	// TODO I want to redo the queue to have indexes instead of pointers in order to make some stuff not garbage
	char** songs;
	char* music_dir = get_music_path_from_env();
	int num_songs = list_directory_entries(music_dir, &songs);
	struct MusicQueue* queue = qnew();

	// Initialize a bunch of variables for the TUI
	int selected = 0;
	int scroll_val = 0; // The lib window usually ain't tall enough for all songs; this prints the songs starting with the scroll_val'th song

	enum modes {
		NORMAL_MODE,
		JUMP_SET_MODE,
		SEARCH_MODE,
		QUEUE_MODE,
		COMMAND_MODE,
	};
	enum modes mode = NORMAL_MODE;

	// State for JUMP_SET_MODE
	int jk_jump_distance = 1;
	// State for SEARCH_MODE
	char query_str[query_str_max_len] = "";
	for (int i = 0; i < query_str_max_len; i++) {
		query_str[i] = '\0';
	}
	int next = 0;

	int is_playing = false; // This variable is necessary to know whether or not to play when a new song is added to the queue.

	list_songs(num_songs, songs, scroll_val, selected, windows[LIBRARY]);

	// Always start by connecting to bluetooth
	{ // This is in brackets to make msg go out of scope so that it can be reused later
		char msg[] = "Attempting to connect to device with MAC address " BLUETOOTH_MAC_ADDRESS " via bluetooth";
		replace_window_text(windows[COMMAND], msg, 100);
		system("bluetoothctl connect " BLUETOOTH_MAC_ADDRESS " 2>&1 >/dev/null &");
		msg[0] = '\0';
		replace_window_text(windows[COMMAND], msg, 0);
	}

	// Begin the command-interpreting loop
	char command = '\0';
	char key = '\0';
	while(read(super_pipe[0], &command, 1)) {

		// Check if a failure is being reported
		if (command == 'f') {
			fprintf(stderr, "A subprocess experienced a failure. Exiting now.\n");
			exit(1);
		}

		// If there is a song on the queue, and none are currently playing, play the next song
		if (command == 'c' || (command == 's' && !is_playing)) {
			if (mpg_listener_pid != 0) {
				waitpid(mpg_listener_pid, 0, 0); // Avoid defunct processes. Release the dead process' spirit; may it find peace.
				mpg_listener_pid = 0; // The previous pid is no longer used; don't go killing unclaimed pids
			}

			// Pop the next song off the queue
			char* song = qpop(queue);

			// Update the display of the queue (This needs to happen after qpop so that a qadd during a song has the same queue state as this line)
			print_queue(queue, windows[QUEUE], song != NULL);

			// Check if the queue was empty
			if (song == NULL) {
				is_playing = false;
				continue;
			}
			is_playing = true;

			// Create an absolute path to the song
			int dirlen = strlen(music_dir);
			int songlen = strlen(song);
			int comblen = dirlen + songlen + 2; // +2 for the null-terminator and the extra slash
			char combined_path[comblen]; // Technically, this is on the stack, and that isn't great; however, when mpg forks off, it gets its own memory, and the stack context containing combined_path is never overwritten during the use of the variable in that process
			joinnpath(combined_path, music_dir, song, comblen);

			// Actually fork off the process to play the song
			mpg_listener_pid = play_song(super_pipe[1], combined_path);

			continue;
		} else if (command == 's' && is_playing) {
			continue; // An 's' sent while a song is playing can simply be ignored.
		}

		// From this point forward it is assumed that (command == 'k')

		// Get the key
		read(super_pipe[0], &key, 1); // Should this have error checking? Maybe.

		short activating = true; // This is used to allow changing modes during one keypress; this means that a keypress can do something and exit one mode, then cause an action in another mode.

		while(activating) {
			activating = false;

			// Process the key based on the mode
			switch (mode) {

				// Prefix j or k with a distance to jump
				case JUMP_SET_MODE:
				{
					// Increase the jk_jump_distance
					if ('0' <= key && key <= '9') {
						jk_jump_distance *= 10; // Shift left
						jk_jump_distance += (key - '0'); // Put the most recently pressed key in the one's place
						break;
					}

					// Reset state and exit JUMP_SET_MODE
					if (key != 'j' && key != 'k') {
						jk_jump_distance = 1;
					}
					mode = NORMAL_MODE;
					activating = true;
				}
				break;

				// Select items from the library
				case NORMAL_MODE:
				{

					// Do an action based on the key
					switch (key) {

						// Quit the application
						case 'q':
						{
							return 0;
						}
						break;

						// Jump down
						case 'j':
						case 'G':
						{
							selected = selected + jk_jump_distance;
							if (key == 'G' || selected >= num_songs) {
								selected = num_songs - 1;
							}

							int window_height = getmaxy(windows[LIBRARY]);
							scroll_val = determine_scroll_val(selected, scroll_val, window_height);

							list_songs(num_songs, songs, scroll_val, selected, windows[LIBRARY]);

							jk_jump_distance = 1;
						}
						break;

						// Jump up
						case 'k':
						case 'g':
						{
							selected = selected - jk_jump_distance;
							if (key == 'g' || selected < 0) {
								selected = 0;
							}

							int window_height = getmaxy(windows[LIBRARY]);
							scroll_val = determine_scroll_val(selected, scroll_val, window_height);

							list_songs(num_songs, songs, scroll_val, selected, windows[LIBRARY]);

							jk_jump_distance = 1;
						}
						break;

						// Play selected song
						case 'p':
						case key_enter:
						{
							// Add the selection to the queue
							qadd(queue, songs[selected]);

							// Draw the song name to the queue window
							print_queue(queue, windows[QUEUE], is_playing);

							// Send the command to play the song to the pipe
							write(super_pipe[1], "s", 1); // 's' for "Start"
						}
						break;

						// Put the selected song next in the queue
						case 'n':
						{
							// Add the selection to the queue
							qaddnext(queue, songs[selected]);

							// Draw the song name to the queue window
							print_queue(queue, windows[QUEUE], is_playing);

							// Send the command to play the song to the pipe
							write(super_pipe[1], "s", 1); // 's' for "Start"
						}
						break;

						// Skip the currently playing song
						case 's':
						{
							if (mpg_listener_pid != 0) {
								kill(mpg_listener_pid, SIGINT);
								waitpid(mpg_listener_pid, 0, 0);
							}
							mpg_listener_pid = 0;
							write(super_pipe[1], "c", 1); // 'c' for "Continue" (This command needs to be sent here because obviously the mpg_listener will not be sending it)
						}
						break;

						// Stop the currently playing song and go back to the previous one
						case 'b':
						{
							if (mpg_listener_pid != 0) {
								kill(mpg_listener_pid, SIGINT);
								waitpid(mpg_listener_pid, 0, 0);
							}
							mpg_listener_pid = 0;
							write(super_pipe[1], "c", 1); // 'c' for "Continue" (This command needs to be sent here because obviously the mpg_listener will not be sending it)

							// Set queue->skip to be 2 back, so that the next interpretted song is the one before the one currently playing.
							back(queue, NOW_PLAYING);

							// Draw the song name to the queue window
							print_queue(queue, windows[QUEUE], is_playing);

						}
						break;

						// Activate search mode
						case '/':
						{
							mode = SEARCH_MODE;
						}
						break;

						// Connect to my earbuds
						case 't':
						{
							char msg[] = "Attempting to connect to device with MAC address " BLUETOOTH_MAC_ADDRESS " via bluetooth";
							replace_window_text(windows[COMMAND], msg, 100);

							system("bluetoothctl connect " BLUETOOTH_MAC_ADDRESS " 2>&1 >/dev/null &");

							msg[0] = '\0';
							replace_window_text(windows[COMMAND], msg, 0);
						}
						break;

						// Initialize and enter JUMP_SET_MODE
						case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
						{
							jk_jump_distance = key - '0';
							mode = JUMP_SET_MODE;
						}
						break;

						// All of the code from this line to the end of the code for NORMAL_MODE is garbage

						// case 'c':
						// {
						// 	mode = COMMAND_MODE;
						// }

						// case 'r': // refresh song list
						// {
						// 	// this is gonna be a pain to implement - might be smart and lowkey more optimal to start using indexes for the queue instead of actual pointers; then I can just send **songs whenever I send queue, and songs can be accessed via indexes in queue.
						// }

						// Really crappy tools for manipulating the queue
						case 'h':
						{
							// Pretend to move what's playing without actually changing what's playing in order to manipulate the queue (must change current playing song back manually)
							move_play_back(queue);
							// Draw the song name to the queue window
							print_queue(queue, windows[QUEUE], is_playing);
						}
						break;
						case 'l':
						{
							// Pretend to move what's playing without actually changing what's playing in order to manipulate the queue (must change current playing song back manually)
							move_play(queue);
							// Draw the song name to the queue window
							print_queue(queue, windows[QUEUE], is_playing);
						}
						break;
						case 'd':
						{
							// completely wipe currently playing song from the queue
							del_cur(queue);
							print_queue(queue, windows[QUEUE], is_playing);
						}
						break;

						// Silent quit (eXit); doesn't stop the currently playing song
						case 'x':
						{
							// Restore terminal state and close ncurses
							keypad(stdscr, FALSE);
							echo();
							nocbreak();
							endwin();

							// Terminate child processes
							if (mpg_listener_pid != 0) {
								kill(mpg_listener_pid, SIGKILL); // Can't use SIGINT, because then mpg_listener_pid would interupt mpg123
								waitpid(mpg_listener_pid, 0, 0);
							}

							if (getcher_pid != 0) {
								kill(getcher_pid, SIGINT);
								waitpid(getcher_pid, 0, 0);
							}

							_Exit(0); // Don't let exit_handler fire
						}

						default: break; // This exists to make the compiler happy

					}

				}
				break;

				// Search the library
				case SEARCH_MODE:
				{
					if (key != key_enter && key != key_esc && key != '=') {

						// Modify the query_str
						if ((key == backspace || key == backspace2) && next > 0) {
							query_str[--next] = '\0';
						} else if (next < query_str_max_len) {
							query_str[next++] = key;
						}

						// Write the query_str to the command window
						replace_window_text(windows[COMMAND], query_str, next);

						// Go through each song and select the first one that matches query_str
						for (int i = 0; i<num_songs; i++) {
							if (str_contains_case_insensitive(songs[i], query_str)) {
								// Update the selection in the library window
								int target_select = i;
								int window_height = getmaxy(windows[LIBRARY]);
								scroll_val = determine_scroll_val(target_select, scroll_val, window_height);
								selected = target_select;
								list_songs(num_songs, songs, scroll_val, selected, windows[LIBRARY]);
								break;
							}
						}

						break;
					}

					// Prepare to exit search mode

					// Queue all songs that match the query_str
					if (key == '=') {
						// Loop through all songs and queue each one that matches query_str
						for (int i = 0; i<num_songs; i++) {
							if (str_contains_case_insensitive(songs[i], query_str)) {
								// Queue the selection
								qadd(queue, songs[i]);
								// Write the song name to the queue window
								// add_song_to_window(songs[i], windows[QUEUE]);
								print_queue(queue, windows[QUEUE], is_playing);
							}
						}

						// Start the first queued song if nothing is already playing
						write(super_pipe[1], "s", 1);
					}

					// Clear query_str from the command window
					for (int i = 0; i < query_str_max_len; i++) {
						query_str[i] = '\0';
					}
					next = 0;
					replace_window_text(windows[COMMAND], query_str, next);

					// Queue the selected song
					if (key == key_enter) {
						mode = NORMAL_MODE;
						activating = true;
						break;
					}

					mode = NORMAL_MODE;
				}
				break;

				// Manipulate the queue
				case QUEUE_MODE:
				{
					// Not yet implemented - no longer needs to be implemented immediately because of all of the 'h' and 'l' garbage in NORMAL_MODE
				}
				break;

				case COMMAND_MODE:
				{
					if (key != key_enter && key != key_esc && key != '=') {

						// Modify the query_str
						if ((key == backspace || key == backspace2) && next > 0) {
							query_str[--next] = '\0';
						} else if (next < query_str_max_len) {
							query_str[next++] = key;
						}

						// Write the query_str to the command window
						replace_window_text(windows[COMMAND], query_str, next);

						// Go through each song and select the first one that matches query_str
						for (int i = 0; i<num_songs; i++) {
							if (str_contains_case_insensitive(songs[i], query_str)) {
								// Update the selection in the library window
								int target_select = i;
								int window_height = getmaxy(windows[LIBRARY]);
								scroll_val = determine_scroll_val(target_select, scroll_val, window_height);
								selected = target_select;
								list_songs(num_songs, songs, scroll_val, selected, windows[LIBRARY]);
								break;
							}
						}

						break;
					}

					// Prepare to exit search mode

					// Queue all songs that match the query_str
					if (key == '=') {
						// Loop through all songs and queue each one that matches query_str
						for (int i = 0; i<num_songs; i++) {
							if (str_contains_case_insensitive(songs[i], query_str)) {
								// Queue the selection
								qadd(queue, songs[i]);
								// Write the song name to the queue window
								// add_song_to_window(songs[i], windows[QUEUE]);
								print_queue(queue, windows[QUEUE], is_playing);
							}
						}

						// Start the first queued song if nothing is already playing
						write(super_pipe[1], "s", 1);
					}

					// Clear query_str from the command window
					for (int i = 0; i < query_str_max_len; i++) {
						query_str[i] = '\0';
					}
					next = 0;
					replace_window_text(windows[COMMAND], query_str, next);

					// Queue the selected song
					if (key == key_enter) {
						mode = NORMAL_MODE;
						activating = true;
						break;
					}

					mode = NORMAL_MODE;

				}

				default: break; // This exists to make the compiler happy

			}
		}
	}

	// Everything in this block is a good practice; and a complete waste of time. The operating system will clean up for me. I am leaving these comments here as a reminder not to let something get borked if the code is rearranged later.
	// // Close both ends of the pipe
	// close(super_pipe[0]); // Read end
	// close(super_pipe[1]); // Write end
	// // Free memory
	// // songs
	// free(songs[0]);
	// free(songs);
	// songs = NULL;
	// // windows
	// for (int i = 0; i<NUM_WINDOWS; i++) {
	// 	delwin(windows[i]);
	// 	windows[i] = NULL;
	// }
	// for (int i = 0; i<NUM_WINDOWS; i++) {
	// 	delwin(outer_windows[i]);
	// 	outer_windows[i] = NULL;
	// }
	// // free the queue
	// qdel(queue);
	// queue = NULL;

	return 0;
}

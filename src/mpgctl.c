// Written on 10/13/2026 by Alexander Loff to fork off an mpg process and a listener with a callback.
// I have no love for this file, but this functionality is probably going to look like crap anywhere, so it might as well live here.

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

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "neotunes.h"

int mpg123_pid = 0; // This is global state; it is bad. It's important that mpg123_pid is not modified before forking, as this will affect processes other than the currently running listener.
void play_song_handler(__attribute__((unused)) int signum) {
	kill(mpg123_pid, SIGINT);
	waitpid(mpg123_pid, 0, 0);
	_Exit(1);
}

// This forks off a music player, then returns that player's pid. The fork will fork off another process, which will execvp into mpg123, which will play the song at song_path. The first fork waits for the second fork to finish, then notifies the parent process via write_pipe_fd that the song has finished playing.
int play_song(const int write_pipe_fd, char* song_path) {
	int pid = fork();

	if (pid == -1) {
		perror(FORK_ERR);
		exit(1);
	}

	// mpg123 listener instructions
	if (pid == 0) {
		// Create the mpg123 process
		pid = fork();
		if (pid == -1) {
			write(write_pipe_fd, "f", 1); // 'f' for "Failure"
			perror(FORK_ERR);
			_Exit(1);
		}

		// Prepare to terminate the child
		mpg123_pid = pid;
		signal(SIGINT, play_song_handler);

		// Child process does an execvp into mpg123 and plays the song at song_path
		if (pid == 0) {
			// Setup args for mpg123
			// If these strings aren't on their own lines, my compiler yells at me. I do not like it when my compiler yells at me.
			// char mpg_arg[] = "mpg123";
			// char q_arg[] = "-q";
			// char noctl_arg[] = "--no-control";
			// Replacing former mpg123 w/ this: ffplay -nodisp -autoexit -loglevel quiet
			char mpg_arg[] = "ffplay";
			char q_arg[] = "-nodisp";
			char noctl_arg[] = "-autoexit"; // HEY HYE HYE YOU reADING THIS BLOCK listen UP!! I THE THIS BLOCK GARBO BECAUSE MPG123 GOT BORKED FOR LITERALLY NO REASON RIGTH BEFORE NCL AND I NEEDED A QUICK AND DIRTY FIX. tHIS PART IS UGLY FORMATTED AND REAL UGLY, BUT FFPLAY IS DROPIN REPLACEMENT FOR MPG21E WITH GIVEN ARGS BASICALLY OKIE POKIE:wq
			char noctl_arg1[] = "-loglevel";
			char noctl_arg2[] = "quiet";
			char noctl_arg3[] = "-nostats";
			char noctl_arg4[] = "-hide_banner";
			char *args[] = {mpg_arg, q_arg, noctl_arg, noctl_arg1, noctl_arg2, noctl_arg3, noctl_arg4, song_path, NULL};

			// execvp into mpg123
			// execvp("mpg123", args);
			execvp("ffplay", args);

			// Handle execvp failure
			write(write_pipe_fd, "f", 1); // 'f' for "Failure"
			perror("execvp failed");
			_Exit(127);
		}

		// Wait for completion, then use the callback to notify the parent.
		waitpid(mpg123_pid, 0, 0);
		write(write_pipe_fd, "c", 1); // 'c' for "Complete"

		_Exit(0);
	}

	// Return the child's pid to the parent
	return pid;
}

# makefile for neotunes

# This is neotunes, a terminal music player written in C.
# Copyright (C) 2026  Alexander Loff
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

.PHONY: all clean debug

binary = neotunes
files = src/neotunes.c src/path_utils.c src/posix_utils.c src/mpgctl.c src/queue.c src/curses_utils.c
gcc_flags = -o $(binary) -lncurses -lpanel -Wall -Wextra -Wfloat-equal -Wshadow -Wcast-align -Wstrict-prototypes -Wwrite-strings -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wformat=2 -Werror -Wuninitialized -Winit-self -Wpointer-arith -O

all: $(files)
	gcc $(gcc_flags) $(files)

debug: $(files)
	gcc -g -O0 $(gcc_flags) $(files)

clean:
	rm -f $(binary)

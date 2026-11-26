// Alexander Loff 10/10/2026

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
#include <string.h>

// This is used to combine paths. For instance, '/home/myhome' and 'myfile' would result in the combined string '/home/myhome/myfile'. That said, '/home/myhome/' and './myfile' would become '/home/myhome/./myfile'
// The result string is written to combined.
// This function returns 0 for success. If buffer_size would be exceeded by the combined path, then it returns 1. If 1 is returned, then the data at "char* combined" will not be modified.
int joinnpath(char* combined, const char* path1, const char* path2, int buffer_size) {
	int len = strlen(path1) + strlen(path2) + 1;

	// Exception case
	if (len >= buffer_size) return 1; // if len == buffer_size, then a null byte would be written outside of the bounds of the array, creating undefined behavior.

	// Write the stuff to combined
	for (int i = 0; i < buffer_size; i++) {
		combined[i] = '\0';
	}
	strcat(combined, path1); // strncat? I hardly know 'er!
	strcat(combined, "/");
	strcat(combined, path2);

	return 0;
}

/*
 * bigfiles.c
 *
 * Copyright 2014 Bob Parker <rlp1938@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include "config.h"

char *helptext = "\n\tUsage: duplicates [option] dir_to_search\n"
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n"
  ;


void help_print(int forced);
char *dostrdup(const char *s);
FILE *dofopen(const char *path, const char *mode);
void recursedir(char *headdir, FILE *fpo);



int main(int argc, char **argv)
{
	int opt, result, verbosity;
	struct stat sb;
	char *workfile0;
	char *workfile1;
	char command[FILENAME_MAX];
	FILE *fpo, *fpi;
	char *topdir;
	char line[PATH_MAX];

	// set default values

	while((opt = getopt(argc, argv, ":hvdc:")) != -1) {
		switch(opt){
		case 'h':
			help_print(0);
		break;
		case 'd':
			//keepworkfiles = 1;	// don't trash the workfiles at end
		break;
		case 'c':

		break;
		case 'v':
			verbosity++;	// 4 levels of verbosity, 0-3. 0 no progress
							// report, 1 print every 100th pathname,
							// 2 every tenth, 3 (and above) print every
							// pathname.
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			help_print(1);
		break;
		case '?':
			fprintf(stderr, "Illegal option: %c\n",optopt);
			help_print(1);
		break;
		} //switch()
	}//while()
	// now process the non-option arguments

	// 1.Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No directory provided\n");
		help_print(1);
	}

	// 2. Check that the dir exists.
	if ((stat(argv[optind], &sb)) == -1){
		perror(argv[optind]);
		help_print(EXIT_FAILURE);
	}

	// 3. Check that this is a dir
	if (!(S_ISDIR(sb.st_mode))) {
		fprintf(stderr, "Not a directory: %s\n", argv[optind]);
		help_print(EXIT_FAILURE);
	}

	// generate my workfile names
	sprintf(command, "/tmp/%sbigfiles0", getenv("USER"));
	workfile0 = dostrdup(command);
	sprintf(command, "/tmp/%sbigfiles1", getenv("USER"));
	workfile1 = dostrdup(command);

	// List the files
	topdir = argv[optind];
	{
		// get rid of trailing '/'
		int len = strlen(topdir);
		if (topdir[len-1] == '/') topdir[len-1] = '\0';
	}
	fpo = dofopen(workfile0, "w");
	recursedir(topdir, fpo);
	fclose(fpo);
	// Now sort them
	if (setenv("LC_ALL", "C", 1) == -1){	// sort bitwise L-R
		perror("LC_ALL=C");
		exit(EXIT_FAILURE);
	}
	sprintf(command, "sort -nr %s > %s",
						workfile0, workfile1);
	result = system(command);
	if(result == -1){
		perror(command);
		exit(EXIT_FAILURE);
	}

	// Now list the results
	fpi = dofopen(workfile1, "r");
	while(fgets(line, PATH_MAX, fpi)){
		fputs(line, stdout);
	}

	return 0;
}

FILE *dofopen(const char *path, const char *mode)
{
	// fopen with error handling
	FILE *fp = fopen(path, mode);
	if(!(fp)){
		perror(path);
		exit(EXIT_FAILURE);
	}
	return fp;
} // dofopen()

void recursedir(char *headdir, FILE *fpo)
{
	/* open the dir at headdir and process according to file type.
	*/
	DIR *dirp;
	struct dirent *de;

	dirp = opendir(headdir);
	if (!(dirp)) {
		perror(headdir);
		exit(EXIT_FAILURE);
	}
	/*
	struct dirent {
		ino_t          d_ino;       // inode number
        off_t          d_off;       // not an offset; see NOTES
        unsigned short d_reclen;    // length of this record
        unsigned char  d_type;      // type of file; not supported
                                    // by all filesystem types
        char           d_name[256]; // filename
	};

	DT_BLK      This is a block device.
    DT_CHR      This is a character device.
    DT_DIR      This is a directory.
    DT_FIFO     This is a named pipe (FIFO).
    DT_LNK      This is a symbolic link.
    DT_REG      This is a regular file.
    DT_SOCK     This is a UNIX domain socket.
    DT_UNKNOWN  The file type is unknown.

	*/
	while((de = readdir(dirp))) {
		if (strcmp(de->d_name, "..") == 0) continue;
		if (strcmp(de->d_name, ".") == 0) continue;

		switch(de->d_type) {
			char newpath[FILENAME_MAX];
			struct stat sb;
			// Nothing to do for these.
			case DT_BLK:
			case DT_CHR:
			case DT_FIFO:
			case DT_SOCK:
			continue;
			break;
/*
 * Processing for regular files and symlinks.
 * The output record looks like this:
 * <-- files size in bytes --> <-- path -->
*/
			case DT_LNK:
			case DT_REG:
			strcpy(newpath, headdir);
			strcat(newpath, "/");
			strcat(newpath, de->d_name);
			if (stat(newpath, &sb) == -1) {
				perror(newpath);
				break;
			}

			fprintf(fpo, "%lu %s\n", sb.st_size, newpath );
			break;
			case DT_DIR:
			// recurse using this pathname.
			strcpy(newpath, headdir);
			strcat(newpath, "/");
			strcat(newpath, de->d_name);
			recursedir(newpath, fpo);
			break;
			// Just report the error but nothing else.
			case DT_UNKNOWN:
			fprintf(stderr, "Unknown type:\n%s/%s\n\n", headdir,
					de->d_name);
			break;

		} // switch()
	} // while
	closedir(dirp);
} // recursedir()

char *dostrdup(const char *s)
{
	/*
	 * strdup() with in built error handling
	*/
	char *cp = strdup(s);
	if(!(cp)) {
		perror(s);
		exit(EXIT_FAILURE);
	}
	return cp;
} // dostrdup()

void help_print(int forced){
    fputs(helptext, stderr);
    exit(forced);
} // help_print()

/* extractrom.c - Extracts the rom.zip file from a RUU update.
 *
 * Copyright (C) 2010 Kenny Millington
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * REQUIREMENTS
 * This program requires a patched unshield 0.6 to correctly support newer
 * installshield cab files.
 *
 * $ wget http://sourceforge.net/projects/synce/files/Unshield/0.6/unshield-0.6.tar.gz/download
 * $ tar xzf unshield-0.6.tar.gz
 * $ cd unshield-0.6
 * $ patch -p1 < ../unshield.patch
 * $ ./configure --prefix=/usr && make && sudo make install
 * 
 * COMPILATION
 * $ gcc -Wall -lunshield -o extractrom extractrom.c 
 * 
 * USAGE
 * $ ./extractrom /path/to/RUU.exe
 *
 * If successful it will create rom.zip in your current directory.
 *
 * NOTES
 * The code could probably be better, but c'est la vie! :>
 *
 * TODO
 * 1. Cleanup the temp directory.
 * 2. Refactor extract_ruu_file so it does one-pass to extract both of the 
 *    required files.
 * 3. extract_ruu_file needs to try CHUNK_SIZE * 1.5 if it fails to find a file
 *    incase the matching strings lies over a chunk boundary (or some other 
 *    method to catch this scenario).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libunshield.h>

#define CHUNK_SIZE 1024
#define DATA1_LENGTH_OFFSET 24
#define DATA1_PREFIX "Disk1\\"
#define DATA1_FILELEN_BUFFER 20
#define ROM_ZIP_GROUP "<Support>Language Independent OS Independent Files"
#define PATH_MAX 256

bool extract_rom_zip(char *path) {
    bool result = false;
    int i;
    char *filename;
    char *output_path = calloc(1, strlen(path) + 9);
    strncpy(output_path, path, strlen(path));
    strncat(output_path, "/rom.zip", 8);
    
    unshield_set_log_level(UNSHIELD_LOG_LEVEL_LOWEST);
    Unshield *unshield = unshield_open("data1.cab");
    UnshieldFileGroup *group = unshield_file_group_find(unshield,ROM_ZIP_GROUP);
    
    for(i = group->first_file; i <= group->last_file; i++) {
        if(unshield_file_is_valid(unshield, i)) {
            filename = (char *)unshield_file_name(unshield, i);

            if(strncmp(filename, "rom.zip", 7) == 0) {
                unshield_file_save(unshield, i, output_path);
                result = true;
                break;
            }
        }
    }

    unshield_close(unshield);
    
    return result;
}

bool extract_ruu_file(FILE *ruu, char *filename, int chunk_size) {
    char *buffer = malloc(sizeof(char)*chunk_size);
    size_t length;
    int offset = 0;
    bool result = false;
    
    char *search = calloc(sizeof(char),
                          strlen(filename) + strlen(DATA1_PREFIX) + 1);

    strncpy(search, DATA1_PREFIX, strlen(DATA1_PREFIX));
    strncat(search, filename, strlen(filename));
    
    rewind(ruu);
    do {
        int i = 0;
        length = fread(buffer, 1, chunk_size, ruu);
        for(i=0; i <= length - strlen(search); i++) {
            if(memcmp(search, buffer + i, strlen(search)) == 0) {
                offset += i;
                fseek(ruu, offset + DATA1_LENGTH_OFFSET, SEEK_SET);
                
                /* Obtain the file length
                 * (TODO: probably a better way of doing this).
                 */
                int j = 0;
                char lenbuf;
                char filelen[DATA1_FILELEN_BUFFER];
                memset(&filelen, 0, DATA1_FILELEN_BUFFER);
                fread(&lenbuf, 1, 1, ruu);
                for(j=0; memcmp(&lenbuf, "\x00", 1) != 0; j++) {
                    filelen[j] = lenbuf;
                    fread(&lenbuf, 1, 1, ruu);
                }
                
                int filelength;
                sscanf(filelen, "%d", &filelength);
                
                FILE *out = fopen(filename, "wb");
                
                while(filelength > 0) {
                    length = fread(buffer, 1, chunk_size, ruu);
                    fwrite(buffer, 1, length, out);
                    filelength -= length;
                }
                
                fclose(out);

                result = true;
                break;

            }
        }

        offset += length;
    } while(!result && length == chunk_size);

    free(buffer);
    free(search);

    return result;
}

int main(int argc, char **argv) {
    if(argc < 2) {
        printf("Usage: %s <RUU-file.exe>\n", argv[0]);
        return 1;
    }
    
    // Open RUU file
    FILE *ruu = fopen(argv[1], "rb");
    
    if(ruu == NULL) {
        printf("Error: '%s' does not exist!\n", argv[1]);
        return 2;
    }
    
    // Save current working directory
    char currdir[PATH_MAX];
    getcwd(currdir, PATH_MAX);

    // Create and change into temporary directory
    char tempdir[PATH_MAX];
    snprintf(tempdir, PATH_MAX, "/tmp/extractrom-XXXXXX");
    mkdtemp(tempdir);
    chdir(tempdir);
    
    // Extract installshield cab files from RUU
    printf("Extracting data1.cab...\n");
    if(!extract_ruu_file(ruu, "data1.cab", CHUNK_SIZE)) {
        printf("Error: Failed to extract data1.cab from %s\n", argv[1]);
        fclose(ruu);
        return 3;
    }
    
    printf("Extracting data1.hdr...\n");
    if(!extract_ruu_file(ruu, "data1.hdr", CHUNK_SIZE)) {
        printf("Error: Failed to extract data1.hdr from %s\n", argv[1]);
        fclose(ruu);
        return 4;
    }

    // Close RUU file
    fclose(ruu);

    // Extract rom.zip from data1.cab
    printf("Extracting rom.zip...\n");
    if(!extract_rom_zip(currdir)) {
        printf("Error: Failed to extract rom.zip from 'data1.cab'\n");
        return 5;
    }
    
    // Report success
    printf("Done!\n");
    return 0;
} 

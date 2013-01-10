/* unruu.c - Extracts the rom zip files from a RUU update.
 *
 * Copyright (C) 2010-2011 Kenny Millington
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
 * TODO
 * 1. extract_ruu_files needs correctly handle the edge case of the match string
 *    laying over the CHUNK_SIZE boundary. 
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <libunshield.h>

#define CHUNK_SIZE 4096
#define RUU_LENGTH_OFFSET 24
#define RUU_FILE_PREFIX "Disk1\\"
#define RUU_FILES "data1.cab,data1.hdr"
#define RUU_FILELEN_BUFFER 20
#define ROM_ZIP_GROUP "<Support>Language Independent OS Independent Files"
#define PATH_MAX 256

char *lame_make_unicode(char *string) {
    char *buf = calloc(strlen(string) * 2, sizeof(char));
    int i;

    for(i = 0; i < strlen(string); i++) {
        buf[i*2] = string[i];
    }

    return buf;
}

void cleanup(char *tempdir, char *currdir) {
    char files[] = RUU_FILES;
    char *ruu_file = strtok(files, ",");
    
    printf("Cleaning up...\n");

    while(ruu_file != NULL) {
        unlink(ruu_file);
        ruu_file = strtok(NULL, ",");
    }

    chdir(currdir);
    rmdir(tempdir);
}

bool extract_rom_zip(char *path) {
    bool result = false;
    int i;
    char *filename;
    char output_path[256] = {0};
    
    printf("Extracting rom zip files...\n");
    
    unshield_set_log_level(UNSHIELD_LOG_LEVEL_LOWEST);
    Unshield *unshield = unshield_open("data1.cab");
    UnshieldFileGroup *group = unshield_file_group_find(unshield,ROM_ZIP_GROUP);
    
    for(i = group->first_file; i <= group->last_file; i++) {
        if(unshield_file_is_valid(unshield, i)) {
            filename = (char *)unshield_file_name(unshield, i);

            if(!strncmp(filename, "rom", 3)) {
                if(!strncmp(filename + strlen(filename) - 3, "zip", 3)) {
                    printf("Extracting %s...", filename);
                    fflush(stdout);
                    snprintf(output_path, sizeof(output_path), "%s/%s", 
                             path, filename);
                    unshield_file_save(unshield, i, output_path);
                    printf("done.\n");
                    result = true;
                }
            }
        }
    }

    unshield_close(unshield);
    
    return result;
}

bool extract_ruu_files(FILE *ruu) {
    char *buffer = malloc(sizeof(char)*CHUNK_SIZE);
    size_t length;
    int count = 1;
    int result = 0;
    
    int c = 0;
    for(c = 0; c < strlen(RUU_FILES); c++)
        if(strncmp(RUU_FILES+c, ",", 1) == 0)
            count++;
    
    char files[] = RUU_FILES;
    char **search = calloc(sizeof(char *), count);
    
    char *ruu_file = strtok(files, ",");
    for(c = 0; c < count && ruu_file != NULL; c++) {
        *(search+c) = 
            calloc(sizeof(char), strlen(ruu_file)+strlen(RUU_FILE_PREFIX)+1);
        strncpy(*(search+c), RUU_FILE_PREFIX, strlen(RUU_FILE_PREFIX));
        strncat(*(search+c), ruu_file, strlen(ruu_file));
        ruu_file = strtok(NULL, ",");
    }
    
    printf("Extracting temporary files...\n");

    rewind(ruu);
    do {
        char *i;
        length = fread(buffer, 1, CHUNK_SIZE, ruu);
        for(c=0; c < count; c++) {
            int uf = 0;
            ruu_file = *(search+c);
            
            i = memmem(buffer, length, ruu_file, strlen(ruu_file));

            if(!i) {
                char *u = lame_make_unicode(ruu_file);
                i = memmem(buffer, length, u, strlen(ruu_file) * 2);
                free(u);

                if(i) {
                    uf = 1;
                }
            }

            if(i) {
                int j = 0;
                j = uf  == 0 ? RUU_LENGTH_OFFSET : RUU_LENGTH_OFFSET * 2;
                fseek(ruu, (i-buffer)+j-CHUNK_SIZE, SEEK_CUR);
                /* Obtain the file length
                 * (TODO: probably a better way of doing this).
                 */
                char lenbuf;
                char filelen[RUU_FILELEN_BUFFER];
                memset(&filelen, 0, RUU_FILELEN_BUFFER);
                fread(&lenbuf, 1, 1, ruu);
                if(uf == 0) {
                    for(j=0; lenbuf != 0 && j < RUU_FILELEN_BUFFER; j++) {
                        filelen[j] = lenbuf;
                        fread(&lenbuf, 1, 1, ruu);
                    }
                } else {
                    char last = 0xff;
                    j = 0;
                    while(last != 0 || lenbuf != 0) {
                        if(lenbuf != 0) {
                            filelen[j++] = lenbuf;
                        }
                        last = lenbuf;
                        fread(&lenbuf, 1, 1, ruu);
                    }
                }
            
                int filelength;
                sscanf(filelen, "%d", &filelength);
                if(uf) fread(&lenbuf, 1, 1, ruu);

                FILE *out;
                out = fopen(ruu_file + strlen(RUU_FILE_PREFIX), "wb");
            
                while(filelength > 0) {
                    length = fread(
                        buffer, 1, 
                        filelength > CHUNK_SIZE ? CHUNK_SIZE : filelength, 
                        ruu
                    );
                    fwrite(buffer, 1, length, out);
                    filelength -= length;
                }
            
                fclose(out);
                
                result++;
                length = CHUNK_SIZE;
                break;
            }
        }

    } while(result < count && length == CHUNK_SIZE);

    free(buffer);
    for(c=0; c < count; c++)
        free(*(search+c));
    free(search);

    return result == count;
}

int main(int argc, char **argv) {
    if(argc < 2) {
        printf("Usage: %s RUU.exe\n", argv[0]);
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
    snprintf(tempdir, PATH_MAX, "/tmp/unruu-XXXXXX");
    mkdtemp(tempdir);
    chdir(tempdir);
    
    // Extract installshield cab files from RUU
    if(!extract_ruu_files(ruu)) {
        printf("Error: Failed to extract required files from %s\n", argv[1]);
        fclose(ruu);
        cleanup(tempdir, currdir);
        return 3;
    }
    
    // Close RUU file
    fclose(ruu);

    // Extract rom zip files from data1.cab
    if(!extract_rom_zip(currdir)) {
        printf("Error: Failed to extract rom zip files from 'data1.cab'\n");
        cleanup(tempdir, currdir);
        return 5;
    }
    
    // Report success
    cleanup(tempdir, currdir);
    printf("Done!\n");
    return 0;
} 

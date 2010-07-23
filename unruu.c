/* unruu.c - Extracts the rom.zip file from a RUU update.
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
 * $ gcc -Wall -lunshield -o unruu unruu.c
 * 
 * USAGE
 * $ ./unruu /path/to/RUU.exe
 *
 * If successful it will create rom.zip in your current directory.
 *
 * NOTES
 * The code could probably be better, but c'est la vie! :>
 *
 * TODO
 * 1. extract_ruu_files needs correctly handle the edge case of the match string
 *    laying over the CHUNK_SIZE boundary. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libunshield.h>

#define CHUNK_SIZE 4096
#define RUU_LENGTH_OFFSET 24
#define RUU_FILE_PREFIX "Disk1\\"
#define RUU_FILES "data1.cab,data1.hdr"
#define RUU_FILELEN_BUFFER 20
#define ROM_ZIP_GROUP "<Support>Language Independent OS Independent Files"
#define PATH_MAX 256
#define ROM_ZIP_SKEW 512
#define ROM_ZIP_HDR "\x50\x4B\x03\x04"

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

void check_and_fix_rom_zip(char *rom_file) {
    FILE *rom = fopen(rom_file, "rb");
    char *buf = malloc(CHUNK_SIZE * sizeof(char));
    int i;
    
    fread(buf, 1, ROM_ZIP_SKEW, rom);
    
    for(i = 0; i < ROM_ZIP_SKEW - strlen(ROM_ZIP_HDR); i++) {
        if(memcmp(buf+i, ROM_ZIP_HDR, strlen(ROM_ZIP_HDR)) == 0) {
            if(i > 0) {
                printf("Zip-skew (%d) detected, fixing...\n", i);
                FILE *rom2 = fopen("rom2.zip", "wb+");
                fseek(rom, i, SEEK_SET);
                
                int length;

                while((length = fread(buf, 1, CHUNK_SIZE, rom)) > 0 )
                    fwrite(buf, 1, length, rom2);

                fclose(rom);
                unlink(rom_file);
                rom = fopen(rom_file, "wb");
                
                rewind(rom2);
                while( (length = fread(buf, 1, CHUNK_SIZE, rom2)) > 0 )
                    fwrite(buf, 1, length, rom);
                
                fclose(rom2);
                unlink("rom2.zip");
            }

            break;
        }
    }

    fclose(rom);
}

bool extract_rom_zip(char *path) {
    bool result = false;
    int i;
    char *filename;
    char *output_path = calloc(1, strlen(path) + 9);
    strncpy(output_path, path, strlen(path));
    strncat(output_path, "/rom.zip", 8);
    
    printf("Extracting rom.zip...\n");
    
    unshield_set_log_level(UNSHIELD_LOG_LEVEL_LOWEST);
    Unshield *unshield = unshield_open("data1.cab");
    UnshieldFileGroup *group = unshield_file_group_find(unshield,ROM_ZIP_GROUP);
    
    for(i = group->first_file; i <= group->last_file; i++) {
        if(unshield_file_is_valid(unshield, i)) {
            filename = (char *)unshield_file_name(unshield, i);

            if(strncmp(filename, "rom.zip", 7) == 0) {
                unshield_file_save(unshield, i, output_path);
                check_and_fix_rom_zip(output_path);
                result = true;
                break;
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
        int i = 0;
        length = fread(buffer, 1, CHUNK_SIZE, ruu);
        for(c=0; c < count; c++) {
            ruu_file = *(search+c);
            
            for(i=0; i <= length - strlen(ruu_file); i++) {
                if(memcmp(ruu_file, buffer + i, strlen(ruu_file)) == 0) {
                    fseek(ruu, i + RUU_LENGTH_OFFSET - CHUNK_SIZE, SEEK_CUR);
                
                    /* Obtain the file length
                     * (TODO: probably a better way of doing this).
                     */
                    int j = 0;
                    char lenbuf;
                    char filelen[RUU_FILELEN_BUFFER];
                    memset(&filelen, 0, RUU_FILELEN_BUFFER);
                    fread(&lenbuf, 1, 1, ruu);
                    for(j=0; memcmp(&lenbuf, "\x00", 1) != 0 && 
                             j < RUU_FILELEN_BUFFER; j++) {
                        filelen[j] = lenbuf;
                        fread(&lenbuf, 1, 1, ruu);
                    }
                
                    int filelength;
                    sscanf(filelen, "%d", &filelength);
                
                    FILE *out;
                    out = fopen(ruu_file + strlen(RUU_FILE_PREFIX), "wb");
                
                    while(filelength > 0) {
                        length = fread(buffer, 1, CHUNK_SIZE, ruu);
                        fwrite(buffer, 1, length, out);
                        filelength -= length;
                    }
                
                    fclose(out);
                    
                    result++;
                    break;
                }
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

    // Extract rom.zip from data1.cab
    if(!extract_rom_zip(currdir)) {
        printf("Error: Failed to extract rom.zip from 'data1.cab'\n");
        cleanup(tempdir, currdir);
        return 5;
    }
    
    // Report success
    cleanup(tempdir, currdir);
    printf("Done!\n");
    return 0;
} 

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "ftree.h"
#include "hash.h"

int dopen(const char *, const char *);
char *path_concat(const char *, const char *);
void perform_unconditional_copy(char *, char *);
void perform_conditional_copy(FILE *, char *, char *);

int num_processes = 1;

int copy_ftree(const char *src, const char *dest) {
    struct stat src_buf;
    struct stat dest_buf;
    
    // Check if source directory and destination directory is valid
    if ((lstat(src, &src_buf) == 0 && lstat(dest, &dest_buf) == 0) && (S_ISDIR(src_buf.st_mode) && S_ISDIR(dest_buf.st_mode))) {
        dopen(src, dest);
    } else {
        perror("stat");
        exit(-1);
    }
    
    return num_processes;
}

int dopen(const char *src, const char *dest) {
    // Open source directory
    DIR *src_dir;
    if ((src_dir = opendir(src)) == NULL) {
        perror("opendir");
        return -1;
    }
    
    // Retrieve source directory information
    struct stat src_dir_buf;
    if (lstat(src, &src_dir_buf) == -1){
        perror("stat");
        exit(-1);
    }
    
    // Open destination directory
    DIR *dest_dir;
    if ((dest_dir = opendir(dest)) == NULL) {
        
        // Make a copy of destination path
        char *dest_path = malloc(strlen(dest) + 1);
        if (dest_path == NULL) {
            perror("malloc");
            exit(-1);
        }
        strcpy(dest_path, dest);
        
        // Prepare destination path for build
        char dest_final_index = dest_path[strlen(dest_path) - 1];
        switch(dest_final_index) {
            case '/':
                dest_path[strlen(dest_path) - 1] = '\0';
                break;
            default:
                dest_path[strlen(dest_path) - 1] = dest_path[strlen(dest_path) - 1];
        }
        
        char *dest_path_index = dest_path + 1;
        
        // Make a copy of source path
        char *src_path = malloc(strlen(src) + 1);
        if (src_path == NULL) {
            perror("malloc");
            exit(-1);
        }
        strcpy(src_path, src);
        
        // Prepare source path for build
        char src_final_index = dest_path[strlen(dest_path) - 1];
        switch(src_final_index) {
            case '/':
                src_path[strlen(src_path) - 1] = '\0';
                break;
            default:
                src_path[strlen(src_path) - 1] = src_path[strlen(src_path) - 1];
        }
        
        char *src_path_index = src_path + 1;
        
        struct stat src_path_buf;
        if (lstat(src_path, &src_path_buf) == -1){
            perror("stat");
            exit(-1);
        }
        
        // Build directory path
        while (*dest_path_index != '\0' && *src_path_index != '\0') {
            if (*dest_path_index == '/' && *src_path_index == '/') {
                *dest_path_index = '\0';
                *src_path_index = '\0';
                if (mkdir(dest_path, src_path_buf.st_mode & 0777) == -1) {
                    perror("mkdir");
                    exit(-1);
                }
                *dest_path_index = '/';
            }
            dest_path_index++;
        }
        
        if (mkdir(dest_path, src_dir_buf.st_mode & 0777) == -1) {
            perror("mkdir");
            exit(-1);
        }
    }
    
    // Retrieve destination directory information
    struct stat dest_dir_buf;
    if (lstat(dest, &dest_dir_buf) == -1){
        perror("stat");
        exit(-1);
    }
    
    // Check if source directory and destination directory permissions match
    if ((src_dir_buf.st_mode & 0777) != (dest_dir_buf.st_mode & 0777)) {
        if (chmod(dest, src_dir_buf.st_mode & 0777) == -1) {
            perror("chmod");
            exit(-1);
        }
    }
    
    struct dirent *dp;
    
    // Read contents of source directory
    while ((dp = readdir(src_dir)) != NULL) {
        
        // Ignore files and directories that begin with '.'
        if (dp -> d_name[0] == '.') {
            continue;
        }
        
        // Append file names to source and destination paths
        char *new_src_path = path_concat(src, dp->d_name);
        char *new_dest_path = path_concat(dest, dp->d_name);
        
        // Retrieve new source path information
        struct stat src_buf;
        if (lstat(new_src_path, &src_buf) == -1){
            perror("stat");
            exit(-1);
        }
        
        // Check if we are working with a file
        if ((S_ISREG(src_buf.st_mode))) {
            
            // Check if source file already exists in destination path
            struct stat dest_buf;
            if (lstat(new_dest_path, &dest_buf) == 0) {
                
                // Open source file for reading
                FILE *src_file = fopen(new_src_path, "rb");
                if (src_file == NULL) {
                    fprintf(stderr, "ERROR: Could not open source file for reading: %s\n", new_src_path);
                    exit(-1);
                }
                
                // Open destination file for reading
                FILE *dest_file = fopen(new_dest_path, "rb");
                if (src_file == NULL) {
                    fprintf(stderr, "ERROR: Could not open destination file for reading: %s\n", new_dest_path);
                    exit(-1);
                }
                
                // Check for valid file permissions
                if ((src_buf.st_mode & 0777) == 000) {
                    fprintf(stderr, "ERROR: Could not access file permissions: %s\n", new_src_path);
                    continue;
                }
                
                // Check if source and destination permissions do not match
                if ((src_buf.st_mode & 0777) != (dest_buf.st_mode & 0777)) {
                    if (chmod(new_dest_path, src_buf.st_mode & 0777) == -1) {
                        perror("chmod");
                        exit(-1);
                    }
                }
                
                // Check if source and destination sizes match
                if (src_buf.st_size == dest_buf.st_size) {
                    
                    // Check if source and destination hash values do not match
                    if(strcmp(hash(src_file), hash(dest_file)) != 0) {
                        perform_conditional_copy(dest_file, new_dest_path, new_src_path);
                    }
                    
                // Perform copy if source and destination sizes do not match
                } else {
                    
                    // Check for valid file permissions
                    if ((src_buf.st_mode & 0777) == 000) {
                        fprintf(stderr, "ERROR: Could not access file permissions: %s\n", new_src_path);
                        continue;
                    }
                    
                    // Check if source and destination permissions do not match
                    if ((src_buf.st_mode & 0777) != (dest_buf.st_mode & 0777)) {
                        if (chmod(new_dest_path, src_buf.st_mode & 0777) == -1) {
                            perror("chmod");
                            exit(-1);
                        }
                    }
                    
                    perform_conditional_copy(dest_file, new_dest_path, new_src_path);
                }
                
                // Close source file
                int error;
                error = fclose(src_file);
                if (error != 0) {
                    fprintf(stderr, "ERROR: Could not close source file: %s:\n", new_src_path);
                    exit(-1);
                }
                
                // Close destination file
                error = fclose(dest_file);
                if (error != 0) {
                    fprintf(stderr, "ERROR: Could not close destination file: %s\n", new_dest_path);
                    exit(-1);
                }
                
            // If source file does not already exist in destination path
            } else {
                
                // Copy contents from source file to destination file
                perform_unconditional_copy(new_src_path, new_dest_path);
                
            }
            
        // If we are working with a sub-directory
        } else if (S_ISDIR(src_buf.st_mode)) {
            
            // Recurse on new paths
            pid_t pid;
            
            pid = fork();
            
            if (pid == -1) {
                perror("fork");
                exit(-1);
                
            // Fork into child process for sub-directory
            } else if (pid == 0) {
                num_processes = 1;
                dopen(new_src_path, new_dest_path);
                exit(num_processes);
                
            // Wait in parent process for child process to terminate
            } else {
                int stat;
                while (wait(&stat) != -1){
                    if (WIFEXITED(stat)) {
                        char cvalue = WEXITSTATUS(stat);
                        
                        // Update number of processes
                        if (cvalue < 0) {
                            if (num_processes < 0) {
                               num_processes += cvalue;
                            } else {
                                num_processes = -num_processes + cvalue;
                            }
                        } else {
                            if (num_processes < 0) {
                                num_processes -= cvalue;
                            } else {
                                num_processes += cvalue;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
    }

void perform_unconditional_copy(char *new_src_path, char *new_dest_path) {
    int error;
    long siz;
    char *buffer = malloc(BUFSIZ);
    if (buffer == NULL) {
        perror("malloc");
        exit(-1);
    }
    
    // Open source file for reading
    FILE *src_file = fopen(new_src_path, "rb");
    if (src_file == NULL) {
        fprintf(stderr, "ERROR: Could not open source file for reading: %s\n", new_src_path);
        exit(-1);
    }
    
    // Open destination file for writing
    FILE *dest_file = fopen(new_dest_path, "wb");
    if (dest_file == NULL) {
        fprintf(stderr, "ERROR: Could not open destination file for reading: %s\n", new_dest_path);
        exit(-1);
    }
    
    // Read contents from source file and write to destination file
    do {
        siz = fread(buffer, sizeof(char), BUFSIZ, src_file);
        fwrite(buffer, sizeof(char), siz, dest_file);
    } while(!feof(src_file));
    
    // Close source file
    error = fclose(src_file);
    if (error != 0) {
        fprintf(stderr, "ERROR: Could not close source file: %s\n", new_src_path);
        exit(-1);
    }
    
    // Close destination file
    error = fclose(dest_file);
    if (error != 0) {
        fprintf(stderr, "ERROR: Could not close destination file: %s\n", new_dest_path);
        exit(-1);
    }
}

void perform_conditional_copy(FILE *dest_file, char *new_dest_path, char *new_src_path) {
    int error;
    
    // Close destination file
    fclose(dest_file);
    error = fclose(dest_file);
    if (error != 0) {
        fprintf(stderr, "ERROR: Could not close destination file: %s\n", new_dest_path);
        exit(-1);
    }
    
    // Open destination file for writing
    dest_file = fopen(new_dest_path, "wb");
    if (dest_file == NULL) {
        fprintf(stderr, "ERROR: Could not open destination file for reading: %s\n", new_dest_path);
        exit(-1);
    }
    
    // Perform copy if source and destination hash values do not match
    perform_unconditional_copy(new_src_path, new_dest_path);
}


char *path_concat(const char *pre_build, const char *post_build) {
    long s = strlen(pre_build) + strlen(post_build) + 1;
    char* dirname = malloc(s+1);
    if (dirname == NULL) {
        perror("malloc");
        exit(-1);
    }
    
    // Build new path
    strcpy(dirname, pre_build);
    strcat(dirname, "/");
    strcat(dirname, post_build);
    
    dirname[s+1] = '\0';
    return dirname;
}

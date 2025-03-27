#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

#define MAX_PATH 4096

typedef struct {
    char name[256];
    char type;
    off_t size;
    time_t mtime;
    ino_t inode;
    dev_t device;
} FileInfo;

void print_file_info(const FileInfo *file) {
    char time_str[20];
    struct tm *tm_info = localtime(&file->mtime);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("%c %10ld %20s %10lu %10lu %s\n",
           file->type,
           (long)file->size,
           time_str,
           (unsigned long)file->inode,
           (unsigned long)file->device,
           file->name);
}

int get_file_info(const char *path, const char *name, FileInfo *file) {
    struct stat stat_buf;
    char full_path[MAX_PATH];
    
    snprintf(full_path, MAX_PATH, "%s/%s", path, name);
    
    if (lstat(full_path, &stat_buf)) {
        fprintf(stderr, "Error getting info for %s: %s\n", full_path, strerror(errno));
        return -1;
    }
    
    strncpy(file->name, name, sizeof(file->name) - 1);
    file->name[sizeof(file->name) - 1] = '\0';
    
    if (S_ISREG(stat_buf.st_mode)) file->type = '-';
    else if (S_ISDIR(stat_buf.st_mode)) file->type = 'd';
    else if (S_ISLNK(stat_buf.st_mode)) file->type = 'l';
    else if (S_ISCHR(stat_buf.st_mode)) file->type = 'c';
    else if (S_ISBLK(stat_buf.st_mode)) file->type = 'b';
    else if (S_ISFIFO(stat_buf.st_mode)) file->type = 'p';
    else if (S_ISSOCK(stat_buf.st_mode)) file->type = 's';
    else file->type = '?';
    
    file->size = stat_buf.st_size;
    file->mtime = stat_buf.st_mtime;
    file->inode = stat_buf.st_ino;
    file->device = stat_buf.st_dev;
    
    return 0;
}

int list_directory(const char *path) {
    DIR *dir;
    struct dirent *entry;
    FileInfo *files = NULL;
    size_t count = 0, capacity = 0;
    
    if (!(dir = opendir(path))) {
        fprintf(stderr, "Cannot open directory %s: %s\n", path, strerror(errno));
        return -1;
    }
    
    printf("\nDirectory: %s\n", path);
    printf("Type       Size          Last Modified            Inode    Device   Name\n");
    printf("------------------------------------------------------------------------\n");
    
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (count >= capacity) {
            capacity = (capacity == 0) ? 16 : capacity * 2;
            FileInfo *new_files = realloc(files, capacity * sizeof(FileInfo));
            if (!new_files) {
                fprintf(stderr, "Memory allocation failed\n");
                closedir(dir);
                free(files);
                return -1;
            }
            files = new_files;
        }
        
        if (get_file_info(path, entry->d_name, &files[count]) == 0) {
            count++;
        }
    }
    
    for (size_t i = 0; i < count; i++) {
        print_file_info(&files[i]);
    }
    
    closedir(dir);
    free(files);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        // Если аргументы не переданы, используем текущий каталог
        return list_directory(".");
    }
    
    for (int i = 1; i < argc; i++) {
        if (list_directory(argv[i]) != 0) {
            fprintf(stderr, "Failed to process directory: %s\n", argv[i]);
        }
    }
    
    return 0;
}

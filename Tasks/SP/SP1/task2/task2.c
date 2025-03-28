#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>

#define MAX_FILENAME_LEN 1024
#define MAX_PATH_LEN 4096
#define MAX_SEARCH_LEN 4096
#define MAX_COPIES 100
#define MAX_FILE_SIZE (100 * 1024 * 1024)
#define COPIES_DIR "copies"

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

void create_copies_dir() {
    struct stat st = {0};
    if (stat(COPIES_DIR, &st) == -1) {
        if (mkdir(COPIES_DIR, 0755) == -1) {
            LOG_ERROR("Failed to create directory '%s': %s", COPIES_DIR, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

// Проверка hex-строки
int validate_hex(const char *hex) {
    if (!hex) return 0;

    size_t len = strlen(hex);
    if (len == 0 || len > 8) return 0;

    for (size_t i = 0; i < len; i++) {
        if (!isxdigit(hex[i])) return 0;
    }
    return 1;
}

// Парсинг hex-строки
uint32_t parse_hex(const char *hex) {
    uint32_t mask;
    if (sscanf(hex, "%x", &mask) != 1) {
        return 0;
    }
    return mask;
}

int file_exists(const char *filename) {
    if (!filename) return 0;

    struct stat st;
    if (stat(filename, &st) == -1) {
        return 0;
    }
    return 1;
}

int is_symlink(const char *filename) {
    if (!filename) return 0;

    struct stat st;
    if (lstat(filename, &st) == -1) {
        return 0;
    }
    return S_ISLNK(st.st_mode);
}

const char* get_filename(const char* path) {
    const char* filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}

void make_copy_path(char* dest, const char* src_path, int copy_num) {
    const char* filename = get_filename(src_path);
    snprintf(dest, MAX_PATH_LEN, "%s/%s_%d", COPIES_DIR, filename, copy_num);
}

void process_xorN(const char *filename, int n) {
    if (!filename || n < 2 || n > 6) {
        LOG_ERROR("Invalid arguments");
        return;
    }

    size_t block_size;
    switch(n) {
        case 2: block_size = 1; break;   // 2^2 бит = 4 бита (0.5 байта), берем 1 байт
        case 3: block_size = 1; break;   // 2^3 бит = 1 байт
        case 4: block_size = 2; break;   // 2^4 бит = 2 байта
        case 5: block_size = 4; break;   // 2^5 бит = 4 байта
        case 6: block_size = 8; break;   // 2^6 бит = 8 байт
        default: return;
    }

    FILE *file = NULL;
    uint8_t *block = NULL;
    uint8_t *result = NULL;

    file = fopen(filename, "rb");
    if (!file) {
        LOG_ERROR("Failed to open %s: %s", filename, strerror(errno));
        return;
    }

    block = malloc(block_size);
    result = malloc(block_size);
    if (!block || !result) {
        LOG_ERROR("Memory allocation failed for block size %zu bytes", block_size);
        goto cleanup;
    }
    memset(result, 0, block_size);

    size_t bytes_read;
    while ((bytes_read = fread(block, 1, block_size, file))) {
        if (bytes_read < block_size) {
            memset(block + bytes_read, 0, block_size - bytes_read);
        }

        for (size_t i = 0; i < block_size; i++) {
            result[i] ^= block[i];
        }
    }

    if (ferror(file)) {
        LOG_ERROR("Error reading file %s", filename);
    } else {
        printf("%s XOR%d result (hex): ", filename, n);
        for (size_t i = 0; i < block_size; i++) {
            printf("%02X", result[i]);
        }
        printf("\n");
    }

    cleanup:
        if (block) free(block);
        if (result) free(result);
        if (file && fclose(file) != 0) {
        LOG_ERROR("Failed to close file %s: %s", filename, strerror(errno));
    }
}

// Операция mask
void process_mask(const char *filename, uint32_t mask) {
    if (!filename) {
        LOG_ERROR("Invalid arguments");
        return;
    }

    FILE *file = fopen(filename, "rb");
    if (!file) {
        LOG_ERROR("Failed to open %s: %s", filename, strerror(errno));
        return;
    }

    uint32_t value;
    size_t count = 0;
    while (fread(&value, sizeof(value), 1, file) == 1) {
        if ((value & mask) == mask) {
            count++;
        }
    }

    if (ferror(file)) {
        LOG_ERROR("Error reading file %s", filename);
    } else {
        printf("%s Mask %08X: %zu\n", filename, mask, count);
    }

    if (fclose(file) != 0) {
        LOG_ERROR("Failed to close file %s: %s", filename, strerror(errno));
    }
}

// Операция copyN
void process_copyN(const char *filename, int n) {
    if (!filename || n <= 0 || n > MAX_COPIES) {
        LOG_ERROR("Invalid arguments");
        return;
    }

    if (strlen(filename) >= MAX_FILENAME_LEN) {
        LOG_ERROR("Filename too long");
        return;
    }

    if (is_symlink(filename)) {
        LOG_ERROR("Symbolic links are not supported");
        return;
    }

    if (!file_exists(filename)) {
        LOG_ERROR("File does not exist: %s", filename);
        return;
    }

    create_copies_dir();

    pid_t *pids = malloc(n * sizeof(pid_t));
    if (!pids) {
        LOG_ERROR("Memory allocation failed");
        return;
    }

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            LOG_ERROR("fork failed: %s", strerror(errno));
            continue;
        } else if (pids[i] == 0) {
            char new_path[MAX_PATH_LEN];
            int copy_num = i + 1;

            int attempts = 0;
            do {
                if (attempts > 0) {
                    snprintf(new_path, sizeof(new_path), "%s/%s_%d_%d",
                            COPIES_DIR, get_filename(filename), copy_num, attempts);
                } else {
                    snprintf(new_path, sizeof(new_path), "%s/%s_%d",
                            COPIES_DIR, get_filename(filename), copy_num);
                }

                if (access(new_path, F_OK) != 0) {
                    break;
                }
                attempts++;
            } while (attempts < 100);

            if (attempts >= 100) {
                LOG_ERROR("Cannot find available filename for copy");
                exit(EXIT_FAILURE);
            }

            int src_fd = open(filename, O_RDONLY);
            if (src_fd == -1) {
                LOG_ERROR("Failed to open source file %s: %s", filename, strerror(errno));
                exit(EXIT_FAILURE);
            }

            int dst_fd = open(new_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
            if (dst_fd == -1) {
                LOG_ERROR("Failed to create destination file %s: %s", new_path, strerror(errno));
                close(src_fd);
                exit(EXIT_FAILURE);
            }

            char buffer[4096];
            ssize_t bytes;
            while ((bytes = read(src_fd, buffer, sizeof(buffer))) > 0) {
                if (write(dst_fd, buffer, bytes) != bytes) {
                    LOG_ERROR("Write error to %s: %s", new_path, strerror(errno));
                    close(src_fd);
                    close(dst_fd);
                    unlink(new_path);
                    exit(EXIT_FAILURE);
                }
            }

            if (bytes == -1) {
                LOG_ERROR("Read error from %s: %s", filename, strerror(errno));
                close(src_fd);
                close(dst_fd);
                unlink(new_path);
                exit(EXIT_FAILURE);
            }

            close(src_fd);
            if (close(dst_fd) != 0) {
                LOG_ERROR("Failed to close destination file %s: %s", new_path, strerror(errno));
                unlink(new_path);
                exit(EXIT_FAILURE);
            }

            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < n; i++) {
        if (pids[i] == -1) continue;

        int status;
        if (waitpid(pids[i], &status, 0) == -1) {
            LOG_ERROR("waitpid failed: %s", strerror(errno));
        } else if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == EXIT_SUCCESS) {
                char new_path[MAX_PATH_LEN];
                snprintf(new_path, sizeof(new_path), "%s/%s_%d", COPIES_DIR, get_filename(filename), i + 1);
                printf("Created copy: %s\n", new_path);
            } else {
                LOG_ERROR("Failed to create copy %d of %s", i + 1, filename);
            }
        }
    }

    free(pids);
}

// Поиск подстроки
int find_substring(const char *buffer, size_t buf_len,
                  const char *pattern, size_t pat_len) {
    if (pat_len == 0) return 1;
    if (buf_len < pat_len) return 0;

    for (size_t i = 0; i <= buf_len - pat_len; i++) {
        if (memcmp(buffer + i, pattern, pat_len) == 0) {
            return 1;
        }
    }
    return 0;
}

// Операция find
void process_find(const char *filename, const char *search_str) {
    if (!filename || !search_str) {
        LOG_ERROR("Invalid arguments");
        return;
    }

    size_t search_len = strlen(search_str);
    if (search_len == 0) {
        printf("Empty search string\n");
        return;
    }

    if (search_len > MAX_SEARCH_LEN) {
        LOG_ERROR("Search string too long");
        return;
    }

    if (!file_exists(filename)) {
        LOG_ERROR("File does not exist: %s", filename);
        return;
    }

    FILE *file = fopen(filename, "rb");
    if (!file) {
        LOG_ERROR("Failed to open %s: %s", filename, strerror(errno));
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size > MAX_FILE_SIZE) {
        LOG_ERROR("File too large: %s", filename);
        fclose(file);
        return;
    }

    char *buffer = malloc(file_size);
    if (!buffer) {
        LOG_ERROR("Memory allocation failed");
        fclose(file);
        return;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);

    if (find_substring(buffer, bytes_read, search_str, search_len)) {
        printf("Found in: %s\n", filename);
    }

    free(buffer);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <file1> <file2> ... <command> [args]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  xorN - XOR operation (N=2..6)\n");
        fprintf(stderr, "  mask <hex> - count numbers matching mask\n");
        fprintf(stderr, "  copyN - create N copies of each file in 'copies' directory\n");
        fprintf(stderr, "  find <string> - search string in files\n");
        return 1;
    }

    // Определение команды
    int cmd_index = -1;
    enum { MODE_XORN, MODE_MASK, MODE_COPYN, MODE_FIND } mode;
    int n = 0;
    uint32_t mask = 0;
    char *search_str = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "xor", 3) == 0) {
            char *end;
            long num = strtol(argv[i] + 3, &end, 10);
            if (*end == '\0' && num >= 2 && num <= 6) {
                cmd_index = i;
                mode = MODE_XORN;
                n = (int)num;
                break;
            }
        } else if (strcmp(argv[i], "mask") == 0 && i + 1 < argc) {
            if (validate_hex(argv[i+1])) {
                cmd_index = i;
                mode = MODE_MASK;
                mask = parse_hex(argv[i+1]);
                break;
            } else {
                LOG_ERROR("Invalid hex mask");
                return 1;
            }
        } else if (strncmp(argv[i], "copy", 4) == 0) {
            char *end;
            long num = strtol(argv[i] + 4, &end, 10);
            if (*end == '\0' && num > 0 && num <= MAX_COPIES) {
                cmd_index = i;
                mode = MODE_COPYN;
                n = (int)num;
                break;
            } else {
                LOG_ERROR("Invalid copy count (1-%d)", MAX_COPIES);
                return 1;
            }
        } else if (strcmp(argv[i], "find") == 0 && i + 1 < argc) {
            cmd_index = i;
            mode = MODE_FIND;
            search_str = argv[i+1];
            break;
        }
    }

    if (cmd_index == -1) {
        LOG_ERROR("Invalid command");
        return 1;
    }

    int num_files = cmd_index - 1;
    if (num_files < 1) {
        LOG_ERROR("No input files specified");
        return 1;
    }

    // Проверка существования файлов
    for (int i = 1; i < cmd_index; i++) {
        if (!file_exists(argv[i])) {
            LOG_ERROR("File does not exist: %s", argv[i]);
            return 1;
        }
    }

    switch (mode) {
        case MODE_XORN:
            for (int i = 1; i < cmd_index; i++) {
                process_xorN(argv[i], n);
            }
            break;
        case MODE_MASK:
            for (int i = 1; i < cmd_index; i++) {
                process_mask(argv[i], mask);
            }
            break;
        case MODE_COPYN:
            for (int i = 1; i < cmd_index; i++) {
                process_copyN(argv[i], n);
            }
            break;
        case MODE_FIND:
            for (int i = 1; i < cmd_index; i++) {
                process_find(argv[i], search_str);
            }
            break;
        default:
            LOG_ERROR("Unknown mode");
            return 1;
    }

    return 0;
}
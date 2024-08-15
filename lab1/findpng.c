#include "findpng.h"

bool is_png(FILE *f)
{
    // Buffer to store the 8-byte header
    unsigned char buffer[8];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer), f);
    if (bytesRead < sizeof(buffer))
    {
        return false;
    }

    if (buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47 &&
        buffer[4] == 0x0D && buffer[5] == 0x0A && buffer[6] == 0x1A && buffer[7] == 0x0A)
    {
        return true;
    }

    return false;
}

// Function to normalize path by removing redundant slashes
char *normalize_path(const char *path) {
    char *normalized = (char *)malloc(strlen(path) + 1);
    if (normalized == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    const char *src = path;
    char *dst = normalized;

    while (*src) {
        *dst = *src++;
        if (*dst == '/') {
            while (*src == '/') {
                src++;
            }
        }
        dst++;
    }
    *dst = '\0';

    return normalized;
}

// Opens a network stream and checks the directory;
void check_file(char* parent, int* png_count) {
    // Open a directory stream
    DIR* dir = opendir(parent);

    // Check if invalid directory
    if (dir == NULL) {
        return;
    }

    // Go through each file in the directory
    struct dirent* file;
    
    while ((file = readdir(dir))) {
        // Skip "." and ".." directories
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0)
            continue;

        // Convert the filepath to a string (char array) (max filepath is 4096 characters)
        char filepath[4096];
        snprintf(filepath, sizeof(filepath), "%s/%s", parent, file->d_name);

        // Skip if file is a directory
        if (file->d_type == DT_DIR) {
            // Recursively call check_file until we get to a file
            check_file(filepath, png_count);
        }

        FILE* f = fopen(filepath, "rb");

        // Check if file is a png
        bool isPng = is_png(f);
        if (isPng) {
            char *normalized_filepath = normalize_path(filepath);
            printf("%s\n", normalized_filepath);
            free(normalized_filepath);
            (*png_count)++;
        }
        fclose(f);
    }
    // Close the directory stream
    closedir(dir);
}

int main(int argc, char *argv[])
{
    // Check if argument given
    if (argc < 2) {
        printf("findpng: No PNG file found\n");
        return -1;
    }
    char* directory = argv[1];
    // Count the number of PNG files
    int png_count = 0;
    check_file(directory, &png_count);
    if (png_count == 0) {
        printf("findpng: No PNG file found\n");
    }

    return 0;
}

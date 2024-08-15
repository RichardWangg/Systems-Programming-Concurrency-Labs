#include "pnginfo.h"

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

void get_png_data_IHDR(FILE *f, uint32_t *h, uint32_t *w)
{
    fseek(f, 16, SEEK_SET);
    fread(w, sizeof(uint32_t), 1, f);
    fread(h, sizeof(uint32_t), 1, f);

    *w = ntohl(*w);
    *h = ntohl(*h);
}


void check_chunks(FILE *f) {
    unsigned char length_chunk[4];
    unsigned char type_chunk[4];
    unsigned char *data_chunk;
    unsigned char crc_chunk[4];
    unsigned long length, comp_crc, exp_crc;

        // Each chunk must be 4 bytes
        while (fread(length_chunk, 1, 4, f) == 4) {
            length = ntohl(*(unsigned long *)length_chunk);

            if (fread(type_chunk, 1, 4, f) != 4) {
                break;
            }

            data_chunk = (unsigned char *)malloc(length + 4);
            memcpy(data_chunk, type_chunk, 4);

            if (fread(data_chunk + 4, 1, length, f) != length) {
                free(data_chunk);
                break;
            }

            if (fread(crc_chunk, 1, 4, f) != 4) {
                free(data_chunk);
                break;
            }

            comp_crc = crc(data_chunk, length + 4);
            exp_crc = ntohl(*(unsigned long *)crc_chunk);

            if (comp_crc != exp_crc) {
                printf("IDAT chunk CRC error: computed %08lx, expected %08lx\n", (unsigned long)comp_crc, (unsigned long)exp_crc);
                free(data_chunk);
            }

            free(data_chunk);

            if (memcmp(type_chunk, "IEND", 4) == 0) {
                break;
            }
        }
}

// int main(int argc, char *argv[])
// {
//     if (argc < 2)
//     {
//         printf("Usage: %s <png_file>\n", argv[0]);
//         return -1;
//     }

//     FILE *f = fopen(argv[1], "rb");
//     if (f == NULL)
//     {
//         printf("failure");
//         return -1;
//     }

//     if (is_png(f))
//     {
//         uint32_t height, width;
//         get_png_data_IHDR(f, &height, &width);
//         printf("%s: %u x %u\n", argv[1], width, height);
//         // Resetting pointer to point to first chunk
//         fseek(f, 8, SEEK_SET);
//         check_chunks(f);
//     }
//     else
//     {
//         printf("%s: Not a PNG file\n", argv[1]);
//     }

//     fclose(f);
//     return 0;
// }
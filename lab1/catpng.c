#include "catpng.h"


int main(int argc, char *argv[])
{
    // Check if argument given
    if (argc < 2) {
        printf("findpng: No PNG file found\n");
        return -1;
    }

    // Storing IHDR chunk
    uint32_t width, height, total_height = 0;
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 16, SEEK_SET);
    fread(&width, 4, 1, f);
    width = ntohl(width);
    fclose(f);

    // Storing data buffer (concataneted data)
    uint8_t* buffer = NULL;
    uint32_t buffer_size = 0;

    for (int i = 1; i < argc; i++) {
        char* filepath = argv[i];
        
        // Open the file
        FILE* f = fopen(filepath, "rb");

        // Get width and height from IHDR
        fseek(f, 20, SEEK_SET);
        fread(&height, 4, 1, f);
        total_height += ntohl(height);

        // Get png IDAT chunk (starts at byte 34)
        // First get the number of bytes in the data
        uint64_t data_length = 0;
        fseek(f, 33, SEEK_SET);
        fread(&data_length, 4, 1, f);
        
        // Get data from IDAT 
        data_length = ntohl(data_length);
        uint8_t* idat_data = malloc(data_length);
        fseek(f, 4, SEEK_CUR);
        fread(idat_data, data_length, 1, f);

        // Uncompress the data
        uint8_t* uncompressed_data = malloc((width*4 + 1)*ntohl(height));

        // This is the new inflated size of the data
        uint64_t len_inf = 0;

        int ret = mem_inf(uncompressed_data, &len_inf, idat_data, data_length);
        free(idat_data);
        idat_data = NULL;
        if (ret == 0) { /* success */
            printf("len_inf = %lu\n", len_inf);       
        } else { /* failure */
            fprintf(stderr,"mem_inf failed. ret = %d.\n", ret);
        }

        // Copy the new data to the buffer by first copyying over the existing buffer to a new temporary buffer
        uint8_t* temp_buffer = realloc(buffer, buffer_size + len_inf);
        buffer = temp_buffer;
        memcpy(buffer + buffer_size, uncompressed_data, len_inf);
        buffer_size += len_inf;

        free(uncompressed_data);
        uncompressed_data = NULL;
        fclose(f);
    }
    /*
    CLEAN UP
    */
    // Create the output file
    FILE* output = fopen("all.png", "wb+");

    // Compress the data
    uint8_t* compressed_data = malloc(buffer_size);
    uint64_t len_def = 0;
    int ret = mem_def(compressed_data, &len_def, buffer, buffer_size, Z_DEFAULT_COMPRESSION);
    if (ret == 0) { /* success */
        printf("original len = %d, len_def = %lu\n", buffer_size, len_def);
    } else { /* failure */
        fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
        return ret;
    }

    // Remove unnessecary storage by copying over the data
    uint64_t new_size = htonl(len_def);
    uint8_t* new_data = malloc(len_def);
    memcpy(new_data, compressed_data, len_def);
    
    /*
    Header
    */

    unsigned char png_header[] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(png_header, 1, 8, output);

    /*
    IHDR
    */
    // Length
    uint32_t IHDR_length = htonl(13);
    fwrite(&IHDR_length, 1, 4, output);

    // Type
    uint32_t IHDR_type = htonl(1229472850);

    // Data field
    unsigned char IHDR[17];
    uint32_t IHDR_width = htonl(width);
    uint32_t IHDR_height = htonl(total_height);
    uint8_t IHDR_bit_depth = 8;
    uint8_t IHDR_colour_type = 6;
    uint8_t IHDR_compression_method = 0;
    uint8_t IHDR_filter_method = 0;
    uint8_t IHDR_interlace = 0;

    memcpy(IHDR, &IHDR_type, sizeof(uint32_t));
    memcpy(IHDR + 4, &IHDR_width, sizeof(uint32_t));
    memcpy(IHDR + 8, &IHDR_height, sizeof(uint32_t));
    IHDR[12] = IHDR_bit_depth;
    IHDR[13] = IHDR_colour_type;
    IHDR[14] = IHDR_compression_method;
    IHDR[15] = IHDR_filter_method;
    IHDR[16] = IHDR_interlace;
    fwrite(IHDR, 1, 17, output);

    // CRC
    uint32_t IHDR_crc = htonl(crc(IHDR, 17));
    fwrite(&IHDR_crc, 1, 4, output);

    /*
    IDAT
    */
    // Length
    fwrite(&new_size, 1, 4, output);

    // Type
    uint32_t IDAT_type = htonl(1229209940);
    fwrite(&IDAT_type, 1, 4, output);

    // Data
    fwrite(compressed_data, 1, len_def, output);

    // CRC
    // Copy over the buffer with the type at the start
    uint8_t* temp_buff = malloc(len_def + 4);
    memset(temp_buff, 0, len_def + 4);
    memcpy(temp_buff, &IDAT_type, sizeof(uint32_t));
    memcpy(temp_buff + 4, new_data, len_def);
    uint32_t IDAT_crc = htonl(crc(temp_buff, len_def + 4));
    free(temp_buff);
    fwrite(&IDAT_crc, 1, 4, output);

    /*
    IEND 
    */
    // Length
    uint32_t IEND_length = htonl(0);
    fwrite(&IEND_length, 1, 4, output);

    // Type
    uint32_t IEND_type = htonl(1229278788);
    fwrite(&IEND_type, 1, 4, output);

    // CRC
    unsigned char IEND_temp[4] = {73, 69, 78, 68};
    uint32_t IEND_crc = htonl(crc(IEND_temp, 4));
    fwrite(&IEND_crc, 1, 4, output);

    fclose(output);
    free(new_data);
    free(compressed_data);
    free(buffer);

    printf("DONE\n");
    return 0;

}

#include "concat.h"
#include "paster2.h"

#define HEIGHT 6
#define WIDTH 400
#define SIZE_INF 9606
/*
Size of 1 uncompressed PNG segment: 9606
*/

// Concatenate a png segment into the shared memory
void concat(IMG_DAT* shmem, RECV_BUF* segment) {
    uint8_t* seek = (uint8_t*)segment->buf;

    if (is_png(segment->buf)) {
        // printf("Concat Buffer data is a valid PNG\n");
    } else {
        printf("Concat buffer data is NOT a valid PNG\n");
        // Print first few bytes for debugging
        for (size_t i = 0; i < segment->size && i < 20; ++i) {
            printf("%02x ", (unsigned char)segment->buf[i]);
        }
        printf("\n");
    }

    // Get IDAT chunk length (starts at byte 33)
    uint32_t data_length;
    memcpy(&data_length, seek + 33, 4);
    data_length = ntohl(data_length);

    // IDAT chunk data
    unsigned char *IDAT_data = malloc(sizeof(unsigned char) * data_length);
    memcpy(IDAT_data, seek + 41, data_length);

    unsigned char *uncompressed_data = malloc(sizeof(char) * HEIGHT * (WIDTH * 4 + 1));
    uint64_t len_inf = (WIDTH*4 + 1)*HEIGHT;

    int ret = mem_inf(uncompressed_data, &len_inf, IDAT_data, data_length);
    if (ret == 0) { /* success */
        // printf("original len = %d, len_def = %lu\n", buffer_size, len_def);
    } else { /* failure */
        // fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
    }

    memcpy(shmem->IDATdata[segment->seq].buf, uncompressed_data, len_inf);
    shmem->uncompressed_IDAT_total += HEIGHT * (WIDTH * 4 + 1);
    shmem->uncompressed_IDAT[segment->seq] = HEIGHT * (WIDTH * 4 + 1);

    /*CLEAN UP*/
    free(IDAT_data);
    free(uncompressed_data);
}

void process_buffer(IMG_DAT* shmem) {
    // printf("process_buffer()\n");

    // Storing data buffer (concataneted data)
    uint32_t buffer_size = shmem->uncompressed_IDAT_total;
    uint8_t* buffer = (uint8_t*)malloc(buffer_size);

    unsigned int current_size = 0;
    for (int i = 0; i < NUM_PARTS; i++) {
        memcpy(buffer + current_size, shmem->IDATdata[i].buf, shmem->uncompressed_IDAT[i]);
        current_size += shmem->uncompressed_IDAT[i];
    }
    /*
    CLEAN UP
    */
    // Create the output file
    FILE* output = fopen("all.png", "wb+");

    // Compress the data
    uint8_t* compressed_data = (uint8_t*)malloc(current_size);
    uint64_t len_def = 0;
    int ret = mem_def(compressed_data, &len_def, buffer, current_size, Z_DEFAULT_COMPRESSION);
    if (ret == 0) { /* success */
        // printf("original len = %d, len_def = %lu\n", buffer_size, len_def);
    } else { /* failure */
        // fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
    }

    // Remove unnessecary storage by copying over the data
    uint64_t new_size = htonl(len_def);
    char* new_data = malloc(len_def);
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
    uint32_t IHDR_width = htonl(WIDTH);
    uint32_t IHDR_height = htonl(HEIGHT*NUM_PARTS);
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

    // printf("DONE\n");
    return;

}
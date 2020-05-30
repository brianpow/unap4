#include "asprintf.h"

typedef struct
{
    uint32_t start;
    uint32_t size;
    unsigned char EOD;
} __attribute__((packed)) ap4_data_detail_t;

typedef struct
{
    unsigned char start;
    unsigned char type;
    uint8_t size;
    unsigned char type2;
    uint8_t size2;
    ap4_data_detail_t *data;
} __attribute__((packed)) ap4_data_t;

bool isAllNull(char *buff, size_t size)
{
    for (int i = 0; i < size; i++)
    {
        if (buff[i] != '\x00')
            return false;
    }
    return true;
}

char *getFilename(const char *filename, char *ext, uint32_t start, uint32_t size, int padding)
{
    char *buff, *prefix, *newFilename;
    size_t ssize;
    char *pos = strrchr((char *)filename, '.');

    prefix = (char *)malloc(sizeof(char) * (pos - filename + 1));
    strncpy(prefix, filename, pos - filename);
    prefix[pos - filename] = '\0';
    asprintf(&buff, "%%s 0x%%0%d" PRIx32 "-0x%%0%d" PRIx32 " (%%" PRIu32 ").%%s", padding, padding);
    asprintf(&newFilename, buff, prefix, start, start + size, size, ext);
    free(buff);
    return newFilename;
}

// https://stackoverflow.com/questions/12791864/c-program-to-check-little-vs-big-endian
int is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } e = {0x01000000};

    return e.c[0];
}

typedef struct
{
    u_int32_t start;
    u_int32_t size;
    u_char EOD;
} __attribute__((packed)) ap4_data_detail_t;

typedef struct
{
    u_char start;
    u_char type;
    u_int8_t size;
    u_char type2;
    u_int8_t size2;
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

char *getFilename(const char *filename, char *ext, u_int32_t start, u_int32_t size, int padding)
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
        u_int32_t i;
        char c[4];
    } e = {0x01000000};

    return e.c[0];
}

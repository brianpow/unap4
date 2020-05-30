#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <inttypes.h>
#include "unap4.h"
#include "byteswap.h"

bool bVerbose = false;
bool bDryRun = false;
bool bDoubleDash = false;

size_t writeFile(char *name, char *buff, size_t size)
{
    size_t byteWritten;
    FILE *ofp = fopen(name, "wb");
    if (ofp == NULL)
    {
        return 0;
    }
    byteWritten = fwrite(buff, sizeof(char), size, ofp);

    fclose(ofp);
    return byteWritten;
}

size_t lookup(FILE *fp, size_t maxPos)
{
    char *pattern = "\x00\x01";
    uint64_t size = 5;
    char tmp[size];
    size_t pos = ftell(fp);

    while (size == fread(&tmp, sizeof(char), size, fp))
    {
        if (memcmp(tmp, pattern, 2) == 0 && tmp[3] == '\x01' && tmp[2] == tmp[4])
        {
            return ftell(fp) - size;
        }

        pos++;
        if (fseek(fp, pos, SEEK_SET) || (maxPos > 0 && pos > maxPos - size))
            return 0;
    }
    return 0;
}

bool isMP3Frame(char *buff)
{
    return (buff[0] == '\xFF' && (buff[1] & '\xE0') == '\xE0');
}

void xorBuffer(char *buff, size_t size, char key)
{
    if (key != '\x00')
        for (size_t i = 0; i < size; i++)
            buff[i] = buff[i] ^ key;
}

bool checkFile(const char *filename)
{
    FILE *fp, *ofp;
    char *outfilename, *buff;
    uint32_t indexCount = 0, mp3Count = 0, othersCount = 0;
    size_t bytes = 0, indexPos = 0, fileSize = 0, maxPos = 0;
    uint64_t padding = 0;
    char tmp, xorKey;
    ap4_data_detail_t *ap4_detail;

    int big_endian = is_big_endian();

    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        printf("File %s not found.", filename);
        return false;
    }
    setvbuf(fp, NULL, _IOFBF, 10240000);
    printf("Processing %s...\n", filename);
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    padding = snprintf(NULL, 0, "%x", fileSize);
    fseek(fp, 0, SEEK_SET);
    unsigned char header[5];
    uint8_t dataSize;
    ap4_data_detail_t details;

    while (indexPos = lookup(fp, maxPos))
    {
        if (bVerbose)
            printf("%s (%d): [INFO] Found potential TOC at 0x%zx (%zu)\n", __FUNCTION__, __LINE__, indexPos, indexPos);
        fseek(fp, indexPos, SEEK_SET);
        if (maxPos > 0 && indexPos >= maxPos)
        {
            if (bVerbose)
                printf("%s (%d): [INFO] No further data at 0x%zx (%zu)\n", __FUNCTION__, __LINE__, indexPos, indexPos);
            break;
        }

        if (bVerbose)
            printf("%s (%d): [INFO] Reading TOC header at 0x%zx (%zu)\n", __FUNCTION__, __LINE__, indexPos, indexPos);
        bytes = fread(&header, 5, 1, fp);

        if (bytes == 0)
        {
            if (bVerbose)
                printf("%s (%d): [ERR] Failed to read TOC header at position 0x%zx (%zu), only %zu chunk read.\n", __FUNCTION__, __LINE__, indexPos, indexPos, bytes);
            break;
        }

        memcpy(&dataSize, header + 4, 1);

        indexPos = ftell(fp);
        for (uint8_t j = 0; j < dataSize; j++)
        {
            fseek(fp, indexPos, SEEK_SET);
            fread(&details, sizeof(ap4_data_detail_t), 1, fp);
            indexPos += sizeof(ap4_data_detail_t);
            indexCount++;

            if (!big_endian)
            {
                details.start = bswap_32(details.start);
                details.size = bswap_32(details.size);
            }
            if (bVerbose)
                printf("%s (%d): [INFO] Reading index %" PRIu32 " at 0x%zx (%zu), the target should be start: 0x%" PRIx32 " (%" PRIu32 ") with size 0x%" PRIx32 " (%" PRIu32 ")\n", __FUNCTION__, __LINE__, indexCount, indexPos, indexPos, details.start, details.start, details.size, details.size);

            outfilename = getFilename(filename, "mp3", details.start, details.size, padding);

            if (details.start == 0 && details.size == 0)
            {
                if (bVerbose)
                    printf("%s (%d): [ERR] Invalid TOC. Start position or file size is zero.\n", __FUNCTION__, __LINE__);
                continue;
            }

            if (details.start > fileSize - details.size)
            {
                printf("%s (%d): [ERR] Invalid TOC. Start position 0x%" PRIx32 "(%" PRIu32 ")+ file size 0x%" PRIx32 " (%" PRIu32 ") exceed the container size 0x%zx (%zu).\n", __FUNCTION__, __LINE__, details.start, details.start, details.size, details.size, fileSize, fileSize);
                continue;
            }

            fseek(fp, details.start, SEEK_SET);

            buff = (char *)malloc(sizeof(char) * details.size);
            fread(buff, sizeof(char), details.size, fp);

            if (isAllNull(buff, details.size))
            {
                if (bVerbose)
                    printf("%s (%d): [WARN] File with start offset 0x%" PRIx32 " (%" PRIu32 ")  with size 0x%" PRIx32 " (%" PRIu32 ") skipped since all contents are NULL.\n", __FUNCTION__, __LINE__, details.start, details.start, details.size, details.size);
                continue;
            }

            if (!isMP3Frame(buff))
            {
                xorKey = buff[0] ^ '\xFF';
                if ((buff[1] ^ xorKey) && '\xE0' == '\xE0')
                {
                    if (bVerbose)
                        printf("%s (%d): [INFO] File %x %x seems to be encrypted with key 0x%02hhx\n", __FUNCTION__, __LINE__, buff[0], buff[1], xorKey);
                    xorBuffer(buff, details.size, xorKey);
                    mp3Count++;
                }
                else
                {
                    if (bVerbose)
                        printf("%s (%d): [WARN] File doesn't seems to be a valid MP3 file, saving anyway\n", __FUNCTION__, __LINE__);
                    free(outfilename);
                    outfilename = getFilename(filename, "unk", details.start, details.size, padding);
                    othersCount++;
                }
            }
            else
            {
                mp3Count++;
            }
            if (!bDryRun)
                writeFile(outfilename, buff, details.size);
            if (maxPos == 0)
                maxPos = details.start;
            if (bVerbose)
                printf("%s (%d): [INFO] '%s' created from 0x%08x-0x%08x with size %" PRIu32 " bytes.\n", __FUNCTION__, __LINE__, outfilename, details.start, details.start + details.size, details.size, xorKey);
            free(buff);

            free(outfilename);
        }

        fseek(fp, indexPos, SEEK_SET);
    }
    fclose(fp);
    printf("%" PRIu32 " indices found, %" PRIu32 " MP3 files and %" PRIu32 " unknown files created.\n", indexCount, mp3Count, othersCount);
    return true;
}
void usage(const char *filename)
{
    printf("unap4 version 1.0\n");
    printf("Copyright (C) 2020 by Brian Pow. Licence: AGPL v3.0\n\n");
    printf("Usage: %s [-d] [-v] [-h] [--] ap4_file [ap4_file ...]\n\n", filename);
    printf("Options:\n");
    printf("\t-d\tDry run. Don't create any files\n");
    printf("\t-v\tBe verbose\n");
    printf("\t-h\tThis help\n");
    printf("\t--\tStop options parsing. Useful if filename begins with dash.\n");
}

int main(int argc, const char **argv)
{
    if (argc == 1)
        usage(argv[0]);
    else
        for (int i = 1; i < argc; i++)
        {
            if (!bDoubleDash)
            {
                if (!strcmp(argv[i], "--"))
                {
                    bDoubleDash = !bDoubleDash;
                    continue;
                }
                if (!strcmp(argv[i], "-h"))
                {
                    usage(argv[0]);
                    continue;
                }
                if (!strcmp(argv[i], "-d"))
                {
                    bDryRun = !bDryRun;
                    continue;
                }
                if (!strcmp(argv[i], "-v"))
                {
                    bVerbose = !bVerbose;
                    continue;
                }
            }
            checkFile(argv[i]);
        }
}
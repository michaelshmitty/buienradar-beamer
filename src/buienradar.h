#ifndef BUIENRADAR_H
#define BUIENRADAR_H

#define internal static
#define local_persist static
#define global_variable static

typedef struct
{
    char *memory;
    size_t size;
} MemoryStruct;

typedef struct
{
    unsigned char *data;
    size_t size;
} BitMap;

#endif

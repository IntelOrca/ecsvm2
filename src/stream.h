#ifndef ECSVM_STREAM_H
#define ECSVM_STREAM_H

#include <stddef.h>
#include <stdio.h>

typedef int (*ecsvm_stream_read_fn)(
    void *context,
    void *buffer,
    size_t size,
    size_t *out_read
);
typedef int (*ecsvm_stream_write_fn)(
    void *context,
    const void *buffer,
    size_t size,
    size_t *out_written
);
typedef int (*ecsvm_stream_seek_fn)(
    void *context,
    long offset,
    int origin
);

typedef struct ecsvm_stream {
    void *context;
    ecsvm_stream_read_fn read;
    ecsvm_stream_write_fn write;
    ecsvm_stream_seek_fn seek;
} ecsvm_stream_t;

typedef struct ecsvm_file_stream {
    ecsvm_stream_t stream;
    FILE *file;
} ecsvm_file_stream_t;

void ecsvm_stream_init(
    ecsvm_stream_t *stream,
    void *context,
    ecsvm_stream_read_fn read,
    ecsvm_stream_write_fn write,
    ecsvm_stream_seek_fn seek
);
int ecsvm_stream_read(
    ecsvm_stream_t *stream,
    void *buffer,
    size_t size,
    size_t *out_read
);
int ecsvm_stream_write(
    ecsvm_stream_t *stream,
    const void *buffer,
    size_t size,
    size_t *out_written
);
int ecsvm_stream_seek(ecsvm_stream_t *stream, long offset, int origin);

void ecsvm_file_stream_init(ecsvm_file_stream_t *stream, FILE *file);

#endif

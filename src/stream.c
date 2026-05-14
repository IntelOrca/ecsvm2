#include "stream.h"

#include <string.h>

static int ecsvm_file_stream_read(
    void *context,
    void *buffer,
    size_t size,
    size_t *out_read
)
{
    FILE *file;
    size_t read_count;

    file = (FILE *)context;
    read_count = size == 0u ? 0u : fread(buffer, 1u, size, file);
    if (out_read != NULL) {
        *out_read = read_count;
    }

    return size == 0u || read_count == size || !ferror(file);
}

static int ecsvm_file_stream_write(
    void *context,
    const void *buffer,
    size_t size,
    size_t *out_written
)
{
    FILE *file;
    size_t write_count;

    file = (FILE *)context;
    write_count = size == 0u ? 0u : fwrite(buffer, 1u, size, file);
    if (out_written != NULL) {
        *out_written = write_count;
    }

    return size == 0u || write_count == size;
}

static int ecsvm_file_stream_seek(void *context, long offset, int origin)
{
    return fseek((FILE *)context, offset, origin) == 0;
}

void ecsvm_stream_init(
    ecsvm_stream_t *stream,
    void *context,
    ecsvm_stream_read_fn read,
    ecsvm_stream_write_fn write,
    ecsvm_stream_seek_fn seek
)
{
    memset(stream, 0, sizeof(*stream));
    stream->context = context;
    stream->read = read;
    stream->write = write;
    stream->seek = seek;
}

int ecsvm_stream_read(
    ecsvm_stream_t *stream,
    void *buffer,
    size_t size,
    size_t *out_read
)
{
    if (out_read != NULL) {
        *out_read = 0u;
    }

    if (stream == NULL || stream->read == NULL) {
        return 0;
    }

    return stream->read(stream->context, buffer, size, out_read);
}

int ecsvm_stream_write(
    ecsvm_stream_t *stream,
    const void *buffer,
    size_t size,
    size_t *out_written
)
{
    if (out_written != NULL) {
        *out_written = 0u;
    }

    if (stream == NULL || stream->write == NULL) {
        return 0;
    }

    return stream->write(stream->context, buffer, size, out_written);
}

int ecsvm_stream_seek(ecsvm_stream_t *stream, long offset, int origin)
{
    if (stream == NULL || stream->seek == NULL) {
        return 0;
    }

    return stream->seek(stream->context, offset, origin);
}

void ecsvm_file_stream_init(ecsvm_file_stream_t *stream, FILE *file)
{
    memset(stream, 0, sizeof(*stream));
    ecsvm_stream_init(
        &stream->stream,
        file,
        ecsvm_file_stream_read,
        ecsvm_file_stream_write,
        ecsvm_file_stream_seek
    );
    stream->file = file;
}

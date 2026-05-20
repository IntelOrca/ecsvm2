#include "file_util.h"

#include <stdio.h>
#include <stdlib.h>

int ecsvm_read_text_file(const char *path, char **out_text, size_t *out_length)
{
    FILE *file;
    long length;
    char *text;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fclose(file);
        return 0;
    }

    if (length > 0 && fread(text, 1u, (size_t)length, file) != (size_t)length) {
        free(text);
        fclose(file);
        return 0;
    }

    fclose(file);
    text[length] = '\0';
    *out_text = text;
    if (out_length != NULL) {
        *out_length = (size_t)length;
    }
    return 1;
}

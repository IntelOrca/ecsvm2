#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ecsvm_xml_writer_reserve_nodes(ecsvm_xml_writer_t *writer, size_t minimum)
{
    char **node_names;
    size_t capacity;

    if (minimum <= writer->node_capacity) {
        return 1;
    }

    capacity = writer->node_capacity == 0u ? 4u : writer->node_capacity;
    while (capacity < minimum) {
        capacity *= 2u;
    }

    node_names = (char **)realloc(writer->node_names, capacity * sizeof(*writer->node_names));
    if (node_names == NULL) {
        return 0;
    }

    writer->node_names = node_names;
    writer->node_capacity = capacity;
    return 1;
}

static char *ecsvm_xml_copy_string(const char *text)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1u);
    return copy;
}

static int ecsvm_xml_writer_write_bytes(
    ecsvm_xml_writer_t *writer,
    const void *data,
    size_t size
)
{
    size_t written;

    return ecsvm_stream_write(writer->stream, data, size, &written) &&
        written == size;
}

static int ecsvm_xml_writer_puts(ecsvm_xml_writer_t *writer, const char *text)
{
    return text != NULL &&
        ecsvm_xml_writer_write_bytes(writer, text, strlen(text));
}

static int ecsvm_xml_writer_putc(ecsvm_xml_writer_t *writer, int ch)
{
    char byte;

    byte = (char)ch;
    return ecsvm_xml_writer_write_bytes(writer, &byte, 1u);
}

static int ecsvm_xml_writer_indent(ecsvm_xml_writer_t *writer, size_t depth)
{
    size_t index;

    for (index = 0u; index < depth; ++index) {
        if (!ecsvm_xml_writer_puts(writer, "  ")) {
            return 0;
        }
    }

    return 1;
}

static int ecsvm_xml_writer_close_start_tag(ecsvm_xml_writer_t *writer, int newline)
{
    if (!writer->start_tag_open) {
        return 1;
    }

    writer->start_tag_open = 0;
    if (!ecsvm_xml_writer_putc(writer, '>')) {
        return 0;
    }

    if (newline) {
        if (!ecsvm_xml_writer_putc(writer, '\n')) {
            return 0;
        }
        writer->wrote_text = 0;
    }

    return 1;
}

static int ecsvm_xml_writer_escape(
    ecsvm_xml_writer_t *writer,
    const char *text,
    size_t length,
    int attribute_mode
)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        unsigned char ch;

        ch = (unsigned char)text[index];
        switch (ch) {
        case '&':
            if (!ecsvm_xml_writer_puts(writer, "&amp;")) {
                return 0;
            }
            break;
        case '<':
            if (!ecsvm_xml_writer_puts(writer, "&lt;")) {
                return 0;
            }
            break;
        case '>':
            if (!ecsvm_xml_writer_puts(writer, "&gt;")) {
                return 0;
            }
            break;
        case '"':
            if (attribute_mode) {
                if (!ecsvm_xml_writer_puts(writer, "&quot;")) {
                    return 0;
                }
            } else if (!ecsvm_xml_writer_putc(writer, '"')) {
                return 0;
            }
            break;
        case '\'':
            if (attribute_mode) {
                if (!ecsvm_xml_writer_puts(writer, "&apos;")) {
                    return 0;
                }
            } else if (!ecsvm_xml_writer_putc(writer, '\'')) {
                return 0;
            }
            break;
        case '\n':
            if (!ecsvm_xml_writer_puts(writer, "&#x0A;")) {
                return 0;
            }
            break;
        case '\r':
            if (!ecsvm_xml_writer_puts(writer, "&#x0D;")) {
                return 0;
            }
            break;
        case '\t':
            if (!ecsvm_xml_writer_puts(writer, "&#x09;")) {
                return 0;
            }
            break;
        default:
            if (!ecsvm_xml_writer_putc(writer, (int)ch)) {
                return 0;
            }
            break;
        }
    }

    return 1;
}

void ecsvm_xml_writer_init(ecsvm_xml_writer_t *writer, ecsvm_stream_t *stream)
{
    memset(writer, 0, sizeof(*writer));
    writer->stream = stream;
}

void ecsvm_xml_writer_free(ecsvm_xml_writer_t *writer)
{
    size_t index;

    if (writer == NULL) {
        return;
    }

    for (index = 0u; index < writer->node_count; ++index) {
        free(writer->node_names[index]);
    }
    free(writer->node_names);
    memset(writer, 0, sizeof(*writer));
}

int ecsvm_xml_writer_write_declaration(ecsvm_xml_writer_t *writer)
{
    return ecsvm_xml_writer_puts(writer, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
}

int ecsvm_xml_writer_push_node(ecsvm_xml_writer_t *writer, const char *name)
{
    char *name_copy;

    if (writer == NULL || writer->stream == NULL || name == NULL) {
        return 0;
    }

    if (!ecsvm_xml_writer_close_start_tag(writer, 1)) {
        return 0;
    }

    if (!ecsvm_xml_writer_reserve_nodes(writer, writer->node_count + 1u)) {
        return 0;
    }

    name_copy = ecsvm_xml_copy_string(name);
    if (name_copy == NULL) {
        return 0;
    }

    if (!ecsvm_xml_writer_indent(writer, writer->node_count) ||
        !ecsvm_xml_writer_putc(writer, '<') ||
        !ecsvm_xml_writer_puts(writer, name)) {
        free(name_copy);
        return 0;
    }

    writer->node_names[writer->node_count] = name_copy;
    writer->node_count += 1u;
    writer->start_tag_open = 1;
    writer->wrote_text = 0;
    return 1;
}

int ecsvm_xml_writer_write_attribute(
    ecsvm_xml_writer_t *writer,
    const char *name,
    const char *value
)
{
    const char *safe_value;

    if (writer == NULL || !writer->start_tag_open || name == NULL) {
        return 0;
    }

    safe_value = value != NULL ? value : "";
    return ecsvm_xml_writer_putc(writer, ' ') &&
        ecsvm_xml_writer_puts(writer, name) &&
        ecsvm_xml_writer_puts(writer, "=\"") &&
        ecsvm_xml_writer_escape(writer, safe_value, strlen(safe_value), 1) &&
        ecsvm_xml_writer_putc(writer, '"');
}

int ecsvm_xml_writer_write_attribute_size(
    ecsvm_xml_writer_t *writer,
    const char *name,
    size_t value
)
{
    char buffer[32];

    (void)snprintf(buffer, sizeof(buffer), "%zu", value);
    return ecsvm_xml_writer_write_attribute(writer, name, buffer);
}

int ecsvm_xml_writer_write_text(ecsvm_xml_writer_t *writer, const char *text)
{
    return ecsvm_xml_writer_write_text_range(writer, text, text != NULL ? strlen(text) : 0u);
}

int ecsvm_xml_writer_write_text_range(
    ecsvm_xml_writer_t *writer,
    const char *text,
    size_t length
)
{
    if (writer == NULL || text == NULL) {
        return length == 0u;
    }

    if (!ecsvm_xml_writer_close_start_tag(writer, 0)) {
        return 0;
    }

    writer->wrote_text = 1;
    return ecsvm_xml_writer_escape(writer, text, length, 0);
}

int ecsvm_xml_writer_pop_node(ecsvm_xml_writer_t *writer)
{
    char *name;

    if (writer == NULL || writer->node_count == 0u) {
        return 0;
    }

    name = writer->node_names[writer->node_count - 1u];
    writer->node_count -= 1u;
    writer->node_names[writer->node_count] = NULL;

    if (writer->start_tag_open) {
        writer->start_tag_open = 0;
        if (!ecsvm_xml_writer_puts(writer, "/>\n")) {
            free(name);
            return 0;
        }
        writer->wrote_text = 0;
    } else {
        if (!writer->wrote_text &&
            !ecsvm_xml_writer_indent(writer, writer->node_count)) {
            free(name);
            return 0;
        }

        if (!ecsvm_xml_writer_puts(writer, "</") ||
            !ecsvm_xml_writer_puts(writer, name) ||
            !ecsvm_xml_writer_putc(writer, '>') ||
            !ecsvm_xml_writer_putc(writer, '\n')) {
            free(name);
            return 0;
        }
        writer->wrote_text = 0;
    }

    free(name);
    return 1;
}

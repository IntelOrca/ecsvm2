#ifndef ECSVM_XML_H
#define ECSVM_XML_H

#include "stream.h"

#include <stddef.h>

typedef struct ecsvm_xml_writer {
    ecsvm_stream_t *stream;
    char **node_names;
    size_t node_count;
    size_t node_capacity;
    int start_tag_open;
    int wrote_text;
} ecsvm_xml_writer_t;

void ecsvm_xml_writer_init(ecsvm_xml_writer_t *writer, ecsvm_stream_t *stream);
void ecsvm_xml_writer_free(ecsvm_xml_writer_t *writer);
int ecsvm_xml_writer_write_declaration(ecsvm_xml_writer_t *writer);
int ecsvm_xml_writer_push_node(ecsvm_xml_writer_t *writer, const char *name);
int ecsvm_xml_writer_write_attribute(
    ecsvm_xml_writer_t *writer,
    const char *name,
    const char *value
);
int ecsvm_xml_writer_write_attribute_size(
    ecsvm_xml_writer_t *writer,
    const char *name,
    size_t value
);
int ecsvm_xml_writer_write_text(ecsvm_xml_writer_t *writer, const char *text);
int ecsvm_xml_writer_write_text_range(
    ecsvm_xml_writer_t *writer,
    const char *text,
    size_t length
);
int ecsvm_xml_writer_pop_node(ecsvm_xml_writer_t *writer);

#endif

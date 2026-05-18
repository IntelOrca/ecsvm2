#include "ecsvm/ecsbin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ecsvm_text_buffer {
    char *data;
    size_t length;
    size_t capacity;
} ecsvm_text_buffer_t;

typedef struct ecsvm_table_column {
    const char *header;
    size_t width;
    int right_align;
} ecsvm_table_column_t;

static void ecsvm_inspect_set_error(
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic,
    ecsvm_diagnostic_code_t code,
    const char *message
)
{
    if (error_message != NULL && error_message_capacity > 0u) {
        (void)snprintf(error_message, error_message_capacity, "%s", message != NULL ? message : "");
    }
    ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, code, message);
}

static int ecsvm_text_buffer_reserve(ecsvm_text_buffer_t *buffer, size_t additional)
{
    size_t required;
    size_t capacity;
    char *data;

    required = buffer->length + additional + 1u;
    if (required <= buffer->capacity) {
        return 1;
    }

    capacity = buffer->capacity == 0u ? 256u : buffer->capacity;
    while (capacity < required) {
        capacity *= 2u;
    }

    data = (char *)realloc(buffer->data, capacity);
    if (data == NULL) {
        return 0;
    }

    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

static int ecsvm_text_buffer_append_range(
    ecsvm_text_buffer_t *buffer,
    const char *text,
    size_t length
)
{
    if (!ecsvm_text_buffer_reserve(buffer, length)) {
        return 0;
    }

    if (length > 0u) {
        memcpy(buffer->data + buffer->length, text, length);
        buffer->length += length;
    }
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int ecsvm_text_buffer_append(ecsvm_text_buffer_t *buffer, const char *text)
{
    return ecsvm_text_buffer_append_range(buffer, text, strlen(text));
}

static int ecsvm_text_buffer_append_char(ecsvm_text_buffer_t *buffer, char ch)
{
    if (!ecsvm_text_buffer_reserve(buffer, 1u)) {
        return 0;
    }

    buffer->data[buffer->length++] = ch;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int ecsvm_text_buffer_append_repeat(
    ecsvm_text_buffer_t *buffer,
    char ch,
    size_t count
)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (!ecsvm_text_buffer_append_char(buffer, ch)) {
            return 0;
        }
    }
    return 1;
}

static int ecsvm_table_append_separator(
    ecsvm_text_buffer_t *buffer,
    const ecsvm_table_column_t *columns,
    size_t column_count
)
{
    size_t index;

    for (index = 0u; index < column_count; ++index) {
        if (!ecsvm_text_buffer_append(buffer, "| ") ||
            !ecsvm_text_buffer_append_repeat(buffer, '-', columns[index].width) ||
            !ecsvm_text_buffer_append(buffer, " ")) {
            return 0;
        }
    }
    return ecsvm_text_buffer_append(buffer, "|\n");
}

static int ecsvm_table_append_row(
    ecsvm_text_buffer_t *buffer,
    const ecsvm_table_column_t *columns,
    size_t column_count,
    const char *const *cells
)
{
    size_t index;

    for (index = 0u; index < column_count; ++index) {
        size_t length;
        size_t padding;

        length = strlen(cells[index]);
        padding = columns[index].width > length ? columns[index].width - length : 0u;
        if (!ecsvm_text_buffer_append(buffer, "| ")) {
            return 0;
        }
        if (columns[index].right_align && !ecsvm_text_buffer_append_repeat(buffer, ' ', padding)) {
            return 0;
        }
        if (!ecsvm_text_buffer_append(buffer, cells[index])) {
            return 0;
        }
        if (!columns[index].right_align && !ecsvm_text_buffer_append_repeat(buffer, ' ', padding)) {
            return 0;
        }
        if (!ecsvm_text_buffer_append(buffer, " ")) {
            return 0;
        }
    }
    return ecsvm_text_buffer_append(buffer, "|\n");
}

static const char *ecsvm_blob_preview(const ecsvm_ecsbin_blob_t *blob)
{
    static char preview[65];
    size_t index;
    size_t length;

    if (blob == NULL) {
        return "<invalid>";
    }

    length = blob->length < sizeof(preview) - 1u ? (size_t)blob->length : sizeof(preview) - 1u;
    for (index = 0u; index < length; ++index) {
        unsigned char ch;

        ch = blob->data[index];
        if (ch == '\n' || ch == '\r' || ch == '\t' || ch < 32u || ch > 126u) {
            (void)snprintf(preview, sizeof(preview), "%s", "<binary>");
            return preview;
        }
    }

    memcpy(preview, blob->data, length);
    preview[length] = '\0';
    return preview;
}

static size_t ecsvm_max_size(size_t left, size_t right)
{
    return left > right ? left : right;
}

static const char *ecsvm_attribute_data_display(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_attribute_t *attribute
)
{
    uint32_t payload_type_id;
    const ecsvm_ecsbin_type_ref_t *payload_type;
    const ecsvm_ecsbin_blob_t *blob;

    if (module == NULL || attribute == NULL) {
        return "<invalid>";
    }

    if (ecsvm_ecsbin_attribute_expects_type_payload(module, attribute)) {
        if (!ecsvm_ecsbin_attribute_type_payload(module, attribute, &payload_type_id)) {
            return "<invalid>";
        }

        payload_type = ecsvm_ecsbin_type_ref(module, payload_type_id);
        return payload_type != NULL && payload_type->qualified_name != NULL
            ? payload_type->qualified_name
            : "<invalid>";
    }

    blob = ecsvm_ecsbin_blob_ref(module, attribute->data_blob_id);
    return ecsvm_blob_preview(blob);
}

ecsvm_status_t ecsvm_ecsbin_inspect_module(
    const ecsvm_ecsbin_module_t *module,
    char **out_text,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    ecsvm_text_buffer_t buffer;
    size_t index;

    if (diagnostic != NULL) {
        ecsvm_diagnostic_clear(diagnostic);
    }
    if (module == NULL || out_text == NULL) {
        ecsvm_inspect_set_error(error_message, error_message_capacity, diagnostic, ECSVM_DIAGNOSTIC_ARGUMENT, "module and output text are required");
        return ECSVM_ERROR_ARGUMENT;
    }

    *out_text = NULL;
    memset(&buffer, 0, sizeof(buffer));

#define ECSVM_APPEND_TEXT(text) do { if (!ecsvm_text_buffer_append(&buffer, (text))) goto oom; } while (0)
#define ECSVM_APPEND_ROW(cols, count, cells) do { if (!ecsvm_table_append_row(&buffer, (cols), (count), (cells))) goto oom; } while (0)
#define ECSVM_APPEND_SEP(cols, count) do { if (!ecsvm_table_append_separator(&buffer, (cols), (count))) goto oom; } while (0)

    {
        ecsvm_table_column_t cols[2] = {
            { "Table", strlen("Table"), 0 },
            { "Count", strlen("Count"), 1 }
        };
        char count_buf[8][32];
        const char *labels[] = {
            "Type References", "Field References", "Struct Definitions", "Field Definitions",
            "Function References", "Parameters", "Attributes", "Blobs"
        };
        size_t counts[] = {
            module->type_ref_count, module->field_ref_count, module->struct_def_count, module->field_def_count,
            module->function_ref_count, module->parameter_count, module->attribute_count, module->blob_count
        };
        const char *cells[2];

        for (index = 0u; index < 8u; ++index) {
            (void)snprintf(count_buf[index], sizeof(count_buf[index]), "%zu", counts[index]);
            cols[0].width = ecsvm_max_size(cols[0].width, strlen(labels[index]));
            cols[1].width = ecsvm_max_size(cols[1].width, strlen(count_buf[index]));
        }

        ECSVM_APPEND_TEXT("Summary\n");
        cells[0] = cols[0].header; cells[1] = cols[1].header; ECSVM_APPEND_ROW(cols, 2u, cells);
        ECSVM_APPEND_SEP(cols, 2u);
        for (index = 0u; index < 8u; ++index) {
            cells[0] = labels[index]; cells[1] = count_buf[index]; ECSVM_APPEND_ROW(cols, 2u, cells);
        }
        ECSVM_APPEND_TEXT("\n");
    }

    {
        ecsvm_table_column_t cols[4] = {
            { "Index", 5u, 1 }, { "Namespace", 9u, 0 }, { "Name", 4u, 0 }, { "Qualified", 9u, 0 }
        };
        const char *cells[4];
        char index_buf[32];

        for (index = 0u; index < module->type_ref_count; ++index) {
            const ecsvm_ecsbin_type_ref_t *type_ref = &module->type_refs[index];
            (void)snprintf(index_buf, sizeof(index_buf), "%zu", index + 1u);
            cols[0].width = ecsvm_max_size(cols[0].width, strlen(index_buf));
            cols[1].width = ecsvm_max_size(cols[1].width, strlen(type_ref->namespace_name));
            cols[2].width = ecsvm_max_size(cols[2].width, strlen(type_ref->name));
            cols[3].width = ecsvm_max_size(cols[3].width, strlen(type_ref->qualified_name));
        }
        ECSVM_APPEND_TEXT("Type References\n");
        cells[0]=cols[0].header; cells[1]=cols[1].header; cells[2]=cols[2].header; cells[3]=cols[3].header; ECSVM_APPEND_ROW(cols,4u,cells); ECSVM_APPEND_SEP(cols,4u);
        for (index = 0u; index < module->type_ref_count; ++index) {
            (void)snprintf(index_buf, sizeof(index_buf), "%zu", index + 1u);
            cells[0]=index_buf; cells[1]=module->type_refs[index].namespace_name; cells[2]=module->type_refs[index].name; cells[3]=module->type_refs[index].qualified_name; ECSVM_APPEND_ROW(cols,4u,cells);
        }
        ECSVM_APPEND_TEXT("\n");
    }

#define ECSVM_SIMPLE_TABLE_START(title) ECSVM_APPEND_TEXT(title "\n")
    {
        ecsvm_table_column_t cols[3] = { {"Index",5u,1}, {"Name",4u,0}, {"Type",4u,0} };
        const char *cells[3];
        char index_buf[32];
        for (index = 0u; index < module->field_ref_count; ++index) {
            const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->field_refs[index].type_id);
            const char *type_name = type_ref != NULL ? type_ref->qualified_name : "<invalid>";
            (void)snprintf(index_buf, sizeof(index_buf), "%zu", index + 1u);
            cols[0].width = ecsvm_max_size(cols[0].width, strlen(index_buf));
            cols[1].width = ecsvm_max_size(cols[1].width, strlen(module->field_refs[index].name));
            cols[2].width = ecsvm_max_size(cols[2].width, strlen(type_name));
        }
        ECSVM_SIMPLE_TABLE_START("Field References"); cells[0]=cols[0].header; cells[1]=cols[1].header; cells[2]=cols[2].header; ECSVM_APPEND_ROW(cols,3u,cells); ECSVM_APPEND_SEP(cols,3u);
        for (index = 0u; index < module->field_ref_count; ++index) { const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->field_refs[index].type_id); const char *type_name = type_ref != NULL ? type_ref->qualified_name : "<invalid>"; (void)snprintf(index_buf, sizeof(index_buf), "%zu", index + 1u); cells[0]=index_buf; cells[1]=module->field_refs[index].name; cells[2]=type_name; ECSVM_APPEND_ROW(cols,3u,cells);} ECSVM_APPEND_TEXT("\n");
    }

    {
        ecsvm_table_column_t cols[10] = { {"Index",5u,1}, {"Type",4u,0}, {"Kind",4u,0}, {"Flags",5u,1}, {"Field Start",11u,1}, {"Field Count",11u,1}, {"Attribute Start",15u,1}, {"Attribute Count",15u,1}, {"Size",4u,1}, {"Alignment",9u,1} };
        const char *cells[10];
        char bufv[10][32];
        for (index = 0u; index < module->struct_def_count; ++index) {
            const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->struct_defs[index].type_id);
            const char *kind = ecsvm_ecsbin_struct_is_component(module, &module->struct_defs[index]) ? "component" : "struct";
            (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u);
            (void)snprintf(bufv[1], sizeof(bufv[1]), "%u", (unsigned)module->struct_defs[index].flags);
            (void)snprintf(bufv[2], sizeof(bufv[2]), "%u", (unsigned)module->struct_defs[index].field_start);
            (void)snprintf(bufv[3], sizeof(bufv[3]), "%u", (unsigned)module->struct_defs[index].field_count);
            (void)snprintf(bufv[4], sizeof(bufv[4]), "%u", (unsigned)module->struct_defs[index].attribute_start);
            (void)snprintf(bufv[5], sizeof(bufv[5]), "%u", (unsigned)module->struct_defs[index].attribute_count);
            (void)snprintf(bufv[6], sizeof(bufv[6]), "%zu", module->struct_defs[index].size);
            (void)snprintf(bufv[7], sizeof(bufv[7]), "%zu", module->struct_defs[index].alignment);
            cols[0].width = ecsvm_max_size(cols[0].width, strlen(bufv[0])); cols[1].width = ecsvm_max_size(cols[1].width, strlen(type_ref != NULL ? type_ref->qualified_name : "<invalid>")); cols[2].width = ecsvm_max_size(cols[2].width, strlen(kind)); cols[3].width = ecsvm_max_size(cols[3].width, strlen(bufv[1])); cols[4].width = ecsvm_max_size(cols[4].width, strlen(bufv[2])); cols[5].width = ecsvm_max_size(cols[5].width, strlen(bufv[3])); cols[6].width = ecsvm_max_size(cols[6].width, strlen(bufv[4])); cols[7].width = ecsvm_max_size(cols[7].width, strlen(bufv[5])); cols[8].width = ecsvm_max_size(cols[8].width, strlen(bufv[6])); cols[9].width = ecsvm_max_size(cols[9].width, strlen(bufv[7]));
        }
        ECSVM_SIMPLE_TABLE_START("Struct Definitions"); cells[0]=cols[0].header; cells[1]=cols[1].header; cells[2]=cols[2].header; cells[3]=cols[3].header; cells[4]=cols[4].header; cells[5]=cols[5].header; cells[6]=cols[6].header; cells[7]=cols[7].header; cells[8]=cols[8].header; cells[9]=cols[9].header; ECSVM_APPEND_ROW(cols,10u,cells); ECSVM_APPEND_SEP(cols,10u);
        for (index = 0u; index < module->struct_def_count; ++index) { const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->struct_defs[index].type_id); const char *kind = ecsvm_ecsbin_struct_is_component(module, &module->struct_defs[index]) ? "component" : "struct"; (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%u", (unsigned)module->struct_defs[index].flags); (void)snprintf(bufv[2], sizeof(bufv[2]), "%u", (unsigned)module->struct_defs[index].field_start); (void)snprintf(bufv[3], sizeof(bufv[3]), "%u", (unsigned)module->struct_defs[index].field_count); (void)snprintf(bufv[4], sizeof(bufv[4]), "%u", (unsigned)module->struct_defs[index].attribute_start); (void)snprintf(bufv[5], sizeof(bufv[5]), "%u", (unsigned)module->struct_defs[index].attribute_count); (void)snprintf(bufv[6], sizeof(bufv[6]), "%zu", module->struct_defs[index].size); (void)snprintf(bufv[7], sizeof(bufv[7]), "%zu", module->struct_defs[index].alignment); cells[0]=bufv[0]; cells[1]=type_ref != NULL ? type_ref->qualified_name : "<invalid>"; cells[2]=kind; cells[3]=bufv[1]; cells[4]=bufv[2]; cells[5]=bufv[3]; cells[6]=bufv[4]; cells[7]=bufv[5]; cells[8]=bufv[6]; cells[9]=bufv[7]; ECSVM_APPEND_ROW(cols,10u,cells);} ECSVM_APPEND_TEXT("\n");
    }

    {
        ecsvm_table_column_t cols[4] = { {"Index",5u,1}, {"Field",5u,0}, {"Attribute Start",15u,1}, {"Attribute Count",15u,1} };
        const char *cells[4]; char bufv[3][32];
        for (index = 0u; index < module->field_def_count; ++index) { const ecsvm_ecsbin_field_ref_t *field_ref = (module->field_defs[index].field_id == 0u || module->field_defs[index].field_id > module->field_ref_count) ? NULL : &module->field_refs[module->field_defs[index].field_id - 1u]; (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%u", (unsigned)module->field_defs[index].attribute_start); (void)snprintf(bufv[2], sizeof(bufv[2]), "%u", (unsigned)module->field_defs[index].attribute_count); cols[0].width = ecsvm_max_size(cols[0].width, strlen(bufv[0])); cols[1].width = ecsvm_max_size(cols[1].width, strlen(field_ref != NULL ? field_ref->name : "<invalid>")); cols[2].width = ecsvm_max_size(cols[2].width, strlen(bufv[1])); cols[3].width = ecsvm_max_size(cols[3].width, strlen(bufv[2])); }
        ECSVM_SIMPLE_TABLE_START("Field Definitions"); cells[0]=cols[0].header; cells[1]=cols[1].header; cells[2]=cols[2].header; cells[3]=cols[3].header; ECSVM_APPEND_ROW(cols,4u,cells); ECSVM_APPEND_SEP(cols,4u);
        for (index = 0u; index < module->field_def_count; ++index) { const ecsvm_ecsbin_field_ref_t *field_ref = (module->field_defs[index].field_id == 0u || module->field_defs[index].field_id > module->field_ref_count) ? NULL : &module->field_refs[module->field_defs[index].field_id - 1u]; (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%u", (unsigned)module->field_defs[index].attribute_start); (void)snprintf(bufv[2], sizeof(bufv[2]), "%u", (unsigned)module->field_defs[index].attribute_count); cells[0]=bufv[0]; cells[1]=field_ref != NULL ? field_ref->name : "<invalid>"; cells[2]=bufv[1]; cells[3]=bufv[2]; ECSVM_APPEND_ROW(cols,4u,cells);} ECSVM_APPEND_TEXT("\n");
    }

    {
        ecsvm_table_column_t cols[9] = { {"Index",5u,1}, {"Namespace",9u,0}, {"Name",4u,0}, {"Return Type",11u,0}, {"Parameter Start",15u,1}, {"Parameter Count",15u,1}, {"Attribute Start",15u,1}, {"Attribute Count",15u,1}, {"Body Blob",9u,1} };
        const char *cells[9]; char bufv[6][32];
        for (index = 0u; index < module->function_ref_count; ++index) { const ecsvm_ecsbin_type_ref_t *return_type = ecsvm_ecsbin_function_return_type(module, &module->function_refs[index]); (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%u", (unsigned)module->function_refs[index].parameter_start); (void)snprintf(bufv[2], sizeof(bufv[2]), "%u", (unsigned)module->function_refs[index].parameter_count); (void)snprintf(bufv[3], sizeof(bufv[3]), "%u", (unsigned)module->function_refs[index].attribute_start); (void)snprintf(bufv[4], sizeof(bufv[4]), "%u", (unsigned)module->function_refs[index].attribute_count); (void)snprintf(bufv[5], sizeof(bufv[5]), "%u", (unsigned)module->function_refs[index].body_blob_id); cols[0].width = ecsvm_max_size(cols[0].width, strlen(bufv[0])); cols[1].width = ecsvm_max_size(cols[1].width, strlen(module->function_refs[index].namespace_name)); cols[2].width = ecsvm_max_size(cols[2].width, strlen(module->function_refs[index].name)); cols[3].width = ecsvm_max_size(cols[3].width, strlen(return_type != NULL ? return_type->qualified_name : "<invalid>")); cols[4].width = ecsvm_max_size(cols[4].width, strlen(bufv[1])); cols[5].width = ecsvm_max_size(cols[5].width, strlen(bufv[2])); cols[6].width = ecsvm_max_size(cols[6].width, strlen(bufv[3])); cols[7].width = ecsvm_max_size(cols[7].width, strlen(bufv[4])); cols[8].width = ecsvm_max_size(cols[8].width, strlen(bufv[5])); }
        ECSVM_SIMPLE_TABLE_START("Function References"); cells[0]=cols[0].header; cells[1]=cols[1].header; cells[2]=cols[2].header; cells[3]=cols[3].header; cells[4]=cols[4].header; cells[5]=cols[5].header; cells[6]=cols[6].header; cells[7]=cols[7].header; cells[8]=cols[8].header; ECSVM_APPEND_ROW(cols,9u,cells); ECSVM_APPEND_SEP(cols,9u);
        for (index = 0u; index < module->function_ref_count; ++index) { const ecsvm_ecsbin_type_ref_t *return_type = ecsvm_ecsbin_function_return_type(module, &module->function_refs[index]); (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%u", (unsigned)module->function_refs[index].parameter_start); (void)snprintf(bufv[2], sizeof(bufv[2]), "%u", (unsigned)module->function_refs[index].parameter_count); (void)snprintf(bufv[3], sizeof(bufv[3]), "%u", (unsigned)module->function_refs[index].attribute_start); (void)snprintf(bufv[4], sizeof(bufv[4]), "%u", (unsigned)module->function_refs[index].attribute_count); (void)snprintf(bufv[5], sizeof(bufv[5]), "%u", (unsigned)module->function_refs[index].body_blob_id); cells[0]=bufv[0]; cells[1]=module->function_refs[index].namespace_name; cells[2]=module->function_refs[index].name; cells[3]=return_type != NULL ? return_type->qualified_name : "<invalid>"; cells[4]=bufv[1]; cells[5]=bufv[2]; cells[6]=bufv[3]; cells[7]=bufv[4]; cells[8]=bufv[5]; ECSVM_APPEND_ROW(cols,9u,cells);} ECSVM_APPEND_TEXT("\n");
    }

    {
        ecsvm_table_column_t cols[6] = { {"Index",5u,1}, {"Name",4u,0}, {"Type",4u,0}, {"Default Value Blob",18u,1}, {"Attribute Start",15u,1}, {"Attribute Count",15u,1} };
        const char *cells[6]; char bufv[4][32];
        for (index = 0u; index < module->parameter_count; ++index) { const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->parameters[index].type_id); (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%u", (unsigned)module->parameters[index].default_value_blob_id); (void)snprintf(bufv[2], sizeof(bufv[2]), "%u", (unsigned)module->parameters[index].attribute_start); (void)snprintf(bufv[3], sizeof(bufv[3]), "%u", (unsigned)module->parameters[index].attribute_count); cols[0].width = ecsvm_max_size(cols[0].width, strlen(bufv[0])); cols[1].width = ecsvm_max_size(cols[1].width, strlen(module->parameters[index].name)); cols[2].width = ecsvm_max_size(cols[2].width, strlen(type_ref != NULL ? type_ref->qualified_name : "<invalid>")); cols[3].width = ecsvm_max_size(cols[3].width, strlen(bufv[1])); cols[4].width = ecsvm_max_size(cols[4].width, strlen(bufv[2])); cols[5].width = ecsvm_max_size(cols[5].width, strlen(bufv[3])); }
        ECSVM_SIMPLE_TABLE_START("Parameters"); cells[0]=cols[0].header; cells[1]=cols[1].header; cells[2]=cols[2].header; cells[3]=cols[3].header; cells[4]=cols[4].header; cells[5]=cols[5].header; ECSVM_APPEND_ROW(cols,6u,cells); ECSVM_APPEND_SEP(cols,6u);
        for (index = 0u; index < module->parameter_count; ++index) { const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->parameters[index].type_id); (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%u", (unsigned)module->parameters[index].default_value_blob_id); (void)snprintf(bufv[2], sizeof(bufv[2]), "%u", (unsigned)module->parameters[index].attribute_start); (void)snprintf(bufv[3], sizeof(bufv[3]), "%u", (unsigned)module->parameters[index].attribute_count); cells[0]=bufv[0]; cells[1]=module->parameters[index].name; cells[2]=type_ref != NULL ? type_ref->qualified_name : "<invalid>"; cells[3]=bufv[1]; cells[4]=bufv[2]; cells[5]=bufv[3]; ECSVM_APPEND_ROW(cols,6u,cells);} ECSVM_APPEND_TEXT("\n");
    }

    {
        ecsvm_table_column_t cols[3] = { {"Index",5u,1}, {"Type",4u,0}, {"Data",4u,0} };
        const char *cells[3]; char index_buf[32];
        for (index = 0u; index < module->attribute_count; ++index) { const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->attributes[index].type_id); const char *data_text = ecsvm_attribute_data_display(module, &module->attributes[index]); (void)snprintf(index_buf, sizeof(index_buf), "%zu", index + 1u); cols[0].width = ecsvm_max_size(cols[0].width, strlen(index_buf)); cols[1].width = ecsvm_max_size(cols[1].width, strlen(type_ref != NULL ? type_ref->qualified_name : "<invalid>")); cols[2].width = ecsvm_max_size(cols[2].width, strlen(data_text)); }
        ECSVM_SIMPLE_TABLE_START("Attributes"); cells[0]=cols[0].header; cells[1]=cols[1].header; cells[2]=cols[2].header; ECSVM_APPEND_ROW(cols,3u,cells); ECSVM_APPEND_SEP(cols,3u);
        for (index = 0u; index < module->attribute_count; ++index) { const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->attributes[index].type_id); (void)snprintf(index_buf, sizeof(index_buf), "%zu", index + 1u); cells[0]=index_buf; cells[1]=type_ref != NULL ? type_ref->qualified_name : "<invalid>"; cells[2]=ecsvm_attribute_data_display(module, &module->attributes[index]); ECSVM_APPEND_ROW(cols,3u,cells);} ECSVM_APPEND_TEXT("\n");
    }

    {
        ecsvm_table_column_t cols[4] = { {"Index",5u,1}, {"Offset",6u,1}, {"Length",6u,1}, {"Preview",7u,0} };
        const char *cells[4]; char bufv[3][32];
        for (index = 0u; index < module->blob_count; ++index) { (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%llu", (unsigned long long)module->blobs[index].offset); (void)snprintf(bufv[2], sizeof(bufv[2]), "%llu", (unsigned long long)module->blobs[index].length); cols[0].width = ecsvm_max_size(cols[0].width, strlen(bufv[0])); cols[1].width = ecsvm_max_size(cols[1].width, strlen(bufv[1])); cols[2].width = ecsvm_max_size(cols[2].width, strlen(bufv[2])); cols[3].width = ecsvm_max_size(cols[3].width, strlen(ecsvm_blob_preview(&module->blobs[index]))); }
        ECSVM_SIMPLE_TABLE_START("Blobs"); cells[0]=cols[0].header; cells[1]=cols[1].header; cells[2]=cols[2].header; cells[3]=cols[3].header; ECSVM_APPEND_ROW(cols,4u,cells); ECSVM_APPEND_SEP(cols,4u);
        for (index = 0u; index < module->blob_count; ++index) { (void)snprintf(bufv[0], sizeof(bufv[0]), "%zu", index + 1u); (void)snprintf(bufv[1], sizeof(bufv[1]), "%llu", (unsigned long long)module->blobs[index].offset); (void)snprintf(bufv[2], sizeof(bufv[2]), "%llu", (unsigned long long)module->blobs[index].length); cells[0]=bufv[0]; cells[1]=bufv[1]; cells[2]=bufv[2]; cells[3]=ecsvm_blob_preview(&module->blobs[index]); ECSVM_APPEND_ROW(cols,4u,cells);} 
    }

#undef ECSVM_APPEND_TEXT
#undef ECSVM_APPEND_ROW
#undef ECSVM_APPEND_SEP
#undef ECSVM_SIMPLE_TABLE_START

    *out_text = buffer.data;
    return ECSVM_OK;

oom:
    free(buffer.data);
    ecsvm_inspect_set_error(error_message, error_message_capacity, diagnostic, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, "out of memory while formatting inspect output");
    return ECSVM_ERROR_MEMORY;
}

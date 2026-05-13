#include "ecsvm/component.h"
#include "ecsvm/ecsbin.h"
#include "ecsvm/project.h"
#include "ecsvm/ecsvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct vec3 {
    float x;
    float y;
    float z;
} vec3_t;

typedef struct demo_components {
    ecsvm_core_components_t core;
    ecsvm_component_id_t position;
    ecsvm_component_id_t velocity;
} demo_components_t;

static demo_components_t g_demo_components;

int ecsvm_run_pong(void);
int ecsvm_run_pong_binary(const char *ecsbin_path);

static ecsvm_status_t demo_gravity(ecsvm_context_t *ctx)
{
    const float gravity = -9.8f;
    size_t index;

    for (index = 0u; index < ecsvm_entity_count(ctx->engine); ++index) {
        ecsvm_entity_t entity;
        vec3_t *velocity;

        entity = ecsvm_entity_at(ctx->engine, index);
        if (!ecsvm_component_has(ctx->engine, g_demo_components.velocity, entity)) {
            continue;
        }

        velocity = (vec3_t *)ecsvm_component_get_mutable(
            ctx->engine,
            g_demo_components.velocity,
            entity
        );
        if (velocity == NULL) {
            return ECSVM_ERROR_NOT_FOUND;
        }

        velocity->y += gravity;
    }

    ctx->api.log(ctx->api.userdata, "gravity complete");
    return ECSVM_OK;
}

static ecsvm_status_t demo_integrate(ecsvm_context_t *ctx)
{
    size_t index;

    for (index = 0u; index < ecsvm_entity_count(ctx->engine); ++index) {
        ecsvm_entity_t entity;
        vec3_t *position;
        const vec3_t *velocity;

        entity = ecsvm_entity_at(ctx->engine, index);
        if (!ecsvm_component_has(ctx->engine, g_demo_components.position, entity) ||
            !ecsvm_component_has(ctx->engine, g_demo_components.velocity, entity)) {
            continue;
        }

        position = (vec3_t *)ecsvm_component_get_mutable(
            ctx->engine,
            g_demo_components.position,
            entity
        );
        velocity = (const vec3_t *)ecsvm_component_get(
            ctx->engine,
            g_demo_components.velocity,
            entity
        );
        if (position == NULL || velocity == NULL) {
            return ECSVM_ERROR_NOT_FOUND;
        }

        position->x += velocity->x;
        position->y += velocity->y;
        position->z += velocity->z;
    }

    ctx->api.log(ctx->api.userdata, "integrate complete");
    return ECSVM_OK;
}

static void print_usage(const char *argv0)
{
    fprintf(
        stderr,
        "usage: %s --self-test | --pong | build <project> | run <project|ecsbin> | decompile <ecsbin> | inspect <ecsbin>\n",
        argv0
    );
}

typedef struct string_list {
    const char **items;
    size_t count;
    size_t capacity;
} string_list_t;

static void string_list_free(string_list_t *list)
{
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static int string_list_push_unique(string_list_t *list, const char *value)
{
    size_t index;
    const char *normalized;
    const char **items;
    size_t capacity;

    if (list == NULL) {
        return 0;
    }

    normalized = value != NULL ? value : "";
    for (index = 0u; index < list->count; ++index) {
        if (strcmp(list->items[index], normalized) == 0) {
            return 1;
        }
    }

    if (list->count == list->capacity) {
        capacity = list->capacity == 0u ? 8u : list->capacity * 2u;
        items = (const char **)realloc(list->items, capacity * sizeof(*items));
        if (items == NULL) {
            return 0;
        }
        list->items = items;
        list->capacity = capacity;
    }

    list->items[list->count] = normalized;
    list->count += 1u;
    return 1;
}

static const char *source_builtin_type_name(const char *qualified_name)
{
    if (qualified_name == NULL) {
        return NULL;
    }
    if (strcmp(qualified_name, "core.Entity") == 0) {
        return "entity";
    }
    if (strcmp(qualified_name, "core.Int32") == 0) {
        return "i32";
    }
    if (strcmp(qualified_name, "core.UInt32") == 0) {
        return "u32";
    }
    if (strcmp(qualified_name, "core.Float32") == 0) {
        return "f32";
    }
    if (strcmp(qualified_name, "core.Void") == 0) {
        return "void";
    }
    if (strcmp(qualified_name, "core.Blob") == 0) {
        return "blob";
    }
    if (strcmp(qualified_name, "core.String") == 0) {
        return "string";
    }
    if (strcmp(qualified_name, "core.Bool") == 0) {
        return "bool";
    }
    return NULL;
}

static const char *display_type_name(
    const ecsvm_ecsbin_type_ref_t *type_ref,
    const char *current_namespace
)
{
    const char *builtin_name;

    if (type_ref == NULL) {
        return "<invalid>";
    }

    builtin_name = source_builtin_type_name(type_ref->qualified_name);
    if (builtin_name != NULL) {
        return builtin_name;
    }

    if (current_namespace != NULL &&
        type_ref->namespace_name != NULL &&
        strcmp(type_ref->namespace_name, current_namespace) == 0) {
        return type_ref->name;
    }

    return type_ref->qualified_name;
}

static void print_prefixed_multiline(const char *text, const char *line_prefix)
{
    const char *cursor;

    if (text == NULL) {
        return;
    }

    cursor = text;
    while (*cursor != '\0') {
        putchar(*cursor);
        if (*cursor == '\n' && cursor[1] != '\0' && line_prefix != NULL) {
            fputs(line_prefix, stdout);
        }
        cursor += 1;
    }
}

static int load_module_or_report(
    const char *path,
    ecsvm_ecsbin_module_t *module
)
{
    char error_message[512];
    ecsvm_status_t status;

    memset(module, 0, sizeof(*module));
    status = ecsvm_ecsbin_load(path, module, error_message, sizeof(error_message));
    if (status != ECSVM_OK) {
        fprintf(
            stderr,
            "failed to load ecsbin: %s\n",
            error_message[0] != '\0' ? error_message : ecsvm_status_string(status)
        );
        return 0;
    }

    return 1;
}

static int print_decompiled_struct(
    const ecsvm_ecsbin_module_t *module,
    size_t struct_index,
    const char *current_namespace,
    const char *indent
)
{
    const ecsvm_ecsbin_struct_def_t *definition;
    const ecsvm_ecsbin_type_ref_t *type_ref;
    size_t attribute_index;
    size_t field_index;
    int is_component;

    definition = &module->struct_defs[struct_index];
    type_ref = ecsvm_ecsbin_type_ref(module, definition->type_id);
    if (type_ref == NULL) {
        fprintf(stderr, "decompile failed: struct type reference is invalid\n");
        return 0;
    }

    is_component = ecsvm_ecsbin_struct_is_component(module, definition);
    for (attribute_index = 0u; attribute_index < definition->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *attribute_type;

        attribute = ecsvm_ecsbin_attribute_ref(module, definition->attribute_start + (uint32_t)attribute_index);
        attribute_type = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
        if (attribute_type == NULL) {
            fprintf(stderr, "decompile failed: struct attribute is invalid\n");
            return 0;
        }
        if (is_component && strcmp(attribute_type->qualified_name, "core.Component") == 0) {
            continue;
        }

        printf(
            "%s[%s]\n",
            indent,
            display_type_name(attribute_type, current_namespace)
        );
    }

    printf("%s%s %s {\n", indent, is_component ? "component" : "struct", type_ref->name);
    for (field_index = 0u; field_index < definition->field_count; ++field_index) {
        const ecsvm_ecsbin_field_ref_t *field_ref;
        const ecsvm_ecsbin_type_ref_t *field_type;

        if (definition->field_start == 0u ||
            definition->field_start - 1u + field_index >= module->field_ref_count) {
            fprintf(stderr, "decompile failed: struct field range is invalid\n");
            return 0;
        }

        field_ref = &module->field_refs[definition->field_start - 1u + field_index];
        field_type = ecsvm_ecsbin_type_ref(module, field_ref->type_id);
        if (field_type == NULL) {
            fprintf(stderr, "decompile failed: field type reference is invalid\n");
            return 0;
        }

        printf(
            "%s    %s: %s;\n",
            indent,
            field_ref->name,
            display_type_name(field_type, current_namespace)
        );
    }
    printf("%s}\n", indent);
    return 1;
}

static int print_decompiled_function(
    const ecsvm_ecsbin_module_t *module,
    size_t function_index,
    const char *current_namespace,
    const char *indent
)
{
    const ecsvm_ecsbin_function_ref_t *function_ref;
    const ecsvm_ecsbin_type_ref_t *return_type;
    size_t parameter_index;

    function_ref = &module->function_refs[function_index];
    return_type = ecsvm_ecsbin_function_return_type(module, function_ref);
    if (return_type == NULL) {
        fprintf(stderr, "decompile failed: function return type is invalid\n");
        return 0;
    }

    printf("%sfn %s(", indent, function_ref->name);
    for (parameter_index = 0u; parameter_index < function_ref->parameter_count; ++parameter_index) {
        const ecsvm_ecsbin_parameter_t *parameter;
        const ecsvm_ecsbin_type_ref_t *parameter_type;

        parameter = ecsvm_ecsbin_parameter_ref(
            module,
            function_ref->parameter_start + (uint32_t)parameter_index
        );
        parameter_type = parameter != NULL ? ecsvm_ecsbin_type_ref(module, parameter->type_id) : NULL;
        if (parameter == NULL || parameter_type == NULL) {
            fprintf(stderr, "decompile failed: function parameter is invalid\n");
            return 0;
        }

        if (parameter_index > 0u) {
            fputs(", ", stdout);
        }
        printf(
            "%s: %s",
            parameter->name,
            display_type_name(parameter_type, current_namespace)
        );
        if (parameter->default_value_blob_id != 0u) {
            const ecsvm_ecsbin_blob_t *default_value_blob;

            default_value_blob = ecsvm_ecsbin_blob_ref(module, parameter->default_value_blob_id);
            if (default_value_blob == NULL) {
                fprintf(stderr, "decompile failed: parameter default value blob is invalid\n");
                return 0;
            }

            printf(
                " = %.*s",
                (int)default_value_blob->length,
                (const char *)default_value_blob->data
            );
        }
    }
    putchar(')');

    if (strcmp(return_type->qualified_name, "core.Void") != 0) {
        printf(": %s", display_type_name(return_type, current_namespace));
    }

    if (function_ref->body_blob_id == 0u) {
        puts(";");
    } else {
        char *body_source;
        char error_message[512];
        ecsvm_status_t status;

        body_source = NULL;
        status = ecsvm_ecsbin_decompile_function_body(
            module,
            function_ref,
            &body_source,
            error_message,
            sizeof(error_message)
        );
        if (status != ECSVM_OK) {
            fprintf(
                stderr,
                "decompile failed: %s\n",
                error_message[0] != '\0' ? error_message : ecsvm_status_string(status)
            );
            free(body_source);
            return 0;
        }

        putchar(' ');
        print_prefixed_multiline(body_source, indent);
        putchar('\n');
        free(body_source);
    }

    return 1;
}

static int run_decompile_command(const char *path)
{
    ecsvm_ecsbin_module_t module;
    string_list_t namespaces;
    size_t index;

    memset(&namespaces, 0, sizeof(namespaces));
    if (!load_module_or_report(path, &module)) {
        return 1;
    }

    for (index = 0u; index < module.struct_def_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *type_ref;

        type_ref = ecsvm_ecsbin_type_ref(&module, module.struct_defs[index].type_id);
        if (type_ref == NULL ||
            !string_list_push_unique(&namespaces, type_ref->namespace_name)) {
            fprintf(stderr, "decompile failed: out of memory while grouping namespaces\n");
            string_list_free(&namespaces);
            ecsvm_ecsbin_unload(&module);
            return 1;
        }
    }

    for (index = 0u; index < module.function_ref_count; ++index) {
        if (!string_list_push_unique(&namespaces, module.function_refs[index].namespace_name)) {
            fprintf(stderr, "decompile failed: out of memory while grouping namespaces\n");
            string_list_free(&namespaces);
            ecsvm_ecsbin_unload(&module);
            return 1;
        }
    }

    for (index = 0u; index < namespaces.count; ++index) {
        const char *namespace_name;
        const char *indent;
        size_t struct_index;
        size_t function_index;
        int emitted_anything;

        namespace_name = namespaces.items[index];
        indent = namespace_name[0] == '\0' ? "" : "    ";
        emitted_anything = 0;

        if (index > 0u) {
            putchar('\n');
        }
        if (namespace_name[0] != '\0') {
            printf("namespace %s {\n", namespace_name);
        }

        for (struct_index = 0u; struct_index < module.struct_def_count; ++struct_index) {
            const ecsvm_ecsbin_type_ref_t *type_ref;

            type_ref = ecsvm_ecsbin_type_ref(&module, module.struct_defs[struct_index].type_id);
            if (type_ref == NULL || strcmp(type_ref->namespace_name, namespace_name) != 0) {
                continue;
            }
            if (emitted_anything) {
                putchar('\n');
            }
            if (!print_decompiled_struct(&module, struct_index, namespace_name, indent)) {
                string_list_free(&namespaces);
                ecsvm_ecsbin_unload(&module);
                return 1;
            }
            emitted_anything = 1;
        }

        for (function_index = 0u; function_index < module.function_ref_count; ++function_index) {
            if (strcmp(module.function_refs[function_index].namespace_name, namespace_name) != 0) {
                continue;
            }
            if (emitted_anything) {
                putchar('\n');
            }
            if (!print_decompiled_function(&module, function_index, namespace_name, indent)) {
                string_list_free(&namespaces);
                ecsvm_ecsbin_unload(&module);
                return 1;
            }
            emitted_anything = 1;
        }

        if (namespace_name[0] != '\0') {
            puts("}");
        }
    }

    string_list_free(&namespaces);
    ecsvm_ecsbin_unload(&module);
    return 0;
}

static const char *blob_preview(const ecsvm_ecsbin_blob_t *blob)
{
    static char preview[65];
    size_t index;
    size_t length;
    int printable_only;

    if (blob == NULL) {
        return "<invalid>";
    }

    length = blob->length < sizeof(preview) - 1u ? (size_t)blob->length : sizeof(preview) - 1u;
    printable_only = 1;
    for (index = 0u; index < length; ++index) {
        unsigned char ch;

        ch = blob->data[index];
        if (ch == '\n' || ch == '\r' || ch == '\t' || ch < 32u || ch > 126u) {
            printable_only = 0;
            break;
        }
    }

    if (!printable_only) {
        snprintf(preview, sizeof(preview), "<binary>");
        return preview;
    }

    memcpy(preview, blob->data, length);
    preview[length] = '\0';
    return preview;
}

static int run_inspect_command(const char *path)
{
    ecsvm_ecsbin_module_t module;
    size_t index;

    if (!load_module_or_report(path, &module)) {
        return 1;
    }

    printf("| Table | Count |\n");
    printf("| --- | ---: |\n");
    printf("| Type References | %zu |\n", module.type_ref_count);
    printf("| Field References | %zu |\n", module.field_ref_count);
    printf("| Struct Definitions | %zu |\n", module.struct_def_count);
    printf("| Field Definitions | %zu |\n", module.field_def_count);
    printf("| Function References | %zu |\n", module.function_ref_count);
    printf("| Parameters | %zu |\n", module.parameter_count);
    printf("| Attributes | %zu |\n", module.attribute_count);
    printf("| Blobs | %zu |\n", module.blob_count);

    printf("\n| Index | Namespace | Name | Qualified |\n");
    printf("| ---: | --- | --- | --- |\n");
    for (index = 0u; index < module.type_ref_count; ++index) {
        printf(
            "| %zu | %s | %s | %s |\n",
            index + 1u,
            module.type_refs[index].namespace_name,
            module.type_refs[index].name,
            module.type_refs[index].qualified_name
        );
    }

    printf("\n| Index | Name | Type |\n");
    printf("| ---: | --- | --- |\n");
    for (index = 0u; index < module.field_ref_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *type_ref;

        type_ref = ecsvm_ecsbin_type_ref(&module, module.field_refs[index].type_id);
        printf(
            "| %zu | %s | %s |\n",
            index + 1u,
            module.field_refs[index].name,
            type_ref != NULL ? type_ref->qualified_name : "<invalid>"
        );
    }

    printf("\n| Index | Type | Kind | Flags | Field Start | Field Count | Attribute Start | Attribute Count | Size | Alignment |\n");
    printf("| ---: | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n");
    for (index = 0u; index < module.struct_def_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *type_ref;

        type_ref = ecsvm_ecsbin_type_ref(&module, module.struct_defs[index].type_id);
        printf(
            "| %zu | %s | %s | %u | %u | %u | %u | %u | %zu | %zu |\n",
            index + 1u,
            type_ref != NULL ? type_ref->qualified_name : "<invalid>",
            ecsvm_ecsbin_struct_is_component(&module, &module.struct_defs[index]) ? "component" : "struct",
            (unsigned)module.struct_defs[index].flags,
            (unsigned)module.struct_defs[index].field_start,
            (unsigned)module.struct_defs[index].field_count,
            (unsigned)module.struct_defs[index].attribute_start,
            (unsigned)module.struct_defs[index].attribute_count,
            module.struct_defs[index].size,
            module.struct_defs[index].alignment
        );
    }

    printf("\n| Index | Field | Attribute Start | Attribute Count |\n");
    printf("| ---: | --- | ---: | ---: |\n");
    for (index = 0u; index < module.field_def_count; ++index) {
        const ecsvm_ecsbin_field_ref_t *field_ref;

        field_ref = module.field_defs[index].field_id == 0u ||
            module.field_defs[index].field_id > module.field_ref_count
            ? NULL
            : &module.field_refs[module.field_defs[index].field_id - 1u];
        printf(
            "| %zu | %s | %u | %u |\n",
            index + 1u,
            field_ref != NULL ? field_ref->name : "<invalid>",
            (unsigned)module.field_defs[index].attribute_start,
            (unsigned)module.field_defs[index].attribute_count
        );
    }

    printf("\n| Index | Namespace | Name | Return Type | Parameter Start | Parameter Count | Attribute Start | Attribute Count | Body Blob |\n");
    printf("| ---: | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |\n");
    for (index = 0u; index < module.function_ref_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *return_type;

        return_type = ecsvm_ecsbin_function_return_type(&module, &module.function_refs[index]);
        printf(
            "| %zu | %s | %s | %s | %u | %u | %u | %u | %u |\n",
            index + 1u,
            module.function_refs[index].namespace_name,
            module.function_refs[index].name,
            return_type != NULL ? return_type->qualified_name : "<invalid>",
            (unsigned)module.function_refs[index].parameter_start,
            (unsigned)module.function_refs[index].parameter_count,
            (unsigned)module.function_refs[index].attribute_start,
            (unsigned)module.function_refs[index].attribute_count,
            (unsigned)module.function_refs[index].body_blob_id
        );
    }

    printf("\n| Index | Name | Type | Default Value Blob | Attribute Start | Attribute Count |\n");
    printf("| ---: | --- | --- | ---: | ---: | ---: |\n");
    for (index = 0u; index < module.parameter_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *type_ref;

        type_ref = ecsvm_ecsbin_type_ref(&module, module.parameters[index].type_id);
        printf(
            "| %zu | %s | %s | %u | %u | %u |\n",
            index + 1u,
            module.parameters[index].name,
            type_ref != NULL ? type_ref->qualified_name : "<invalid>",
            (unsigned)module.parameters[index].default_value_blob_id,
            (unsigned)module.parameters[index].attribute_start,
            (unsigned)module.parameters[index].attribute_count
        );
    }

    printf("\n| Index | Type | Data |\n");
    printf("| ---: | --- | --- |\n");
    for (index = 0u; index < module.attribute_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *type_ref;

        type_ref = ecsvm_ecsbin_type_ref(&module, module.attributes[index].type_id);
        printf(
            "| %zu | %s | %s |\n",
            index + 1u,
            type_ref != NULL ? type_ref->qualified_name : "<invalid>",
            module.attributes[index].data
        );
    }

    printf("\n| Index | Offset | Length | Preview |\n");
    printf("| ---: | ---: | ---: | --- |\n");
    for (index = 0u; index < module.blob_count; ++index) {
        printf(
            "| %zu | %llu | %llu | %s |\n",
            index + 1u,
            (unsigned long long)module.blobs[index].offset,
            (unsigned long long)module.blobs[index].length,
            blob_preview(&module.blobs[index])
        );
    }

    ecsvm_ecsbin_unload(&module);
    return 0;
}

static int run_self_test(void)
{
    ecsvm_component_desc_t position_desc;
    ecsvm_component_desc_t velocity_desc;
    ecsvm_system_desc_t gravity_desc;
    ecsvm_system_desc_t integrate_desc;
    ecsvm_engine_t *engine;
    ecsvm_hierarchy_component_t hierarchy;
    vec3_t position;
    vec3_t velocity;
    ecsvm_entity_t entity;
    ecsvm_blob_t string_blob;
    ecsvm_status_t status;

    engine = ecsvm_engine_create();
    if (engine == NULL) {
        fprintf(stderr, "failed to create engine\n");
        return 1;
    }

    status = ecsvm_register_core_components(engine, &g_demo_components.core);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register built-ins: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    position_desc.name = "app.transform";
    position_desc.size = sizeof(position);
    position_desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    velocity_desc.name = "app.velocity";
    velocity_desc.size = sizeof(velocity);
    velocity_desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;

    status = ecsvm_engine_register_component(engine, &position_desc, &g_demo_components.position);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register position: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_engine_register_component(engine, &velocity_desc, &g_demo_components.velocity);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register velocity: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    gravity_desc.name = "app.gravity";
    gravity_desc.callback = demo_gravity;
    gravity_desc.alloc = NULL;
    gravity_desc.free = NULL;
    gravity_desc.log = NULL;
    gravity_desc.user_data = NULL;

    integrate_desc.name = "app.integrate";
    integrate_desc.callback = demo_integrate;
    integrate_desc.alloc = NULL;
    integrate_desc.free = NULL;
    integrate_desc.log = NULL;
    integrate_desc.user_data = NULL;

    status = ecsvm_engine_register_system(engine, &gravity_desc, NULL);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register gravity: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_engine_register_system(engine, &integrate_desc, NULL);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register integrate: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    entity = ecsvm_entity_create(engine);
    if (entity == ECSVM_INVALID_ENTITY) {
        fprintf(stderr, "failed to create entity\n");
        ecsvm_engine_destroy(engine);
        return 1;
    }

    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;

    velocity.x = 1.0f;
    velocity.y = 10.0f;
    velocity.z = 0.0f;

    hierarchy.parent = ECSVM_INVALID_ENTITY;
    hierarchy.first_child = ECSVM_INVALID_ENTITY;
    hierarchy.next_sibling = ECSVM_INVALID_ENTITY;
    hierarchy.prev_sibling = ECSVM_INVALID_ENTITY;

    status = ecsvm_component_set(engine, g_demo_components.position, entity, &position);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to attach position: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_component_set(engine, g_demo_components.velocity, entity, &velocity);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to attach velocity: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_component_set(
        engine,
        g_demo_components.core.hierarchy,
        entity,
        &hierarchy
    );
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to attach hierarchy: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_string_create(engine, "ecsvm self-test", &string_blob);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to allocate string: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_engine_run(engine);
    if (status != ECSVM_OK) {
        fprintf(stderr, "runtime failed: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    position = *(const vec3_t *)ecsvm_component_get(engine, g_demo_components.position, entity);
    printf(
        "self-test ok: entity=%u systems=%zu position=(%.2f, %.2f, %.2f) message=\"%s\"\n",
        (unsigned)entity,
        ecsvm_engine_system_count(engine),
        position.x,
        position.y,
        position.z,
        ecsvm_string_cstr(engine, string_blob)
    );

    ecsvm_engine_destroy(engine);
    return 0;
}

int main(int argc, char **argv)
{
    char output_path[512];
    char error_message[512];

    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        return run_self_test();
    }

    if (argc == 2 && strcmp(argv[1], "--pong") == 0) {
        return ecsvm_run_pong();
    }

    if (argc == 3 && strcmp(argv[1], "build") == 0) {
        ecsvm_status_t status;

        status = ecsvm_project_build(
            argv[2],
            output_path,
            sizeof(output_path),
            error_message,
            sizeof(error_message)
        );
        if (status != ECSVM_OK) {
            fprintf(stderr, "build failed: %s\n", error_message[0] != '\0' ? error_message : ecsvm_status_string(status));
            return 1;
        }

        printf("%s\n", output_path);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "run") == 0) {
        const char *binary_path;

        if (ecsvm_path_is_directory(argv[2])) {
            ecsvm_status_t status;

            status = ecsvm_project_build(
                argv[2],
                output_path,
                sizeof(output_path),
                error_message,
                sizeof(error_message)
            );
            if (status != ECSVM_OK) {
                fprintf(stderr, "build failed: %s\n", error_message[0] != '\0' ? error_message : ecsvm_status_string(status));
                return 1;
            }
            binary_path = output_path;
        } else if (ecsvm_path_has_extension(argv[2], ".ecsbin")) {
            binary_path = argv[2];
        } else {
            fprintf(stderr, "run expects a project directory or .ecsbin file\n");
            return 1;
        }

        return ecsvm_run_pong_binary(binary_path);
    }

    if (argc == 3 && strcmp(argv[1], "decompile") == 0) {
        return run_decompile_command(argv[2]);
    }

    if (argc == 3 && strcmp(argv[1], "inspect") == 0) {
        return run_inspect_command(argv[2]);
    }

    print_usage(argv[0]);
    return 1;
}

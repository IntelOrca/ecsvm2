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
int ecsvm_run_ecsbin(const char *ecsbin_path);

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

#include "ecsvm/diagnostic.h"
#include "ecsvm/logger.h"
#include "ecs_tree.h"
#include "project_internal.h"
#include "stream.h"
#include "xml.h"

#ifdef _WIN32
#include <direct.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#else
#include <limits.h>
#include <unistd.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

static void print_usage(const char *argv0)
{
    fprintf(
        stderr,
        "usage: %s [--log-level error|warning|info|debug] --self-test | --pong | build [--core-lib <ecsbin>] <project> | run [--core-lib <ecsbin>] <project|ecsbin> | decompile <ecsbin> | inspect <ecsbin> | parse <file.ecs>\n",
        argv0
    );
}

static int ecsvm_cli_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

#ifdef _WIN32
    return (path[0] >= 'A' && path[0] <= 'Z' && path[1] == ':') ||
        (path[0] >= 'a' && path[0] <= 'z' && path[1] == ':') ||
        path[0] == '\\' ||
        path[0] == '/';
#else
    return path[0] == '/';
#endif
}

static char *ecsvm_cli_make_absolute_path(const char *path)
{
    char cwd[PATH_MAX];
    size_t length;
    char *result;

    if (path == NULL) {
        return NULL;
    }

    if (ecsvm_cli_path_is_absolute(path)) {
        return ecsvm_copy_string(path);
    }

#ifdef _WIN32
    if (_getcwd(cwd, (int)sizeof(cwd)) == NULL) {
#else
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
#endif
        return ecsvm_copy_string(path);
    }

    length = strlen(cwd) + 1u + strlen(path) + 1u;
    result = (char *)malloc(length);
    if (result == NULL) {
        return NULL;
    }

    (void)snprintf(result, length, "%s/%s", cwd, path);
    return result;
}

static int ecsvm_cli_read_source_file(
    const char *path,
    ecsvm_source_file_t *file,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    FILE *stream;
    long length;

    memset(file, 0, sizeof(*file));
    file->path = ecsvm_cli_make_absolute_path(path);
    if (file->path == NULL) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while preparing source path");
        ecsvm_diagnostic_set(diagnostic, path, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
        return 0;
    }

    stream = fopen(path, "rb");
    if (stream == NULL) {
        ecsvm_set_error(error_message, error_message_capacity, "failed to read source file");
        ecsvm_diagnostic_set(diagnostic, file->path, 0u, 0u, ECSVM_DIAGNOSTIC_IO, "failed to read source file");
        return 0;
    }

    if (fseek(stream, 0, SEEK_END) != 0) {
        fclose(stream);
        ecsvm_set_error(error_message, error_message_capacity, "failed to seek source file");
        ecsvm_diagnostic_set(diagnostic, file->path, 0u, 0u, ECSVM_DIAGNOSTIC_IO, "failed to seek source file");
        return 0;
    }

    length = ftell(stream);
    if (length < 0 || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        ecsvm_set_error(error_message, error_message_capacity, "failed to read source file");
        ecsvm_diagnostic_set(diagnostic, file->path, 0u, 0u, ECSVM_DIAGNOSTIC_IO, "failed to read source file");
        return 0;
    }

    file->source = (char *)malloc((size_t)length + 1u);
    if (file->source == NULL) {
        fclose(stream);
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while reading source file");
        ecsvm_diagnostic_set(
            diagnostic,
            file->path,
            0u,
            0u,
            ECSVM_DIAGNOSTIC_OUT_OF_MEMORY,
            "out of memory while reading source file"
        );
        return 0;
    }

    if (length > 0 && fread(file->source, 1u, (size_t)length, stream) != (size_t)length) {
        fclose(stream);
        ecsvm_set_error(error_message, error_message_capacity, "failed to read source file");
        ecsvm_diagnostic_set(diagnostic, file->path, 0u, 0u, ECSVM_DIAGNOSTIC_IO, "failed to read source file");
        return 0;
    }

    fclose(stream);
    file->source[length] = '\0';
    file->length = (size_t)length;
    return 1;
}

static void log_failure(
    const ecsvm_logger_t *logger,
    const char *prefix,
    ecsvm_status_t status,
    const ecsvm_diagnostic_t *diagnostic,
    const char *error_message
)
{
    char formatted[1024];

    if (ecsvm_diagnostic_format(diagnostic, formatted, sizeof(formatted)) && formatted[0] != '\0') {
        ecsvm_logger_log(logger, ECSVM_LOG_LEVEL_ERROR, "%s: %s", prefix, formatted);
        return;
    }

    ecsvm_logger_log(
        logger,
        ECSVM_LOG_LEVEL_ERROR,
        "%s: %s",
        prefix,
        (error_message != NULL && error_message[0] != '\0') ? error_message : ecsvm_status_string(status)
    );
}

static int load_module_for_cli(
    const char *path,
    ecsvm_ecsbin_module_t *module,
    const ecsvm_logger_t *logger,
    ecsvm_diagnostic_t *diagnostic
)
{
    char error_message[512];
    ecsvm_status_t status;

    status = ecsvm_ecsbin_load_ex(path, module, error_message, sizeof(error_message), diagnostic);
    if (status != ECSVM_OK) {
        log_failure(logger, "failed to load ecsbin", status, diagnostic, error_message);
        return 0;
    }

    return 1;
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

    memset(&gravity_desc, 0, sizeof(gravity_desc));
    memset(&integrate_desc, 0, sizeof(integrate_desc));

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

    status = ecsvm_engine_tick(engine);
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
    ecsvm_logger_t logger;
    ecsvm_stdio_logger_t logger_state;
    ecsvm_log_level_t log_level;
    ecsvm_diagnostic_t diagnostic;
    int argi;

    log_level = ECSVM_LOG_LEVEL_ERROR;
    ecsvm_diagnostic_clear(&diagnostic);
    ecsvm_stdio_logger_init(&logger, &logger_state, stderr, stderr, log_level);

    argi = 1;
    while (argi < argc) {
        if (strcmp(argv[argi], "--log-level") == 0) {
            if (argi + 1 >= argc || !ecsvm_log_level_parse(argv[argi + 1], &log_level)) {
                fprintf(stderr, "invalid --log-level value\n");
                return 1;
            }
            ecsvm_stdio_logger_init(&logger, &logger_state, stderr, stderr, log_level);
            argi += 2;
            continue;
        }
        if (strncmp(argv[argi], "--log-level=", 12) == 0) {
            if (!ecsvm_log_level_parse(argv[argi] + 12, &log_level)) {
                fprintf(stderr, "invalid --log-level value\n");
                return 1;
            }
            ecsvm_stdio_logger_init(&logger, &logger_state, stderr, stderr, log_level);
            argi += 1;
            continue;
        }
        break;
    }

    if (argc - argi == 1 && strcmp(argv[argi], "--self-test") == 0) {
        return run_self_test();
    }

    if (argc - argi == 1 && strcmp(argv[argi], "--pong") == 0) {
        return ecsvm_run_pong();
    }

    if ((argc - argi == 2 || argc - argi == 4) && strcmp(argv[argi], "build") == 0) {
        ecsvm_status_t status;
        const char *core_library_path;
        const char *project_path;

        core_library_path = NULL;
        project_path = argv[argi + 1];
        if (argc - argi == 4 &&
            strcmp(argv[argi + 1], "--core-lib") == 0) {
            core_library_path = argv[argi + 2];
            project_path = argv[argi + 3];
        }

        if (argc - argi != 2 &&
            !(argc - argi == 4 && strcmp(argv[argi + 1], "--core-lib") == 0)) {
            print_usage(argv[0]);
            return 1;
        }

        status = ecsvm_project_build_with_core_ex(
            project_path,
            core_library_path,
            output_path,
            sizeof(output_path),
            error_message,
            sizeof(error_message),
            &logger,
            &diagnostic
        );
        if (status != ECSVM_OK) {
            log_failure(&logger, "build failed", status, &diagnostic, error_message);
            return 1;
        }

        printf("%s\n", output_path);
        return 0;
    }

    if ((argc - argi == 2 || argc - argi == 4) && strcmp(argv[argi], "run") == 0) {
        const char *binary_path;
        const char *core_library_path;
        const char *run_target;

        core_library_path = NULL;
        run_target = argv[argi + 1];
        if (argc - argi == 4 &&
            strcmp(argv[argi + 1], "--core-lib") == 0) {
            core_library_path = argv[argi + 2];
            run_target = argv[argi + 3];
        }
        if (argc - argi != 2 &&
            !(argc - argi == 4 && strcmp(argv[argi + 1], "--core-lib") == 0)) {
            print_usage(argv[0]);
            return 1;
        }

        if (ecsvm_path_is_directory(run_target)) {
            ecsvm_status_t status;

            status = ecsvm_project_build_with_core_ex(
                run_target,
                core_library_path,
                output_path,
                sizeof(output_path),
                error_message,
                sizeof(error_message),
                &logger,
                &diagnostic
            );
            if (status != ECSVM_OK) {
                log_failure(&logger, "build failed", status, &diagnostic, error_message);
                return 1;
            }
            binary_path = output_path;
        } else if (ecsvm_path_has_extension(run_target, ".ecsbin")) {
            binary_path = run_target;
        } else {
            ecsvm_logger_log(&logger, ECSVM_LOG_LEVEL_ERROR, "run expects a project directory or .ecsbin file");
            return 1;
        }

        return ecsvm_run_ecsbin(binary_path);
    }

    if (argc - argi == 2 && strcmp(argv[argi], "decompile") == 0) {
        ecsvm_ecsbin_module_t module;
        char *source;
        ecsvm_status_t status;

        memset(&module, 0, sizeof(module));
        if (!load_module_for_cli(argv[argi + 1], &module, &logger, &diagnostic)) {
            return 1;
        }

        source = NULL;
        status = ecsvm_ecsbin_decompile_module(&module, &source, error_message, sizeof(error_message), &diagnostic);
        if (status != ECSVM_OK) {
            ecsvm_ecsbin_unload(&module);
            free(source);
            log_failure(&logger, "decompile failed", status, &diagnostic, error_message);
            return 1;
        }

        fputs(source, stdout);
        free(source);
        ecsvm_ecsbin_unload(&module);
        return 0;
    }

    if (argc - argi == 2 && strcmp(argv[argi], "inspect") == 0) {
        ecsvm_ecsbin_module_t module;
        char *text;
        ecsvm_status_t status;

        memset(&module, 0, sizeof(module));
        if (!load_module_for_cli(argv[argi + 1], &module, &logger, &diagnostic)) {
            return 1;
        }

        text = NULL;
        status = ecsvm_ecsbin_inspect_module(&module, &text, error_message, sizeof(error_message), &diagnostic);
        if (status != ECSVM_OK) {
            ecsvm_ecsbin_unload(&module);
            free(text);
            log_failure(&logger, "inspect failed", status, &diagnostic, error_message);
            return 1;
        }

        fputs(text, stdout);
        free(text);
        ecsvm_ecsbin_unload(&module);
        return 0;
    }

    if (argc - argi == 2 && strcmp(argv[argi], "parse") == 0) {
        ecsvm_ecs_tree_t tree;
        ecsvm_file_stream_t stdout_stream;
        ecsvm_source_file_t file;
        ecsvm_xml_writer_t writer;
        int ok;
        int wrote_xml;

        memset(&file, 0, sizeof(file));
        ok = ecsvm_cli_read_source_file(
            argv[argi + 1],
            &file,
            error_message,
            sizeof(error_message),
            &diagnostic
        );
        if (ok) {
            ok = ecsvm_lex_source(&file, error_message, sizeof(error_message), &diagnostic) &&
                ecsvm_parse_file(&file, error_message, sizeof(error_message), &diagnostic);
        }

        ecsvm_ecs_tree_init(&tree, &file, &diagnostic);
        ecsvm_file_stream_init(&stdout_stream, stdout);
        ecsvm_xml_writer_init(&writer, &stdout_stream.stream);
        wrote_xml = ecsvm_xml_writer_write_declaration(&writer) &&
            ecsvm_ecs_tree_write_xml(&tree, &writer);
        ecsvm_xml_writer_free(&writer);
        if (!wrote_xml) {
            ecsvm_source_file_free(&file);
            ecsvm_logger_log(&logger, ECSVM_LOG_LEVEL_ERROR, "failed to write parse output");
            return 1;
        }

        ecsvm_source_file_free(&file);
        return ok ? 0 : 1;
    }

    print_usage(argv[0]);
    return 1;
}

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

int ecsvm_run_ecsbin(const char *ecsbin_path);
int ecsvm_run_project(const char *project_path, const char *core_library_path, const char *ecsbin_path);

static ecsvm_status_t demo_gravity(ecsvm_engine_t *engine)
{
    const float gravity = -9.8f;
    size_t index;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    for (index = 0u; index < ecsvm_entity_count(engine); ++index) {
        ecsvm_entity_t entity;
        vec3_t *velocity;

        entity = ecsvm_entity_at(engine, index);
        if (!ecsvm_component_has(engine, g_demo_components.velocity, entity)) {
            continue;
        }

        velocity = (vec3_t *)ecsvm_component_get_mutable(
            engine,
            g_demo_components.velocity,
            entity
        );
        if (velocity == NULL) {
            return ECSVM_ERROR_NOT_FOUND;
        }

        velocity->y += gravity;
    }

    engine->log(engine, "gravity complete");
    return ECSVM_OK;
}

static ecsvm_status_t demo_integrate(ecsvm_engine_t *engine)
{
    size_t index;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    for (index = 0u; index < ecsvm_entity_count(engine); ++index) {
        ecsvm_entity_t entity;
        vec3_t *position;
        const vec3_t *velocity;

        entity = ecsvm_entity_at(engine, index);
        if (!ecsvm_component_has(engine, g_demo_components.position, entity) ||
            !ecsvm_component_has(engine, g_demo_components.velocity, entity)) {
            continue;
        }

        position = (vec3_t *)ecsvm_component_get_mutable(
            engine,
            g_demo_components.position,
            entity
        );
        velocity = (const vec3_t *)ecsvm_component_get(
            engine,
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

    engine->log(engine, "integrate complete");
    return ECSVM_OK;
}

#include "ecsvm/diagnostic.h"
#include "ecsvm/logger.h"
#include "ecs_tree.h"
#include "file_util.h"
#include "path_util.h"
#include "project_internal.h"
#include "stream.h"
#include "xml.h"

static void print_usage(const char *argv0)
{
    fprintf(
        stderr,
        "usage: %s [--log-level error|warning|info|debug] --self-test | build [--core-lib <ecsbin>] <project> | run [--core-lib <ecsbin>] <project|ecsbin> | decompile <ecsbin> | inspect <ecsbin> | parse <file.ecs>\n",
        argv0
    );
}

static int ecsvm_cli_read_source_file(
    const char *path,
    ecsvm_source_file_t *file,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    memset(file, 0, sizeof(*file));
    file->path = ecsvm_path_make_absolute(path);
    if (file->path == NULL) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while preparing source path");
        ecsvm_diagnostic_set(diagnostic, path, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
        return 0;
    }

    if (!ecsvm_read_text_file(path, &file->source, &file->length)) {
        ecsvm_set_error(error_message, error_message_capacity, "failed to read source file");
        ecsvm_diagnostic_set(diagnostic, file->path, 0u, 0u, ECSVM_DIAGNOSTIC_IO, "failed to read source file");
        return 0;
    }
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

static int parse_optional_core_lib_args(
    int argc,
    char **argv,
    int argi,
    const char **out_core_library_path,
    const char **out_target
)
{
    if (argc - argi == 2) {
        *out_core_library_path = NULL;
        *out_target = argv[argi + 1];
        return 1;
    }

    if (argc - argi == 4 && strcmp(argv[argi + 1], "--core-lib") == 0) {
        *out_core_library_path = argv[argi + 2];
        *out_target = argv[argi + 3];
        return 1;
    }

    return 0;
}

static int build_project_for_cli(
    const char *project_path,
    const char *core_library_path,
    char *output_path,
    size_t output_path_capacity,
    char *error_message,
    size_t error_message_capacity,
    const ecsvm_logger_t *logger,
    ecsvm_diagnostic_t *diagnostic
)
{
    ecsvm_status_t status;

    status = ecsvm_project_build_with_core_ex(
        project_path,
        core_library_path,
        output_path,
        output_path_capacity,
        error_message,
        error_message_capacity,
        logger,
        diagnostic
    );
    if (status != ECSVM_OK) {
        log_failure(logger, "build failed", status, diagnostic, error_message);
        return 0;
    }

    return 1;
}

typedef ecsvm_status_t (*ecsvm_cli_module_text_fn)(
    const ecsvm_ecsbin_module_t *module,
    char **out_text,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
);

static int run_module_text_command(
    const char *path,
    const char *failure_prefix,
    ecsvm_cli_module_text_fn command,
    char *error_message,
    size_t error_message_capacity,
    const ecsvm_logger_t *logger,
    ecsvm_diagnostic_t *diagnostic
)
{
    ecsvm_ecsbin_module_t module;
    char *text;
    ecsvm_status_t status;

    memset(&module, 0, sizeof(module));
    if (!load_module_for_cli(path, &module, logger, diagnostic)) {
        return 1;
    }

    text = NULL;
    status = command(&module, &text, error_message, error_message_capacity, diagnostic);
    if (status != ECSVM_OK) {
        ecsvm_ecsbin_unload(&module);
        free(text);
        log_failure(logger, failure_prefix, status, diagnostic, error_message);
        return 1;
    }

    fputs(text, stdout);
    free(text);
    ecsvm_ecsbin_unload(&module);
    return 0;
}

static int run_self_test(void)
{
    ecsvm_component_desc_t position_desc;
    ecsvm_component_desc_t velocity_desc;
    ecsvm_system_definition_t gravity_desc;
    ecsvm_system_definition_t integrate_desc;
    ecsvm_engine_t *engine;
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
    gravity_desc.main = demo_gravity;

    integrate_desc.name = "app.integrate";
    integrate_desc.main = demo_integrate;

    status = ecsvm_engine_register_system(engine, &gravity_desc);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register gravity: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_engine_register_system(engine, &integrate_desc);
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

    if (!ecsvm_component_has(engine, g_demo_components.core.hierarchy, entity)) {
        fprintf(stderr, "failed to auto-attach hierarchy\n");
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

    if ((argc - argi == 2 || argc - argi == 4) && strcmp(argv[argi], "build") == 0) {
        const char *core_library_path;
        const char *project_path;

        if (!parse_optional_core_lib_args(argc, argv, argi, &core_library_path, &project_path)) {
            print_usage(argv[0]);
            return 1;
        }

        if (!build_project_for_cli(
                project_path,
                core_library_path,
                output_path,
                sizeof(output_path),
                error_message,
                sizeof(error_message),
                &logger,
                &diagnostic
            )) {
            return 1;
        }

        printf("%s\n", output_path);
        return 0;
    }

    if ((argc - argi == 2 || argc - argi == 4) && strcmp(argv[argi], "run") == 0) {
        const char *core_library_path;
        const char *run_target;

        if (!parse_optional_core_lib_args(argc, argv, argi, &core_library_path, &run_target)) {
            print_usage(argv[0]);
            return 1;
        }

        if (ecsvm_path_is_directory(run_target)) {
            if (!build_project_for_cli(
                    run_target,
                    core_library_path,
                    output_path,
                    sizeof(output_path),
                    error_message,
                    sizeof(error_message),
                    &logger,
                    &diagnostic
                )) {
                return 1;
            }
            return ecsvm_run_project(run_target, core_library_path, output_path);
        } else if (ecsvm_path_has_extension(run_target, ".ecsbin")) {
            return ecsvm_run_ecsbin(run_target);
        } else {
            ecsvm_logger_log(&logger, ECSVM_LOG_LEVEL_ERROR, "run expects a project directory or .ecsbin file");
            return 1;
        }
    }

    if (argc - argi == 2 && strcmp(argv[argi], "decompile") == 0) {
        return run_module_text_command(
            argv[argi + 1],
            "decompile failed",
            ecsvm_ecsbin_decompile_module,
            error_message,
            sizeof(error_message),
            &logger,
            &diagnostic
        );
    }

    if (argc - argi == 2 && strcmp(argv[argi], "inspect") == 0) {
        return run_module_text_command(
            argv[argi + 1],
            "inspect failed",
            ecsvm_ecsbin_inspect_module,
            error_message,
            sizeof(error_message),
            &logger,
            &diagnostic
        );
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

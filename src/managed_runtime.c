#include "ecsvm_internal.h"

#include "bin_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ecsvm_managed_local {
    uint32_t name_blob_id;
    ecsvm_managed_value_t value;
} ecsvm_managed_local_t;

typedef struct ecsvm_managed_runtime {
    ecsvm_engine_t *engine;
    const ecsvm_ecsbin_module_t *module;
} ecsvm_managed_runtime_t;

typedef struct ecsvm_managed_frame {
    ecsvm_managed_runtime_t *runtime;
    const ecsvm_ecsbin_function_ref_t *function_ref;
    ecsvm_ecsbin_ast_blob_t ast;
    ecsvm_managed_value_t *arguments;
    ecsvm_managed_local_t *locals;
    size_t local_count;
    size_t local_capacity;
    ecsvm_managed_value_t return_value;
    int has_return;
} ecsvm_managed_frame_t;

typedef struct ecsvm_managed_system_binding {
    ecsvm_managed_runtime_t *runtime;
    uint32_t function_id;
} ecsvm_managed_system_binding_t;

enum {
    ECSVM_MANAGED_STACK_BUFFER_CAPACITY = 128,
    ECSVM_MANAGED_CALL_ARGUMENT_LIMIT = 16
};

static const ecsvm_ecsbin_blob_t *ecsvm_managed_blob(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id
)
{
    return ecsvm_ecsbin_blob_ref(module, blob_id);
}

static int ecsvm_managed_blob_equals_cstr(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id,
    const char *text
)
{
    const ecsvm_ecsbin_blob_t *blob;
    size_t length;

    blob = ecsvm_managed_blob(module, blob_id);
    length = strlen(text);
    return blob != NULL &&
        blob->length == length &&
        (length == 0u || memcmp(blob->data, text, length) == 0);
}

static int ecsvm_managed_blob_to_double(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id,
    double *out_value
)
{
    const ecsvm_ecsbin_blob_t *blob;
    char stack_buffer[ECSVM_MANAGED_STACK_BUFFER_CAPACITY];
    char *buffer;
    char *endptr;
    int ok;

    blob = ecsvm_managed_blob(module, blob_id);
    if (blob == NULL || out_value == NULL) {
        return 0;
    }

    buffer = blob->length < sizeof(stack_buffer)
        ? stack_buffer
        : (char *)malloc((size_t)blob->length + 1u);
    if (buffer == NULL) {
        return 0;
    }

    memcpy(buffer, blob->data, (size_t)blob->length);
    buffer[blob->length] = '\0';
    errno = 0;
    *out_value = strtod(buffer, &endptr);
    ok = errno == 0 && endptr != buffer && *endptr == '\0';
    if (buffer != stack_buffer) {
        free(buffer);
    }
    return ok;
}

static int ecsvm_managed_is_truthy(const ecsvm_managed_value_t *value)
{
    switch (value->kind) {
        case ECSVM_MANAGED_VALUE_NULL:
        case ECSVM_MANAGED_VALUE_VOID:
            return 0;
        case ECSVM_MANAGED_VALUE_BOOL:
            return value->boolean_value != 0;
        case ECSVM_MANAGED_VALUE_NUMBER:
            return value->number_value != 0.0;
        case ECSVM_MANAGED_VALUE_STRING:
            return value->blob_id != 0u;
        default:
            return 0;
    }
}

static int ecsvm_managed_values_equal(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_managed_value_t *left,
    const ecsvm_managed_value_t *right
)
{
    if (left->kind != right->kind) {
        return 0;
    }

    switch (left->kind) {
        case ECSVM_MANAGED_VALUE_VOID:
        case ECSVM_MANAGED_VALUE_NULL:
            return 1;
        case ECSVM_MANAGED_VALUE_BOOL:
            return left->boolean_value == right->boolean_value;
        case ECSVM_MANAGED_VALUE_NUMBER:
            return left->number_value == right->number_value;
        case ECSVM_MANAGED_VALUE_STRING: {
            const ecsvm_ecsbin_blob_t *left_blob;
            const ecsvm_ecsbin_blob_t *right_blob;

            left_blob = ecsvm_managed_blob(module, left->blob_id);
            right_blob = ecsvm_managed_blob(module, right->blob_id);
            return left_blob != NULL &&
                right_blob != NULL &&
                left_blob->length == right_blob->length &&
                (left_blob->length == 0u || memcmp(left_blob->data, right_blob->data, (size_t)left_blob->length) == 0);
        }
        default:
            return 0;
    }
}

static ecsvm_managed_value_t ecsvm_managed_null_value(void)
{
    ecsvm_managed_value_t value;
    memset(&value, 0, sizeof(value));
    value.kind = ECSVM_MANAGED_VALUE_NULL;
    return value;
}

static ecsvm_managed_value_t ecsvm_managed_void_value(void)
{
    ecsvm_managed_value_t value;
    memset(&value, 0, sizeof(value));
    value.kind = ECSVM_MANAGED_VALUE_VOID;
    return value;
}

static int ecsvm_managed_frame_set_local(
    ecsvm_managed_frame_t *frame,
    uint32_t name_blob_id,
    ecsvm_managed_value_t value
)
{
    size_t index;

    for (index = 0u; index < frame->local_count; ++index) {
        if (frame->locals[index].name_blob_id == name_blob_id) {
            frame->locals[index].value = value;
            return 1;
        }
    }

    if (frame->local_count == frame->local_capacity) {
        size_t capacity;
        ecsvm_managed_local_t *locals;

        capacity = frame->local_capacity == 0u ? 8u : frame->local_capacity * 2u;
        locals = (ecsvm_managed_local_t *)realloc(frame->locals, capacity * sizeof(*locals));
        if (locals == NULL) {
            return 0;
        }
        frame->locals = locals;
        frame->local_capacity = capacity;
    }

    frame->locals[frame->local_count].name_blob_id = name_blob_id;
    frame->locals[frame->local_count].value = value;
    frame->local_count += 1u;
    return 1;
}

static int ecsvm_managed_frame_get_local(
    const ecsvm_managed_frame_t *frame,
    uint32_t name_blob_id,
    ecsvm_managed_value_t *out_value
)
{
    size_t index;

    for (index = 0u; index < frame->local_count; ++index) {
        if (frame->locals[index].name_blob_id == name_blob_id) {
            *out_value = frame->locals[index].value;
            return 1;
        }
    }

    return 0;
}

static ecsvm_status_t ecsvm_managed_invoke_function(
    ecsvm_managed_runtime_t *runtime,
    uint32_t function_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
);

static ecsvm_status_t ecsvm_managed_eval_expression(
    ecsvm_managed_frame_t *frame,
    uint32_t node_index,
    ecsvm_managed_value_t *out_value
)
{
    const ecsvm_ecsbin_ast_node_t *node;
    const ecsvm_ecsbin_blob_t *blob;

    node = &frame->ast.nodes[node_index];
    switch (node->kind) {
        case ECSVM_ECSBIN_AST_NODE_IDENTIFIER:
            if (node->value_kind == ECSVM_ECSBIN_AST_VALUE_PARAMETER_ID) {
                uint32_t parameter_index;
                parameter_index = node->value - frame->function_ref->parameter_start;
                if (frame->function_ref->parameter_start == 0u ||
                    node->value < frame->function_ref->parameter_start ||
                    parameter_index >= frame->function_ref->parameter_count) {
                    return ECSVM_ERROR_ARGUMENT;
                }
                *out_value = frame->arguments[parameter_index];
                return ECSVM_OK;
            }
            if (node->value_kind == ECSVM_ECSBIN_AST_VALUE_BLOB_ID &&
                ecsvm_managed_frame_get_local(frame, node->value, out_value)) {
                return ECSVM_OK;
            }
            return ECSVM_ERROR_NOT_FOUND;
        case ECSVM_ECSBIN_AST_NODE_LITERAL_EXPRESSION:
            if (node->value_kind != ECSVM_ECSBIN_AST_VALUE_BLOB_ID) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "true")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = 1;
                return ECSVM_OK;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "false")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = 0;
                return ECSVM_OK;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "null")) {
                *out_value = ecsvm_managed_null_value();
                return ECSVM_OK;
            }
            blob = ecsvm_managed_blob(frame->runtime->module, node->value);
            if (blob == NULL) {
                return ECSVM_ERROR_NOT_FOUND;
            }
            if (blob->length >= 2u &&
                blob->data[0] == '"' &&
                blob->data[blob->length - 1u] == '"') {
                out_value->kind = ECSVM_MANAGED_VALUE_STRING;
                out_value->blob_id = node->value;
                return ECSVM_OK;
            }
            out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
            if (!ecsvm_managed_blob_to_double(frame->runtime->module, node->value, &out_value->number_value)) {
                return ECSVM_ERROR_ARGUMENT;
            }
            return ECSVM_OK;
        case ECSVM_ECSBIN_AST_NODE_GROUPING_EXPRESSION:
            return ecsvm_managed_eval_expression(frame, node->first_child, out_value);
        case ECSVM_ECSBIN_AST_NODE_UNARY_EXPRESSION: {
            ecsvm_managed_value_t operand;
            if (ecsvm_managed_eval_expression(frame, node->first_child, &operand) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "!")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = !ecsvm_managed_is_truthy(&operand);
                return ECSVM_OK;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "-")) {
                if (operand.kind != ECSVM_MANAGED_VALUE_NUMBER) {
                    return ECSVM_ERROR_ARGUMENT;
                }
                out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
                out_value->number_value = -operand.number_value;
                return ECSVM_OK;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "+")) {
                *out_value = operand;
                return ECSVM_OK;
            }
            return ECSVM_ERROR_ARGUMENT;
        }
        case ECSVM_ECSBIN_AST_NODE_BINARY_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *right_node;
            ecsvm_managed_value_t left;
            ecsvm_managed_value_t right;

            right_node = &frame->ast.nodes[frame->ast.nodes[node->first_child].next_sibling];
            if (ecsvm_managed_eval_expression(frame, node->first_child, &left) != ECSVM_OK ||
                ecsvm_managed_eval_expression(frame, frame->ast.nodes[node->first_child].next_sibling, &right) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }

            (void)right_node;
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "+") ||
                ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "-") ||
                ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "*") ||
                ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "/")) {
                if (left.kind != ECSVM_MANAGED_VALUE_NUMBER ||
                    right.kind != ECSVM_MANAGED_VALUE_NUMBER) {
                    return ECSVM_ERROR_ARGUMENT;
                }
                out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
                if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "+")) {
                    out_value->number_value = left.number_value + right.number_value;
                } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "-")) {
                    out_value->number_value = left.number_value - right.number_value;
                } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "*")) {
                    out_value->number_value = left.number_value * right.number_value;
                } else {
                    if (right.number_value == 0.0) {
                        return ECSVM_ERROR_ARGUMENT;
                    }
                    out_value->number_value = left.number_value / right.number_value;
                }
                return ECSVM_OK;
            }

            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "==") ||
                ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "!=")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = ecsvm_managed_values_equal(frame->runtime->module, &left, &right);
                if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "!=")) {
                    out_value->boolean_value = !out_value->boolean_value;
                }
                return ECSVM_OK;
            }

            if (left.kind != ECSVM_MANAGED_VALUE_NUMBER ||
                right.kind != ECSVM_MANAGED_VALUE_NUMBER) {
                return ECSVM_ERROR_ARGUMENT;
            }

            out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "<")) {
                out_value->boolean_value = left.number_value < right.number_value;
            } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, ">")) {
                out_value->boolean_value = left.number_value > right.number_value;
            } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "<=")) {
                out_value->boolean_value = left.number_value <= right.number_value;
            } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, ">=")) {
                out_value->boolean_value = left.number_value >= right.number_value;
            } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "&&")) {
                out_value->boolean_value = ecsvm_managed_is_truthy(&left) && ecsvm_managed_is_truthy(&right);
            } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "||")) {
                out_value->boolean_value = ecsvm_managed_is_truthy(&left) || ecsvm_managed_is_truthy(&right);
            } else {
                return ECSVM_ERROR_ARGUMENT;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_ASSIGNMENT_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *left_node;
            ecsvm_managed_value_t value;

            left_node = &frame->ast.nodes[node->first_child];
            if (left_node->kind != ECSVM_ECSBIN_AST_NODE_IDENTIFIER ||
                left_node->value_kind != ECSVM_ECSBIN_AST_VALUE_BLOB_ID ||
                ecsvm_managed_eval_expression(frame, left_node->next_sibling, &value) != ECSVM_OK ||
                !ecsvm_managed_frame_set_local(frame, left_node->value, value)) {
                return ECSVM_ERROR_ARGUMENT;
            }
            *out_value = value;
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_CALL_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *callee;
            const ecsvm_ecsbin_ast_node_t *argument_list;
            ecsvm_managed_value_t arguments[ECSVM_MANAGED_CALL_ARGUMENT_LIMIT];
            size_t argument_count;
            uint32_t child_index;
            uint32_t function_id;

            callee = &frame->ast.nodes[node->first_child];
            function_id = node->value_kind == ECSVM_ECSBIN_AST_VALUE_FUNCTION_REF_ID
                ? node->value
                : 0u;
            if (function_id == 0u) {
                return ECSVM_ERROR_ARGUMENT;
            }

            argument_list = callee->next_sibling == 0u ? NULL : &frame->ast.nodes[callee->next_sibling];
            if (argument_list == NULL ||
                argument_list->kind != ECSVM_ECSBIN_AST_NODE_ARGUMENT_LIST) {
                return ECSVM_ERROR_ARGUMENT;
            }

            argument_count = 0u;
            child_index = argument_list->first_child;
            while (child_index != 0u) {
                if (argument_count >= sizeof(arguments) / sizeof(arguments[0]) ||
                    ecsvm_managed_eval_expression(frame, child_index, &arguments[argument_count]) != ECSVM_OK) {
                    return ECSVM_ERROR_ARGUMENT;
                }
                argument_count += 1u;
                child_index = frame->ast.nodes[child_index].next_sibling;
            }

            return ecsvm_managed_invoke_function(
                frame->runtime,
                function_id,
                arguments,
                argument_count,
                out_value
            );
        }
        default:
            return ECSVM_ERROR_ARGUMENT;
    }
}

static ecsvm_status_t ecsvm_managed_execute_statement(
    ecsvm_managed_frame_t *frame,
    uint32_t node_index
)
{
    const ecsvm_ecsbin_ast_node_t *node;

    node = &frame->ast.nodes[node_index];
    switch (node->kind) {
        case ECSVM_ECSBIN_AST_NODE_BLOCK: {
            uint32_t child_index;
            child_index = node->first_child;
            while (child_index != 0u && !frame->has_return) {
                ecsvm_status_t status;
                status = ecsvm_managed_execute_statement(frame, child_index);
                if (status != ECSVM_OK) {
                    return status;
                }
                child_index = frame->ast.nodes[child_index].next_sibling;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_DECLARATION: {
            const ecsvm_ecsbin_ast_node_t *name_node;
            const ecsvm_ecsbin_ast_node_t *value_node;
            ecsvm_managed_value_t value;

            name_node = &frame->ast.nodes[node->first_child];
            value = ecsvm_managed_null_value();
            value_node = &frame->ast.nodes[name_node->next_sibling];
            if (value_node->kind == ECSVM_ECSBIN_AST_NODE_TYPE_EXPRESSION) {
                value_node = value_node->next_sibling == 0u ? NULL : &frame->ast.nodes[value_node->next_sibling];
            }
            if (value_node != NULL) {
                ecsvm_status_t status;
                status = ecsvm_managed_eval_expression(
                    frame,
                    value_node - frame->ast.nodes,
                    &value
                );
                if (status != ECSVM_OK) {
                    return status;
                }
            }
            return ecsvm_managed_frame_set_local(frame, name_node->value, value)
                ? ECSVM_OK
                : ECSVM_ERROR_MEMORY;
        }
        case ECSVM_ECSBIN_AST_NODE_RETURN_STATEMENT:
            frame->return_value = node->first_child == 0u
                ? ecsvm_managed_void_value()
                : ecsvm_managed_null_value();
            if (node->first_child != 0u) {
                ecsvm_status_t status;
                status = ecsvm_managed_eval_expression(frame, node->first_child, &frame->return_value);
                if (status != ECSVM_OK) {
                    return status;
                }
            }
            frame->has_return = 1;
            return ECSVM_OK;
        case ECSVM_ECSBIN_AST_NODE_EXPRESSION_STATEMENT: {
            ecsvm_managed_value_t value;
            return ecsvm_managed_eval_expression(frame, node->first_child, &value);
        }
        case ECSVM_ECSBIN_AST_NODE_IF_STATEMENT: {
            const ecsvm_ecsbin_ast_node_t *condition_node;
            const ecsvm_ecsbin_ast_node_t *then_node;
            const ecsvm_ecsbin_ast_node_t *else_node;
            ecsvm_managed_value_t condition;

            condition_node = &frame->ast.nodes[node->first_child];
            then_node = &frame->ast.nodes[condition_node->next_sibling];
            else_node = then_node->next_sibling == 0u ? NULL : &frame->ast.nodes[then_node->next_sibling];
            if (ecsvm_managed_eval_expression(frame, node->first_child, &condition) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (ecsvm_managed_is_truthy(&condition)) {
                return ecsvm_managed_execute_statement(frame, condition_node->next_sibling);
            }
            if (else_node != NULL && else_node->kind == ECSVM_ECSBIN_AST_NODE_ELSE_CLAUSE &&
                else_node->first_child != 0u) {
                return ecsvm_managed_execute_statement(frame, else_node->first_child);
            }
            return ECSVM_OK;
        }
        default:
            return ECSVM_ERROR_ARGUMENT;
    }
}

static ecsvm_status_t ecsvm_managed_invoke_function(
    ecsvm_managed_runtime_t *runtime,
    uint32_t function_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    const ecsvm_function_entry_t *function_entry;
    const ecsvm_ecsbin_function_ref_t *function_ref;
    ecsvm_managed_frame_t frame;
    ecsvm_status_t status;

    function_entry = ecsvm_engine_function(runtime->engine, function_id);
    if (function_entry == NULL || function_entry->function_ref == NULL) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    function_ref = function_entry->function_ref;
    if (function_ref->parameter_count != argument_count) {
        return ECSVM_ERROR_ARGUMENT;
    }
    if (function_entry->native_callback != NULL) {
        return function_entry->native_callback(
            runtime->engine,
            runtime->module,
            function_ref,
            arguments,
            argument_count,
            out_value
        );
    }
    if (!function_entry->has_managed_body) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    memset(&frame, 0, sizeof(frame));
    frame.runtime = runtime;
    frame.function_ref = function_ref;
    frame.arguments = (ecsvm_managed_value_t *)arguments;
    frame.ast = function_entry->managed_body;
    frame.return_value = ecsvm_managed_void_value();

    status = ecsvm_managed_execute_statement(&frame, frame.ast.nodes[0].first_child);
    if (status == ECSVM_OK) {
        *out_value = frame.has_return ? frame.return_value : ecsvm_managed_void_value();
    }

    free(frame.locals);
    return status;
}

static ecsvm_status_t ecsvm_managed_system_callback(ecsvm_context_t *ctx)
{
    ecsvm_managed_system_binding_t *binding;
    ecsvm_managed_value_t result;

    binding = (ecsvm_managed_system_binding_t *)ctx->api.userdata;
    if (binding == NULL || binding->runtime == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    return ecsvm_managed_invoke_function(
        binding->runtime,
        binding->function_id,
        NULL,
        0u,
        &result
    );
}

static int ecsvm_run_loaded_ecs_module(const ecsvm_ecsbin_module_t *module)
{
    ecsvm_engine_t *engine;
    ecsvm_managed_runtime_t runtime;
    ecsvm_managed_system_binding_t *bindings;
    size_t system_count;
    size_t function_index;
    ecsvm_status_t status;
    char error_message[256];
    int exit_code;

    memset(&runtime, 0, sizeof(runtime));
    bindings = NULL;
    system_count = 0u;
    exit_code = 1;

    for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;
        function_ref = &module->function_refs[function_index];
        if (function_ref->body_blob_id != 0u &&
            ecsvm_ecsbin_function_has_attribute(module, function_ref, "core.System")) {
            system_count += 1u;
        }
    }

    if (system_count == 0u) {
        fprintf(stderr, "no managed systems found in module\n");
        return 1;
    }

    engine = ecsvm_engine_create();
    if (engine == NULL) {
        fprintf(stderr, "failed to create engine\n");
        return 1;
    }

    runtime.engine = engine;
    runtime.module = module;
    error_message[0] = '\0';
    status = ecsvm_engine_register_builtin_components(engine);
    if (status == ECSVM_OK) {
        status = ecsvm_ecsbin_register_components(engine, module);
    }
    if (status == ECSVM_OK) {
        status = ecsvm_engine_load_functions(engine, module, error_message, sizeof(error_message));
    }
    if (status != ECSVM_OK) {
        if (error_message[0] != '\0') {
            fprintf(stderr, "failed to register managed module state: %s\n", error_message);
        } else {
            fprintf(stderr, "failed to register managed module state: %s\n", ecsvm_status_string(status));
        }
        ecsvm_engine_destroy(engine);
        return 1;
    }

    bindings = (ecsvm_managed_system_binding_t *)calloc(system_count, sizeof(*bindings));
    if (bindings == NULL) {
        fprintf(stderr, "out of memory while preparing managed systems\n");
        ecsvm_engine_destroy(engine);
        return 1;
    }

    system_count = 0u;
    for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;
        ecsvm_system_desc_t desc;

        function_ref = &module->function_refs[function_index];
        if (function_ref->body_blob_id == 0u ||
            !ecsvm_ecsbin_function_has_attribute(module, function_ref, "core.System")) {
            continue;
        }
        if (function_ref->parameter_count != 0u) {
            fprintf(stderr, "managed runtime currently supports only zero-parameter systems (%s)\n", function_ref->qualified_name);
            goto cleanup;
        }

        memset(&desc, 0, sizeof(desc));
        bindings[system_count].runtime = &runtime;
        bindings[system_count].function_id = (uint32_t)function_index + 1u;
        desc.name = function_ref->qualified_name;
        desc.callback = ecsvm_managed_system_callback;
        desc.user_data = &bindings[system_count];
        status = ecsvm_engine_register_system(engine, &desc, NULL);
        if (status != ECSVM_OK) {
            fprintf(stderr, "failed to register managed system %s: %s\n", function_ref->qualified_name, ecsvm_status_string(status));
            goto cleanup;
        }
        system_count += 1u;
    }

    status = ecsvm_engine_run(engine);
    if (status != ECSVM_OK) {
        fprintf(stderr, "managed runtime failed: %s\n", ecsvm_status_string(status));
        goto cleanup;
    }

    exit_code = 0;

cleanup:
    free(bindings);
    ecsvm_engine_destroy(engine);
    return exit_code;
}

int ecsvm_run_ecsbin(const char *ecsbin_path)
{
    ecsvm_ecsbin_module_t module;
    char error_message[512];
    ecsvm_status_t status;
    int exit_code;

    memset(&module, 0, sizeof(module));
    status = ecsvm_ecsbin_load(
        ecsbin_path,
        &module,
        error_message,
        sizeof(error_message)
    );
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to load ecsbin: %s\n", error_message[0] != '\0' ? error_message : ecsvm_status_string(status));
        return 1;
    }

    exit_code = ecsvm_run_loaded_ecs_module(&module);
    ecsvm_ecsbin_unload(&module);
    return exit_code;
}

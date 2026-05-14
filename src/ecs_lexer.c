#include "project_internal.h"

static int ecsvm_is_identifier_start(int ch)
{
    return (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') ||
        ch == '_';
}

static int ecsvm_is_identifier_continue(int ch)
{
    return ecsvm_is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

static ecsvm_token_kind_t ecsvm_keyword_kind(const char *text, size_t length)
{
    if (length == 6u && memcmp(text, "import", 6u) == 0) {
        return ECSVM_TOKEN_KEY_IMPORT;
    }
    if (length == 9u && memcmp(text, "namespace", 9u) == 0) {
        return ECSVM_TOKEN_KEY_NAMESPACE;
    }
    if (length == 6u && memcmp(text, "struct", 6u) == 0) {
        return ECSVM_TOKEN_KEY_STRUCT;
    }
    if (length == 9u && memcmp(text, "component", 9u) == 0) {
        return ECSVM_TOKEN_KEY_COMPONENT;
    }
    if (length == 9u && memcmp(text, "attribute", 9u) == 0) {
        return ECSVM_TOKEN_KEY_ATTRIBUTE;
    }
    if (length == 6u && memcmp(text, "system", 6u) == 0) {
        return ECSVM_TOKEN_KEY_SYSTEM;
    }
    if (length == 5u && memcmp(text, "const", 5u) == 0) {
        return ECSVM_TOKEN_KEY_CONST;
    }
    if (length == 2u && memcmp(text, "fn", 2u) == 0) {
        return ECSVM_TOKEN_KEY_FN;
    }
    if (length == 2u && memcmp(text, "if", 2u) == 0) {
        return ECSVM_TOKEN_KEY_IF;
    }
    if (length == 4u && memcmp(text, "else", 4u) == 0) {
        return ECSVM_TOKEN_KEY_ELSE;
    }
    if (length == 3u && memcmp(text, "let", 3u) == 0) {
        return ECSVM_TOKEN_KEY_LET;
    }
    if (length == 6u && memcmp(text, "return", 6u) == 0) {
        return ECSVM_TOKEN_KEY_RETURN;
    }
    if (length == 4u && memcmp(text, "true", 4u) == 0) {
        return ECSVM_TOKEN_KEY_TRUE;
    }
    if (length == 5u && memcmp(text, "false", 5u) == 0) {
        return ECSVM_TOKEN_KEY_FALSE;
    }
    if (length == 4u && memcmp(text, "null", 4u) == 0) {
        return ECSVM_TOKEN_KEY_NULL;
    }

    return ECSVM_TOKEN_IDENTIFIER;
}

int ecsvm_lex_source(
    ecsvm_source_file_t *file,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    size_t offset;
    size_t line;
    size_t column;

    offset = 0u;
    line = 1u;
    column = 1u;
    if (diagnostic != NULL) {
        ecsvm_diagnostic_clear(diagnostic);
    }
    while (offset < file->length) {
        ecsvm_token_t token;
        int ch;

        ch = (unsigned char)file->source[offset];
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            offset += 1u;
            column += 1u;
            continue;
        }
        if (ch == '\n') {
            offset += 1u;
            line += 1u;
            column = 1u;
    if (diagnostic != NULL) {
        ecsvm_diagnostic_clear(diagnostic);
    }
            continue;
        }
        if (ch == '/' && offset + 1u < file->length && file->source[offset + 1u] == '/') {
            offset += 2u;
            column += 2u;
            while (offset < file->length && file->source[offset] != '\n') {
                offset += 1u;
                column += 1u;
            }
            continue;
        }
        if (ch == '/' && offset + 1u < file->length && file->source[offset + 1u] == '*') {
            offset += 2u;
            column += 2u;
            while (offset + 1u < file->length &&
                   !(file->source[offset] == '*' && file->source[offset + 1u] == '/')) {
                if (file->source[offset] == '\n') {
                    line += 1u;
                    column = 1u;
    if (diagnostic != NULL) {
        ecsvm_diagnostic_clear(diagnostic);
    }
                } else {
                    column += 1u;
                }
                offset += 1u;
            }
            if (offset + 1u >= file->length) {
                ecsvm_set_error(error_message, error_message_capacity, "unterminated block comment");
                ecsvm_diagnostic_set(diagnostic, file->path, line, column, ECSVM_DIAGNOSTIC_UNTERMINATED_COMMENT, "unterminated block comment");
                return 0;
            }
            offset += 2u;
            column += 2u;
            continue;
        }

        memset(&token, 0, sizeof(token));
        token.offset = offset;
        token.line = line;
        token.column = column;
        token.length = 1u;

        if (ecsvm_is_identifier_start(ch)) {
            size_t start;

            start = offset;
            while (offset < file->length && ecsvm_is_identifier_continue((unsigned char)file->source[offset])) {
                offset += 1u;
                column += 1u;
            }
            token.kind = ecsvm_keyword_kind(file->source + start, offset - start);
            token.offset = start;
            token.length = offset - start;
        } else if (ch >= '0' && ch <= '9') {
            size_t start;

            start = offset;
            while (offset < file->length) {
                int digit;

                digit = (unsigned char)file->source[offset];
                if ((digit < '0' || digit > '9') && digit != '.') {
                    break;
                }
                offset += 1u;
                column += 1u;
            }
            token.kind = ECSVM_TOKEN_NUMBER;
            token.offset = start;
            token.length = offset - start;
        } else if (ch == '"') {
            size_t start;

            start = offset;
            offset += 1u;
            column += 1u;
            while (offset < file->length && file->source[offset] != '"') {
                if (file->source[offset] == '\n') {
                    ecsvm_set_error(error_message, error_message_capacity, "unterminated string literal");
                    ecsvm_diagnostic_set(diagnostic, file->path, line, column, ECSVM_DIAGNOSTIC_UNTERMINATED_STRING, "unterminated string literal");
                    return 0;
                }
                if (file->source[offset] == '\\' && offset + 1u < file->length) {
                    offset += 2u;
                    column += 2u;
                } else {
                    offset += 1u;
                    column += 1u;
                }
            }
            if (offset >= file->length) {
                ecsvm_set_error(error_message, error_message_capacity, "unterminated string literal");
                ecsvm_diagnostic_set(diagnostic, file->path, line, column, ECSVM_DIAGNOSTIC_UNTERMINATED_STRING, "unterminated string literal");
                return 0;
            }
            offset += 1u;
            column += 1u;
            token.kind = ECSVM_TOKEN_STRING;
            token.offset = start;
            token.length = offset - start;
        } else {
            switch (ch) {
            case '{':
                token.kind = ECSVM_TOKEN_LBRACE;
                break;
            case '}':
                token.kind = ECSVM_TOKEN_RBRACE;
                break;
            case '[':
                token.kind = ECSVM_TOKEN_LBRACKET;
                break;
            case ']':
                token.kind = ECSVM_TOKEN_RBRACKET;
                break;
            case '(':
                token.kind = ECSVM_TOKEN_LPAREN;
                break;
            case ')':
                token.kind = ECSVM_TOKEN_RPAREN;
                break;
            case ':':
                token.kind = ECSVM_TOKEN_COLON;
                break;
            case ';':
                token.kind = ECSVM_TOKEN_SEMICOLON;
                break;
            case '.':
                token.kind = ECSVM_TOKEN_DOT;
                break;
            case ',':
                token.kind = ECSVM_TOKEN_COMMA;
                break;
            case '=':
                token.kind = ECSVM_TOKEN_EQUAL;
                break;
            case '!':
                token.kind = ECSVM_TOKEN_BANG;
                break;
            case '+':
                token.kind = ECSVM_TOKEN_PLUS;
                break;
            case '-':
                token.kind = ECSVM_TOKEN_MINUS;
                break;
            case '*':
                token.kind = ECSVM_TOKEN_STAR;
                break;
            case '/':
                token.kind = ECSVM_TOKEN_SLASH;
                break;
            case '%':
                token.kind = ECSVM_TOKEN_PERCENT;
                break;
            case '<':
                token.kind = ECSVM_TOKEN_LT;
                break;
            case '>':
                token.kind = ECSVM_TOKEN_GT;
                break;
            case '&':
                token.kind = ECSVM_TOKEN_AMPERSAND;
                break;
            case '|':
                token.kind = ECSVM_TOKEN_PIPE;
                break;
            case '^':
                token.kind = ECSVM_TOKEN_CARET;
                break;
            case '~':
                token.kind = ECSVM_TOKEN_TILDE;
                break;
            default:
                ecsvm_set_error(error_message, error_message_capacity, "unexpected character in source file");
                ecsvm_diagnostic_set(diagnostic, file->path, line, column, ECSVM_DIAGNOSTIC_UNEXPECTED_CHARACTER, "unexpected character in source file");
                return 0;
            }

            offset += 1u;
            column += 1u;
        }

        if (!ecsvm_token_array_push(&file->tokens, token)) {
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while lexing");
            ecsvm_diagnostic_set(diagnostic, file->path, token.line, token.column, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, "out of memory while lexing");
            return 0;
        }
    }

    if (!ecsvm_token_array_push(
            &file->tokens,
            (ecsvm_token_t){ ECSVM_TOKEN_EOF, file->length, 0u, line, column }
        )) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while lexing");
        ecsvm_diagnostic_set(diagnostic, file->path, line, column, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, "out of memory while lexing");
        return 0;
    }

    return 1;
}

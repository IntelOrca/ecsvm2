#ifndef ECSVM_ECS_SYNTAX_DEFS_H
#define ECSVM_ECS_SYNTAX_DEFS_H

#include <stdint.h>

#define ECSVM_TOKEN_KIND_ITEMS(X) \
    X(EOF, = 0, NULL) \
    X(IDENTIFIER, , NULL) \
    X(NUMBER, , NULL) \
    X(STRING, , NULL) \
    X(LBRACE, , "{") \
    X(RBRACE, , "}") \
    X(LBRACKET, , "[") \
    X(RBRACKET, , "]") \
    X(LPAREN, , "(") \
    X(RPAREN, , ")") \
    X(COLON, , ":") \
    X(SEMICOLON, , ";") \
    X(DOT, , ".") \
    X(COMMA, , ",") \
    X(EQUAL, , "=") \
    X(BANG, , "!") \
    X(PLUS, , "+") \
    X(MINUS, , "-") \
    X(STAR, , "*") \
    X(SLASH, , "/") \
    X(PERCENT, , "%") \
    X(LT, , "<") \
    X(GT, , ">") \
    X(AMPERSAND, , "&") \
    X(PIPE, , "|") \
    X(CARET, , "^") \
    X(TILDE, , "~") \
    X(KEY_IMPORT, , "import") \
    X(KEY_NAMESPACE, , "namespace") \
    X(KEY_STRUCT, , "struct") \
    X(KEY_COMPONENT, , "component") \
    X(KEY_ATTRIBUTE, , "attribute") \
    X(KEY_SYSTEM, , "system") \
    X(KEY_CONST, , "const") \
    X(KEY_FN, , "fn") \
    X(KEY_IF, , "if") \
    X(KEY_FOR, , "for") \
    X(KEY_IN, , "in") \
    X(KEY_ELSE, , "else") \
    X(KEY_LET, , "let") \
    X(KEY_RETURN, , "return") \
    X(KEY_TRUE, , "true") \
    X(KEY_FALSE, , "false") \
    X(KEY_NULL, , "null")

#define ECSVM_AST_NODE_KIND_ITEMS(X) \
    X(ROOT, = 1) \
    X(BLOCK, ) \
    X(GROUP_PAREN, = 3) \
    X(GROUP_BRACKET, = 4) \
    X(TOKEN, = 5) \
    X(DECLARATION, ) \
    X(RETURN_STATEMENT, ) \
    X(IF_STATEMENT, ) \
    X(FOR_IN_STATEMENT, ) \
    X(ELSE_CLAUSE, ) \
    X(EXPRESSION_STATEMENT, ) \
    X(ASSIGNMENT_EXPRESSION, ) \
    X(BINARY_EXPRESSION, ) \
    X(UNARY_EXPRESSION, ) \
    X(CALL_EXPRESSION, ) \
    X(ARGUMENT_LIST, ) \
    X(MEMBER_EXPRESSION, ) \
    X(INDEX_EXPRESSION, ) \
    X(GROUPING_EXPRESSION, ) \
    X(LITERAL_EXPRESSION, ) \
    X(IDENTIFIER, ) \
    X(TYPE_EXPRESSION, ) \
    X(OBJECT_LITERAL, ) \
    X(OBJECT_FIELD, )

#define ECSVM_AST_VALUE_KIND_ITEMS(X) \
    X(NONE, = 0) \
    X(BLOB_ID, ) \
    X(TYPE_REF_ID, ) \
    X(FIELD_REF_ID, ) \
    X(FUNCTION_REF_ID, ) \
    X(PARAMETER_ID, )

typedef struct ecsvm_shared_ast_node {
    uint32_t kind;
    uint32_t first_child;
    uint32_t last_child;
    uint32_t next_sibling;
    uint32_t token_kind;
    uint32_t value_kind;
    uint32_t value;
} ecsvm_shared_ast_node_t;

#endif

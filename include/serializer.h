#ifndef SERIALIZER_H
#define SERIALIZER_H

#include "ast.h"

// JSON string builder structure
typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
} StringBuilder;

// StringBuilder functions
StringBuilder* string_builder_create(size_t initial_capacity);
void string_builder_destroy(StringBuilder* sb);
void string_builder_append(StringBuilder* sb, const char* str);
void string_builder_append_char(StringBuilder* sb, char c);
void string_builder_append_int(StringBuilder* sb, int value);
void string_builder_append_double(StringBuilder* sb, double value);
void string_builder_append_bool(StringBuilder* sb, bool value);
char* string_builder_to_string(StringBuilder* sb);

// JSON serialization functions
char* serialize_ast_to_json(const ASTNode* node);
void serialize_node_list_to_json(StringBuilder* sb, const NodeList* list);
void serialize_string_list_to_json(StringBuilder* sb, const StringList* list);

// Node-specific serialization functions
void serialize_program_node(StringBuilder* sb, const ProgramNode* node);
void serialize_function_definition_node(StringBuilder* sb, const FunctionDefinitionNode* node);
void serialize_assignment_node(StringBuilder* sb, const AssignmentNode* node);
void serialize_print_statement_node(StringBuilder* sb, const PrintStatementNode* node);
void serialize_wait_statement_node(StringBuilder* sb, const WaitStatementNode* node);
void serialize_move_mouse_node(StringBuilder* sb, const MoveMouseNode* node);
void serialize_key_operation_node(StringBuilder* sb, const KeyOperationNode* node);
void serialize_button_operation_node(StringBuilder* sb, const ButtonOperationNode* node);
void serialize_binary_operation_node(StringBuilder* sb, const BinaryOperationNode* node);
void serialize_identifier_node(StringBuilder* sb, const IdentifierNode* node);
void serialize_integer_node(StringBuilder* sb, const IntegerNode* node);
void serialize_float_node(StringBuilder* sb, const FloatNode* node);
void serialize_time_node(StringBuilder* sb, const TimeNode* node);
void serialize_string_node(StringBuilder* sb, const StringNode* node);
void serialize_function_call_node(StringBuilder* sb, const FunctionCallNode* node);
void serialize_boolean_node(StringBuilder* sb, const BooleanNode* node);
void serialize_while_loop_node(StringBuilder* sb, const WhileLoopNode* node);
void serialize_repeat_loop_node(StringBuilder* sb, const RepeatLoopNode* node);
void serialize_control_statement_node(StringBuilder* sb, const ControlStatementNode* node);
void serialize_increment_decrement_node(StringBuilder* sb, const IncrementDecrementNode* node);
void serialize_if_statement_node(StringBuilder* sb, const IfStatementNode* node);

// JSON escaping utilities
char* escape_json_string(const char* str);
void unescape_json_string(char* str);

#endif // SERIALIZER_H

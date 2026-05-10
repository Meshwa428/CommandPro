#pragma once
#include <cstdint>

namespace Synapse {

enum OpCode : uint8_t {
    OP_CONSTANT,      // [const_index_high, const_index_low]
    OP_CONSTANT_8,    // [const_index]
    OP_CONSTANT_16,   // alias for OP_CONSTANT
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    
    OP_POP,           // Pop top of stack
    OP_DUP,           // Duplicate top of stack
    
    OP_GET_LOCAL,     // [slot_index]
    OP_SET_LOCAL,     // [slot_index]
    OP_MOVE_LOCAL,    // [slot_index] - Pushes and nulls out slot
    OP_GET_GLOBAL,    // [name_const_index]
    OP_SET_GLOBAL,    // [name_const_index]
    OP_DEFINE_GLOBAL, // [name_const_index]
    OP_MOVE_GLOBAL,   // [name_const_index] - Pushes and nulls out global
    OP_INC_GLOBAL,    // [name_const_index]

    OP_EQUAL,
    OP_STRICT_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_INT_DIVIDE,
    OP_EXPONENT,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_NEGATE,

    OP_PRINT,
    OP_PRINTLN,
    
    OP_JUMP,          // [offset_16bit]
    OP_JUMP_IF_FALSE, // [offset_16bit]
    OP_LOOP,          // [offset_16bit] (jump back)
    
    OP_CALL,          // [arg_count]
    OP_RETURN,
    
    OP_TUPLE,         // [element_count]
    OP_LIST,          // [element_count]
    OP_MAP,           // [item_count * 2]
    OP_INDEX_GET,
    OP_INDEX_SET,
    
    // Automation opcodes
    OP_MOUSE_MOVE,
    OP_MOUSE_CLICK,
    OP_KEY_PRESS,
    OP_KEY_TYPE,
    
    OP_WAIT,
    OP_ASK,            // [prompt_const, var_const, type_const]

    // Specialized Opcodes
    OP_GET_LOCAL_0,
    OP_GET_LOCAL_1,
    OP_GET_LOCAL_2,
    OP_GET_LOCAL_3,
    OP_GET_LOCAL_4,
    OP_GET_LOCAL_5,
    OP_GET_LOCAL_6,
    OP_GET_LOCAL_7,
    OP_GET_LOCAL_8,

    OP_SET_LOCAL_0,
    OP_SET_LOCAL_1,
    OP_SET_LOCAL_2,
    OP_SET_LOCAL_3,
    OP_SET_LOCAL_4,
    OP_SET_LOCAL_5,
    OP_SET_LOCAL_6,
    OP_SET_LOCAL_7,
    OP_SET_LOCAL_8,

    OP_INC_LOCAL,
    OP_INC_LOCAL_0,
    OP_INC_LOCAL_1,
    OP_INC_LOCAL_2,
    OP_INC_LOCAL_3,
    OP_INC_LOCAL_4,
    OP_INC_LOCAL_5,

    // Specialized String Opcodes
    OP_STRING_ADD,
    OP_STRING_EQUAL
};

} // namespace Synapse

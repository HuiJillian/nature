#ifndef NATURE_SRC_ASSEMBLER_AMD64_ASM_H_
#define NATURE_SRC_ASSEMBLER_AMD64_ASM_H_

#include "src/value.h"
#include "src/lib/list.h"
//#include "opcode.h"

// 指令字符宽度
#define BYTE 1 // 1 byte = 8 位
#define WORD 2 // 2 byte = 16 位
#define DWORD 4 // 4 byte = 32 位
#define QWORD 8 // 8 byte = 64位
#define OWORD 16
#define YWORD 32 // 32 byte
#define ZWORD 64 // 64 byte

#define REG(_reg) ({ \
     asm_operand_t *reg_operand = NEW(asm_operand_t); \
     reg_operand->type = ASM_OPERAND_TYPE_REGISTER;  \
     reg_operand->size = _reg->size;\
     reg_operand->value = _reg;    \
     reg_operand;\
})

#define REG_OPERAND(_name, _index, _size) ({ \
     asm_operand_register_t *reg = NEW(asm_operand_register_t); \
     reg->name = _name;\
     reg->index = _index;\
     reg->size = _size;\
     reg;\
})

#define SYMBOL(_name, _is_label, _is_local) ({ \
     asm_operand_t *operand = NEW(asm_operand_t); \
     operand->type = ASM_OPERAND_TYPE_SYMBOL;  \
     asm_operand_symbol_t *_symbol = NEW(asm_operand_symbol_t); \
     _symbol->name = _name;    \
     _symbol->is_label = _is_label;    \
     _symbol->is_local = _is_local;    \
     operand->size = 0;\
     operand->value = _symbol;    \
     operand;\
})

#define DISP_REG(_reg, _disp) ({ \
     asm_operand_t *operand = NEW(asm_operand_t); \
     operand->type = ASM_OPERAND_TYPE_DISP_REGISTER;  \
     asm_operand_disp_register_t *_disp_reg = NEW(asm_operand_disp_register_t); \
     _disp_reg->reg = _reg;    \
     _disp_reg->disp = _disp;    \
     operand->size = _reg->size;\
     operand->value = _disp_reg;    \
     operand;\
})

#define INDIRECT_REG(_reg) ({ \
     asm_operand_t *operand = NEW(asm_operand_t); \
     operand->type = ASM_OPERAND_TYPE_INDIRECT_REGISTER;  \
     asm_operand_indirect_register_t *_indirect = NEW(asm_operand_indirect_register_t); \
     _indirect->reg = _reg;    \
     operand->size = _reg->size;\
     operand->value = _indirect;    \
     operand;\
})

#define SIB_REG(_base, _index, _scale) ({ \
     asm_operand_t *operand = NEW(asm_operand_t); \
     operand->type = ASM_OPERAND_TYPE_SIB_REGISTER;  \
     asm_operand_sib_register_t *_sib = NEW(asm_operand_sib_register_t); \
     _sib->base = _base;\
     _sib->index = _index;\
     _sib->scale = _scale;\
     operand->size = _base->size;\
     operand->value = _sib;    \
     operand;\
})

#define RIP_RELATIVE(_disp) ({ \
     asm_operand_t *operand = NEW(asm_operand_t); \
     operand->type = ASM_OPERAND_TYPE_RIP_RELATIVE;  \
     asm_operand_rip_relative_t *rip = NEW(asm_operand_rip_relative_t); \
     rip->disp = _disp;\
     operand->size = QWORD;\
     operand->value = rip;    \
     operand;\
})

#define UINT8(_value) VALUE_OPERAND(asm_operand_uint8_t, ASM_OPERAND_TYPE_UINT8, (_value), BYTE);
#define UINT16(_value) VALUE_OPERAND(asm_operand_uint16_t, ASM_OPERAND_TYPE_UINT16, (_value), WORD);
#define UINT32(_value) VALUE_OPERAND(asm_operand_uint32_t, ASM_OPERAND_TYPE_UINT32, (_value), DWORD);
#define UINT64(_value) VALUE_OPERAND(asm_operand_uint64_t, ASM_OPERAND_TYPE_UINT64, (_value), DWORD);
#define INT8(_value) VALUE_OPERAND(asm_operand_int8_t, ASM_OPERAND_TYPE_INT8, (_value), BYTE);
#define INT32(_value) VALUE_OPERAND(asm_operand_int32_t, ASM_OPERAND_TYPE_INT32, (_value), DWORD);
#define FLOAT32(_value) VALUE_OPERAND(asm_operand_float32_t, ASM_OPERAND_TYPE_FLOAt32, (_value), DWORD);
#define FLOAT64(_value) VALUE_OPERAND(asm_operand_float64_t, ASM_OPERAND_TYPE_FLOAt64, (_value), QWORD);

#define VALUE_OPERAND(_type, _operand_type, _value, _size) ({ \
    asm_operand_t *number_operand = malloc(sizeof(asm_operand_t));\
    number_operand->type = (_operand_type);\
    _type *number = malloc(sizeof(_type));\
    number->value = (_value);\
    number_operand->size = (_size);\
    number_operand->value = number;\
    number_operand;\
})

typedef enum {
  ASM_OPERAND_TYPE_REGISTER = 1,
  ASM_OPERAND_TYPE_INDIRECT_REGISTER,
  ASM_OPERAND_TYPE_SIB_REGISTER,
  ASM_OPERAND_TYPE_RIP_RELATIVE,
  ASM_OPERAND_TYPE_DISP_REGISTER,
  ASM_OPERAND_TYPE_SYMBOL,
  ASM_OPERAND_TYPE_UINT8,
  ASM_OPERAND_TYPE_UINT16,
  ASM_OPERAND_TYPE_UINT32,
  ASM_OPERAND_TYPE_UINT64,
  ASM_OPERAND_TYPE_INT8,
  ASM_OPERAND_TYPE_INT32,
  ASM_OPERAND_TYPE_FLOAT32,
  ASM_OPERAND_TYPE_FLOAT64,
} asm_operand_type;

typedef struct {
  uint8_t value;
} asm_operand_uint8_t;

typedef struct {
  uint16_t value;
} asm_operand_uint16_t;

typedef struct {
  uint32_t value;
} asm_operand_uint32_t;

typedef struct {
  uint64_t value;
} asm_operand_uint64_t;

typedef struct {
  int32_t value;
} asm_operand_int32_t;

typedef struct {
  int8_t value;
} asm_operand_int8_t;

typedef struct {
  float value;
} asm_operand_float32_t;

typedef struct {
  double value;
} asm_operand_float64_t;

/**
 * 汇编指令参数
 */
typedef struct {
  string name;
  uint8_t index; // index 对应 intel 手册表中的索引，可以直接编译进 modrm 中
  uint8_t size;
} asm_operand_register_t; // size 是个啥？

typedef struct {
  asm_operand_register_t *base; // 决定了宽度
  asm_operand_register_t *index;
  uint8_t scale;
} asm_operand_sib_register_t;

typedef struct {
  asm_operand_register_t *reg;
} asm_operand_indirect_register_t; // (%rax)

typedef struct {
  asm_operand_register_t *reg;
  uint8_t disp;
} asm_operand_disp_register_t;

typedef struct {
  int32_t disp;
} asm_operand_rip_relative_t;

typedef struct {
  asm_operand_type type;
  uint8_t size;
  void *value; // asm_operand_register
} asm_operand_t;

typedef struct {
  string name; // 符号名称
  bool is_label; // label 类型符号还是数据类型符号
  bool is_local; // 是内部符号，还是全局符号(global_fn,global_var 或者当前文件不存在的 var)
} asm_operand_symbol_t;

/**
 * 汇编指令结构(即如何编写汇编指令)
 * 指令名称可以包含 label
 */
typedef struct {
  string name; // 指令名称
  uint8_t count;
  asm_operand_t *operands[4]; // 最多 4 个参数
} asm_inst_t;

// 数据段(编译进符号表即可，数据类型需要兼容高级类型)
//typedef struct {
//  char *name;
//  asm_var_decl_type type;
////  size_t size; // 单位 Byte
//  union {
//    int int_value;
//    float float_value;
//    char *string_value;
//  };
//} asm_var_decl;
//
//struct {
//  asm_var_decl list[UINT16_MAX];
//  uint16_t count;
//} asm_data;

asm_operand_t *asm_symbol_operand(asm_inst_t asm_inst);

#endif //NATURE_SRC_ASSEMBLER_AMD64_ASM_H_

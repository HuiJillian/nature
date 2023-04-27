#ifndef NATURE_SRC_LIR_H_
#define NATURE_SRC_LIR_H_

#include "utils/linked.h"
#include "utils/helper.h"
#include "utils/table.h"
#include "src/structs.h"
#include "src/module.h"
#include "src/symbol/symbol.h"
#include "src/register/register.h"

#define TEMP_IDENT "t"
#define TEMP_VAR_IDENT "v"
#define TEMP_LABEL "l"
#define ITERATOR_CURSOR "cursor"
#define CONTINUE_IDENT "continue"
#define FOR_COND_IDENT "for_cond"
#define FOR_TRADITION_IDENT "for_tradition"
#define FOR_TRADITION_END_IDENT "for_tradition_end"
#define FOR_COND_END_IDENT "for_cond_end"
#define FOR_ITERATOR_IDENT "for_iterator"
#define FOR_END_ITERATOR_IDENT "for_end_iterator"
#define END_IF_IDENT "if_end"
#define IF_ALTERNATE_IDENT "if_alternate"
#define IF_CONTINUE_IDENT "if_continue"
#define ERRORT_TYPE_IDENT "errort"
#define ERRORT_MSG_IDENT "msg"

#define FN_RUNTIME_IDENT "@fn_runtime"

// RT = runtime
// CT = compile time
#define RT_CALL_LIST_NEW "list_new"
#define RT_CALL_LIST_ACCESS "list_access"
#define RT_CALL_LIST_ASSIGN "list_assign"
#define RT_CALL_LIST_SPLICE "list_slice"
#define RT_CALL_LIST_LENGTH "list_length"
#define RT_CALL_LIST_PUSH "list_push"
#define RT_CALL_LIST_CONCAT "linked_concat"

#define LIST_PUSH_KEY "push"
#define LIST_LENGTH_KEY "length"

#define MAP_DELETE_KEY "delete"
#define MAP_LENGTH_KEY "length"

#define SET_CONTAINS_KEY "contains"
#define SET_ADD_KEY "add"
#define SET_DELETE_KEY "delete"


#define RT_CALL_MAP_NEW "map_new"
#define RT_CALL_MAP_ACCESS "map_access"
#define RT_CALL_MAP_ASSIGN "map_assign"
#define RT_CALL_MAP_LENGTH "map_length"
#define RT_CALL_MAP_DELETE "map_delete"

#define RT_CALL_SET_CALL_IDENT "set"
#define RT_CALL_SET_NEW "set_new"
#define RT_CALL_SET_ADD "set_add" // 往集合中添加元素
#define RT_CALL_SET_CONTAINS "set_contains" // s.contain()
#define RT_CALL_SET_DELETE "set_delete" // 将元素从 set 中移除

#define RT_CALL_STRUCT_NEW "struct_new"

// 参考 python, tuple 不允许单独赋值，必须初始化时进行赋值
#define RT_CALL_TUPLE_NEW "tuple_new"
#define RT_CALL_TUPLE_ASSIGN "tuple_assign"
#define RT_CALL_TUPLE_ACCESS "tuple_access"

#define RT_CALL_CONVERT_ANY "convert_any"
#define RT_CALL_CONVERT_BOOL "convert_bool"
#define RT_CALL_NUMBER_CASTING "number_casting"

#define RT_CALL_ITERATOR_NEXT_KEY "iterator_next_key"
#define RT_CALL_ITERATOR_VALUE "iterator_value"

#define RT_CALL_FN_NEW "fn_new"

#define RT_CALL_ENV_NEW "env_new"
#define RT_CALL_ENV_ASSIGN "env_assign"
#define RT_CALL_ENV_CLOSURE "env_closure"
#define RT_CALL_ENV_ASSIGN_REF "env_assign_ref"
#define RT_CALL_ENV_ACCESS_REF "env_access_ref"

#define RT_CALL_STRING_NEW "string_new"

#define RT_CALL_MEMORY_MOVE "memory_move"

#define RT_CALL_PROCESSOR_ATTACH_ERRORT "processor_attach_errort"
#define RT_CALL_PROCESSOR_REMOVE_ERRORT "processor_remove_errort"
#define RT_CALL_PROCESSOR_HAS_ERRORT "processor_has_errort"

#define OP(_node) ((lir_op_t*)_node->value)

#define OP_PUSH(_op) linked_push(m->compiler_current->operations, _op)


typedef enum {
    LIR_OPERAND_NULL = 0,
    LIR_OPERAND_VAR, // 虚拟寄存器? 那我凭什么给虚拟寄存器分配内存地址？又或者是 symbol?
    LIR_OPERAND_REG,
    LIR_OPERAND_SYMBOL_VAR, // 虚拟寄存器? 那我凭什么给虚拟寄存器分配内存地址？
    LIR_OPERAND_STACK,
    LIR_OPERAND_PHI_BODY,
    LIR_OPERAND_FORMAL_PARAMS,
    LIR_OPERAND_ACTUAL_PARAMS,
    LIR_OPERAND_SYMBOL_LABEL, // 指令里面都有 label 指令了，operand 其实只需要 symbol 就行了，没必要多余的 label 误导把？
    LIR_OPERAND_IMM,
    LIR_OPERAND_INDIRECT_ADDR,
} lir_operand_type_t;

typedef enum {
    LIR_OPCODE_ADD = 1,
    LIR_OPCODE_SUB,
    LIR_OPCODE_MUL,
    LIR_OPCODE_DIV,
    LIR_OPCODE_REM, // remainder
    LIR_OPCODE_SHR, // >>
    LIR_OPCODE_SHL, // <<
    LIR_OPCODE_AND, // &
    LIR_OPCODE_OR, // |
    LIR_OPCODE_XOR, // ^
    LIR_OPCODE_BNOT, // ~ 取反
    LIR_OPCODE_NOT, // ! 取反
    LIR_OPCODE_NEG, // -取负数

    LIR_OPCODE_CLR, // clean reg
    LIR_OPCODE_CLV, // clean up var, result is var，等同于首次变量注册的功能
    LIR_OPCODE_SLT, // set less than <
    LIR_OPCODE_SLE, // set less eq <=
    LIR_OPCODE_SGT, // >
    LIR_OPCODE_SGE, // >=
    LIR_OPCODE_SEE, // ==
    LIR_OPCODE_SNE, // !=

    LIR_OPCODE_LEA, // 取地址, lea _,_ => v_1 (v_1 必须是有效的内存地址)
//    LIR_OPCODE_LIA, // load indirect addr to reg(var) ，将内存中的数据加载到寄存器中, amd64: mov [rax] -> rdx
//    LIR_OPCODE_SIA, // store reg(var) to indirect addr，将寄存器中的数据存入内存

    LIR_OPCODE_PHI, // 复合指令, 位置在 first_param
    LIR_OPCODE_MOVE,
    LIR_OPCODE_BEQ, // branch if eq a,b
    LIR_OPCODE_BAL, // branch always
    LIR_OPCODE_PUSH, // first
    LIR_OPCODE_POP, // output
    LIR_OPCODE_CALL, // 复合指令，位置在 second
    LIR_OPCODE_RT_CALL,
    LIR_OPCODE_BUILTIN_CALL, // BUILTIN_CALL print params -> nil
    LIR_OPCODE_RETURN, // return != ret, 其主要是做了 mov res -> rax
    LIR_OPCODE_LABEL,
    LIR_OPCODE_FN_BEGIN, // output 为 formal_params 操作数
    LIR_OPCODE_FN_END, // 无操作数
} lir_opcode_t;

typedef struct lir_operand_t lir_operand_t;

/**
 * 存放在寄存器或者内存中, var a = 1
 */
typedef struct {
    string ident; // ssa 后的新名称
    string old; // ssa 之前的名称

    flag_t flag; // TODO 可以直接在 flag 中写入是否必须分配寄存器到信息
    type_t type;
} lir_var_t;

/**
 * mov DWORD 0x1,[rbp-8] 假设 rbp = 100, 则表示将 0x1 存储在 92 ~ 96 之间
 * 也就是 stack 是向下增长，但是存储数据是从低地址往搞地址存储
 */
typedef struct {
    int slot;
    int size;
} lir_stack_t;

/**
 * 如果是地址之间的移动操作直接使用 var 即可，indirect_addr_t 对应 native 中的 indirect_addr
 * var t1 = [] =>  call new_array -> t1
 * t1[0] = 24 => mov 24 -> indirect_addr(t1, 0)
 */
typedef struct {
    lir_operand_t *base; // compiler 完成后为 var,  reg alloc 后为 reg
    uint64_t offset; // 偏移量是可以计算出来的, 默认为 0, 单位字节
    type_t type;// lir 为了保证通用性，只能有类型，不能有 size, 指向地址存储的数据的类型
} lir_indirect_addr_t;

//typedef struct {
//    lir_operand_t *base; // 只允许是 [indirect_addr_t?]
//    type_t type;
//} lir_lea_addr_t; // 读取 base 所在的地址

typedef struct {
    char *ident;
    bool is_local; // 是否为局部符号, 否则就是 global, 可以被链接器链接
} lir_symbol_label_t;

typedef struct {
    string ident;
    type_kind type;
} lir_symbol_var_t; // 外部符号引用, 外部符号引用

typedef struct {
    union {
        uint64_t uint_value; // 8bit, 负数使用补码存储
        int64_t int_value; // 8bit, 负数使用补码存储
        double float_value; // 8bit = c.double
        bool bool_value; // 1bit
        string string_value; // 8bit
    };
    type_kind kind;
} lir_imm_t;


struct lir_operand_t {
    lir_operand_type_t assert_type;
    // TODO 统一替换成 union
//    union {
//        lir_var_t var;
//        lir_stack_t stack;
//        lir_indirect_addr_t indirect_addr;
//        lir_symbol_label_t symbol_label;
//        lir_symbol_var_t symbol_var;
//        lir_imm_t imm;
//    };

    void *value;
    lir_flag_t pos; // 在 opcode 中的位置信息
};

/**
 * 四元组
 * add first second -> result
 * move first -> result // a = 12
 * 例如
 * call sum.n 12, 14 // 指令是 call
 * first param 是函数名称（label）
 * second param 是函数参数，函数调用并不产生新的变量，因此没必要放在 result 中
 * 原则上会新增变量的放在 result,使用变量放在 first/second
 *
 * label: 同样也是使用 first_param
 */
typedef struct lir_op_t {
    lir_opcode_t code;
    lir_operand_t *first; // 参数1
    lir_operand_t *second; // 参数2
    lir_operand_t *output; // 参数3
    int id; // 编号, 也就是寄存器分配期间的 position, 一般都是顺序编码的
} lir_op_t;

static inline lir_operand_t *int_operand(uint64_t val) {
    lir_imm_t *imm_operand = NEW(lir_imm_t);
    imm_operand->kind = TYPE_INT;
    imm_operand->uint_value = val;
    lir_operand_t *operand = NEW(lir_operand_t);
    operand->assert_type = LIR_OPERAND_IMM;
    operand->value = imm_operand;
    return operand;
}

static inline lir_operand_t *bool_operand(bool val) {
    lir_imm_t *imm_operand = NEW(lir_imm_t);
    imm_operand->kind = TYPE_BOOL;
    imm_operand->bool_value = val;
    lir_operand_t *operand = NEW(lir_operand_t);
    operand->assert_type = LIR_OPERAND_IMM;
    operand->value = imm_operand;
    return operand;
}

static inline lir_operand_t *float_operand(double val) {
    lir_imm_t *imm_operand = NEW(lir_imm_t);
    imm_operand->kind = TYPE_FLOAT;
    imm_operand->float_value = val;
    lir_operand_t *operand = NEW(lir_operand_t);
    operand->assert_type = LIR_OPERAND_IMM;
    operand->value = imm_operand;
    return operand;
}

static inline lir_operand_t *string_operand(char *str) {
    lir_imm_t *imm_operand = NEW(lir_imm_t);
    imm_operand->kind = TYPE_RAW_STRING;
    imm_operand->string_value = str;
    lir_operand_t *operand = NEW(lir_operand_t);
    operand->assert_type = LIR_OPERAND_IMM;
    operand->value = imm_operand;
    return operand;
}

static inline lir_operand_t *operand_new(lir_operand_type_t type, void *value) {
    lir_operand_t *operand = NEW(lir_operand_t);
    operand->assert_type = type;
    operand->value = value;
    return operand;
}

/**
 * 符号使用
 * @param ident
 * @param type
 * @return
 */
static inline lir_var_t *lir_var_new(module_t *m, char *ident) {
    lir_var_t *var = NEW(lir_var_t);
    var->ident = ident;
    var->old = ident;
    var->flag = 0;

    symbol_t *s = symbol_table_get(ident);
    assertf(s, "notfound symbol=%s", ident);
    assertf(s->type == SYMBOL_VAR, "symbol=%s type not var", ident);

    ast_var_decl *global_var = s->ast_value;
    var->type = global_var->type;
    var->flag |= type_base_trans_alloc(global_var->type.kind);

    return var;
}

static inline lir_operand_t *var_operand(module_t *m, char *ident) {
    lir_var_t *var = lir_var_new(m, ident);
    return operand_new(LIR_OPERAND_VAR, var);
}

static inline lir_operand_t *label_operand(char *ident, bool is_local) {
    lir_symbol_label_t *label = NEW(lir_symbol_label_t);
    label->ident = ident;
    label->is_local = is_local;
    return operand_new(LIR_OPERAND_SYMBOL_LABEL, label);
}

static inline lir_operand_t *symbol_label_operand(module_t *m, char *ident) {
    symbol_t *s = symbol_table_get(ident);
    assertf(s, "notfound symbol=%s", ident);
    assertf(s->type == SYMBOL_FN, "symbol=%s type not fn", ident);

    // 构造 label
    return label_operand(ident, s->is_local);
}


static inline lir_operand_t *lir_copy_label_operand(lir_operand_t *l) {
    lir_symbol_label_t *label = l->value;
    return label_operand(label->ident, label->is_local);
}

static inline slice_t *extract_operands(lir_operand_t *operand, uint64_t flag) {
    slice_t *result = slice_new();
    if (!operand) {
        return result;
    }

    if (flag & FLAG(operand->assert_type)) {
        slice_push(result, operand);
        return result;
    }

    if (operand->assert_type == LIR_OPERAND_INDIRECT_ADDR) {
        lir_indirect_addr_t *addr = operand->value;
        if (FLAG(addr->base->assert_type) & flag) {
            slice_push(result, addr->base);
        }
        return result;
    }

    if (operand->assert_type == LIR_OPERAND_ACTUAL_PARAMS) {
        slice_t *operands = operand->value;
        for (int i = 0; i < operands->count; ++i) {
            lir_operand_t *o = operands->take[i];
            assert(o->assert_type != LIR_OPERAND_ACTUAL_PARAMS && "ACTUAL_PARAM nesting is not allowed");

            slice_concat(result, extract_operands(o, flag));
        }
        return result;
    }

    if (flag & FLAG(LIR_OPERAND_VAR) && operand->assert_type == LIR_OPERAND_FORMAL_PARAMS) {
        slice_t *formal_params = operand->value;
        for (int i = 0; i < formal_params->count; ++i) { // 这里都是 def flag
            lir_var_t *var = formal_params->take[i];
            slice_push(result, operand_new(LIR_OPERAND_VAR, var));
        }
    }

    if (flag & FLAG(LIR_OPERAND_VAR) && operand->assert_type == LIR_OPERAND_PHI_BODY) {
        slice_t *body = operand->value;
        for (int i = 0; i < body->count; ++i) {
            lir_var_t *var = body->take[i];
            slice_push(result, operand_new(LIR_OPERAND_VAR, var));
        }
    }

    return result;
}


static inline slice_t *op_extract_operands(lir_op_t *op, uint64_t operand_flag) {
    slice_t *result = extract_operands(op->output, operand_flag);
    slice_concat(result, extract_operands(op->first, operand_flag));
    slice_concat(result, extract_operands(op->second, operand_flag));

    return result;
}

// 目前仅支持 var 的 copy
static inline lir_operand_t *lir_operand_copy(lir_operand_t *operand) {
    if (!operand) {
        return NULL;
    }

    lir_operand_t *new_operand = NEW(lir_operand_t);
    new_operand->assert_type = operand->assert_type;
    new_operand->value = operand->value;

    if (new_operand->assert_type == LIR_OPERAND_SYMBOL_LABEL) {
        return lir_copy_label_operand(new_operand);
    }

    if (new_operand->assert_type == LIR_OPERAND_VAR) {
        lir_var_t *var = new_operand->value;
        lir_var_t *new_var = NEW(lir_var_t);
        new_var->ident = var->ident;
        new_var->old = var->old;
        new_var->type = var->type;
        new_var->flag = 0; // 即使是同一个 var 在不同的位置承担的 flag 也是不同的
//        new_var->indirect_addr = var->indirect_addr;
        new_operand->value = new_var;
        return new_operand;
    }

    if (new_operand->assert_type == LIR_OPERAND_REG) {
        reg_t *reg = new_operand->value;
        reg_t *new_reg = NEW(reg_t);
        new_reg->name = reg->name;
        new_reg->index = reg->index;
        new_reg->size = reg->size;
        new_reg->alloc_id = reg->alloc_id;
        new_reg->flag = 0;
        new_operand->value = new_reg;
        return new_operand;
    }

    if (new_operand->assert_type == LIR_OPERAND_INDIRECT_ADDR) {
        lir_indirect_addr_t *addr = new_operand->value;
        lir_indirect_addr_t *new_addr = NEW(lir_indirect_addr_t);
        new_addr->base = lir_operand_copy(addr->base);
        new_addr->offset = addr->offset;
        new_addr->type = addr->type;
        new_operand->value = new_addr;
        return new_operand;
    }

    if (new_operand->assert_type == LIR_OPERAND_ACTUAL_PARAMS) {
        slice_t *new_value = slice_new();

        slice_t *operands = operand->value;
        for (int i = 0; i < operands->count; ++i) {
            lir_operand_t *o = operands->take[i];
            slice_push(new_value, lir_operand_copy(o));
        }
        new_operand->value = new_value;
        return new_operand;
    }

    return new_operand;
}

/**
 * TODO 并不是 intput 就一定是 use, 比如 memmove(var_dst, var_src) 中都 dst 就属于 def
 * @param operand
 */
static inline void set_operand_flag(lir_operand_t *operand) {
    if (!operand) {
        return;
    }

    if (operand->assert_type == LIR_OPERAND_VAR) {
        // 仅 output 且 indirect_addr = false 才配置 def
        lir_var_t *var = operand->value;
        var->flag |= FLAG(operand->pos); // 冗余 operand 的位置信息
        if (operand->pos == LIR_FLAG_OUTPUT) {
            var->flag |= FLAG(LIR_FLAG_DEF);
        } else {
            var->flag |= FLAG(LIR_FLAG_USE);
        }

        return;
    }

    if (operand->assert_type == LIR_OPERAND_REG) {
        reg_t *reg = operand->value;
        reg->flag |= FLAG(operand->pos);
        if (operand->pos == LIR_FLAG_OUTPUT) {
            reg->flag |= FLAG(LIR_FLAG_DEF);
        } else {
            reg->flag |= FLAG(LIR_FLAG_USE);
        }
        return;
    }

    if (operand->assert_type == LIR_OPERAND_INDIRECT_ADDR) {
        lir_indirect_addr_t *addr = operand->value;
        if (addr->base->assert_type == LIR_OPERAND_VAR) {
            lir_var_t *var = addr->base->value;
            var->flag |= FLAG(LIR_FLAG_USE);
            var->flag |= FLAG(LIR_FLAG_INDIRECT_ADDR_BASE);
        }
        return;
    }

    if (operand->assert_type == LIR_OPERAND_FORMAL_PARAMS) {
        slice_t *formal_params = operand->value;
        for (int i = 0; i < formal_params->count; ++i) { // 这里都是 def flag
            lir_var_t *var = formal_params->take[i];
            var->flag |= FLAG(LIR_FLAG_DEF);
        }
        return;
    }

    // 剩下的都是 use 直接提取出来即可
    slice_t *operands = extract_operands(operand, FLAG(LIR_OPERAND_VAR) | FLAG(LIR_OPERAND_REG));
    for (int i = 0; i < operands->count; ++i) {
        lir_operand_t *o = operands->take[i];
        set_operand_flag(o); // 符合嵌入的全部定义成 USE
    }
}


static inline lir_op_t *
lir_op_new(lir_opcode_t code, lir_operand_t *first, lir_operand_t *second, lir_operand_t *result) {
    lir_op_t *op = NEW(lir_op_t);
    op->code = code;
    op->first = lir_operand_copy(first); // 这里的 copy 并不深度，而是 copy 了指针！
    op->second = lir_operand_copy(second);
    op->output = lir_operand_copy(result);

    op->first && (op->first->pos = LIR_FLAG_FIRST);
    op->second && (op->second->pos = LIR_FLAG_SECOND);
    op->output && (op->output->pos = LIR_FLAG_OUTPUT);

    set_operand_flag(op->first);
    set_operand_flag(op->second);
    set_operand_flag(op->output);

    return op;
}

static inline lir_op_t *lir_op_result(lir_opcode_t code, lir_operand_t *result) {
    return lir_op_new(code, NULL, NULL, result);
}

static inline lir_op_t *lir_op_label(char *ident, bool is_local) {
    return lir_op_new(LIR_OPCODE_LABEL, NULL, NULL, label_operand(ident, is_local));
}

static inline lir_op_t *lir_op_unique_label(module_t *m, char *ident) {
    char *unique_ident = make_unique_ident(m, ident);
    return lir_op_label(unique_ident, true);
}

static inline lir_op_t *lir_op_bal(lir_operand_t *label) {
    return lir_op_new(LIR_OPCODE_BAL, NULL, NULL, lir_copy_label_operand(label));
}

static inline lir_op_t *lir_op_move(lir_operand_t *dst, lir_operand_t *src) {
    return lir_op_new(LIR_OPCODE_MOVE, src, NULL, dst);
}

static inline lir_op_t *lir_op_lea(lir_operand_t *dst, lir_operand_t *src) {
    return lir_op_new(LIR_OPCODE_LEA, src, NULL, dst);
}

static inline type_kind operand_type_kind(lir_operand_t *operand) {
    assert(operand->assert_type != LIR_OPERAND_REG);

    if (operand->assert_type == LIR_OPERAND_VAR) {
        lir_var_t *var = operand->value;
        return var->type.kind;
    }

    if (operand->assert_type == LIR_OPERAND_INDIRECT_ADDR) {
        lir_indirect_addr_t *addr = operand->value;
        return addr->type.kind;
    }

    if (operand->assert_type == LIR_OPERAND_SYMBOL_VAR) {
        lir_symbol_var_t *s = operand->value;
        return s->type;
    }

    if (operand->assert_type == LIR_OPERAND_IMM) {
        lir_imm_t *imm = operand->value;
        return imm->kind;
    }

    return TYPE_UNKNOWN;
}

/**
 * operand 此时并没有 pos 需要替换成新的 operand,替换的过程中需要重新设定
 * @param operand
 * @param pos
 * @return
 */
static inline lir_operand_t *lir_reset_operand(lir_operand_t *operand, uint8_t pos) {
    lir_operand_t *temp = lir_operand_copy(operand);
    temp->pos = pos;
    set_operand_flag(temp);
    return temp;
}


/**
 * 已经经过了 ssa 的处理，才 first op 需要排除 label 和 phi
 * @param block
 */
static inline void lir_set_quick_op(basic_block_t *block) {
    linked_node *current = linked_first(block->operations)->succ;
    while (current->value != NULL && OP(current)->code == LIR_OPCODE_PHI) {
        current = current->succ;
    }
    assert(current);
    // current code not opcode phi
    block->first_op = current;
    block->last_op = linked_last(block->operations);
}


static inline lir_op_t *rt_call(char *name, lir_operand_t *result, int arg_count, ...) {
    slice_t *params_operand = slice_new();

    va_list args;
    va_start(args, arg_count); // 初始化参数
    for (int i = 0; i < arg_count; ++i) {
        lir_operand_t *param = va_arg(args, lir_operand_t*);
        slice_push(params_operand, param);
    }
    va_end(args);
    lir_operand_t *call_params_operand = operand_new(LIR_OPERAND_ACTUAL_PARAMS, params_operand);
    return lir_op_new(LIR_OPCODE_RT_CALL, label_operand(name, false), call_params_operand, result);
}

static inline lir_op_t *lir_call(char *name, lir_operand_t *result, int arg_count, ...) {
    slice_t *params_operand = slice_new();

    va_list args;
    va_start(args, arg_count); // 初始化参数
    for (int i = 0; i < arg_count; ++i) {
        lir_operand_t *param = va_arg(args, lir_operand_t*);
        slice_push(params_operand, param);
    }
    va_end(args);
    lir_operand_t *call_params_operand = operand_new(LIR_OPERAND_ACTUAL_PARAMS, params_operand);
    return lir_op_new(LIR_OPCODE_CALL, label_operand(name, false), call_params_operand, result);
}

/**
 * 临时变量是否影响变量入栈？
 * @param type
 * @return
 */
static inline lir_operand_t *temp_var_operand(module_t *m, type_t type) {
    assert(type.kind > 0);

    string result = var_unique_ident(m, TEMP_IDENT);

    symbol_table_set_var(result, type);

    return operand_new(LIR_OPERAND_VAR, lir_var_new(m, result));
}

/**
 * 临时变量是否影响变量入栈？
 * @param type
 * @return
 */
static inline lir_operand_t *custom_var_operand(module_t *m, type_t type, char *ident) {
    string result = var_unique_ident(m, ident);

    symbol_table_set_var(result, type);

    return operand_new(LIR_OPERAND_VAR, lir_var_new(m, result));
}


/**
 * @param m
 * @param operand
 * @return
 */
static inline lir_operand_t *lea_operand_pointer(module_t *m, lir_operand_t *operand) {
    lir_operand_t *var_operand = operand;
    if (operand->assert_type == LIR_OPERAND_IMM) {
        lir_imm_t *imm = operand->value;
        // 确保参数入栈
        lir_operand_t *temp_operand = temp_var_operand(m, type_basic_new(imm->kind));
        OP_PUSH(lir_op_move(temp_operand, operand));
        var_operand = temp_operand;
    }

    // symbol label 是一个指针，memory_fn_t 中存储的就是这个指针的值，所以需要将其取出来，然后再复制给一个栈临时变量。
    if (operand->assert_type == LIR_OPERAND_SYMBOL_LABEL) {
        assert(false);
//        lir_symbol_label_t *symbol = operand->value;
        // temp_operand 是 type_fn, 就像 type_string, type_array 一样
//        lir_operand_t *temp_operand = temp_var_operand(m, type_basic_new(TYPE_FN));
//        OP_PUSH(lir_op_lea(temp_operand, operand));
//        var_operand = temp_operand;
    }

    assertf(var_operand->assert_type == LIR_OPERAND_VAR || var_operand->assert_type == LIR_OPERAND_INDIRECT_ADDR ||
            var_operand->assert_type == LIR_OPERAND_SYMBOL_LABEL ||
            var_operand->assert_type == LIR_OPERAND_SYMBOL_VAR,
            "only support lea var/symbol/addr, actual=%d",
            var_operand->assert_type);

    lir_var_t *var = var_operand->value;
    lir_operand_t *value_ref = temp_var_operand(m, type_ptrof(var->type));
    OP_PUSH(lir_op_lea(value_ref, var_operand));
    return value_ref;
}


closure_t *lir_closure_new(ast_fndef_t *fndef);

static inline basic_block_t *lir_new_basic_block(char *name, uint16_t label_index) {
    basic_block_t *basic_block = NEW(basic_block_t);
    basic_block->name = name;
    basic_block->id = label_index;

    basic_block->operations = linked_new();
    basic_block->preds = slice_new();
    basic_block->succs = slice_new();

    basic_block->forward_succs = slice_new();
    basic_block->incoming_forward_count = 0;
    basic_block->use = slice_new();
    basic_block->def = slice_new();
    basic_block->loop_ends = slice_new();
    basic_block->live = slice_new();
    basic_block->live_in = slice_new();
    basic_block->live_out = slice_new();
    basic_block->domers = slice_new();
    basic_block->df = slice_new();
    basic_block->imm_domees = slice_new();
    basic_block->loop.index = -1;
    memset(basic_block->loop.index_map, 0, sizeof(basic_block->loop.index_map));
    basic_block->loop.depth = 0;
    basic_block->loop.visited = false;
    basic_block->loop.active = false;
    basic_block->loop.header = false;
    basic_block->loop.end = false;

    return basic_block;
}

static inline bool lir_blocks_contains(slice_t *blocks, uint16_t id) {
    for (int i = 0; i < blocks->count; ++i) {
        basic_block_t *block = blocks->take[i];
        if (block->id == id) {
            return true;
        }
    }
    return false;
}

static inline lir_operand_t *lir_new_phi_body(module_t *m, lir_var_t *var, uint8_t count) {
    lir_operand_t *operand = NEW(lir_operand_t);

    slice_t *phi_body = slice_new();
    for (int i = 0; i < count; ++i) {
        slice_push(phi_body, lir_var_new(m, var->ident));
    }

    operand->assert_type = LIR_OPERAND_PHI_BODY;
    operand->value = phi_body;
    return operand;
}


static inline bool lir_op_branch_cmp(lir_op_t *op) {
    return op->code == LIR_OPCODE_BEQ;
}

static inline bool lir_op_branch(lir_op_t *op) {
    return op->code == LIR_OPCODE_BAL || op->code == LIR_OPCODE_BEQ;
}

static inline bool lir_op_call(lir_op_t *op) {
    return op->code == LIR_OPCODE_CALL || op->code == LIR_OPCODE_RT_CALL;
}

static inline bool lir_operand_equal(lir_operand_t *a, lir_operand_t *b) {
    if (a->assert_type != b->assert_type) {
        return false;
    }

    if (a->assert_type == LIR_OPERAND_REG) {
        reg_t *reg_a = a->value;
        reg_t *reg_b = b->value;
        return reg_a->index == reg_b->index;
    }

    if (a->assert_type == LIR_OPERAND_STACK) {
        lir_stack_t *stack_a = a->value;
        lir_stack_t *stack_b = b->value;
        return stack_a->slot == stack_b->slot;
    }

    return false;
}

static inline bool lir_op_contain_cmp(lir_op_t *op) {
    if (op->code == LIR_OPCODE_BEQ ||
        op->code == LIR_OPCODE_SGT ||
        op->code == LIR_OPCODE_SGE ||
        op->code == LIR_OPCODE_SEE ||
        op->code == LIR_OPCODE_SNE ||
        op->code == LIR_OPCODE_SLT ||
        op->code == LIR_OPCODE_SLE) {
        return true;
    }
    return false;
}


static inline bool lir_op_term(lir_op_t *op) {
    if (op->code == LIR_OPCODE_ADD ||
        op->code == LIR_OPCODE_SUB) {
        return true;
    }
    return false;
}

static inline slice_t *lir_op_operands(lir_op_t *op, flag_t operand_flag, flag_t vr_flag, bool extract_value) {
    slice_t *temps = op_extract_operands(op, operand_flag);
    slice_t *results = slice_new();
    for (int i = 0; i < temps->count; ++i) {
        lir_operand_t *operand = temps->take[i];
        assertf(FLAG(operand->assert_type) & operand_flag, "operand type is not and operand flag");

        // 只有 var 或者 reg 现需要进行 vr 校验
        if (operand->assert_type == LIR_OPERAND_VAR) {
            lir_var_t *var = operand->value;
            // def or use
            if (!(var->flag & vr_flag)) {
                continue;
            }
        } else if (operand->assert_type == LIR_OPERAND_REG) {
            reg_t *reg = operand->value;
            if (!(reg->flag & vr_flag)) {
                continue;
            }
        }

        if (extract_value) {
            slice_push(results, operand->value);
        } else {
            slice_push(results, operand);
        }
    }

    return results;
}

/**
 * @param op
 * @param vr_flag  use or def
 * @return lir_var_t
 */
static inline slice_t *lir_var_operands(lir_op_t *op, flag_t vr_flag) {
    return lir_op_operands(op, FLAG(LIR_OPERAND_VAR), vr_flag, true);
}

/**
 * 尽量在不影响原有 operand 的情况下进行值的替换, 这样通用性更强
 * @param c
 * @param b
 * @param operand
 */
static inline void convert_to_var(closure_t *c, basic_block_t *b, linked_node *node, lir_operand_t *operand) {
    type_kind kind = operand_type_kind(operand);
    lir_operand_t *temp = temp_var_operand(c->module, type_basic_new(kind));
    slice_push(c->globals, temp->value);
    linked_insert_before(b->operations, node, lir_op_move(temp, operand));
    temp = lir_reset_operand(temp, operand->pos);
    operand->assert_type = temp->assert_type;
    operand->value = temp->value;
}

#endif //NATURE_SRC_LIR_H_

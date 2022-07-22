#ifndef NATURE_SRC_LIR_H_
#define NATURE_SRC_LIR_H_

#include "src/lib/list.h"
#include "src/value.h"
#include "src/ast.h"
#include "src/lib/table.h"
#include "src/register/register.h"

#define TEMP_IDENT "t"
#define CONTINUE_IDENT "continue"
#define WHILE_IDENT "while"
#define END_WHILE_IDENT "end_while"
#define FOR_IDENT "for"
#define END_FOR_IDENT "end_for"
#define END_IF_IDENT "end_if"
#define ALTERNATE_IF_IDENT "alternate_if"

#define RUNTIME_CALL_LIST_NEW "list_new"
#define RUNTIME_CALL_LIST_VALUE "list_value"

#define RUNTIME_CALL_MAP_NEW "map_new"
#define RUNTIME_CALL_MAP_VALUE "map_value"
#define RUNTIME_CALL_ITERATE_COUNT "iterate_count"

#define RUNTIME_CALL_ITERATE_GEN_KEY "iterate_gen_key"
#define RUNTIME_CALL_ITERATE_GEN_VALUE "iterate_gen_value"

#define RUNTIME_CALL_ENV_NEW "env_new"
#define RUNTIME_CALL_SET_ENV "set_env"
#define RUNTIME_CALL_GET_ENV "get_env"

#define RUNTIME_CALL_STRING_NEW "string_new"
#define RUNTIME_CALL_STRING_ADDR "string_addr"
#define RUNTIME_CALL_STRING_LENGTH "string_length"

#define LIST_OP_COPY(dst, src) \
({                             \
    dst->type = src->type; \
    dst->first = src->first; \
    dst->second = src->second; \
    dst->result = src->result; \
})

#define LIR_NEW_IMMEDIATE_OPERAND(operand_type, key, val) \
({                                               \
   lir_operand_immediate *imm_operand = malloc(sizeof(lir_operand_immediate)); \
   imm_operand->type = operand_type; \
   imm_operand->key = val; \
   lir_operand *operand = malloc(sizeof(lir_operand)); \
   operand->type = LIR_OPERAND_TYPE_IMMEDIATE; \
   operand->value = imm_operand;              \
   operand; \
})

#define LIR_NEW_OPERAND(_type, _value) \
({                                 \
  lir_operand *_operand = NEW(lir_operand); \
  _operand->type = _type;           \
  _operand->value = _value;    \
  _operand;                                   \
})

#define LIR_NEW_VAR_OPERAND(_ident) \
({                                 \
  lir_operand_var *_var = NEW(lir_operand_var); \
  _var->old = _ident;    \
  _var->ident = _ident;             \
  _var;                                   \
})

#define LIR_COPY_VAR_OPERAND(_original) \
({                                 \
  lir_operand_var *_var = NEW(lir_operand_var); \
  _var->old = (_original)->old;    \
  _var->ident = (_original)->ident; \
  _var->local = (_original)->local; \
  _var->reg_id = (_original)->reg_id; \
  _var->size = (_original)->size; \
  _var;                                   \
})

#define LIR_UNIQUE_NAME(_ident) \
({                                 \
   char *temp_name = malloc(strlen(_ident) + sizeof(int) + 2); \
   sprintf(temp_name, "%s_%d", _ident, lir_unique_count++); \
   temp_name;                                   \
})

int lir_unique_count;
int lir_line;

typedef enum {
    LIR_OPERAND_TYPE_NULL,
    LIR_OPERAND_TYPE_VAR, // 虚拟寄存器? 那我凭什么给虚拟寄存器分配内存地址？又或者是 symbol?
    LIR_OPERAND_TYPE_SYMBOL, // 虚拟寄存器? 那我凭什么给虚拟寄存器分配内存地址？
    LIR_OPERAND_TYPE_REG,
    LIR_OPERAND_TYPE_PHI_BODY,
    LIR_OPERAND_TYPE_FORMAL_PARAM,
    LIR_OPERAND_TYPE_ACTUAL_PARAM,
    LIR_OPERAND_TYPE_LABEL,
    LIR_OPERAND_TYPE_IMMEDIATE,
    LIR_OPERAND_TYPE_MEMORY,
} lir_operand_type;

typedef struct lir_operand {
    lir_operand_type type;
    void *value;
} lir_operand;

typedef enum {
    LIR_IMMEDIATE_TYPE_INT,
    LIR_IMMEDIATE_TYPE_BOOL,
    LIR_IMMEDIATE_TYPE_FLOAT,
} lir_immediate_type;

//typedef struct {
//  string ident;
//} lir_operand_reg;

typedef struct {
    string ident;
    ast_type type;
    uint16_t *stack_frame_offset;
} lir_local_var;

/**
 * 存放在寄存器或者内存中, var a = 1
 */
typedef struct {
    string ident; // ssa 后的新名称
    string old;
    uint8_t reg_id; // reg list index, 寄存器分配
    lir_local_var *local; // local 如果为 nil 就是外部符号引用
    uint8_t size; // BYTE/QWORD/DWORD ? 指针怎么说？
} lir_operand_var;

typedef struct {
    char *ident;
    bool is_local; // 是否为局部符号, 否则就是 global, 可以被链接器链接
} lir_operand_label;

typedef struct {
    string ident;
} lir_operand_symbol; // 外部符号引用, 外部符号引用

typedef struct lir_vars {
    uint8_t count;
    lir_operand_var *list[UINT8_MAX];
} lir_vars, lir_operand_phi_body;

typedef size_t memory_address;

typedef struct {
    lir_operand *base;
    size_t offset; // 偏移量是可以计算出来的
    size_t length; // 数据长度
} lir_operand_memory;

typedef struct {
    union {
        int64_t int_value;
        float float_value;
        bool bool_value;
        string string_value;
    };
    type_category type;
} lir_operand_immediate;

typedef struct {
    lir_vars vars;
    uint8_t count;
} lir_operand_formal_param;

typedef struct {
    lir_operand *list[UINT8_MAX];
    uint8_t count;
} lir_operand_actual_param;

typedef enum {
    LIR_OP_TYPE_NULL,
    LIR_OP_TYPE_ADD,
    LIR_OP_TYPE_SUB,
    LIR_OP_TYPE_MUL,
    LIR_OP_TYPE_DIV,
    LIR_OP_TYPE_LT,
    LIR_OP_TYPE_LTE,
    LIR_OP_TYPE_GT,
    LIR_OP_TYPE_GTE,
    LIR_OP_TYPE_EQ_EQ,
    LIR_OP_TYPE_NOT_EQ,
    LIR_OP_TYPE_NOT,
    LIR_OP_TYPE_MINUS,

    LIR_OP_TYPE_PHI,
    LIR_OP_TYPE_MOVE,
    LIR_OP_TYPE_CMP_GOTO, // cmp 两个参数总是相等，就跳转 cmp + je
    LIR_OP_TYPE_GOTO,
    LIR_OP_TYPE_PUSH,
    LIR_OP_TYPE_POP,
    LIR_OP_TYPE_CALL,
    LIR_OP_TYPE_RUNTIME_CALL,
    LIR_OP_TYPE_BUILTIN_CALL, // BUILTIN_CALL print params => nil
    LIR_OP_TYPE_RETURN, // return 并不能真的就推出函数执行
    LIR_OP_TYPE_LABEL,
    LIR_OP_TYPE_FN_BEGIN,
    LIR_OP_TYPE_FN_END,
} lir_op_type;

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
typedef struct lir_op {
    lir_op_type type;
    lir_operand *first; // 参数1
    lir_operand *second; // 参数2
    lir_operand *result; // 参数3

    // result 类型
    // TYPE_BOOL = 1, TYPE_FLOAT = 8, TYPE_INT = 8, ,
    // TYPE_STRING/LIST/MAP/SET = 8
    // CUSTOM_TYPE 如何处理？
    type_category result_type;

//    string struct_name;

    int id; // 编号
} lir_op;

// type 列表
typedef struct {
    lir_op *front;
    lir_op *rear;
    uint16_t count;
} list_op;

typedef struct {
    uint8_t count;
    struct lir_basic_block *list[UINT8_MAX];
} lir_basic_blocks;

typedef struct {
    uint8_t flag;
    uint8_t tree_high;
    uint8_t index_list[UINT8_MAX];
    uint8_t index;
    uint8_t depth;
} loop_detection;

typedef struct lir_basic_block {
    string name;
    uint8_t label; // label 标号, 基本块编号(可以方便用于数组索引)， 和 op_label 还是要稍微区分一下,

    lir_op *first_op; // 链表结构， 开始处的指令

    list *operates;

    lir_basic_blocks preds;
    lir_basic_blocks succs;
    lir_basic_blocks forward_succs;
    uint8_t incoming_forward_count; // 正向进入到该节点的节点数量

    lir_vars use;
    lir_vars def;
    lir_vars live_out;
    lir_vars live_in; // 一个变量如果在当前块被使用，或者再当前块的后继块中被使用，则其属于入口活跃
    lir_basic_blocks dom; // 当前块被哪些基本块支配
    lir_basic_blocks df;
    lir_basic_blocks be_idom; // 哪些块已当前块作为最近支配块,其组成了支配者树
    struct lir_basic_block *idom; // 当前块的最近支配者

    // loop detection
    loop_detection loop;
} lir_basic_block;

/**
 * 1. cfg 需要专门构造一个结尾 basic block 么，用来处理函数返回值等？其一定位于 blocks[count - 1]
 * 形参有一条专门的指令 lir_formal_param 编译这条指令的时候处理形参即可
 * lir_formal_param 在寄存器分配阶段已经分配了合适的 stack or reg, 所以依次遍历处理即可
 * 如果函数的返回值大于 8 个字节，则需要引用传递返回, ABI 规定形参 1 为引用返回地址
 * 假如形参和所有局部变量占用的总长为 N Byte, 那么 -n(rbp) 处存储的就是最后一个形参的位置(向上存储)
 * 所以还是需要 closure 增加一个字段记录大值地址响应值, 从而可以正常返回
 *
 * 2. closure 可以能定义在文件中的全局函数，也可能是定义在结构体中的局部函数，在类型推导阶段是有能力识别到函数的左值
 * 是一个变量，还是结构体的元素
 */
typedef struct closure {
    lir_vars globals; // closure 中定义的变量列表, 用于 ssa 构建
    lir_vars formal_params; // closure 形参列表, 堆栈分配时有一席之地。
    regs_t fixed_regs; // 作为临时寄存器使用到的寄存器
    lir_basic_blocks blocks; // 根据解析顺序得到

    lir_basic_block *entry; // 基本块入口
    lir_basic_blocks order_blocks; // 寄存器分配前根据权重进行重新排序
    table *interval_table; // key包括 fixed register name 和 variable.ident

    // 定义环境
    string name;
    string end_label; // 结束地址
    string env_name;
    struct closure *parent;
    list *operates; // 指令列表

    table *local_vars; // 主要是用于栈分配(但是该结构不适合遍历)

    // 大响应值分配的栈偏移(初始时肯定为 rdi,然后被分配到内存中, 根据观察，栈内存分配没有考虑过函数内的进出栈)
    uint16_t return_offset;
    uint16_t stack_length; // 栈长度, byte, 等于局部变量的长度
} closure;

lir_operand *lir_new_phi_body(lir_operand_var *var, uint8_t count);

lir_basic_block *lir_new_basic_block();
//string lir_label_to_string(uint8_t label);

closure *lir_new_closure(ast_closure_decl *ast);

/**
 * 符号使用
 * @param ident
 * @param type
 * @return
 */
lir_operand_var *lir_new_var_operand(closure *c, string ident);

/**
 * 符号定义
 * 定义一个 local var operand
 * @param ident
 * @param type
 */
void lir_new_local_var(closure *c, string ident, ast_type type);

lir_operand *lir_new_temp_var_operand(closure *c, ast_type type);

lir_operand *lir_new_param_var_operand();

lir_operand *lir_new_memory_operand(lir_operand *base, size_t offset, size_t length);

lir_operand *lir_new_label_operand(string ident, bool is_local);

lir_operand_actual_param *lir_new_actual_param();

lir_op *lir_op_label(string name, bool is_local);

lir_op *lir_op_unique_label(string name);

lir_op *lir_op_goto(lir_operand *label);

//lir_op *lir_new_push(lir_operand *operand);
lir_op *lir_op_move(lir_operand *dst, lir_operand *src);

lir_op *lir_op_new(lir_op_type type, lir_operand *first, lir_operand *second, lir_operand *result);

lir_op *lir_op_call(string name, lir_operand *result, int arg_count, ...);

bool lir_blocks_contains(lir_basic_blocks blocks, uint8_t label);

#endif //NATURE_SRC_LIR_H_
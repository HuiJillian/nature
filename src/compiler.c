#include <string.h>
#include <stdio.h>
#include "compiler.h"
#include "src/debug/debug.h"

int compiler_line = 0;

lir_opcode_t ast_op_convert[] = {
        [AST_OP_ADD] = LIR_OPCODE_ADD,
        [AST_OP_SUB] = LIR_OPCODE_SUB,
        [AST_OP_MUL] = LIR_OPCODE_MUL,
        [AST_OP_DIV] = LIR_OPCODE_DIV,
        [AST_OP_REM] = LIR_OPCODE_REM,

        [AST_OP_LSHIFT] = LIR_OPCODE_SHL,
        [AST_OP_RSHIFT] = LIR_OPCODE_SHR,
        [AST_OP_AND] = LIR_OPCODE_AND,
        [AST_OP_OR] = LIR_OPCODE_OR,
        [AST_OP_XOR] = LIR_OPCODE_XOR,

        [AST_OP_LT] = LIR_OPCODE_SLT,
        [AST_OP_LE] = LIR_OPCODE_SLE,
        [AST_OP_GT] = LIR_OPCODE_SGT,
        [AST_OP_GE] = LIR_OPCODE_SGE,
        [AST_OP_EE] = LIR_OPCODE_SEE,
        [AST_OP_NE] = LIR_OPCODE_SNE,

        [AST_OP_BNOT] = LIR_OPCODE_NOT,
        [AST_OP_NEG] = LIR_OPCODE_NEG,
};

static lir_operand_t *global_fn_symbol(module_t *m, ast_expr expr) {
    if (expr.assert_type != AST_EXPR_IDENT) {
        return NULL;
    }

    ast_ident *ident = expr.value;
    symbol_t *s = symbol_table_get(ident->literal);
    assertf(s, "ident %s not declare");
    if (s->type != SYMBOL_FN) {
        return NULL;
    }
    return label_operand(ident->literal, s->is_local);
}

static lir_operand_t *compiler_temp_var_operand(module_t *m, type_t type) {
    assert(type.kind > 0);
    lir_operand_t *temp = temp_var_operand(m, type);
    OP_PUSH(lir_op_new(LIR_OPCODE_CLV, NULL, NULL, temp));
    return temp;
}

/**
 * @param m
 * @param expr
 * @return
 */
static lir_operand_t *compiler_ident(module_t *m, ast_expr expr) {
    ast_ident *ident = expr.value;
    symbol_t *s = symbol_table_get(ident->literal);
    assertf(s, "ident %s not declare");

    char *closure_name = m->compiler_current->closure_name;
    if (closure_name && str_equal(s->ident, closure_name)) {
        // symbol 中的该符号已经改写成 closure var 了，该 closure var 通过 last param 丢了进来
        // 所以直接使用 fn 该 fn 就行了，该 fn 一定被赋值了，就放心好了
        assertf(s->type == SYMBOL_VAR, "closure symbol=%s not var");
        assertf(m->compiler_current->fn_runtime_operand, "closure->fn_runtime_operand not init");
        lir_operand_t *operand = m->compiler_current->fn_runtime_operand;
        lir_var_t *var = operand->value;
        assert(str_equal(var->ident, ident->literal));
    }

    if (s->type == SYMBOL_FN) {
        // 现在 symbol fn 是作为一个 type_nf 值进行传递，所以需要取出其 label 进行处理。
        // 即使是 global fn 也不例外, compiler call symbol 已经进行了特殊处理，进不到这里来
        lir_operand_t *result = temp_var_operand(m, type_basic_new(TYPE_FN));
        OP_PUSH(lir_op_lea(result, symbol_label_operand(m, ident->literal)));
        return result;
    }

    if (s->type == SYMBOL_VAR) {
        ast_var_decl *var = s->ast_value;
        if (s->is_local) {
            return operand_new(LIR_OPERAND_VAR, lir_var_new(m, ident->literal));
        } else {
            lir_symbol_var_t *symbol = NEW(lir_symbol_var_t);
            symbol->ident = ident->literal;
            symbol->kind = var->type.kind;
            return operand_new(LIR_OPERAND_SYMBOL_VAR, symbol);
        }
    }
    assertf(false, "ident %s exception", ident);
    exit(1);
}

static void compiler_list_assign(module_t *m, ast_assign_stmt *stmt) {
    ast_list_access_t *list_access = stmt->left.value;
    lir_operand_t *list_target = compiler_expr(m, list_access->left);
    lir_operand_t *index_target = compiler_expr(m, list_access->index);

    // 取 value 栈指针,如果 value 不是 var， 会自动转换成 var
    lir_operand_t *value_ref = lea_operand_pointer(m, compiler_expr(m, stmt->right));

    // mov $1, -4(%rbp) // 以 var 的形式入栈
    // mov -4(%rbp), rcx // 参数 1, move 将 -4(%rbp) 处的值穿递给了 rcx, 而不是 -4(%rbp) 这个栈地址
    OP_PUSH(rt_call(RT_CALL_LIST_ASSIGN, NULL,
                    3, list_target, index_target, value_ref));
}

/**
 * @param m
 * @param stmt
 */
static void compiler_env_assign(module_t *m, ast_assign_stmt *stmt) {
    ast_env_access *ast = stmt->left.value;
    lir_operand_t *index = int_operand(ast->index);

    lir_operand_t *src_ref = lea_operand_pointer(m, compiler_expr(m, stmt->right));
    uint64_t size = type_sizeof(stmt->right.type);
    assertf(m->compiler_current->fn_runtime_operand, "have env access, must have fn_runtime_operand");

    OP_PUSH(rt_call(RT_CALL_ENV_ASSIGN_REF, NULL, 4,
                    m->compiler_current->fn_runtime_operand,
                    index, src_ref, int_operand(size)));
}


static void compiler_map_assign(module_t *m, ast_assign_stmt *stmt) {
    ast_map_access_t *map_access = stmt->left.value;
    lir_operand_t *map_target = compiler_expr(m, map_access->left);
    lir_operand_t *key_ref = lea_operand_pointer(m, compiler_expr(m, map_access->key));
    lir_operand_t *value_ref = lea_operand_pointer(m, compiler_expr(m, stmt->right));
    lir_op_t *call_op = rt_call(RT_CALL_MAP_ASSIGN, NULL, 3, map_target, key_ref, value_ref);
    OP_PUSH(call_op);
}

static void compiler_struct_assign(module_t *m, ast_assign_stmt *stmt) {
    ast_struct_select_t *struct_access = stmt->left.value;
    type_t struct_type = struct_access->left.type;
    lir_operand_t *struct_target = compiler_expr(m, struct_access->left);
    uint64_t offset = type_struct_offset(struct_type.struct_, struct_access->key);
    uint64_t item_size = type_sizeof(struct_access->property->type);

    lir_operand_t *src_ref = lea_operand_pointer(m, compiler_expr(m, stmt->right));

    // move by item size
    OP_PUSH(rt_call(RT_CALL_MEMORY_MOVE, NULL,
                    5,
                    struct_target,
                    int_operand(offset),
                    src_ref,
                    int_operand(0),
                    int_operand(item_size)));
}

/**
 * ident = operand
 * @param c
 * @param stmt
 */
static void compiler_ident_assign(module_t *m, ast_assign_stmt *stmt) {
    // 如果 left 是 var
    lir_operand_t *src = compiler_expr(m, stmt->right);
    lir_operand_t *dst = compiler_ident(m, stmt->left); // ident
    OP_PUSH(lir_op_move(dst, src));
}


//
//
/**
 * 将 tuple 按递归解析赋值给 tuple_destr 中声明的所有 var
 * 递归将导致优先从左侧进行展开, 需要注意的是，仅支持 left 表达式，且需要走 assign
 * @param m
 * @param destr
 * @param tuple_target
 */
static void compiler_tuple_destr(module_t *m, ast_tuple_destr *destr, lir_operand_t *tuple_target) {
    uint64_t offset = 0;
    for (int i = 0; i < destr->elements->length; ++i) {
        ast_expr *element = ct_list_value(destr->elements, i);
        // tuple_operand 对应到当前 index 到值
        uint64_t item_size = type_sizeof(element->type);
        offset = align((int64_t) offset, (int64_t) item_size);

        lir_operand_t *temp = compiler_temp_var_operand(m, element->type);
        lir_operand_t *dst_ref = lea_operand_pointer(m, temp);

        OP_PUSH(rt_call(RT_CALL_MEMORY_MOVE,
                        NULL,
                        5,
                        dst_ref,
                        int_operand(0),
                        tuple_target,
                        int_operand(offset),
                        int_operand(item_size)));

        // temp 用与临时存储 tuple 中的值，下面则是真正的使用该临时值
        if (element->assert_type == AST_VAR_DECL) {
            // var_decl 独有
            ast_var_decl *var_decl = element->value;
            lir_operand_t *dst = var_operand(m, var_decl->ident);
            OP_PUSH(lir_op_move(dst, temp));

        } else if (can_assign(element->assert_type)) {
            assert(temp->assert_type == LIR_OPERAND_VAR);
            // element 是左值
            ast_assign_stmt *assign_stmt = NEW(ast_assign_stmt);
            assign_stmt->left = *element;
            // temp is ident， 把 ident 解析出来
            assign_stmt->right = *ast_ident_expr(((lir_var_t *) temp->value)->ident);
            compiler_assign(m, assign_stmt);
        } else if (element->assert_type == AST_EXPR_TUPLE_DESTR) {
            compiler_tuple_destr(m, element->value, temp);
        } else {
            assertf(false, "var tuple destr must var/tuple_destr");
        }
        offset += item_size;
    }
}

/**
 * (a, b, (c[0], d.b)) = operand
 * @param m
 * @param stmt
 */
static void compiler_tuple_destr_stmt(module_t *m, ast_assign_stmt *stmt) {
    ast_tuple_destr *destr = stmt->left.value;
    lir_operand_t *tuple_target = compiler_expr(m, stmt->right);
    compiler_tuple_destr(m, destr, tuple_target);
}

/**
 * var (a, b, (c, d)) = operand
 * @param m
 * @param var_tuple_def
 * @return
 */
static void compiler_var_tuple_def_stmt(module_t *m, ast_var_tuple_def_stmt *var_tuple_def) {
    // 理论上只需要不停的 move 就行了
    lir_operand_t *tuple_target = compiler_expr(m, var_tuple_def->right);
    compiler_tuple_destr(m, var_tuple_def->tuple_destr, tuple_target);
}

/**
 * 这里不包含如 var a = 1 这样的 assign
 * a = b + 1 + 3
 * @param stmt
 * @return
 */
static void compiler_vardef(module_t *m, ast_vardef_stmt *stmt) {
    lir_operand_t *src = compiler_expr(m, stmt->right);
    lir_operand_t *dst = var_operand(m, stmt->var_decl.ident);

    OP_PUSH(lir_op_move(dst, src));
}

/**
 * a = 1 // left_target is lir_var_operand
 * a.b = 1 // left_target is lir_memory(base_address)
 * (a, b, (a.b, b[0])) = operand
 * @param c
 * @param stmt
 * @return
 */
static void compiler_assign(module_t *m, ast_assign_stmt *stmt) {
    ast_expr left = stmt->left;

    // map assign list[0] = 1
    if (left.assert_type == AST_EXPR_LIST_ACCESS) {

        return compiler_list_assign(m, stmt);
    }

    // set assign m["a"] = 2
    if (left.assert_type == AST_EXPR_MAP_ACCESS) {
        return compiler_map_assign(m, stmt);
    }

    if (left.assert_type == AST_EXPR_ENV_ACCESS) {
        return compiler_env_assign(m, stmt);
    }

    // struct assign p.name = "wei"
    if (left.assert_type == AST_EXPR_STRUCT_SELECT) {
        return compiler_struct_assign(m, stmt);
    }

    if (left.assert_type == AST_EXPR_TUPLE_DESTR) {
        return compiler_tuple_destr_stmt(m, stmt);
    }

    // a = 1
    if (left.assert_type == AST_EXPR_IDENT) {
        return compiler_ident_assign(m, stmt);
    }

    // tuple[0] = 1 x 禁止这种操作
    // set[0] = 1 x 同样进制这种操作，set 只能通过 add 来添加 key
    assertf(left.assert_type != AST_EXPR_TUPLE_ACCESS, "tuple dose not support item assign");
    assertf(false, "dose not support assign to %d", left.assert_type);
}

/**
 * 类似这样仅做了声明没有立即赋值，这里进行空赋值,从而能够保障有内存空间分配.
 * int a;
 * float b;
 * @param c
 * @param var_decl
 * @return
 */
static lir_operand_t *compiler_var_decl(module_t *m, ast_var_decl *var_decl) {
    lir_operand_t *operand = var_operand(m, var_decl->ident);
    OP_PUSH(lir_op_new(LIR_OPCODE_CLV, NULL, NULL, operand));
    return operand;
}


/**
 * rt_call get count => count
 * for_iterator:
 *  cmp_goto count == 0 to end for
 *  rt_call get key => key
 *  rt_call get value => value // 可选
 *  ....
 *  sub count, 1 => count
 *  goto for:
 * end_for_iterator:
 * @param c
 * @param for_in_stmt
 * @return
 */
static void compiler_for_iterator(module_t *m, ast_for_iterator_stmt *ast) {
    // map or list
    lir_operand_t *iterator_target = compiler_expr(m, ast->iterate);

    uint64_t rtype_index = ct_find_rtype_index(ast->iterate.type);

    // cursor 初始值
    lir_operand_t *cursor_operand = custom_var_operand(m, type_basic_new(TYPE_INT), ITERATOR_CURSOR);
    OP_PUSH(lir_op_move(cursor_operand, int_operand(-1)));

    // make label
    lir_op_t *for_start_label = lir_op_unique_label(m, FOR_ITERATOR_IDENT);
    lir_op_t *for_end_label = lir_op_unique_label(m, FOR_END_IDENT);

    // set label
    OP_PUSH(for_start_label);

    // key 和 value 需要进行一次初始化
    lir_operand_t *key_target = compiler_var_decl(m, &ast->key);
    lir_operand_t *key_ref = lea_operand_pointer(m, key_target);

    OP_PUSH(rt_call(
            RT_CALL_ITERATOR_NEXT_KEY,
            cursor_operand,
            4,
            iterator_target,
            int_operand(rtype_index),
            cursor_operand, // 当前的 cursor 的值
            key_ref));

    // 基于 key 已经可以判断迭代是否还有了，下面的 next value 直接根据 cursor_operand 取值即可
    OP_PUSH(lir_op_new(LIR_OPCODE_BEQ, int_operand(-1),
                       cursor_operand, lir_copy_label_operand(for_end_label->output)));

    // 添加 continue label
    OP_PUSH(lir_op_unique_label(m, FOR_CONTINUE_IDENT));

    // gen value
    if (ast->value) {
        lir_operand_t *value_target = compiler_var_decl(m, ast->value);
        lir_operand_t *value_ref = lea_operand_pointer(m, value_target);

        OP_PUSH(rt_call(
                RT_CALL_ITERATOR_VALUE, NULL, 4,
                iterator_target,
                int_operand(rtype_index),
                cursor_operand, value_ref));

    }
    // block
    compiler_block(m, ast->body);

    // goto for start
    OP_PUSH(lir_op_bal(for_start_label->output));

    OP_PUSH(for_end_label);
}


/**
 *
 * @param c
 * @param ast
 */
static void compiler_for_cond(module_t *m, ast_for_cond_stmt *ast) {
    lir_op_t *for_start = lir_op_unique_label(m, FOR_COND_IDENT);
    OP_PUSH(for_start);
    lir_operand_t *for_end_operand = label_operand(make_unique_ident(m, FOR_END_IDENT), true);

    lir_operand_t *condition_target = compiler_expr(m, ast->condition);
    lir_op_t *cmp_goto = lir_op_new(LIR_OPCODE_BEQ, bool_operand(false), condition_target, for_end_operand);

    OP_PUSH(cmp_goto);
    OP_PUSH(lir_op_unique_label(m, FOR_CONTINUE_IDENT));
    compiler_block(m, ast->body);

    // bal => goto
    OP_PUSH(lir_op_bal(for_start->output));

    OP_PUSH(lir_op_new(LIR_OPCODE_LABEL, NULL, NULL, for_end_operand));
}

static void compiler_for_tradition(module_t *m, ast_for_tradition_stmt *ast) {
    // init
    compiler_stmt(m, ast->init);

    lir_op_t *for_start = lir_op_unique_label(m, FOR_TRADITION_IDENT);
    OP_PUSH(for_start);

    lir_operand_t *for_end_operand = label_operand(make_unique_ident(m, FOR_END_IDENT), true);

    // cond -> for_end
    lir_operand_t *cond_target = compiler_expr(m, ast->cond);
    lir_op_t *cmp_goto = lir_op_new(LIR_OPCODE_BEQ, bool_operand(false), cond_target, for_end_operand);
    OP_PUSH(cmp_goto);

    // continue
    OP_PUSH(lir_op_unique_label(m, FOR_CONTINUE_IDENT));

    // block
    compiler_block(m, ast->body);

    // update
    compiler_stmt(m, ast->update);

    // bal for_start_label
    OP_PUSH(lir_op_bal(for_start->output));

    // label for_end
    OP_PUSH(lir_op_new(LIR_OPCODE_LABEL, NULL, NULL, for_end_operand));
}

static void compiler_return(module_t *m, ast_return_stmt *ast) {
    if (ast->expr != NULL) {
        assert(m->compiler_current->return_operand);
        lir_operand_t *src = compiler_expr(m, *ast->expr);
        OP_PUSH(lir_op_move(m->compiler_current->return_operand, src));

        // 用来做可达分析
        OP_PUSH(lir_op_new(LIR_OPCODE_RETURN, NULL, NULL, NULL));
    }

    OP_PUSH(lir_op_bal(label_operand(m->compiler_current->end_label, false)));
}

static void compiler_if(module_t *m, ast_if_stmt *if_stmt) {
    // 编译 condition
    lir_operand_t *condition_target = compiler_expr(m, if_stmt->condition);

    // 判断结果是否为 false, false 对应 else
    lir_operand_t *false_target = bool_operand(false);
    lir_operand_t *end_label_operand = label_operand(make_unique_ident(m, END_IF_IDENT), true);
    lir_operand_t *alternate_label_operand = label_operand(make_unique_ident(m, IF_ALTERNATE_IDENT), true);

    lir_op_t *cmp_goto;
    if (if_stmt->alternate->count == 0) {
        cmp_goto = lir_op_new(LIR_OPCODE_BEQ, false_target, condition_target,
                              lir_copy_label_operand(end_label_operand));
    } else {
        cmp_goto = lir_op_new(LIR_OPCODE_BEQ, false_target, condition_target,
                              lir_copy_label_operand(alternate_label_operand));
    }
    OP_PUSH(cmp_goto);
    OP_PUSH(lir_op_unique_label(m, IF_CONTINUE_IDENT));

    // 编译 consequent block
    compiler_block(m, if_stmt->consequent);
    OP_PUSH(lir_op_bal(end_label_operand));

    // 编译 alternate block
    if (if_stmt->alternate->count != 0) {
        OP_PUSH(lir_op_new(LIR_OPCODE_LABEL, NULL, NULL, alternate_label_operand));
        compiler_block(m, if_stmt->alternate);
    }

    // 追加 end_if 标签
    OP_PUSH(lir_op_new(LIR_OPCODE_LABEL, NULL, NULL, end_label_operand));
}

/**
 * - 函数参数使用 param var 存储,按约定从左到右(code.result 为 param, code.first 为实参)
 * - code.operand 模仿 phi body 弄成列表的形式！
 * @param c
 * @param expr
 * @return
 */
static lir_operand_t *compiler_call(module_t *m, ast_expr expr) {
    ast_call *call = expr.value;

    // global ident call optimize to 'call symbol'
    lir_operand_t *base_target = global_fn_symbol(m, call->left);
    if (!base_target) {
        base_target = compiler_expr(m, call->left);
    }

    lir_operand_t *return_target = NULL;
    // TYPE_VOID 是否有返回值
    if (call->return_type.kind != TYPE_VOID) {
        return_target = temp_var_operand(m, call->return_type);
    }

    slice_t *params = slice_new();
    type_fn_t *formal_fn = call->left.type.fn;
    assert(formal_fn);

    // call 所有的参数都丢到 params 变量中
    for (int i = 0; i < formal_fn->formal_types->length; ++i) {
        if (!formal_fn->rest || i < formal_fn->formal_types->length - 1) {
            ast_expr *actual_param = ct_list_value(call->actual_params, i);
            lir_operand_t *actual_param_operand = compiler_expr(m, *actual_param);
            slice_push(params, actual_param_operand);
            continue;
        }

        type_t *rest_type = ct_list_value(formal_fn->formal_types, i);
        assertf(rest_type->kind == TYPE_LIST, "rest param must list type");

        // actual 剩余的所有参数进行 compiler_expr 之后 都需要用一个数组收集起来，并写入到 target_operand 中
        lir_operand_t *rest_target = temp_var_operand(m, *rest_type);
        lir_operand_t *rtype_index = int_operand(ct_find_rtype_index(*rest_type));
        lir_operand_t *element_index = int_operand(ct_find_rtype_index(rest_type->list->element_type));
        lir_operand_t *capacity = int_operand(0);
        OP_PUSH(rt_call(RT_CALL_LIST_NEW, rest_target, 3, rtype_index, element_index, capacity));

        for (int j = i; j < call->actual_params->length; ++j) {
            ast_expr *actual_param = ct_list_value(call->actual_params, j);
            lir_operand_t *rest_actual_param = compiler_expr(m, *actual_param);

            // 将栈上的地址传递给 list 即可,不需要管栈中存储的值
            lir_operand_t *param_ref = lea_operand_pointer(m, rest_actual_param);
            OP_PUSH(rt_call(RT_CALL_LIST_PUSH, NULL, 2, rest_target, param_ref));
        }

        slice_push(params, rest_target);
        break;
    }

    // 使用一个 int_operand(0) 预留出 fn_runtime 所需的空间,这里不需要也不能判断出 target 是否有空间引用，所以统一预留
    // call 本身不需要做任何的调整
    slice_push(params, int_operand(0));


    // call base_target,params -> target
    lir_op_t *call_op = lir_op_new(LIR_OPCODE_CALL, base_target,
                                   operand_new(LIR_OPERAND_ACTUAL_PARAMS, params), return_target);

    // 触发 call 指令, 结果存储在 target 指令中
    OP_PUSH(call_op);

    // 判断 call op 是否存在 error, 如果存在 error 则不允许往下执行，
    // 而应该直接跳转到函数结束部分,这样 errort 就会继续向上传递
    // builtin call 不会抛出异常只是直接 panic， 所以不需要判断 has_errort
    if (!is_builtin_call(formal_fn->name) && !call->catch) {
        lir_operand_t *has_errort = temp_var_operand(m, type_basic_new(TYPE_BOOL));
        OP_PUSH(rt_call(RT_CALL_PROCESSOR_HAS_ERRORT, has_errort, 0));
        // 如果 eq = true 直接去到 fn_end, 但是此时并没有一个合适到 return 语句。

        // beq has_errort,true -> fn_end_label
        OP_PUSH(lir_op_new(LIR_OPCODE_BEQ,
                           bool_operand(true), has_errort,
                           label_operand(m->compiler_current->error_label, true)));

        m->compiler_current->to_error_label = true;
    }


    return return_target;
}


static lir_operand_t *compiler_binary(module_t *m, ast_expr expr) {
    ast_binary_expr *binary_expr = expr.value;

    lir_opcode_t type = ast_op_convert[binary_expr->operator];

    lir_operand_t *left_target = compiler_expr(m, binary_expr->left);
    lir_operand_t *right_target = compiler_expr(m, binary_expr->right);
    lir_operand_t *result_target = temp_var_operand(m, expr.type);

    OP_PUSH(lir_op_new(type, left_target, right_target, result_target));

    return result_target;
}

/**
 * - (1 + 1)
 * NOT first_param => result_target
 * @param c
 * @param expr
 * @param result_target
 * @return
 */
static lir_operand_t *compiler_unary(module_t *m, ast_expr expr) {
    ast_unary_expr *unary_expr = expr.value;
    lir_operand_t *target = temp_var_operand(m, expr.type);
    lir_operand_t *first = compiler_expr(m, unary_expr->operand);

    // 判断 first 的类型，如果是 imm 数，则直接对 int_value 取反，否则使用 lir minus  指令编译
    // !imm 为异常, parse 阶段已经识别了, [] 有可能
    if (unary_expr->operator == AST_OP_NEG && first->assert_type == LIR_OPERAND_IMM) {
        lir_imm_t *imm = first->value;
        assertf(is_number(imm->kind), "only number can neg operate");
        if (imm->kind == TYPE_INT) {
            imm->int_value = 0 - imm->int_value;
        } else {
            imm->float_value = 0 - imm->float_value;
        }
        // move 操作即可
        OP_PUSH(lir_op_move(target, first));
        return target;
    }

    //
    if (unary_expr->operator == AST_OP_NOT) {
        assert(unary_expr->operand.type.kind == TYPE_BOOL);
        if (first->assert_type == LIR_OPERAND_IMM) {
            lir_imm_t *imm = first->value;
            imm->bool_value = !imm->bool_value;
            OP_PUSH(lir_op_move(target, first));
            return target;
        }

        // bool not to bit xor  !true = xor $1,true
        OP_PUSH(lir_op_new(LIR_OPCODE_XOR, first, bool_operand(true), target));
        return target;
    }

    // &var
    if (unary_expr->operator == AST_OP_LA) {
        return lea_operand_pointer(m, first);
    }


    // neg source -> target
    assertf(unary_expr->operator != AST_OP_IA, "not support IA op");
    lir_opcode_t type = ast_op_convert[unary_expr->operator];
    lir_op_t *unary = lir_op_new(type, first, NULL, target);
    OP_PUSH(unary);

    return target;
}

/**
 * int a = list[0]
 * string s = list[1]
 */
static lir_operand_t *compiler_list_access(module_t *m, ast_expr expr) {
    ast_list_access_t *ast = expr.value;

    lir_operand_t *list_target = compiler_expr(m, ast->left);
    lir_operand_t *index_target = compiler_expr(m, ast->index);

    lir_operand_t *result = compiler_temp_var_operand(m, expr.type);
    // 读取 result 的指针地址，给到 access 进行写入
    lir_operand_t *result_ref = lea_operand_pointer(m, result);

    OP_PUSH(rt_call(RT_CALL_LIST_ACCESS, NULL,
                    3, list_target, index_target, result_ref));

    return result;
}

/**
 * origin [1, foo, bar(), car.done]
 * call runtime.make_list => t1
 * move 1 => t1[0]
 * move foo => t1[1]
 * move bar() => t1[2]
 * move car.done => t1[3]
 * move t1 => target
 * @param c
 * @param new_list
 * @param target
 * @return
 */
static lir_operand_t *compiler_list_new(module_t *m, ast_expr expr) {
    ast_list_new *ast = expr.value;

    lir_operand_t *list_target = temp_var_operand(m, expr.type);

    type_list_t *list_decl = expr.type.list;
    // call list_new
    lir_operand_t *rtype_index = int_operand(ct_find_rtype_index(expr.type));

    lir_operand_t *element_index = int_operand(ct_find_rtype_index(list_decl->element_type));

    lir_operand_t *capacity = int_operand(0);

    // 传递 list element type size 或者自己计算出来也行
    lir_op_t *call_op = rt_call(RT_CALL_LIST_NEW, list_target, 3,
                                rtype_index, element_index, capacity);
    OP_PUSH(call_op);

    // 值初始化 assign
    for (int i = 0; i < ast->values->length; ++i) {
        ast_expr *item_expr = ct_list_value(ast->values, i);
        lir_operand_t *value_ref = lea_operand_pointer(m, compiler_expr(m, *item_expr));


        OP_PUSH(rt_call(RT_CALL_LIST_PUSH, NULL, 2, list_target, value_ref));
    }

    return list_target;
}

/**
 * 1. 根据 c->env_name 得到 base_target   call GET_ENV
 * var a = b + 3 // 其中 b 是外部环境变量,需要改写成 GET_ENV
 * b = 12 + c  // 类似这样对外部变量的重新赋值操作，此时 b 的访问直接改成了
 * @param c
 * @param ast
 * @param target
 * @return
 */
static lir_operand_t *compiler_env_access(module_t *m, ast_expr expr) {
    ast_env_access *ast = expr.value;
    lir_operand_t *index = int_operand(ast->index);
    lir_operand_t *result = compiler_temp_var_operand(m, expr.type);
    lir_operand_t *dst_ref = lea_operand_pointer(m, result);

    uint64_t size = type_sizeof(expr.type);
    assertf(m->compiler_current->fn_runtime_operand, "have env access, must have fn_runtime_operand");

    OP_PUSH(rt_call(RT_CALL_ENV_ACCESS_REF, NULL,
                    4,
                    m->compiler_current->fn_runtime_operand,
                    index,
                    dst_ref,
                    int_operand(size)));

    return result;
}

/**
 * foo.bar
 * foo[0].bar
 * foo.bar.car
 * 证明非变量
 * @param c
 * @param ast
 * @param target
 * @return
 */
static lir_operand_t *compiler_map_access(module_t *m, ast_expr expr) {
    ast_map_access_t *ast = expr.value;


    // compiler base address left_target
    lir_operand_t *map_target = compiler_expr(m, ast->left);
    type_t type_map_decl = ast->left.type;

    // compiler key to temp var
    lir_operand_t *key_target_ref = lea_operand_pointer(m, compiler_expr(m, ast->key));
    lir_operand_t *value_target = compiler_temp_var_operand(m, type_map_decl.map->value_type);
    lir_operand_t *value_target_ref = lea_operand_pointer(m, value_target);

    // runtime get slot by temp var runtime.map_offset(base, "key")
    lir_op_t *call_op = rt_call(RT_CALL_MAP_ACCESS, NULL,
                                3, map_target, key_target_ref, value_target_ref);
    OP_PUSH(call_op);

    return value_target;
}


/**
 * @param c
 * @param ast
 * @param base_target
 * @return
 */
static lir_operand_t *compiler_set_new(module_t *m, ast_expr expr) {
    ast_set_new *ast = expr.value;
    type_t typedecl = expr.type;

    uint64_t rtype_index = ct_find_rtype_index(typedecl);
    uint64_t key_index = ct_find_rtype_index(typedecl.map->key_type);

    lir_operand_t *set_target = temp_var_operand(m, expr.type);
    lir_op_t *call_op = rt_call(RT_CALL_SET_NEW, set_target,
                                2, int_operand(rtype_index), int_operand(key_index));
    OP_PUSH(call_op);

    // 默认值初始化 rt_call map_assign
    for (int i = 0; i < ast->keys->length; ++i) {
        ast_map_element *element = ct_list_value(ast->keys, i);
        ast_expr key_expr = element->key;
        lir_operand_t *key_ref = lea_operand_pointer(m, compiler_expr(m, key_expr));


        call_op = rt_call(RT_CALL_SET_ADD, NULL, 2, set_target, key_ref);
        OP_PUSH(call_op);
    }

    return set_target;
}

/**
 * @param c
 * @param ast
 * @param base_target
 * @return
 */
static lir_operand_t *compiler_map_new(module_t *m, ast_expr expr) {
    ast_map_new *ast = expr.value;
    type_t map_type = expr.type;

    uint64_t map_rtype_index = ct_find_rtype_index(map_type);
    uint64_t key_index = ct_find_rtype_index(map_type.map->key_type);
    uint64_t value_index = ct_find_rtype_index(map_type.map->value_type);

    lir_operand_t *map_target = temp_var_operand(m, expr.type);
    lir_op_t *call_op = rt_call(RT_CALL_MAP_NEW, map_target,
                                3,
                                int_operand(map_rtype_index),
                                int_operand(key_index),
                                int_operand(value_index));
    OP_PUSH(call_op);

    // 默认值初始化 rt_call map_assign
    for (int i = 0; i < ast->elements->length; ++i) {
        ast_map_element *element = ct_list_value(ast->elements, i);
        ast_expr key_expr = element->key;
        ast_expr value_expr = element->value;
        lir_operand_t *key_ref = lea_operand_pointer(m, compiler_expr(m, key_expr));
        lir_operand_t *value_ref = lea_operand_pointer(m, compiler_expr(m, value_expr));

        call_op = rt_call(RT_CALL_MAP_ASSIGN, NULL, 3, map_target, key_ref, value_ref);
        OP_PUSH(call_op);
    }

    return map_target;
}

/**
 * mov [base+slot,n] => target
 * bar().baz
 * @param c
 * @param ast
 * @param target
 * @return
 */
static lir_operand_t *compiler_struct_select(module_t *m, ast_expr expr) {
    ast_struct_select_t *ast = expr.value;

    lir_operand_t *struct_target = compiler_expr(m, ast->left);
    type_t t = ast->left.type;
    uint64_t item_size = type_sizeof(ast->property->type);
    uint64_t offset = type_struct_offset(t.struct_, ast->key);

    lir_operand_t *dst = compiler_temp_var_operand(m, ast->property->type);
    lir_operand_t *dst_ref = lea_operand_pointer(m, dst);

    OP_PUSH(rt_call(RT_CALL_MEMORY_MOVE, NULL,
                    5,
                    dst_ref,
                    int_operand(0),
                    struct_target,
                    int_operand(offset),
                    int_operand(item_size)));

    return dst;
}

/**
 * mov [base+slot,n] => target
 * bar().baz
 * @param c
 * @param ast
 * @param target
 * @return
 */
static lir_operand_t *compiler_tuple_access(module_t *m, ast_expr expr) {
    ast_tuple_access_t *ast = expr.value;

    lir_operand_t *tuple_target = compiler_expr(m, ast->left);
    type_t t = ast->left.type;
    uint64_t item_size = type_sizeof(ast->element_type);
    uint64_t offset = type_tuple_offset(t.tuple, ast->index);

    lir_operand_t *dst = compiler_temp_var_operand(m, ast->element_type);
    lir_operand_t *dst_ref = lea_operand_pointer(m, dst);
    OP_PUSH(rt_call(RT_CALL_MEMORY_MOVE, NULL,
                    5,
                    dst_ref,
                    int_operand(0),
                    tuple_target,
                    int_operand(offset),
                    int_operand(item_size)));

    return dst;
}

/**
 * foo.bar = 1
 *
 * person baz = person {
 *  age = 100
 *  sex = true
 * }
 *
 * @param c
 * @param ast
 * @param target
 * @return
 */
static lir_operand_t *compiler_struct_new(module_t *m, ast_expr expr) {
    ast_struct_new_t *ast = expr.value;
    lir_operand_t *struct_target = temp_var_operand(m, expr.type);

    type_t type = ast->type;

    uint64_t rtype_index = ct_find_rtype_index(type);

    OP_PUSH(rt_call(RT_CALL_STRUCT_NEW, struct_target,
                    1, int_operand(rtype_index)));

    // 快速赋值,由于 struct 的相关属性都存储在 type 中，所以偏移量等值都需要在前端完成计算
    for (int i = 0; i < ast->properties->length; ++i) {
        struct_property_t *p = ct_list_value(ast->properties, i);

        // struct 的 key.key 是不允许使用表达式的, 计算偏移，进行 move
        uint64_t offset = type_struct_offset(type.struct_, p->key);
        uint64_t item_size = type_sizeof(p->type);

        assertf(p->right, "struct new property_expr value empty");

        ast_expr *property_expr = p->right;
        lir_operand_t *property_target = compiler_expr(m, *property_expr);
        lir_operand_t *src_ref = lea_operand_pointer(m, property_target);

        // move by item size
        OP_PUSH(rt_call(RT_CALL_MEMORY_MOVE, NULL,
                        5,
                        struct_target,
                        int_operand(offset),
                        src_ref,
                        int_operand(0),
                        int_operand(item_size)));
    }

    return struct_target;
}


/**
 * var a = (1, a, 1.25)
 * @param c
 * @param ast
 * @param target
 * @return
 */
static lir_operand_t *compiler_tuple_new(module_t *m, ast_expr expr) {
    ast_tuple_new *ast = expr.value;

    type_t typedecl = expr.type;
    uint64_t rtype_index = ct_find_rtype_index(typedecl);

    lir_operand_t *tuple_target = temp_var_operand(m, expr.type);
    OP_PUSH(rt_call(RT_CALL_TUPLE_NEW, tuple_target,
                    1, int_operand(rtype_index)));

    uint64_t offset = 0;
    for (int i = 0; i < ast->elements->length; ++i) {
        ast_expr *element = ct_list_value(ast->elements, i);

        uint64_t item_size = type_sizeof(element->type);
        // tuple 和 struct 一样需要对齐，不然没法做 gc_bits
        offset = align((int64_t) offset, (int64_t) item_size);

        // tuple_target 中包含到是一个执行堆区到地址，直接将该堆区到地址丢给 memory_move 即可
        // offset(var) var must assign reg
        lir_operand_t *src_ref = lea_operand_pointer(m, compiler_expr(m, *element));

        // move by item size
        OP_PUSH(rt_call(RT_CALL_MEMORY_MOVE, NULL,
                        5,
                        tuple_target,
                        int_operand(offset),
                        src_ref,
                        int_operand(0),
                        int_operand(item_size)));
        offset += item_size;
    }

    return tuple_target;
}

static lir_operand_t *compiler_type_convert(module_t *m, ast_expr expr) {
    ast_type_convert_t *convert = expr.value;
    lir_operand_t *input = compiler_expr(m, convert->operand);
    uint64_t input_rtype_index = ct_find_rtype_index(convert->operand.type);

    if (is_number(convert->target_type.kind) && is_number(convert->operand.type.kind)) {
        lir_operand_t *output = compiler_temp_var_operand(m, convert->target_type);
        lir_operand_t *output_rtype = int_operand(ct_find_rtype_index(convert->target_type));
        lir_operand_t *output_ref = lea_operand_pointer(m, output);
        lir_operand_t *input_ref = lea_operand_pointer(m, input);

        OP_PUSH(rt_call(RT_CALL_NUMBER_CASTING, NULL, 4,
                        int_operand(input_rtype_index), input_ref, output_rtype, output_ref));
        return output;
    }

    lir_operand_t *output = temp_var_operand(m, convert->target_type);
    if (convert->target_type.kind == TYPE_BOOL) {
        OP_PUSH(rt_call(RT_CALL_CONVERT_BOOL, output, 2, int_operand(input_rtype_index), input));
        return output;
    }
    if (convert->target_type.kind == TYPE_ANY) {
        lir_operand_t *input_ref = lea_operand_pointer(m, input);
        OP_PUSH(rt_call(RT_CALL_CONVERT_ANY, output, 2, int_operand(input_rtype_index), input_ref));
        return output;
    }
    assertf(false, "not support convert to type %s", type_kind_string[convert->target_type.kind]);
    exit(1);
}

static lir_operand_t *compiler_catch(module_t *m, ast_expr expr) {
    ast_catch *catch = expr.value;
    type_t tuple_type = expr.type;

    lir_operand_t *call_result_operand = compiler_expr(m, (ast_expr) {
            .assert_type = AST_CALL,
            .type = catch->call->return_type,
            .value = catch->call
    });


    // remove error by runtime processor
    symbol_t *symbol = symbol_table_get(ERRORT_TYPE_IDENT);
    ast_typedef_stmt *typedef_stmt = symbol->ast_value;
    assertf(typedef_stmt->type.status == REDUCTION_STATUS_DONE, "errort type not reduction");
    lir_operand_t *errort_operand = temp_var_operand(m, typedef_stmt->type);
    OP_PUSH(rt_call(RT_CALL_PROCESSOR_REMOVE_ERRORT, errort_operand, 0));

    // call 没有返回值，此时直接 remove errort 即可
    if (!call_result_operand) {
        return errort_operand;
    }


    // make tuple target return
    assertf(call_result_operand->assert_type == LIR_OPERAND_VAR, "compiler call result operand must lir var");
    ast_ident *call_result_ident = NEW(ast_ident);
    call_result_ident->literal = ((lir_var_t *) call_result_operand->value)->ident;
    ast_expr call_result_expr = {
            .assert_type= AST_EXPR_IDENT,
            .type = ((lir_var_t *) call_result_operand)->type,
            .value = call_result_ident
    };


    // temp error ident
    ast_ident *errort_ident = NEW(ast_ident);
    errort_ident->literal = ((lir_var_t *) errort_operand->value)->ident;
    ast_expr errort_expr = {
            .assert_type= AST_EXPR_IDENT,
            .type = ((lir_var_t *) errort_operand->value)->type,
            .value = errort_ident
    };

    // (call(), error_operand())
    ast_tuple_new *tuple = NEW(ast_tuple_new);
    tuple->elements = ct_list_new(sizeof(ast_expr));
    ct_list_push(tuple->elements, &call_result_expr);
    ct_list_push(tuple->elements, &errort_expr);

    return compiler_tuple_new(m, (ast_expr) {
            .type = tuple_type,
            .assert_type = AST_EXPR_TUPLE_NEW,
            .value = tuple
    });
}

/**
 * @param c
 * @param literal
 * @param target  default is empty
 * @return
 */
static lir_operand_t *compiler_literal(module_t *m, ast_expr expr) {
    ast_literal *literal = expr.value;

    switch (literal->kind) {
        case TYPE_STRING: {
            // 转换成 nature string 对象(基于 string_new), 转换的结果赋值给 target
            lir_operand_t *target = temp_var_operand(m, expr.type);
            lir_operand_t *imm_c_string_operand = string_operand(literal->value);
            lir_operand_t *imm_len_operand = int_operand(strlen(literal->value));
            lir_op_t *call_op = rt_call(
                    RT_CALL_STRING_NEW,
                    target,
                    2,
                    imm_c_string_operand,
                    imm_len_operand);
            OP_PUSH(call_op);
            return target;
        }
        case TYPE_RAW_STRING: {
            return string_operand(literal->value);
        }
        case TYPE_INT:
        case TYPE_INT64: {
            // literal 默认编译成 int 类型
            return int_operand(atoi(literal->value));
        }
        case TYPE_FLOAT:
        case TYPE_FLOAT64: {
            return float_operand(atof(literal->value));
        }
        case TYPE_BOOL: {
            bool bool_value = false;
            if (strcmp(literal->value, "true") == 0) {
                bool_value = true;
            }
            return bool_operand(bool_value);
        }

        default: {
            assertf(false, "line: %d, cannot compiler literal->kind", compiler_line);
        }
    }
    exit(1);
}


/**
 * fndef 到 body 已经编译完成并变成了 label, 此时不需要再递归到 fn body 内部,也不需要调整 m->compiler_current
 * 只需要将 fndef 到 env 写入到 fndef->name 对应到 envs 中即可, 返回值则返回函数到唯一 ident 即可
 *
 * fn_decl 允许在 stmt 或者 expr 中, 但是无论是在哪里声明，当前函数都可能会有两个 ident 需要处理
 * 1. fndef->closure_name，该 ident 作为一个 var 编译，其中存储了 runtime_fn_new
 * 2. fndef->symbol_name, 该 ident 作为一个 symbol fn 符号进行编译, 仅当 fndef->closure_name 为空时使用。
 * @param m
 * @param expr
 * @return
 */
static lir_operand_t *compiler_fn_decl(module_t *m, ast_expr expr) {
    // var a = fn() {} 类似此时的右值就是 fndef, 此时可以为 fn 创建对应的 closure 了
    ast_fndef_t *fndef = expr.value;

    // symbol label 不能使用 mov 在变量间自由的传递，所以这里将 symbol label 的 addr 加载出来返回
    lir_operand_t *fn_symbol_operand = symbol_label_operand(m, fndef->symbol_name);
    lir_operand_t *label_addr_operand = temp_var_operand(m, fndef->type);

    if (!fndef->closure_name) {
        if (!expr.target_type.kind) {
            return NULL; // 没有表达式需要接收值
        }

        OP_PUSH(lir_op_lea(label_addr_operand, fn_symbol_operand));
        return label_addr_operand;
    }
    assert(!str_equal(fndef->closure_name, ""));

    // 函数引用了外部的环境变量，所以需要编译成一个闭包
    // make envs
    lir_operand_t *length = int_operand(fndef->capture_exprs->length);
    // rt_call env_new(fndef->name, length)
    lir_operand_t *env_operand = temp_var_operand(m, type_basic_new(TYPE_INT64));
    OP_PUSH(rt_call(RT_CALL_ENV_NEW, env_operand, 1, length));

    slice_t *capture_vars = slice_new();
    for (int i = 0; i < fndef->capture_exprs->length; ++i) {
        ast_expr *item = ct_list_value(fndef->capture_exprs, i);
        // fndef 引用了当前环境的一些 ident, 需要在 ssa 中进行跟踪, ssa 完成后
        if (item->assert_type == AST_EXPR_IDENT) {
            char *ident = ((ast_ident *) item->value)->literal;
            slice_push(capture_vars, lir_var_new(m, ident));
        }

        //  加载 free var 在栈上的指针
        lir_operand_t *stack_addr_ref = lea_operand_pointer(m, compiler_expr(m, *item));
        // rt_call env_assign(fndef->name, index_operand lir_operand)
        OP_PUSH(rt_call(RT_CALL_ENV_ASSIGN, NULL, 4,
                        env_operand,
                        int_operand(ct_reflect_type(item->type).index),
                        int_operand(i),
                        stack_addr_ref));
    }

    // 记录引用关系, ssa 将会实时调整这些地方到值，一旦 ssa 完成这些 var 就有了唯一名称
    if (capture_vars->count > 0) {
        lir_operand_t *capture_operand = operand_new(LIR_OPERAND_VARS, capture_vars);
        OP_PUSH(lir_op_new(LIR_OPCODE_ENV_CAPTURE, capture_operand, NULL, NULL));
    }

    OP_PUSH(lir_op_lea(label_addr_operand, fn_symbol_operand));
    lir_operand_t *result = var_operand(m, fndef->closure_name);
    OP_PUSH(rt_call(RT_CALL_FN_NEW, result, 2, label_addr_operand, env_operand));

    return result;
}

static void compiler_throw(module_t *m, ast_throw_stmt *stmt) {
    // msg to errort
    symbol_t *symbol = symbol_table_get(ERRORT_TYPE_IDENT);
    ast_typedef_stmt *typedef_stmt = symbol->ast_value;
    assertf(typedef_stmt->type.status == REDUCTION_STATUS_DONE, "errort type not reduction");

    // 构建 struct new  结构
    ast_struct_new_t *errort_struct = NEW(ast_struct_new_t);
    errort_struct->type = typedef_stmt->type;
    errort_struct->properties = ct_list_new(sizeof(struct_property_t));
    struct_property_t property = {
            .type = type_basic_new(TYPE_STRING),
            .key = ERRORT_MSG_IDENT,
            .right = &stmt->error,
    };
    ct_list_push(errort_struct->properties, &property);

    // target 是一个 ptr, 指向了一段 memory_struct_t
    lir_operand_t *errort_target = compiler_struct_new(m, (ast_expr) {
            .type = errort_struct->type,
            .assert_type = AST_EXPR_STRUCT_NEW,
            .value = errort_struct
    });

    // attach errort to processor
    OP_PUSH(rt_call(RT_CALL_PROCESSOR_ATTACH_ERRORT, NULL, 1, errort_target));

    // 插入 return 标识(用来做 return check 的，check 完会清除的)
    OP_PUSH(lir_op_new(LIR_OPCODE_RETURN, NULL, NULL, NULL));

    // ret
    OP_PUSH(lir_op_bal(label_operand(m->compiler_current->end_label, false)));
}

static void compiler_stmt(module_t *m, ast_stmt *stmt) {
    switch (stmt->assert_type) {
        case AST_VAR_DECL: {
            compiler_var_decl(m, stmt->value);
            return;
        }
        case AST_STMT_VAR_DEF: {
            return compiler_vardef(m, stmt->value);
        }
        case AST_STMT_ASSIGN: {
            return compiler_assign(m, stmt->value);
        }
        case AST_STMT_VAR_TUPLE_DESTR: {
            return compiler_var_tuple_def_stmt(m, stmt->value);
        }
        case AST_STMT_IF: {
            return compiler_if(m, stmt->value);
        }
        case AST_STMT_FOR_ITERATOR: {
            return compiler_for_iterator(m, stmt->value);
        }
        case AST_STMT_FOR_COND: {
            return compiler_for_cond(m, stmt->value);
        }
        case AST_STMT_FOR_TRADITION: {
            return compiler_for_tradition(m, stmt->value);
        }
        case AST_FNDEF: {
            compiler_fn_decl(m, (ast_expr) {
                    .line = stmt->line,
                    .assert_type = stmt->assert_type,
                    .value = stmt->value,
                    .target_type = NULL
            });
            return;
        }
        case AST_CALL: {
            ast_fndef_t *fndef = stmt->value;
            // stmt 中都 call 都是没有返回值的
            compiler_call(m, (ast_expr) {
                    .line = stmt->line,
                    .assert_type = stmt->assert_type,
                    .type = type_basic_new(TYPE_FN),
                    .value = fndef,
            });
            return;
        }
        case AST_STMT_RETURN: {
            return compiler_return(m, stmt->value);
        }
        case AST_STMT_THROW: {
            return compiler_throw(m, stmt->value);
        }
        case AST_STMT_TYPEDEF: {
            return;
        }
        default: {
            assertf(false, "unknown stmt type=%d", stmt->assert_type);
        }
    }
}

compiler_expr_fn expr_fn_table[] = {
        [AST_EXPR_LITERAL] = compiler_literal,
        [AST_EXPR_IDENT] = compiler_ident,
        [AST_EXPR_ENV_ACCESS] = compiler_env_access,
        [AST_EXPR_BINARY] = compiler_binary,
        [AST_EXPR_UNARY] = compiler_unary,
        [AST_EXPR_LIST_NEW] = compiler_list_new,
        [AST_EXPR_LIST_ACCESS] = compiler_list_access,
        [AST_EXPR_MAP_NEW] = compiler_map_new,
        [AST_EXPR_MAP_ACCESS] = compiler_map_access,
        [AST_EXPR_STRUCT_NEW] = compiler_struct_new,
        [AST_EXPR_STRUCT_SELECT] = compiler_struct_select,
        [AST_EXPR_TUPLE_NEW] = compiler_tuple_new,
        [AST_EXPR_TUPLE_ACCESS] = compiler_tuple_access,
        [AST_EXPR_SET_NEW] = compiler_set_new,
        [AST_CALL] = compiler_call,
        [AST_FNDEF] = compiler_fn_decl,
        [AST_EXPR_CATCH] = compiler_catch,
        [AST_EXPR_TYPE_CONVERT] = compiler_type_convert,
};


static lir_operand_t *compiler_expr(module_t *m, ast_expr expr) {
    // 特殊处理
    compiler_expr_fn fn = expr_fn_table[expr.assert_type];
    assertf(fn, "ast right not support");

    lir_operand_t *operand = fn(m, expr);

    return operand;
}


static void compiler_block(module_t *m, slice_t *block) {
    for (int i = 0; i < block->count; ++i) {
        ast_stmt *stmt = block->take[i];
        compiler_line = stmt->line;
        m->compiler_line = stmt->line;
#ifdef DEBUG_COMPILER
        debug_stmt("COMPILER", *stmt);
#endif
        compiler_stmt(m, stmt);
    }
}


/**
 * 这里主要编译 fn param 和 body, 不编译名称与 env
 * @param m
 * @param fndef
 * @return
 */
static closure_t *compiler_fndef(module_t *m, ast_fndef_t *fndef) {
    // 创建 closure, 并写入到 m module 中
    closure_t *c = lir_closure_new(fndef);
    // 互相关联关系
    m->compiler_current = c;
    c->module = m;
    c->end_label = str_connect("end_", c->symbol_name);
    c->error_label = str_connect("error_", c->symbol_name);

    // label name 使用 symbol_name!
    OP_PUSH(lir_op_label(fndef->symbol_name, false));


    // 编译 fn param -> lir_var_t*
    slice_t *params = slice_new();
    for (int i = 0; i < fndef->formals->length; ++i) {
        ast_var_decl *var_decl = ct_list_value(fndef->formals, i);

        slice_push(params, lir_var_new(m, var_decl->ident));
    }

    // 和 compiler_fndef 不同，compiler_closure 是函数内部的空间中,添加的也是当前 fn 的形式参数
    // 当前 fn 的形式参数在 body 中都是可以随意调用的
    //if 包含 envs 则使用 custom_var_operand 注册一个临时变量，并加入到 LIR_OPCODE_FN_BEGIN 中
    if (fndef->closure_name) {
        // 直接使用 fn->closure_name 作为 runtime name?
        lir_operand_t *fn_runtime_operand = var_operand(m, fndef->closure_name);
        slice_push(params, fn_runtime_operand->value);
        c->fn_runtime_operand = fn_runtime_operand;
    }

    OP_PUSH(lir_op_result(LIR_OPCODE_FN_BEGIN, operand_new(LIR_OPERAND_FORMAL_PARAMS, params)));

    // 返回值处理
    if (fndef->return_type.kind != TYPE_VOID) {
        c->return_operand = custom_var_operand(m, fndef->return_type, "$result");
        // 初始化空值, 让 use-def 关系完整，避免 ssa 生成异常
        OP_PUSH(lir_op_new(LIR_OPCODE_CLV, NULL, NULL, c->return_operand));
    }

    compiler_block(m, fndef->body);

    if (c->to_error_label) {
        // bal end_label
        OP_PUSH(lir_op_bal(label_operand(c->end_label, true)));
        OP_PUSH(lir_op_label(c->error_label, true));
        OP_PUSH(lir_op_new(LIR_OPCODE_RETURN, NULL, NULL, NULL));
        // error handle
        OP_PUSH(lir_op_bal(label_operand(c->end_label, true)));
    }


    OP_PUSH(lir_op_label(c->end_label, true));

    if (fndef->be_capture_locals->count > 0) {
        lir_operand_t *capture_operand = operand_new(LIR_OPERAND_CLOSURE_VARS, slice_new());
        lir_op_t *op = lir_op_new(LIR_OPCODE_ENV_CLOSURE, capture_operand, NULL, NULL);
        OP_PUSH(op);
        c->closure_vars = op->first->value;
        c->closure_var_table = table_new();
    }

    // lower 的时候需要进行特殊的处理
    OP_PUSH(lir_op_new(LIR_OPCODE_FN_END, c->return_operand, NULL, NULL));

    return c;
}

/**
 * @param c
 * @param ast
 * @return
 */
void compiler(module_t *m) {
    for (int i = 0; i < m->ast_fndefs->count; ++i) {
        ast_fndef_t *fndef = m->ast_fndefs->take[i];
        closure_t *closure = compiler_fndef(m, fndef);
        slice_push(m->closures, closure);
    }
}

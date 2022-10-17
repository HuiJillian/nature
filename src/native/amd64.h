#ifndef NATURE_SRC_LIR_NATIVE_AMD64_H_
#define NATURE_SRC_LIR_NATIVE_AMD64_H_

#include "src/binary/opcode/amd64/asm.h"
#include "src/lir/lir.h"
#include "src/register/register.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef slice_t *(*amd64_native_fn)(closure_t *c, lir_op_t *op);

slice_t *amd64_native_fn_formal_params(closure_t *c);

slice_t *amd64_native_fn_begin(closure_t *c, lir_op_t *op);

slice_t *amd64_native_fn_end(closure_t *c, lir_op_t *op);

slice_t *amd64_native_closure(closure_t *c);

slice_t *amd64_native_block(closure_t *c, basic_block_t *block);

/**
 * 分发入口, 基于 code->code 做选择(包含 label code)
 * @param c
 * @param op
 * @return
 */
slice_t *amd64_native_op(closure_t *c, lir_op_t *op);

slice_t *amd64_native_label(closure_t *c, lir_op_t *op);

slice_t *amd64_native_call(closure_t *c, lir_op_t *op);

slice_t *amd64_native_return(closure_t *c, lir_op_t *op);

slice_t *amd64_native_bal(closure_t *c, lir_op_t *op);

slice_t *amd64_native_cmp_goto(closure_t *c, lir_op_t *op);

slice_t *amd64_native_add(closure_t *c, lir_op_t *op);

slice_t *amd64_native_sgt(closure_t *c, lir_op_t *op);

slice_t *amd64_native_mov(closure_t *c, lir_op_t *op);

slice_t *amd64_native_lea(closure_t *c, lir_op_t *op);


// 返回下一个可用的寄存器(属于一条指令的 fixed_reg)
reg_t *amd64_native_next_reg(uint8_t used[2], uint8_t size);

/**
 * 返回下一个可用参数，可能是寄存器参数也可能是内存偏移
 * @param count
 * @param asm_operand
 * @return
 */
reg_t *amd64_native_fn_next_reg_target(uint8_t used[2], type_base_t base);

// 只要返回了指令就有一个使用的寄存器的列表，已经使用的固定寄存器就不能重复使用
slice_t *amd64_native_operand_transform(lir_operand *operand,
                                       amd64_operand_t *asm_operand,
                                       uint8_t used[2]);

uint8_t amd64_formal_min_stack(uint8_t size);


#endif //NATURE_SRC_LIR_NATIVE_AMD64_H_

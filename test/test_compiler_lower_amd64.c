#include "test.h"
#include <stdio.h>
#include "runtime/type/string.h"
#include "runtime/type/type_debug.h"
#include "src/lib/list.h"
#include "src/lib/helper.h"
#include "src/lir/lir.h"
#include "src/assembler/amd64/asm.h"
#include "src/assembler/amd64/register.h"
#include "src/syntax/scanner.h"
#include "src/syntax/parser.h"
#include "src/semantic/analysis.h"
#include "src/semantic/infer.h"
#include "src/compiler.h"
#include "src/cfg.h"
#include "src/lower/amd64/amd64.h"
#include "src/assembler/amd64/register.h"
#include "src/assembler/amd64/opcode.h"
#include "src/assembler/elf/elf.h"

static void test_hello() {
    char *source = file_read("/home/vagrant/Code/nature/test/stubs/001_hello.n");
    // scanner
    list *token_list = scanner(source);
    // parser
    ast_block_stmt stmt_list = parser(token_list);
    // analysis
    ast_closure_decl closure_decl = analysis(stmt_list);
    // infer 类型检查和推导
    infer(&closure_decl);

    // compiler to lir
    compiler_closures closures = compiler(&closure_decl);

    // construct cfg
    for (int i = 0; i < closures.count; ++i) {
        closure *c = closures.list[i];
        // 构造 cfg
        cfg(c);

        // 构造 ssa
//    ssa(c);
#ifdef DEBUG_CFG
        debug_cfg(closures.list[i]);
#endif
    }

    amd64_register_init();
    opcode_init();
    amd64_lower_init();

    list *insts = list_new();
    for (int i = 0; i < closures.count; ++i) {
        closure *c = closures.list[i];
        list_append(insts, amd64_lower_closure(c));
    }

    elf_init("hello.n");
    //  数据段编译(直接从 lower 中取还是从全局变量中取? 后者)
    elf_var_decl_list_build(amd64_decl_list);
    // 代码段
    elf_text_inst_list_build(insts);
    elf_text_inst_list_second_build();
    elf_t elf = elf_new();
    // 编码成二进制
    uint64_t count;
    uint8_t *binary = elf_encoding(elf, &count);
    // 输出到文件
    elf_to_file(binary, count, "hello.o");
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hello),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
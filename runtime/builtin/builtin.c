#include "builtin.h"
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>

static char sprint_buf[1024];

static void builtin_print_operand(int arg_count, builtin_operand_t **operands) {
    for (int i = 0; i < arg_count; ++i) {
        builtin_operand_t *operand = operands[i];
        if (operand->type == TYPE_STRING) {
            string_t *s = operand->point_value;
            write(STDOUT_FILENO, string_addr(s), string_length(s));
            continue;
        }
        if (operand->type == TYPE_FLOAT) {
            int n = sprintf(sprint_buf, "%.5f", operand->float_value);
            write(STDOUT_FILENO, sprint_buf, n);
            continue;
        }
        if (operand->type == TYPE_INT) {
            int n = sprintf(sprint_buf, "%ld", operand->int_value);
            write(STDOUT_FILENO, sprint_buf, n);
            continue;
        }
        if (operand->type == TYPE_BOOL) {
            char *raw = "false";
            if (operand->bool_value) {
                raw = "true";
            }
            write(STDOUT_FILENO, raw, strlen(raw));
            continue;
        }

        write(STDOUT_FILENO, "operand type error\n", 20);
    }

}

void builtin_print(int arg_count, ...) {
    builtin_operand_t *operands[arg_count];
    va_list args;
    va_start(args, arg_count); // 初始化参数
    for (int i = 0; i < arg_count; ++i) {
        operands[i] = va_arg(args, builtin_operand_t*);
    }
    va_end(args);

    builtin_print_operand(arg_count, operands);
}

void builtin_println(int arg_count, ...) {
    builtin_operand_t *operands[arg_count];
    va_list args;
    va_start(args, arg_count); // 初始化参数
    for (int i = 0; i < arg_count; ++i) {
        operands[i] = va_arg(args, builtin_operand_t*);
    }
    va_end(args);

    builtin_print_operand(arg_count, operands);
    write(STDOUT_FILENO, "\n", 1);
}

builtin_operand_t *builtin_new_operand(type_base_t type, void *value) {
    builtin_operand_t *operand = malloc(sizeof(builtin_operand_t));
    operand->type = type;
    operand->point_value = value;
    return operand;
}

#include "builtin.h"
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "runtime/memory.h"

static char sprint_buf[1024];
static char *space = " ";

// type_any_t 是 nature 中的类型，在 c 中是认不出来的
// 这里按理来说应该填写 void*, 写 type_any_t 就是为了语义明确
static void print_arg(n_union_t *arg) {
    assertf(arg, "[runtime.print_arg] arg is null");
    DEBUGF("[runtime.print_arg] arg addr: %p", arg);
    assertf(arg->rtype, "[runtime.print_arg] arg->rtype is null");
    DEBUGF("[runtime.print_arg] arg->rtype: %p, arg->value: %lu", arg->rtype, arg->value.i64_value);

    DEBUGF("[runtime.print_arg] memory_any_t any_base=%p, kind=%s, value_i64_casting=%ld", arg,
           type_kind_str[arg->rtype->kind], arg->value.i64_value);

    if (arg->rtype->kind == TYPE_STRING) {
        // memory_string_t 在内存视角，应该是由 2 块内存组成，一个是字符个数，一个是指向的数据结构(同样是内存视角)
        n_string_t *s = arg->value.ptr_value; // 字符串的内存视角
        assertf(s, "[runtime.print_arg] string is null");

        if (s->length == 0) { // 什么也不需要输出
            DEBUGF("[runtime.print_arg] string length is 0 not write, ")
            return;
        }

        DEBUGF("[runtime.print_arg] string=%p, length=%lu, data=%s, data_str_len=%lu",
               s, s->length, s->data, s->length);
        VOID write(STDOUT_FILENO, s->data, s->length);
        return;
    }
    if (arg->rtype->kind == TYPE_FLOAT64 || arg->rtype->kind == TYPE_FLOAT) {
        int n = sprintf(sprint_buf, "%f", arg->value.f64_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_FLOAT32) {
        int n = sprintf(sprint_buf, "%f", arg->value.f32_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_CPTR) {
        int n = sprintf(sprint_buf, "%p", arg->value.ptr_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_UINT || arg->rtype->kind == TYPE_UINT64) {
        int n = sprintf(sprint_buf, "%lu", arg->value.u64_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_UINT32) {
        int n = sprintf(sprint_buf, "%u", arg->value.u32_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_UINT16) {
        int n = sprintf(sprint_buf, "%u", arg->value.u16_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_UINT8) {
        int n = sprintf(sprint_buf, "%u", arg->value.u8_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_INT || arg->rtype->kind == TYPE_INT64) {
        int n = sprintf(sprint_buf, "%ld", arg->value.i64_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_INT32) {
        int n = sprintf(sprint_buf, "%d", arg->value.i32_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_INT16) {
        int n = sprintf(sprint_buf, "%d", arg->value.i16_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_INT8) {
        int n = sprintf(sprint_buf, "%d", arg->value.i8_value);
        VOID write(STDOUT_FILENO, sprint_buf, n);
        return;
    }
    if (arg->rtype->kind == TYPE_BOOL) {
        char *raw = "false";
        if (arg->value.bool_value) {
            raw = "true";
        }
        VOID write(STDOUT_FILENO, raw, strlen(raw));
        return;
    }
    if (arg->rtype->kind == TYPE_NULL) {
        char *raw = "null";
        VOID write(STDOUT_FILENO, raw, strlen(raw));
        return;
    }

    assertf(false, "[print_arg] unsupported type=%s", type_kind_str[arg->rtype->kind]);
}

void print(n_vec_t *args, bool with_space) {
    // any_trans 将 int 转换成了堆中的一段数据，并将堆里面的其实地址返回了回去
    // 所以 args->data 是一个堆里面的地址，其指向的堆内存区域是 [any_start_ptr1, any_start_ptr2m, ...]
    addr_t base = (addr_t) args->data; // 把 data 中存储的值赋值给 p
    uint64_t element_size = rt_rtype_out_size(args->element_rtype_hash);
    DEBUGF("[runtime.print] memory_list_t base=%p,length=%lu, array_data_base=%lx, element_size=%lu",
           args, args->length, base, element_size);

    for (int i = 0; i < args->length; ++i) {
        addr_t p = base + (i * element_size);

        // 将 p 中存储的地址赋值给 a, 此时 a 中存储的是一个堆中的地址，其结构是 memory_any_t
        n_union_t **value = (void *) p;

        DEBUGF("[runtime.print] arg i=%d, p=0x%lx, p_value=%p ", i, p, *value);
        print_arg(*value);

        if (with_space && i < (args->length - 1)) {
            VOID write(STDOUT_FILENO, space, 1);
        }
    }
}

void println(n_vec_t *args) {
    print(args, true);
    VOID write(STDOUT_FILENO, "\n", 1);
}

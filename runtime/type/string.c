#include "string.h"
#include "array.h"

/**
 * na: string foo = "hello world"
 * 此时 foo 在 elf data section 中, raw_string 就是 section 中的一个地址
 * 为了更多灵活性, 牺牲一个 byte 的空间, 为字符串的结尾添加 \0 结尾?
 * @param raw_string
 * @param length
 * @return
 */
n_string_t *string_new(void *raw_string, uint64_t length) {
    DEBUGF("[string_new] raw_string=%s, length=%lu", (char *) raw_string, length);

    // byte 数组，先手动创建一个简单类型
    rtype_t *element_rtype = gc_rtype(TYPE_UINT8, 0);
    n_array_t *array_data = NULL;
    rtype_t *string_rtype;
    // 如果是空字符串则不需要申请 data 部分的内存空间
    if (length > 0) {
        array_data = rt_array_new(element_rtype, length + 1);
        array_data[length] = '\0';

        // 创建 memory_string_t 类型，并转换成 rtype 进行 堆内存申请
        string_rtype = gc_rtype(TYPE_STRING, 2, TYPE_GC_SCAN, TYPE_GC_NOSCAN);
    } else {
        // 空字符串特殊处理
        string_rtype = gc_rtype(TYPE_STRING, 2, TYPE_GC_NOSCAN, TYPE_GC_NOSCAN);
    }

    DEBUGF("[string_new] rtype gc_bits=%s", bitmap_to_str(string_rtype->gc_bits, 2));
    n_string_t *string_data = runtime_malloc(string_rtype->size, string_rtype);
    string_data->data = array_data;
    string_data->length = length;
    memmove(string_data->data, raw_string, length);

    DEBUGF("[string_new] success, string_data=%p", string_data);
    return string_data;
}

char *string_to_c_string_ref(n_string_t *n_str) {
    DEBUGF("[runtime.string_to_c_string_ref] length=%lu, data=%p, last is 0?  %d", n_str->length, n_str->data,
           n_str->data[n_str->length] == '\0');
    return (char *) n_str->data;

}

char *string_to_c_string(n_string_t *n_str) {
    DEBUGF("[runtime.string_to_c_string] length=%lu, data=%p, last is 0?  %d", n_str->length, n_str->data,
           n_str->data[n_str->length] == '\0');

    char *c_str = mallocz(n_str->length + 1);

    memmove(c_str, n_str->data, n_str->length + 1);

    return c_str;
}

#ifndef NATURE_BITMAP_H
#define NATURE_BITMAP_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


typedef struct {
    uint8_t *bits; // 每一个元素有 8 bit
    uint64_t size; // 位图的大小, 单位 bit
} bitmap_t;

bitmap_t *bitmap_new(uint64_t size);

void bitmap_free(bitmap_t *b);

/**
 * index 从 0 开始计数
 * @param bits
 * @param index
 */
void bitmap_set(uint8_t *bits, uint64_t index);

/**
 * index > size / uint8_t 时，自动进行扩容
 * @param b
 * @param index
 */
void bitmap_grow_set(bitmap_t *b, uint64_t index, bool test);

void bitmap_clear(uint8_t *bits, uint64_t index);

bool bitmap_test(uint8_t *bits, uint64_t index);

int bitmap_set_count(bitmap_t *b);

char *bitmap_to_str(uint8_t *bits, uint64_t count);

#endif //NATURE_BITMAP_H

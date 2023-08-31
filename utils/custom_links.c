#include "custom_links.h"

symdef_t *rt_symdef_ptr;

fndef_t *rt_fndef_ptr;

table_t *rt_rtype_table;

// - symdef
uint64_t ct_symdef_size; // 数量
uint8_t *ct_symdef_data; // 序列化后的 data 大小
uint64_t ct_symdef_count;
symdef_t *ct_symdef_list;

// - fndef
uint64_t ct_fndef_size;
uint8_t *ct_fndef_data;
uint64_t ct_fndef_count;
fndef_t *ct_fndef_list;


// - rtype
uint64_t ct_rtype_count; // 从 list 中提取而来
uint8_t *ct_rtype_data;
uint64_t ct_rtype_size; // rtype + gc_bits + element_kinds 的总数据量大小, sh_size 预申请需要该值，已经在 reflect_type 时计算完毕
list_t *ct_rtype_list;
table_t *ct_rtype_table; // 避免 rtype_list 重复写入

#include "list.h"

list *list_new() {
  list *l = malloc(sizeof(list));
  l->count = 0;

  list_node *empty = list_new_node();
  l->front = empty;
  l->rear = empty;

  return l;
}

// 尾部 push
void list_push(list *l, void *value) {
  list_node *empty = list_new_node();

  l->rear->value = value;
  l->rear->next = empty;

  l->rear = l->rear->next;
  l->count++;
}

// 头部 pop
void *list_pop(list *l) {
  if (l->count == 0) {
    return NULL; // null 表示队列为空
  }

  list_node *temp = l->front;
  void *value = temp->value;

  l->front = temp->next;
  l->count--;
  free(temp);

  return value;
}
list_node *list_new_node() {
  list_node *node = malloc(sizeof(list_node));
  node->value = NULL;
  node->next = NULL;
  return node;
}

bool list_empty(list *l) {
  if (l->count == 0) {
    return true;
  }
  return false;
}




#include <stdio.h>
#include <stdint.h>

typedef void idl_tree_t;

int32_t generate(idl_tree_t *tree, const char *str)
{
  fprintf(stderr, "From %s: %s\n", __FILE__, str);
  return 0;
}

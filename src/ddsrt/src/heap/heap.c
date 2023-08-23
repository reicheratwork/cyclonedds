#include <stdlib.h>
#include <assert.h>

#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/dynlib.h"

typedef struct {
  dds_heap_ops_t ops;
  ddsrt_dynlib_t handle;
} heap_container_t;

static heap_container_t heap;

dds_return_t ddsrt_heap_init(const char *filename, const char *config)
{
  dds_return_t ret = DDS_RETCODE_OK;
  load_heap_lib *init_func = NULL;
  void *addr = &init_func;
  heap.handle = NULL;
  if (filename)
  {
    if ((ret = ddsrt_dlopen(filename, false, &heap.handle)) != DDS_RETCODE_OK)
      return ret;
    else if (!heap.handle)
      return DDS_RETCODE_BAD_PARAMETER;
    else if ((ret = ddsrt_dlsym (heap.handle, "init", addr)) != DDS_RETCODE_OK)
      return ret;
    else if (!init_func) {
      ddsrt_dlclose(heap.handle);
      return DDS_RETCODE_UNSUPPORTED;
    } else {
      if ((ret = init_func(config, &heap.ops)) != DDS_RETCODE_OK)
        ddsrt_dlclose(heap.handle);
      return ret;
    }
  } else {
    return ddsrt_heap_init_impl(&heap.ops);
  }
}

dds_return_t ddsrt_heap_fini(void)
{
  dds_return_t ret = heap.ops.fini();
  if (ret != DDS_RETCODE_OK)
    return ret;
  else if (heap.handle)
    return ddsrt_dlclose(heap.handle);
  else
    return DDS_RETCODE_OK;
}

void* ddsrt_malloc_s (size_t size) {
  return heap.ops.malloc(size);
}

void* ddsrt_malloc (size_t size) {
  void *ptr = NULL;

  if ((ptr = ddsrt_malloc_s(size ? size : 1)) == NULL) {
    abort();
  }

  return ptr;
}

void* ddsrt_calloc (size_t count, size_t size) {
  void *ptr = NULL;

  if (count == 0 || size == 0) {
    count = size = 1;
  }
  if ((ptr = ddsrt_calloc_s(count, size)) == NULL) {
    abort();
  }

  return ptr;
}

void* ddsrt_calloc_s (size_t count, size_t size) {
  return heap.ops.calloc(count, size);
}

void* ddsrt_realloc (void* memblk, size_t size) {
  void *ptr = NULL;

  if ((ptr = ddsrt_realloc_s(memblk, size)) == NULL) {
    abort();
  }

  return ptr;
}

void* ddsrt_realloc_s (void* memblk, size_t size) {
  return heap.ops.realloc(memblk, size ? size : 1);
}

void ddsrt_free (void* memblk) {
  if (memblk)
    heap.ops.free(memblk);
}

bool ddsrt_heap_state(dds_heap_state_t newstate)
{
  if (heap.ops.state)
    return heap.ops.state(newstate);

  return true;
}

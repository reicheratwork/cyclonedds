/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "idl/stream.h"

idl_retcode_t
idl_open_file(const char *path, idl_stream_t **stmptr)
{
  idl_file_stream_t *stm;

  assert(path);
  assert(stmptr);

  if (!(stm = malloc(sizeof(*stm)))) {
    return IDL_RETCODE_NO_MEMORY;
  }
  stm->stream.type = IDL_FILE_STREAM;
  if (!(stm->handle = fopen(path, "wb"))) {
    free(stm);
    return IDL_RETCODE_NO_MEMORY;
  }
  *stmptr = (idl_stream_t *)stm;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_open_memory(idl_stream_t **stmptr)
{
  idl_memory_stream_t *stm;

  assert(stmptr);

  if (!(stm = malloc(sizeof(*stm))))
    return IDL_RETCODE_NO_MEMORY;
  stm->stream.type = IDL_MEMORY_STREAM;
  stm->buffer.size = stm->buffer.used = 0;
  stm->buffer.data = NULL;
  *stmptr = (idl_stream_t *)stm;
  return IDL_RETCODE_OK;
}

void
idl_close(idl_stream_t *stm)
{
  if (stm) {
    if (stm->type == IDL_FILE_STREAM) {
      idl_file_stream_t *fstm = (idl_file_stream_t *)stm;
      assert(stm->type == IDL_FILE_STREAM);
      if (fstm->handle)
        fclose(fstm->handle);
      free(fstm);
    } else {
      idl_memory_stream_t *mstm = (idl_memory_stream_t *)stm;
      assert(stm->type == IDL_MEMORY_STREAM);
      if (mstm->buffer.data)
        free(mstm->buffer.data);
      free(mstm);
    }
  }
}

#define CHUNK (1024)

idl_retcode_t
idl_printf(idl_stream_t *stm, const char *fmt, ...)
{
  va_list ap;
  idl_retcode_t ret;

  assert(stm);
  assert(fmt);

  va_start(ap, fmt);
  ret = idl_vprintf(stm, fmt, ap);
  va_end(ap);

  return ret;
}

idl_retcode_t
idl_vprintf(idl_stream_t *stm, const char *fmt, va_list ap)
{
  int cnt;

  assert(stm);
  assert(fmt);

  if (stm->type == IDL_FILE_STREAM) {
    idl_file_stream_t *fstm = (idl_file_stream_t *)stm;
    cnt = vfprintf(fstm->handle, fmt, ap);
    if (cnt < 0) {
      if (errno == ENOSPC)
        return IDL_RETCODE_NO_SPACE;
      else
        return IDL_RETCODE_BAD_FORMAT;
    }
  } else {
    va_list aq;
    idl_memory_stream_t *mstm = (idl_memory_stream_t *)stm;
    assert(stm->type == IDL_MEMORY_STREAM);
    assert(mstm->buffer.used <= mstm->buffer.size);
    /* try to write to memory buffer, enough space may be available */
    va_copy(aq, ap);
    cnt = vsnprintf(
      mstm->buffer.data + mstm->buffer.used,
      (mstm->buffer.size - mstm->buffer.used) + 1,
      fmt, aq);
    va_end(aq);
    if (cnt < 0)
      return IDL_RETCODE_BAD_FORMAT;
    /* expand buffer if necessary */
    if (mstm->buffer.size - mstm->buffer.used < (size_t)cnt) {
      size_t sz = mstm->buffer.size + (size_t)(((cnt / CHUNK) + 1) * CHUNK);
      char *buf = realloc(mstm->buffer.data, sz + 2);
      if (!buf)
        return IDL_RETCODE_NO_MEMORY;
      /* update buffer */
      mstm->buffer.size = sz;
      mstm->buffer.data = buf;
      /* write to memory buffer */
      cnt = vsnprintf(
        mstm->buffer.data + mstm->buffer.used,
        (mstm->buffer.size - mstm->buffer.used) + 1,
        fmt, ap);
      assert(cnt >= 0);
    }
    assert(mstm->buffer.size - mstm->buffer.used > (size_t)cnt);
    mstm->buffer.used += (size_t)cnt;
    mstm->buffer.data[mstm->buffer.used] = '\0';
  }

  return cnt;
}

idl_retcode_t
idl_puts(idl_stream_t *stm, const char *str)
{
  size_t len;

  assert(stm);
  assert(str);

  len = strlen(str);
  if (stm->type == IDL_FILE_STREAM) {
    idl_file_stream_t *fstm = (idl_file_stream_t *)stm;
    if (fputs(str, fstm->handle) == EOF)
      return IDL_RETCODE_NO_SPACE;
  } else {
    idl_memory_stream_t *mstm = (idl_memory_stream_t *)stm;
    assert(stm->type == IDL_MEMORY_STREAM);
    /* expand buffer if necessary */
    if (mstm->buffer.size - mstm->buffer.used <= len) {
      size_t sz = mstm->buffer.size + (((len / CHUNK) + 1) * CHUNK);
      char *buf = realloc(mstm->buffer.data, sz + 2);
      if (!buf)
        return IDL_RETCODE_NO_MEMORY;
      /* update buffer */
      mstm->buffer.size = sz;
      mstm->buffer.data = buf;
    }
    memmove(mstm->buffer.data, str, len);
    mstm->buffer.used += len;
    mstm->buffer.data[mstm->buffer.used] = '\0';
  }

  return (idl_retcode_t)len;
}

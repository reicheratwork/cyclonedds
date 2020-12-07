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
#ifndef IDL_STREAM_H
#define IDL_STREAM_H

#include <stdarg.h>
#include <stdio.h>

#include "idl/export.h"
#include "idl/retcode.h"

typedef struct idl_buffer idl_buffer_t;
struct idl_buffer {
  char *data;
  size_t size; /**< total number of bytes available */
  size_t used; /**< number of bytes used */
};

typedef struct idl_stream idl_stream_t;
struct idl_stream {
  enum {
    IDL_FILE_STREAM,
    IDL_MEMORY_STREAM
  } type;
};

typedef struct idl_file_stream idl_file_stream_t;
struct idl_file_stream {
  idl_stream_t stream;
  FILE *handle;
};

typedef struct idl_memory_stream idl_memory_stream_t;
struct idl_memory_stream {
  idl_stream_t stream;
  idl_buffer_t buffer;
};

IDL_EXPORT idl_retcode_t
idl_open_file(const char *file, idl_stream_t **stmptr);

IDL_EXPORT idl_retcode_t
idl_open_memory(idl_stream_t **stmptr);

IDL_EXPORT void
idl_close(idl_stream_t *stm);

IDL_EXPORT idl_retcode_t
idl_printf(idl_stream_t *stm, const char *fmt, ...);

IDL_EXPORT idl_retcode_t
idl_vprintf(idl_stream_t *stm, const char *fmt, va_list ap);

IDL_EXPORT idl_retcode_t
idl_puts(idl_stream_t *stm, const char *str);

#endif /* IDL_STREAM_H */

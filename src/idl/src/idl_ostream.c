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

#include "idl/idl_ostream.h"

#include <string.h>
#include <stdarg.h>

struct idl_ostream
{
  FILE* _file;
  idl_buffer_t _buf;
};

idl_ostream_t* create_idl_ostream(FILE* file)
{
  idl_ostream_t* ptr = malloc(sizeof(idl_ostream_t));

  ptr->_buf.data = malloc(IDL_OSTREAM_BUFFER_INCR);
  memset(ptr->_buf.data, 0x0, IDL_OSTREAM_BUFFER_INCR);
  ptr->_buf.size = IDL_OSTREAM_BUFFER_INCR;
  ptr->_buf.used = 0;
  ptr->_file = file;

  return ptr;
}

const char* get_ostream_buffer(const idl_ostream_t* str)
{
  return str->_buf.data;
}

const size_t get_ostream_buffer_size(const idl_ostream_t* str)
{
  return str->_buf.size;
}

const size_t get_ostream_buffer_position(const idl_ostream_t* str)
{
  return str->_buf.used;
}

FILE* get_ostream_file(idl_ostream_t* str)
{
  return str->_file;
}

void destruct_idl_ostream(idl_ostream_t* ostr)
{
  if (ostr)
  {
    if (ostr->_buf.data)
      free(ostr->_buf.data);
    free(ostr);
  }
}

void format_ostream(idl_ostream_t* ostr, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  while (1)
  {
    int wb = vsnprintf(ostr->_buf.data + ostr->_buf.used,
                       ostr->_buf.size - ostr->_buf.used,
                       fmt,
                       args);
    if (wb < 0)
      //a formatting error occurred
      return;
    size_t writtenbytes = (size_t)wb;
    if (writtenbytes < ostr->_buf.size - ostr->_buf.used)
    {
      ostr->_buf.used += writtenbytes;
      break;
    }
    char* temp = realloc(ostr->_buf.data, ostr->_buf.size + IDL_OSTREAM_BUFFER_INCR);
    if (temp)
    {
      ostr->_buf.size += IDL_OSTREAM_BUFFER_INCR;
      ostr->_buf.data = temp;
    }
    else
    {
      printf("out of memory!\n");
      break;
    }
  }

  va_end(args);
}

size_t transfer_ostream_buffer(idl_ostream_t* from, idl_ostream_t* to)
{
  if (from == NULL || to == NULL)
    return 0;

  format_ostream(to, from->_buf.data);

  size_t returnval = from->_buf.used;
  from->_buf.data[0] = 0x0;
  from->_buf.used = 0;

  return returnval;
}

size_t flush_ostream(idl_ostream_t* str)
{
  if (str->_file)
    fprintf(str->_file, "%s", str->_buf.data);

  size_t returnval = str->_buf.used;
  str->_buf.used = 0;

  return returnval;
}

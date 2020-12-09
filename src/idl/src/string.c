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

/*
 * Locale-independent C runtime functions, like strtoull_l and strtold_l, are
 * available on modern operating systems albeit with some quirks.
 *
 * Linux exports newlocale and freelocale from locale.h if _GNU_SOURCE is
 * defined. strtoull_l and strtold_l are exported from stdlib.h, again if
 * _GNU_SOURCE is defined.
 *
 * FreeBSD and macOS export newlocale and freelocale from xlocale.h and
 * export strtoull_l and strtold_l from xlocale.h if stdlib.h is included
 * before.
 *
 * Windows exports _create_locale and _free_locale from locale.h and exports
 * _strtoull_l and _strtold_l from stdlib.h.
 */
#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined _MSC_VER
# include <locale.h>
typedef _locale_t locale_t;
#else
# include <pthread.h>
# include <strings.h>
# if __APPLE__ || __FreeBSD__
#   include <xlocale.h>
# else
#   include <locale.h>
# endif
#endif

#include "idl/string.h"

static locale_t posix_locale(void);

int idl_strcasecmp(const char *s1, const char *s2)
{
  assert(s1);
  assert(s2);
#if _WIN32
  return _stricmp_l(s1, s2, posix_locale());
#else
  return strcasecmp_l(s1, s2, posix_locale());
#endif
}

int idl_strncasecmp(const char *s1, const char *s2, size_t n)
{
  assert(s1);
  assert(s2);
#if _WIN32
  return _strnicmp_l(s1, s2, n, posix_locale());
#else
  return strncasecmp_l(s1, s2, n, posix_locale());
#endif
}

char *idl_strdup(const char *str)
{
#if _WIN32
  return _strdup(str);
#else
  return strdup(str);
#endif
}

char *idl_strndup(const char *str, size_t len)
{
  char *s;
  size_t n;
  for (n=0; n < len && str[n]; n++) ;
  assert(n <= len);
  if (!(s = malloc(n + 1)))
    return NULL;
  memmove(s, str, n);
  s[n] = '\0';
  return s;
}

int
idl_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
#if _WIN32
__pragma(warning(push))
__pragma(warning(disable: 4996))
  return _vsnprintf_l(str, size, fmt, posix_locale(), ap);
__pragma(warning(pop))
#elif __APPLE__ || __FreeBSD__
  return vsnprintf_l(str, size, posix_locale(), fmt, ap);
#else
  int ret;
  locale_t loc, posixloc = posix_locale();
  loc = uselocale(posixloc);
  ret = vsnprintf(str, size, fmt, ap);
  loc = uselocale(loc);
  assert(loc == posixloc);
  return ret;
#endif
}

int
idl_snprintf(char *str, size_t size, const char *fmt, ...)
{
  int ret;
  va_list ap;

  va_start(ap, fmt);
  ret = idl_vsnprintf(str, size, fmt, ap);
  va_end(ap);
  return ret;
}

int
idl_asprintf(
  char **strp,
  const char *fmt,
  ...)
{
  int ret;
  unsigned int len;
  char buf[1] = { '\0' };
  char *str = NULL;
  va_list ap1, ap2;

  assert(strp != NULL);
  assert(fmt != NULL);

  va_start(ap1, fmt);
  va_copy(ap2, ap1); /* va_list cannot be reused */

  if ((ret = idl_vsnprintf(buf, sizeof(buf), fmt, ap1)) >= 0) {
    len = (unsigned int)ret; /* +1 for null byte */
    if ((str = malloc(len + 1)) == NULL) {
      ret = -1;
    } else if ((ret = idl_vsnprintf(str, len + 1, fmt, ap2)) >= 0) {
      assert(((unsigned int)ret) == len);
      *strp = str;
    } else {
      free(str);
    }
  }

  va_end(ap1);
  va_end(ap2);

  return ret;
}

int
idl_vasprintf(
  char **strp,
  const char *fmt,
  va_list ap)
{
  int ret;
  unsigned int len;
  char buf[1] = { '\0' };
  char *str = NULL;
  va_list ap2;

  assert(strp != NULL);
  assert(fmt != NULL);

  va_copy(ap2, ap); /* va_list cannot be reused */

  if ((ret = idl_vsnprintf(buf, sizeof(buf), fmt, ap)) >= 0) {
    len = (unsigned int)ret;
    if ((str = malloc(len + 1)) == NULL) {
      ret = -1;
    } else if ((ret = idl_vsnprintf(str, len + 1, fmt, ap2)) >= 0) {
      assert(((unsigned int)ret) == len);
      *strp = str;
    } else {
      free(str);
    }
  }

  va_end(ap2);

  return ret;
}

unsigned long long idl_strtoull(const char *str, char **endptr, int base)
{
  assert(str);
  assert(base >= 0 && base <= 36);
#if _WIN32
  return _strtoull_l(str, endptr, base, posix_locale());
#else
  return strtoull_l(str, endptr, base, posix_locale());
#endif
}

long double idl_strtold(const char *str, char **endptr)
{
  assert(str);
#if _WIN32
  return _strtold_l(str, endptr, posix_locale());
#else
  return strtold_l(str, endptr, posix_locale());
#endif
}

char *idl_strtok_r(char *str, const char *delim, char **saveptr)
{
#if _WIN32
  return strtok_s(str, delim, saveptr);
#else
  return strtok_r(str, delim, saveptr);
#endif
}

#if defined _WIN32
static __declspec(thread) locale_t locale = NULL;

void WINAPI
idl_cdtor(PVOID handle, DWORD reason, PVOID reserved)
{
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      /* fall through */
    case DLL_THREAD_ATTACH:
      locale = _create_locale(LC_ALL, "C");
      break;
    case DLL_THREAD_DETACH:
      /* fall through */
    case DLL_PROCESS_DETACH:
      _free_locale(locale);
      locale = NULL;
      break;
    default:
      break;
  }
}

#if defined _WIN64
  #pragma comment (linker, "/INCLUDE:_tls_used")
  #pragma comment (linker, "/INCLUDE:tls_callback_func")
  #pragma const_seg(".CRT$XLZ")
  EXTERN_C const PIMAGE_TLS_CALLBACK tls_callback_func = idl_cdtor;
  #pragma const_seg()
#else
  #pragma comment (linker, "/INCLUDE:__tls_used")
  #pragma comment (linker, "/INCLUDE:_tls_callback_func")
  #pragma data_seg(".CRT$XLZ")
  EXTERN_C PIMAGE_TLS_CALLBACK tls_callback_func = idl_cdtor;
  #pragma data_seg()
#endif /* _WIN64 */
static locale_t posix_locale(void)
{
  return locale;
}
#else /* _WIN32 */
static pthread_key_t key;
static pthread_once_t once = PTHREAD_ONCE_INIT;

static void free_locale(void *ptr)
{
  freelocale((locale_t)ptr);
}

static void make_key(void)
{
  (void)pthread_key_create(&key, free_locale);
}

static locale_t posix_locale(void)
{
  locale_t locale;
  (void)pthread_once(&once, make_key);
  if ((locale = pthread_getspecific(key)))
    return locale;
#if __APPLE__ || __FreeBSD__
  locale = newlocale(LC_ALL_MASK, NULL, NULL);
#else
  locale = newlocale(LC_ALL, "C", (locale_t)0);
#endif
  pthread_setspecific(key, locale);
  return locale;
}
#endif /* _WIN32 */

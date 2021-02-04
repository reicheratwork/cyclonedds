/*
 * Copyright(c) 2019 Jeroen Koekkoek
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
#if HAVE_GETOPT_H
# include <getopt.h>
#else
# include "getopt.h"
#endif
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl/tree.h"
#include "idl/string.h"
#include "idl/processor.h"
#include "idl/file.h"

#include "mcpp_lib.h"
#include "mcpp_out.h"

#include "plugin.h"
#include "options.h"

#define IDLC_PREPROCESS (1u<<0)
#define IDLC_COMPILE (1u<<1)
#define IDLC_DEBUG_PREPROCESSOR (1u<<2)
#define IDLC_DEBUG_SCANNER (1u<<3)
#define IDLC_DEBUG_PARSER (1u<<4)
#define IDLC_CASE_SENSITIVE (1u<<5)

static struct {
  char *file; /* path of input file or "-" for STDIN */
  const char *lang;
  uint32_t flags;
  idl_version_t version;
  int help;
  /* (emulated) command line options for mcpp */
  int argc;
  char **argv;
} config;

/* mcpp does not accept userdata */
static idl_retcode_t retcode = IDL_RETCODE_OK;
static idl_pstate_t *pstate = NULL;

static int idlc_putc(int chr, OUTDEST od);
static int idlc_puts(const char *str, OUTDEST od);
static int idlc_printf(OUTDEST od, const char *str, ...);

#define CHUNK (4096)

static int idlc_putn(const char *str, size_t len)
{
  assert(pstate->flags & IDL_WRITE);

  /* tokenize to free up space */
  if ((pstate->buffer.size - pstate->buffer.used) <= len) {
    if ((retcode = idl_parse(pstate)) == IDL_RETCODE_NEED_REFILL)
      retcode = IDL_RETCODE_OK;
    /* move non-tokenized data to start of buffer */
    pstate->buffer.used =
      (uintptr_t)pstate->scanner.limit - (uintptr_t)pstate->scanner.cursor;
    memmove(pstate->buffer.data, pstate->scanner.cursor, pstate->buffer.used);
    pstate->scanner.cursor = pstate->buffer.data;
    pstate->scanner.limit = pstate->scanner.cursor + pstate->buffer.used;
  }

  if (retcode != IDL_RETCODE_OK)
    return -1;

  /* expand buffer if necessary */
  if ((pstate->buffer.size - pstate->buffer.used) <= len) {
    size_t size = pstate->buffer.size + (((len / CHUNK) + 1) * CHUNK);
    char *buf = realloc(pstate->buffer.data, size + 2 /* '\0' + '\0' */);
    if (buf == NULL) {
      retcode = IDL_RETCODE_NO_MEMORY;
      return -1;
    }
    /* update scanner location */
    pstate->scanner.cursor = buf + (pstate->scanner.cursor - pstate->buffer.data);
    pstate->scanner.limit = buf + pstate->buffer.used;
    /* update input buffer */
    pstate->buffer.data = buf;
    pstate->buffer.size = size;
  }

  /* write to buffer */
  memcpy(pstate->buffer.data + pstate->buffer.used, str, len);
  pstate->buffer.used += len;
  assert(pstate->buffer.used <= pstate->buffer.size);
  /* update scanner location */
  pstate->scanner.limit = pstate->buffer.data + pstate->buffer.used;

  return 0;
}

static int idlc_putc(int chr, OUTDEST od)
{
  int ret = -1;
  char str[2] = { (char)chr, '\0' };

  switch (od) {
  case OUT:
    if (!(config.flags & IDLC_COMPILE))
      ret = printf("%c", chr);
    else
      ret = idlc_putn(str, 1);
    break;
  case ERR:
    ret = fprintf(stderr, "%c", chr);
    break;
  case DBG:
    if (config.flags & IDLC_DEBUG_PREPROCESSOR)
      ret = fprintf(stderr, "%c", chr);
    break;
  default:
    assert(0);
    break;
  }

  return ret < 0 ? -1 : ret;
}

static int idlc_puts(const char *str, OUTDEST od)
{
  int ret;
  size_t len = strlen(str);

  assert(str != NULL);
  assert(len <= INT_MAX);
  ret = (int)len;

  switch (od) {
  case OUT:
    if (!(config.flags & IDLC_COMPILE))
      ret = printf("%s", str);
    else
      ret = idlc_putn(str, len);
    break;
  case ERR:
    ret = fprintf(stderr, "%s", str);
    break;
  case DBG:
    if (config.flags & IDLC_DEBUG_PREPROCESSOR)
      ret = fprintf(stderr, "%s", str);
    break;
  default:
    assert(0);
    break;
  }

  return ret < 0 ? -1 : ret;
}

static int idlc_printf(OUTDEST od, const char *fmt, ...)
{
  int ret = -1;
  char *str = NULL;
  int len;
  va_list ap;

  assert(fmt != NULL);

  va_start(ap, fmt);
  if ((len = idl_vasprintf(&str, fmt, ap)) < 0) { /* FIXME: optimize */
    retcode = IDL_RETCODE_NO_MEMORY;
    return -1;
  }
  va_end(ap);

  switch (od) {
  case OUT:
    if (!(config.flags & IDLC_COMPILE))
      ret = printf("%s", str);
    else
      ret = idlc_putn(str, (size_t)len);
    break;
  case ERR:
    ret = fprintf(stderr, "%s", str);
    break;
  case DBG:
    if (config.flags & IDLC_DEBUG_PREPROCESSOR)
      ret = fprintf(stderr, "%s", str);
    break;
  default:
    assert(0);
    break;
  }

  free(str);

  return ret < 0 ? -1 : ret;
}

static idl_retcode_t figure_file(idl_file_t **filep)
{
  idl_file_t *file;
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  char *dir = NULL, *abs = NULL, *norm = NULL;

  if (!(file = malloc(sizeof(*file))))
    goto err_file;
  if (idl_isabsolute(config.file)) {
    if ((ret = idl_normalize_path(config.file, &norm)) < 0)
      goto err_norm;
  } else {
    if (idl_current_path(&dir) < 0)
      goto err_dir;
    if (idl_asprintf(&abs, "%s/%s", dir, config.file) == -1)
      goto err_abs;
    if ((ret = idl_normalize_path(abs, &norm)) < 0)
      goto err_norm;
    free(abs);
    free(dir);
  }
  file->next = NULL;
  file->name = norm;
  *filep = file;
  return IDL_RETCODE_OK;
err_norm:
  if (abs) free(abs);
err_abs:
  if (dir) free(dir);
err_dir:
  free(file);
err_file:
  return ret;
}

static idl_retcode_t idlc_parse(void)
{
  idl_file_t *path = NULL;
  idl_file_t *file = NULL;
  idl_retcode_t ret = IDL_RETCODE_OK;
  //uint32_t flags = IDL_FLAG_ANNOTATIONS | IDL_FLAG_EXTENDED_DATA_TYPES;

  if(config.flags & IDLC_COMPILE) {
    idl_source_t *source;
    if ((ret = idl_create_pstate(config.version, 0u, NULL, &pstate))) {
      return ret;
    }
    assert(config.file);
    if (strcmp(config.file, "-") != 0 && (ret = figure_file(&path)) != 0) {
      idl_delete_pstate(pstate);
      return ret;
    }
    file = malloc(sizeof(*file));
    file->next = NULL;
    file->name = idl_strdup(config.file);
    pstate->files = file;
    pstate->paths = path;
    source = malloc(sizeof(*source));
    source->parent = NULL;
    source->previous = source->next = NULL;
    source->includes = NULL;
    source->system = false;
    source->path = path;
    source->file = file;
    pstate->sources = source;
    /* populate first source file */
    pstate->scanner.position.source = source;
    pstate->scanner.position.file = (const idl_file_t *)file;
    pstate->scanner.position.line = 1;
    pstate->scanner.position.column = 1;
    pstate->flags |= IDL_WRITE;
  }

  if (config.flags & IDLC_PREPROCESS) {
    if (pstate) {
      assert(config.flags & IDLC_COMPILE);
      pstate->flags |= IDL_WRITE;
    }
    mcpp_set_out_func(&idlc_putc, &idlc_puts, &idlc_printf);
    if (mcpp_lib_main(config.argc, config.argv) == 0) {
      assert(!(config.flags & IDLC_COMPILE) || retcode == IDL_RETCODE_OK);
    } else if (config.flags & IDLC_COMPILE) {
      assert(retcode != IDL_RETCODE_OK);
      ret = retcode;
    }
    if (pstate) {
      pstate->flags &= ~IDL_WRITE;
    }
  } else {
    FILE *fin = NULL;
    char buf[1024];
    size_t nrd;
    int nwr;

    if (strcmp(config.file, "-") == 0) {
      fin = stdin;
    } else {
#if _WIN32
      fopen_s(&fin, config.file, "rb");
#else
      fin = fopen(config.file, "rb");
#endif
    }

    if (!fin) {
      if (errno == ENOMEM)
        ret = IDL_RETCODE_NO_MEMORY;
      else if (errno == EACCES)
        ret = IDL_RETCODE_NO_ACCESS;
      else
        ret = IDL_RETCODE_NO_ENTRY;
    } else {
      while ((nrd = fread(buf, sizeof(buf), 1, fin)) > 0) {
        if ((nwr = idlc_putn(buf, nrd)) == -1) {
          ret = retcode;
          assert(ret != 0);
        }
        assert(nrd == (size_t)nwr);
      }
      if (fin != stdin)
        fclose(fin);
    }
  }

  if (ret == IDL_RETCODE_OK && (config.flags & IDLC_COMPILE)) {
    ret = idl_parse(pstate);
    assert(ret != IDL_RETCODE_NEED_REFILL);
  }

  return ret;
}

static int set_debug(const idlc_option_t *opt, const char *optarg)
{
  (void)opt;
  for (size_t off=0, pos=0; ; pos++) {
    if (optarg[pos] == '\0' || optarg[pos] == ',') {
      size_t len = pos - off;
      if (strncmp(optarg + off, "preprocessor", len) == 0)
        config.flags |= IDLC_DEBUG_PREPROCESSOR;
      else if (strncmp(optarg + off, "scanner", len) == 0)
        config.flags |= IDLC_DEBUG_SCANNER;
      else if (strncmp(optarg + off, "parser", len) == 0)
        config.flags |= IDLC_DEBUG_PARSER;
      else if (len)
        return IDLC_BAD_ARGUMENT;
      if (optarg[pos] == '\0')
        break;
      off = pos + 1;
    }
  }
  return 0;
}

static int set_compile_only(const idlc_option_t *opt, const char *optarg)
{
  (void)opt;
  (void)optarg;
  config.flags &= ~IDLC_PREPROCESS;
  config.flags |= IDLC_COMPILE;
  return 0;
}

static int set_preprocess_only(const idlc_option_t *opt, const char *optarg)
{
  (void)opt;
  (void)optarg;
  config.flags &= ~IDLC_COMPILE;
  config.flags |= IDLC_PREPROCESS;
  return 0;
}

static int set_version(const idlc_option_t *opt, const char *optarg)
{
  (void)opt;
  if (optarg && strcmp(optarg, "3.5") == 0)
    config.version = IDL35;
  else if (optarg && strcmp(optarg, "4") == 0)
    config.version = IDL4;
  else
    return IDLC_BAD_ARGUMENT;
  return 0;
}

static int set_case_sensitive(const idlc_option_t *opt, const char *optarg)
{
  (void)opt;
  (void)optarg;
  config.flags |= IDLC_CASE_SENSITIVE;
  return 0;
}

static int add_include(const idlc_option_t *opt, const char *optarg)
{
  (void)opt;
  config.argv[config.argc++] = "-I";
  config.argv[config.argc++] = (char*)optarg;
  return 0;
}

static int add_macro(const idlc_option_t *opt, const char *optarg)
{
  (void)opt;
  config.argv[config.argc++] = "-D";
  config.argv[config.argc++] = (char*)optarg;
  return 0;
}

static const idlc_option_t *compopts[] = {
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &set_debug }, 'd', "", "<component>",
    "Display debug information for <components>(s). Comma separate or use "
    "more than one -d option to specify multiple components.\n"
    "Components: preprocessor, scanner, parser." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &set_compile_only }, 'S', "", "",
    "Compile only." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &set_preprocess_only }, 'E', "", NULL,
    "Preprocess only."},
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &set_case_sensitive }, 'f', "case-sensitive", "",
    "Switch to case-sensitive mode of operation. e.g. to allow constructed "
    "entities to contain fields that differ only in case." },
  &(idlc_option_t){
    IDLC_FLAG, { .flag = &config.help }, 'h', "", "",
    "Display available options." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &add_include }, 'I', "", "<directory>",
    "Add <directory> to include search list." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &add_macro }, 'D', "", "<macro>[=value]",
    "Define <macro> to <value> (default:1)." },
  &(idlc_option_t){
    IDLC_STRING, { .string = &config.lang }, 'l', "", "<language>",
    "Compile representation for <language>. (default:c)." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &set_version }, 'v', "", "<3.5|4>",
    "Set version of IDL information." },
  NULL
};

static const char *figure_language(int argc, char **argv)
{
  const char *lang = "c";

  for (int i=1; i < argc; ) {
    if (argv[i][0] != '-' || argv[i][1] == '\0')
      break;
    if (strcmp(argv[i], "--") == 0)
      break;
    if (argv[i][1] == 'l') {
      if (argv[i][2] != '\0')
        lang = &argv[i][2];
      else if (++i < argc)
        lang = &argv[i][0];
      break;
    } else if (argv[i++][2] == '\0') {
      /* assume argument if not option */
      i += (i < argc && argv[i][0] != '-');
    }
  }

  return lang;
}

int main(int argc, char *argv[])
{
  int exit_code = EXIT_FAILURE;
  const char *prog = argv[0];
  const char *lang;
  idlc_generator_plugin_t gen;
  const idlc_option_t **opts = NULL, **genopts = NULL;
  size_t nopts = 0, ncompopts = 0, ngenopts = 0;

  for (const char *sep = argv[0]; *sep; sep++) {
    if (idl_isseparator(*sep))
      prog = sep + 1;
  }

  config.flags = IDLC_PREPROCESS|IDLC_COMPILE;

  /* determine which generator to use */
  lang = figure_language(argc, argv);
  memset(&gen, 0, sizeof(gen));
  /* FIXME: c language will be builtin. remove strcmp when available */
  if (idlc_load_generator(&gen, lang) == -1)
    fprintf(stderr, "%s: cannot load generator %s\n", prog, lang);

  config.version = IDL35;
  config.argc = 0;
  if (!(config.argv = calloc((size_t)argc + 6, sizeof(config.argv[0]))))
    goto err_argv;
  /* FIXME: introduce compatibility options
   * -e(xtension) with e.g. embedded-struct-def. The -e flags can also be used
   *  to enable/disable building blocks from IDL 4.x.
   * -s with e.g. 3.5 and 4.0 to enable everything allowed in the specific IDL
   *  specification.
   */

  config.argv[config.argc++] = argv[0];
  config.argv[config.argc++] = "-C"; /* keep comments */
  /* config.argv[config.argc++] = "-I-"; unset system include directories */
  config.argv[config.argc++] = "-k"; /* keep white space as is */
  config.argv[config.argc++] = "-N"; /* unset predefined macros */
  config.argv[config.argc++] = "-D";
  /* define __IDLC__ so that sections in a file can be enabled or disabled
     based on a macro */
  config.argv[config.argc++] = "__IDLC__";
  /* parse command line options */
  ncompopts = (sizeof(compopts)/sizeof(compopts[0])) - 1;
  if (gen.generator_options) {
    genopts = gen.generator_options();
    for (; genopts[ngenopts]; ngenopts++) ;
  }
  nopts = ncompopts + ngenopts;
  if (!(opts = calloc(nopts + 1, sizeof(opts[0]))))
    goto err_alloc_opts;
  memcpy(opts, compopts, ncompopts * sizeof(opts[0]));
  memcpy(opts+ncompopts, genopts, ngenopts * sizeof(opts[0]));
  opts[nopts] = NULL;

  switch (parse_options(argc, argv, opts)) {
    case 0:
      break;
    case IDLC_BAD_INPUT:
      fprintf(stderr, "%s: conflicting options in generator %s\n", prog, lang);
      /* fall through */
    default:
      print_usage(prog, "[OPTIONS] FILE");
      goto err_parse_opts;
  }

  if (config.help) {
    print_help(prog, "[OPTIONS] FILE", opts);
  } else {
    idl_retcode_t ret;
    if (optind != (argc - 1)) {
      print_usage(prog, "[OPTIONS] FILE");
      goto err_parse_opts;
    }
    config.file = argv[optind];
    config.argv[config.argc++] = config.file;
    if ((ret = idlc_parse())) {
      fprintf(stderr, "Cannot parse '%s'\n", config.file);
      goto err_parse;
    } else if (config.flags & IDLC_COMPILE) {
      if (gen.generate)
        gen.generate(pstate);
      idl_delete_pstate(pstate);
    }
  }

  exit_code = EXIT_SUCCESS;
err_parse:
err_parse_opts:
  free(opts);
err_alloc_opts:
  free(config.argv);
err_argv:
  return exit_code;
}

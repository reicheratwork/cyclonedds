//
// I contemplated implementing my own streaming functions, but we really
// don't need them. That is, we don't need to offer them to users, but
// we might want to have them for writing simple unit tests.
//
// Insteam of implementing my own functions for all of that, we just depend
// on fmemopen.
//
// It's available on most *NIX platforms and we can sort of have it for
// Windows as well: see cyclonedds/src/ddsrt/tests/log.c
//
// open_memstream and fopenmem are in POSIX 2008 and were proprosed for C11
// http://www.open-std.org/jtc1/sc22/wg14/www/projects#24731-2
//
https://github.com/Arryboom/fmemopen_windows
https://stackoverflow.com/questions/19316573/redirect-file-handle-to-char-buffer
http://mingw.5.n7.nabble.com/fmemopen-without-creating-temp-files-td1576.html
https://github.com/openunix/cygwin/blob/master/newlib/libc/stdio/fmemopen.c
https://lists.gnu.org/archive/html/bug-gnulib/2010-04/msg00440.html
http://polarhome.com/service/man/?qf=open_memstream&tf=2&of=Cygwin&sf=3




http://www.open-std.org/jtc1/sc22/wg14/www/projects
https://www.gnu.org/software/gnulib/manual/gnulib.html#open_005fmemstream
https://github.com/freebsd/freebsd/blob/master/lib/libc/stdio/open_memstream.c

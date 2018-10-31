/* isatty.c */

#include <unistd.h>
#include <reent.h>

#ifndef _COMPILING_NEWLIB
int	_isatty (int __fildes);
#endif

int
isatty (int fd)
{
  return _isatty (fd);
}

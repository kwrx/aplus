#define _BSD_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <aplus/base.h>
#include <aplus/utils/utf8.h>

char *
utf8_encode (const char *str) {
  // encoded char buffer
  unsigned char buf[UTF8_MAX_BUFFER_LENGTH];

  // code pointer pointer
  unsigned char cp = 0;

  // byte for encoded bytes
  unsigned char b = 0;

  // code point value
  unsigned int c = 0;

  // stream index
  int i = 0;

  // encoded buffer index
  int n = 0;

  // byte count
  int count = 0;

  // byte offset
  int offset = 0;

  while ('\0' != (cp = (str[i++]))) {
    if (errno > 0) {
      return NULL;
    }

    c = (unsigned int) cp;

    if (UTF8_IN_URANGE(c, 0xD800, 0xDFFF)) {
      errno = UTF8E_CODE_POINT_OOB;
      break;
    }

    if (EOF == c) {
      buf[n++] = EOF;
      break;
    }

    if (UTF8_IN_URANGE(c, 0x0000, 0x07F)) {
      buf[n++] = c;
      continue;
    }

    if (UTF8_IN_URANGE(c, 0x0080, 0x07FF)) {
      count = 1;
      offset = 0xC0;
    } else if (UTF8_IN_URANGE(c, 0x0800, 0xFFFF)) {
      count = 2;
      offset = 0xE0;
    } else if (UTF8_IN_URANGE(c, 0x10000, 0x10FFFF)) {
      count = 3;
      offset = 0xF0;
    }

    buf[n++] = (c/pow(64, count)) + offset;

    while (count > 0) {
      b = c/pow(64, count - 1);
      buf[n++] = 0x80 + (b % 64);
      count--;
    }
  }

  buf[n] = '\0';

  char* p = __libaplus_malloc(strlen(buf));
  strcpy(p, buf);
  
  return p;
}


char *
utf8_decode (const char *str) {
  // decoded char buffer
  unsigned char buf[UTF8_MAX_BUFFER_LENGTH];

  // byte pointer
  unsigned char bp = 0;

  // utf8 code point
  unsigned char cp = 0;

  // boundaries
  unsigned char lower = 0x80;
  unsigned char upper = 0xBF;

  // byte pointer value
  unsigned int b = 0;

  // stream index
  int i = 0;

  // buffer index
  int n = 0;

  // byte meta
  int seen = 0;
  int needed = 0;

  while ('\0' != (bp = str[i++])) {
    if (errno > 0) {
      return NULL;
    }

    b = (unsigned int) bp;

    if (b == EOF) {
      if (0 != needed) {
        needed = 0;
        errno = UTF8E_EOF_BYTES_NEEDED;
      }

      buf[n++] = EOF;
      continue;
    }

    if (0 == needed) {
      // 1 byte
      if (UTF8_IN_URANGE(b, 0x00, 0x7F)) {
        buf[n++] = b;
        continue;
      }

      if (0 == UTF8_IN_URANGE(b, 0xC2, 0xF4)) {
        errno = UTF8E_BYTE_RANGE;
        continue;
      }

      // 2 byte
      if (UTF8_IN_URANGE(b, 0xC2, 0xDF)) {
        needed = 1;
        cp = b - 0xC0;
      }

      // 3 byte
      if (UTF8_IN_URANGE(b, 0xE0, 0xEF)) {
        if (b == 0xE0) {
          lower = 0xA0;
        } else if (b == 0xED) {
          upper = 0x9F;
        }

        needed = 2;
        cp = b - 0xE0;
      }

      // 4 byte
      if (UTF8_IN_URANGE(b, 0xF0, 0xF4)) {
        if (b == 0xF0) {
          lower = 0x90;
        } else if (b == 0xF4) {
          upper = 0x8F;
        }

        needed = 3;
        cp = b - 0xF0;
      }

      cp = cp * pow(64, needed);
      continue;
    }

    if (0 == UTF8_IN_URANGE(b, lower, upper)) {
      cp = 0;
      needed = 0;
      seen = 0;
      bp = str[--i];
      lower = 0x80;
      upper = 0xBF;
      errno = UTF8E_BYTE_BOUNDARY_RANGE;
      continue;
    }

    lower = 0x80;
    upper = 0xBF;
    seen++;
    cp += (b - 0x80) * pow(64, needed - seen);

    if (seen != needed) {
      continue;
    }

    buf[n++] = cp;
    cp = 0;
    needed = 0;
    seen = 0;
  }

  buf[n] = '\0';

  char* p = __libaplus_malloc(strlen(buf));
  strcpy(p, buf);
  
  return p;
}

#ifndef __BASE64_H__
#define __BASE64_H__

#include <stdint.h>

int base64_encode(uint8_t *srcbuf, int srclen, char *dstbuf, int dstlen);

#endif


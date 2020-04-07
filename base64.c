#include <stdint.h>
#include "base64.h"

static char base64_to_char(uint8_t v)
{
    if (v < 26) {
        return 'A' + (v - 0 );
    } else if (v < 52) {
        return 'a' + (v - 26);
    } else if (v < 62) {
        return '0' + (v - 52);
    } else if (v == 62) {
        return '+';
    } else {
        return '/';
    }
}

static void base64_encode_3bytes(char *dst, uint8_t *src)
{
    uint32_t data = (src[0] << 16) | (src[1] << 8) | (src[2] << 0);
    *dst++ = base64_to_char((data >> 18) & 0x3f);
    *dst++ = base64_to_char((data >> 12) & 0x3f);
    *dst++ = base64_to_char((data >>  6) & 0x3f);
    *dst++ = base64_to_char((data >>  0) & 0x3f);
}

int base64_encode(uint8_t *srcbuf, int srclen, char *dstbuf, int dstlen)
{
    int ret = (srclen + 2) / 3 * 4, i;
    uint8_t tmp[3] = {};

    while (srclen >= 3 && dstlen >= 4) {
        base64_encode_3bytes(dstbuf, srcbuf);
        srcbuf += 3; srclen -= 3;
        dstbuf += 4; dstlen -= 4;
    }

    if (srclen > 0 && dstlen < 4) return -1;
    if (srclen == 0) return ret;

    for (i=0; i<3; i++) {
        tmp[i] = i < srclen ? srcbuf[i] : 3 - srclen;
    }
    base64_encode_3bytes(dstbuf, tmp);
    for (i=0; i<3-srclen; i++) {
        dstbuf[3-i] = '=';
    }
    return ret;
}

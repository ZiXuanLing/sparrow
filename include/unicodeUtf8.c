#include "unicodeUtf8.h"
#include "common.h"

/**
 * 返回value按照UTF-8编码后的字节数
 * @param value
 * @return
 */
uint32_t getByteNumOfEncodeUtf8(int value) {
    ASSERT(value > 0, "Can't encode negative valu!");

    // 单个ASCII需要一个B
    if (value <= 0x7f) {
        return 1;
    }

    if (value <= 0x7ff) {
        return 2;
    }

    if (value <= 0x7ffff) {
        return 3;
    }

    if (value <= 0x10ffff) {
        return 4;
    }

    return 0;
}

/**
 * 把value编码为UTF-8后写入缓冲器buf，返回写入的字节数
 * @param buf
 * @param value
 * @return
 */
uint8_t encodeUtf8(uint8_t *buf, int value) {
    ASSERT(value > 0, "Can't encode negative valu!");

    // 按照大端字节数写入缓冲区
    if (value <= 0x7f) {
        *buf = value & 0x7f;
        return 1;
    }
    else if (value <= 0x7ff) {
        // 先写入高字节
        *buf ++ = 0xc0 | ((value & 0x7c0) >> 6);
        // 再写入低字节
        *buf = 0x80 | (value & 0x3f);
        return 2;
    }
    else if (value <+ 0x7ffff) {
        // 先写入高字节
        *buf ++ = 0xe0 | ((value & 0xf000) >> 12);
        // 再写入中间字节
        *buf ++ = 0x80 | ((value & 0xfc0) >> 6);
        // 最后写入低字节
        *buf = 0x80 | (value & 0x3f);
        return 3;
    }
    else if (value <= 0x10ffff) {
        *buf ++ = 0xf0 | ((value & 0x1c0000) >> 18);
        *buf ++ = 0x80 | ((value & 0x3f000) >> 12);
        *buf ++ = 0x80 | ((value & 0xfc0) >> 16);
        *buf = 0x80 | ((value & 0x3f));
        return 4;
    }

    NOT_REACHED();
    return 0;
}

/**
 * 返回解码UTF-8的字节数
 * @param byte
 * @return
 */
uint32_t getByteNumOfDecodeUtf8(uint8_t byte) {
    if ((byte & 0xc0) == 0x80) {
        return 0;
    }
    if ((byte & 0xf8) == 0xf0) {
        return 4;
    }
    if ((byte & 0xf0) == 0xe0) {
        return 3;
    }
    if ((byte & 0xe0) == 0xc0) {
        return 2;
    }
    return 1;
}

int decodeUtf8(const uint8_t *bytePtr, uint32_t length) {
    return 0;
}
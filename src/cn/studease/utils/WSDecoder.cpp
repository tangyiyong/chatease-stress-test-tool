/*
 * WSDecoder.cpp
 *
 *  Created on: 2016Äê8ÔÂ31ÈÕ
 *      Author: ambition
 */

#include "WSDecoder.h"

void
WSDecoder::decode(WSABUF *wsabuf, DWORD transferred) {
	uint64_t pos = 0;
	char *src = wsabuf->buf;
	char *dst = wsabuf->buf;
	char maskingKey[4] = { '\0' };

dec:
	uint8_t FIN = (*src & 0x80) >> 7;
	uint8_t opcode = *src & 0x0F;
	src++;

	uint8_t mask = (*src & 0x80) >> 7;
	uint64_t len = *src & 0x7F;
	src++;

	if ((opcode & 0x09) == 9 || (opcode & 0x0A) == 10) {
		goto dec; // Ignore ping & pong frame.
	}

	uint64_t byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7;
	if (len < 126) {
		// len = len;
	} else if (len == 126) {
		byte0 = *src++;
		byte1 = *src++;
		len = (byte0 << 8) | byte1;
	} else { // len == 127
		byte0 = *src++;
		byte1 = *src++;
		byte2 = *src++;
		byte3 = *src++;
		byte4 = *src++;
		byte5 = *src++;
		byte6 = *src++;
		byte7 = *src++;
		len = (byte0 << 56) | (byte1 << 48) | (byte2 << 40) | (byte3 << 32) | (byte4 << 24) | (byte5 << 16) | (byte6 << 8) | byte7;
	}

	if (mask > 0) {
		maskingKey[0] = *src++;
		maskingKey[1] = *src++;
		maskingKey[2] = *src++;
		maskingKey[3] = *src++;
	}

	pos = src - wsabuf->buf;
	for (uint64_t i = 0; i < wsabuf->len - pos; i++) {
		if (i == len) { // Decode next frame.
			goto dec;
		}
		if (mask > 0) {
			*dst++ = *src++ ^ maskingKey[i % 4];
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';
	wsabuf->len = dst - wsabuf->buf;
}


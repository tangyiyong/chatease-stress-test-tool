/*
 * WSEncoder.cpp
 *
 *  Created on: 2016Äê8ÔÂ31ÈÕ
 *      Author: ambition
 */

#include "WSEncoder.h"

void
WSEncoder::encode(WSABUF *wsabuf, const char *data) {
	encode(wsabuf, data, 1);
}

void
WSEncoder::encode(WSABUF *wsabuf, const char *data, uint8_t FIN) {
	encode(wsabuf, data, FIN, 1);
}

void
WSEncoder::encode(WSABUF *wsabuf, const char *data, uint8_t FIN, uint8_t opcode) {
	char  maskingKey[4] = { 0x00 };
	encode(wsabuf, data, FIN, opcode, maskingKey);
}

void
WSEncoder::encode(WSABUF *wsabuf, const char *data, uint8_t FIN, uint8_t opcode, char maskingKey[4]) {
	uint64_t  len, extended, i;
	uint8_t   mask;
	char     *dst, *src;

	len = strlen(data);
	mask = maskingKey == NULL ? 0 : 1;

	dst = wsabuf->buf;
	src = (char *) data;

	*dst++ = (FIN << 7) | opcode;
	if (len < 126) {
		*dst++ = (mask << 7) | len;
		extended = 0;
	} else if (len <= 0xFFFF) {
		*dst++ = (mask << 7) | 0x7E;
		*dst++ = len >> 8;
		*dst++ = len;
		extended = 2;
	} else {
		*dst++ = (mask << 7) | 0x7F;
		*dst++ = len >> 56;
		*dst++ = len >> 48;
		*dst++ = len >> 40;
		*dst++ = len >> 32;
		*dst++ = len >> 24;
		*dst++ = len >> 16;
		*dst++ = len >> 8;
		*dst++ = len;
		extended = 8;
	}

	if (mask) {
		*dst++ = maskingKey[0];
		*dst++ = maskingKey[1];
		*dst++ = maskingKey[2];
		*dst++ = maskingKey[3];
	}

	for (i = 0; i < len; i++) {
		if (mask) {
			*dst++ = *src++ ^ maskingKey[i % 4];
		} else {
			*dst++ = *src++;
		}
	}

	*dst = '\0';

	wsabuf->len = len + 2 + extended + (mask ? 4 : 0);
}


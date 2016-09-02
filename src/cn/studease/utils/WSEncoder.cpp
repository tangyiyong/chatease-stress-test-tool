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
	char maskingKey[4] = { 0x00 };
	encode(wsabuf, data, FIN, opcode, maskingKey);
}

void
WSEncoder::encode(WSABUF *wsabuf, const char *data, uint8_t FIN, uint8_t opcode, char maskingKey[4]) {
	uint64_t pos = 0, len = strlen(data);
	uint8_t mask = maskingKey == NULL ? 0 : 1;

enc:
	char *dst = wsabuf->buf;
	*dst++ = (FIN << 7) + opcode;
	pos++;
	if (len < 126) {
		*dst++ = (mask << 7) | len;
		pos++;
	} else if (len <= 0xFFFF) {
		*dst++ = (mask << 7) | 126;
		*dst++ = (len & 0xFF00) >> 8;
		*dst++ = len & 0xFF;
		pos += 3;
	} else {
		*dst++ = (mask << 7) | 127;
		*dst++ = (len & 0xFF00000000000000) >> 56;
		*dst++ = (len & 0xFF000000000000) >> 48;
		*dst++ = (len & 0xFF0000000000) >> 40;
		*dst++ = (len & 0xFF00000000) >> 32;
		*dst++ = (len & 0xFF000000) >> 24;
		*dst++ = (len & 0xFF0000) >> 16;
		*dst++ = (len & 0xFF00) >> 8;
		*dst++ = len & 0xFF;
		pos += 5;
	}

	char *src = (char *) data;
	if (mask > 0) {
		*dst++ = maskingKey[0];
		*dst++ = maskingKey[1];
		*dst++ = maskingKey[2];
		*dst++ = maskingKey[3];
		pos += 4;
	}

	for (uint64_t i = 0; i < len; i++) {
		if (wsabuf->len > 0 && pos >= wsabuf->len) { // Reset len.
			len = i;
			goto enc;
		}
		if (mask > 0) {
			*dst++ = *src++ ^ maskingKey[i % 4];
		} else {
			*dst++ = *src++;
		}
		pos++;
	}
	*dst = '\0';
	wsabuf->len = pos;
}


/*
 * WSDecoder.cpp
 *
 *  Created on: 2016Äê8ÔÂ31ÈÕ
 *      Author: ambition
 */

#include "WSDecoder.h"

void
WSDecoder::decode(WSABUF *wsabuf) {
	uint8_t   fin, rsv1, rsv2, rsv3, opcode, mask;
	uint64_t  len, extended, i, b0, b1, b2, b3, b4, b5, b6, b7;
	char     *dst, *src;

	dst = wsabuf->buf;
	src = wsabuf->buf;

start:

	fin = (*src >> 7) & 0x1;
	rsv1 = (*src >> 6) & 0x1;
	rsv2 = (*src >> 5) & 0x1;
	rsv3 = (*src >> 4) & 0x1;
	opcode = *src & 0xF;
	src++;

	mask = (*src >> 7) & 0x1;
	len = *src & 0x7F;
	src++;

	if (mask) {
		// should not reach here.
		return;
	}

	if (len < 126) {
		extended = 0;
	} else if (len == 126) {
		b0 = *src++;
		b1 = *src++;
		len = (b0 << 8) | b1;
		extended = 2;
	} else { // len == 127
		b0 = *src++;
		b1 = *src++;
		b2 = *src++;
		b3 = *src++;
		b4 = *src++;
		b5 = *src++;
		b6 = *src++;
		b7 = *src++;
		len = (b0 << 56) | (b1 << 48) | (b2 << 40) | (b3 << 32) | (b4 << 24) | (b5 << 16) | (b6 << 8) | b7;
		extended = 8;
	}

	for (i = 0; i < len; i++) {
		*dst++ = *src++;
	}

	if (opcode != 1) {
		// ignore close, ping, pong frames.
		dst -= len;
		goto start;
	}

	if (src < wsabuf->buf + wsabuf->len) {
		// decode next frame.
		goto start;
	}

	*dst = '\0';

	wsabuf->len = dst - wsabuf->buf;
}


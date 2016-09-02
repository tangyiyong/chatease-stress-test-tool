/*
 * WSEncoder.h
 *
 *  Created on: 2016Äê8ÔÂ31ÈÕ
 *      Author: ambition
 */

#ifndef WSENCODER_H_
#define WSENCODER_H_

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

class WSEncoder {
public:
	static void encode(WSABUF *wsabuf, const char *data);
	static void encode(WSABUF *wsabuf, const char *data, uint8_t FIN);
	static void encode(WSABUF *wsabuf, const char *data, uint8_t FIN, uint8_t opcode);
	static void encode(WSABUF *wsabuf, const char *data, uint8_t FIN, uint8_t opcode, char maskingKey[4]);
};

#endif /* WSENCODER_H_ */

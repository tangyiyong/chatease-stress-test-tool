/*
 * WSDecoder.h
 *
 *  Created on: 2016��8��31��
 *      Author: ambition
 */

#ifndef WSDECODER_H_
#define WSDECODER_H_

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

class WSDecoder {
public:
	static void decode(WSABUF *wsabuf, DWORD transferred);
};

#endif /* WSDECODER_H_ */

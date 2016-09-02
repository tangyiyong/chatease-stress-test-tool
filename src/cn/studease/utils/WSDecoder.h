/*
 * WSDecoder.h
 *
 *  Created on: 2016Äê8ÔÂ31ÈÕ
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

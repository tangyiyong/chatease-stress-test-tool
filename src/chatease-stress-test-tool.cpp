//============================================================================
// Name        : websocket-stress-tool.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "cn/studease/utils/Logger.h"
#include "cn/studease/utils/WSEncoder.h"
#include "cn/studease/utils/WSDecoder.h"
#include <iostream>
#include <mswsock.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <winsock2.h>
#include <windows.h>

using namespace std;

#define DEFAULT_BUFFER_LEN 1024

struct threadParams {
	int     id = 0;
	int     connections = 1;
	char   *address;
	int     port;
	HANDLE  completionPort;
};

struct clientParams {
	int     id = 0;
	SOCKET  fd;
	HANDLE  completionPort;
	bool    connected = false;
	bool    upgraded = false;
};

enum OPERATION {
	NONE = 0,
	RECV,
	SEND,
	CLOSE
};

struct IOParams {
	clientParams *client;
	OVERLAPPED    overlapped;
	WSABUF        buffer;
	int           message_n = 0;
	OPERATION     type = NONE;

public:

	IOParams(clientParams *cparams) {
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		client = cparams;

		buffer.buf = new char[DEFAULT_BUFFER_LEN];
		buffer.len = DEFAULT_BUFFER_LEN;
	}

	void destroy() {
		delete buffer.buf;
	}
};


DWORD WINAPI WorkThread(LPVOID param);
void wsabufncpy(WSABUF *dst, WSABUF *src, DWORD len);
bool postRecv(IOParams *ioparams);
bool postSend(IOParams *ioparams);
bool postClose(IOParams *ioparams);
void close(clientParams *cParams, IOParams *ioparams);


int main(int argc, char **argv) {
	char *address;
	int   port, threads, connections, arg;

	Logger::log("GCC VERSION: %s", __VERSION__);
	Logger::log("CPP VERSION: %ld", __cplusplus);
	cout << endl;

	address = (char *) "192.168.1.229";
	port = 80;
	threads = 1;
	connections = 1;

	while ((arg = getopt(argc, argv, ":a:p:c:t:")) != -1) {
		switch (arg) {
		case 'a':
			address = optarg;
			Logger::log("Address: %s", address);
			break;
		case 'p':
			port = atoi(optarg);
			Logger::log("Port: %s", optarg);
			break;
		case 't':
			threads = atoi(optarg);
			Logger::log("Threads: %s", optarg);
			break;
		case 'c':
			connections = atoi(optarg);
			Logger::log("Connections: %s", optarg);
			break;
		case '?':
			Logger::log("Unknown argument \"%d\".", optopt);
			break;
		case ':':
			Logger::log("Required parameter not found.");
			break;
		}
	}

	if (argc) {
		cout << endl;
	}

	WORD requested = MAKEWORD(2, 2);
	WSADATA data;
	if (WSAStartup(requested, &data) != NO_ERROR) {
		Logger::wsaerr("Failed to request windows socket library.");
		system("pause");
		return -1;
	}

	HANDLE completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	vector<HANDLE> threadlist;
	for(int i = 0; i < threads; i++) {
		threadParams *params = new threadParams;
		params->id = i + 1;
		params->connections = connections / threads;
		params->address = address;
		params->port = port;
		params->completionPort = completionPort;

		Logger::log("Creating thread: id=%d ...", params->id);
		HANDLE thread = CreateThread(NULL, 0, WorkThread, params, 0, NULL);
		if (thread == NULL) {
			Logger::err("Failed to create thread[id=%d] handler.");
			continue;
		}

		threadlist.push_back(thread);
	}

	cout << endl;

wait:

	for (vector<HANDLE>::iterator it = threadlist.begin(); it != threadlist.end(); it++) {
		HANDLE thread = *it;
		WaitForSingleObject(thread, INFINITE);
		threadlist.erase(it);
	}
	if (threadlist.size() > 0) {
		goto wait;
	}

	return 0;
}

DWORD WINAPI WorkThread(LPVOID lpParam) {
	threadParams *thParams;
	sockaddr_in   addr;
	HANDLE        completionPort;
	static const char *handshaking = "GET /websocket/websck HTTP/1.1\r\nHost: localhost\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:47.0) Gecko/20100101 Firefox/47.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3\r\nAccept-Encoding: gzip, deflate\r\nSec-WebSocket-Version: 13\r\nOrigin: http://localhost\r\nSec-WebSocket-Key: ALxxV1vN1Sla6iymFEEx8A==\r\nConnection: keep-alive, Upgrade\r\nPragma: no-cache\r\nCache-Control: no-cache\r\nUpgrade: websocket\r\n\r\n";
	DWORD         handshakinglen = strlen(handshaking);

	thParams = (threadParams *) lpParam;
	completionPort = thParams->completionPort;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(thParams->address);
	addr.sin_port = htons(thParams->port);

	for (int i = 0; i < thParams->connections; i++) {
		//Logger::log("Creating client: thread=%d, id=%d ...", thParams->id, i + 1);
		SOCKET fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (fd == INVALID_SOCKET) {
			Logger::wsaerr("Failed to create client: thread=%d, id=%d.", thParams->id, i + 1);
			continue;
		}

		clientParams *cParams = new clientParams;
		cParams->id = i + 1;
		cParams->fd = fd;
		cParams->completionPort = completionPort;

		sockaddr_in si;
		si.sin_family = AF_INET;
		si.sin_addr.s_addr = INADDR_ANY;
		si.sin_port = 0;

		if (bind(fd, (LPSOCKADDR)&si, sizeof(si)) == SOCKET_ERROR) {
			Logger::err("Failed to bind client: thread=%d, id=%d.", thParams->id, cParams->id);
			continue;
		}

		if (CreateIoCompletionPort((HANDLE)fd, completionPort, (ULONG_PTR)cParams, 0) == NULL) {
			Logger::wsaerr("Failed to create IO completion port of client: thread=%d, id=%d.", thParams->id, cParams->id);
			continue;
		}

		GUID guidConnectEx = WSAID_CONNECTEX;
		LPFN_CONNECTEX ConnectEx;
		DWORD dwBytes = 0;
		if (WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidConnectEx, sizeof(guidConnectEx),
				&ConnectEx, sizeof(ConnectEx), &dwBytes, NULL, NULL) == SOCKET_ERROR) {
			Logger::wsaerr("Failed to get ConnectEx of client: thread=%d, id=%d.", thParams->id, cParams->id);
			continue;
		}

		DWORD dwSent = 0;
		OVERLAPPED ol;
		memset(&ol, 0, sizeof(ol));

		//usleep(10000);

		Logger::log("Client[thread=%d, id=%d, fd=%d] connecting ...", thParams->id, cParams->id, fd);
		if (ConnectEx(fd, (sockaddr *)&addr, sizeof(addr), (PVOID)handshaking, handshakinglen, &dwSent, &ol) == SOCKET_ERROR) {
			Logger::wsaerr("Failed to connect, client: thread=%d, id=%d.", thParams->id, cParams->id);
			continue;
		}
	}

	clientParams *cParams;
	IOParams     *ioparams;
	DWORD         transferred;
	LPOVERLAPPED  lpoverlapped;
	int           connection_n = 0;
	BOOL          status;

	Logger::log("Thread %d waiting ...", thParams->id);

	for ( ;; ) {
		status = GetQueuedCompletionStatus(completionPort, &transferred, (PULONG_PTR)&cParams,
				(LPOVERLAPPED *)&lpoverlapped, INFINITE);
		if (status == FALSE) {
			int err = WSAGetLastError();
			Logger::log("Failed to get queued completion status. Error: %d.", err);

			if (err == ERROR_NETNAME_DELETED) {
				close(cParams, NULL);
			}

			continue;
		}

		if (lpoverlapped == NULL) {
			Logger::log("Failed to get overlapped.");
			close(cParams, NULL);
			continue;
		}

		ioparams = CONTAINING_RECORD(lpoverlapped, IOParams, overlapped);
		if (ioparams->client == NULL) {
			ioparams = new IOParams(cParams);
		}

		/*Logger::log("Got status of client: thread=%d, id=%d, fd=%d, transferred=%ld, iotype=%d.",
				thParams->id, cParams->id, cParams->fd, transferred, ioparams->type);*/

		switch (ioparams->type) {
		case NONE:
			cParams->connected = true;
			connection_n++;
			Logger::log("Client connected: thread=%d, id=%d, fd=%d.", thParams->id, cParams->id, cParams->fd);

			if (postRecv(ioparams) == false) {
				close(cParams, ioparams);
				break;
			}
			break;
		case RECV:
			if (transferred == 0) {
				connection_n--;
				close(cParams, ioparams);
				break;
			}

			if (cParams->upgraded == false) {
				cParams->upgraded = true;
				/*Logger::log("Upgraded: thread=%d, id=%d, fd=%d, len=%ld, str=\n%s",
						thParams->id, cParams->id, cParams->fd, ioparams->buffer.len, ioparams->buffer.buf);*/
			} else {
				ioparams->message_n++;

				ioparams->buffer.len = transferred;
				WSDecoder::decode(&ioparams->buffer);
				Logger::log("Recv: thread=%d, id=%d, fd=%d, len=%ld, str=\n%s",
						thParams->id, cParams->id, cParams->fd, ioparams->buffer.len, ioparams->buffer.buf);
			}

			if (postRecv(ioparams) == false) {
				close(cParams, ioparams);
				break;
			}
			break;
		default:
			Logger::log("Unknown I/O type: thread=%d, id=%d, fd=%d, type=%d.",
					thParams->id, cParams->id, cParams->fd, ioparams->type);
			close(cParams, ioparams);
			break;
		}
	}

	return 0;
}


void
wsabufncpy(WSABUF *dst, WSABUF *src, DWORD len) {
	char *d, *s;
	int   n;

	d = dst->buf + dst->len;
	s = src->buf;
	n = len;

	while (n--) {
		*d++ = *s++;
	}

	*d = '\0';

	dst->len += len;
}

bool
postRecv(IOParams *ioparams) {
	clientParams *cParams;

	cParams = ioparams->client;

	memset(ioparams->buffer.buf, 0, DEFAULT_BUFFER_LEN);
	ioparams->buffer.len = DEFAULT_BUFFER_LEN;
	ioparams->type = RECV;

	DWORD dwBytes = 0, dwFlags = 0;
	if (WSARecv(cParams->fd, &ioparams->buffer, 1, &dwBytes, &dwFlags, &ioparams->overlapped, NULL) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			Logger::log("Failed to post recv: id=%d, fd=%d. Error: %d.\n", cParams->id, cParams->fd, err);
			return false;
		}
	}

	return true;
}

bool
postSend(IOParams *ioparams) {
	clientParams *cParams;

	cParams = ioparams->client;

	memset(ioparams->buffer.buf, 0, DEFAULT_BUFFER_LEN);
	ioparams->buffer.len = 0;
	ioparams->type = SEND;

	WSEncoder::encode(&ioparams->buffer, "{\"cmd\":\"message\",\"text\":\"hello all!\",\"type\":\"multi\",\"channel\":{\"id\":1}}");

	DWORD dwBytes = 0, dwFlags = 0;
	if (WSASend(ioparams->client->fd, &ioparams->buffer, 1, &dwBytes, dwFlags, &ioparams->overlapped, NULL) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			Logger::log("Failed to post send: id=%d, fd=%d. Error: %d.\n", cParams->id, cParams->fd, err);
			return false;
		}
	}

	return true;
}

bool
postClose(IOParams *ioparams) {
	clientParams *cParams;

	cParams = ioparams->client;

	memset(ioparams->buffer.buf, 0, DEFAULT_BUFFER_LEN);
	ioparams->buffer.len = 0;
	ioparams->type = CLOSE;

	WSEncoder::encode(&ioparams->buffer, NULL, 1, 8);

	DWORD dwBytes = 0, dwFlags = 0;
	if (WSASend(ioparams->client->fd, &ioparams->buffer, 1, &dwBytes, dwFlags, &ioparams->overlapped, NULL) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			Logger::log("Failed to post close: id=%d, fd=%d. Error: %d.", cParams->id, cParams->fd, err);
			return false;
		}
	}

	return true;
}

void
close(clientParams *cParams, IOParams *ioparams) {
	if (ioparams != NULL) {
		ioparams->destroy();
		delete ioparams;
	}

	closesocket(cParams->fd);
	delete cParams;
}


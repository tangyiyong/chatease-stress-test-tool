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


struct threadParams {
	int id = 0;
	int connections = 1;
	char *address;
	int port;
	HANDLE completionPort;
};

struct clientParams {
	int id = 0;
	SOCKET client;
	HANDLE completionPort;
	bool connected = false;
	bool joined = false;
};

enum OPERATION_TYPE {
	NONE = 0,
	RECV = 1,
	JOIN = 2,
	SEND = 3,
	CLOSE = 4
};

#define MAX_BUFFER_LEN 1024

struct IOParams {
	OVERLAPPED overlapped;
	clientParams *client;
	WSABUF sendbuf;
	WSABUF recvbuf;
	WSABUF tempbuf;
	bool pendding = false;
	int received = 0;
	OPERATION_TYPE type = NONE;

public:
	IOParams(clientParams *clientparams) {
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		client = clientparams;

		recvbuf.buf = new char[MAX_BUFFER_LEN];
		recvbuf.len = MAX_BUFFER_LEN;
		tempbuf.buf = new char[MAX_BUFFER_LEN];
		tempbuf.len = 0;
		sendbuf.buf = new char[MAX_BUFFER_LEN];
		sendbuf.len = 0;

		memset(tempbuf.buf, 0, MAX_BUFFER_LEN);
	}
};


DWORD WINAPI WorkThread(LPVOID param);
void wsabufncpy(WSABUF *dst, WSABUF *src, DWORD len);
bool postRecv(IOParams *ioparams);
bool postJoin(IOParams *ioparams);
bool postSend(IOParams *ioparams);
bool postClose(IOParams *ioparams, uint16_t code, char *explain);
void close(clientParams *clientparams, IOParams *ioparams);


int main(int argc, char **argv) {
	char *address = (char *) "127.0.0.1";
	int port = 80;
	int threads = 1;
	int connections = 1;

	Logger::log("GCC version: %s", __VERSION__);
	Logger::log("CPP version: %ld", __cplusplus);
	Logger::log("\n");

	int arg;
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
	Logger::log("\n");

	WORD requested = MAKEWORD(2, 2);
	WSADATA data;
	if (WSAStartup(requested, &data) != NO_ERROR) {
		int err = WSAGetLastError();
		Logger::log("Error while requesting windows socket library. Error: %d.", err);
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

		Logger::log("Creating thread[id=%d]...", params->id);
		HANDLE thread = CreateThread(NULL, 0, WorkThread, params, 0, NULL);
		if (thread == NULL) {
			int err = (int)GetLastError();
			Logger::log("Failed to create thread[id=%d] handler. Error: %d.", params->id, err);
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
	threadParams *params = (threadParams *) lpParam;

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(params->address);
	addr.sin_port = htons(params->port);

	HANDLE completionPort = params->completionPort;

	char *handshaking = (char *)"GET /websocket/websck HTTP/1.1\r\nHost: localhost\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:47.0) Gecko/20100101 Firefox/47.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3\r\nAccept-Encoding: gzip, deflate\r\nSec-WebSocket-Version: 13\r\nOrigin: http://localhost\r\nSec-WebSocket-Key: ALxxV1vN1Sla6iymFEEx8A==\r\nConnection: keep-alive, Upgrade\r\nPragma: no-cache\r\nCache-Control: no-cache\r\nUpgrade: websocket\r\n\r\n";
	DWORD handshakingLen = strlen(handshaking);

	for(int i = 0; i < params->connections; i++) {
		Logger::log("Creating client[thread=%d, id=%d]...", params->id, i + 1);
		SOCKET client = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (client == INVALID_SOCKET) {
			int err = WSAGetLastError();
			Logger::log("Failed to create client[thread=%d, id=%d]. Error: %d.", params->id, i + 1, err);
			continue;
		}

		clientParams *clientparams = new clientParams;
		clientparams->id = i + 1;
		clientparams->client = client;
		clientparams->completionPort = completionPort;

		sockaddr_in si;
		si.sin_family = AF_INET;
		si.sin_addr.s_addr = INADDR_ANY;
		si.sin_port = 0;
		if (bind(client, (LPSOCKADDR)&si, sizeof(si)) == SOCKET_ERROR) {
			int err = (int)GetLastError();
			Logger::log("Client[thread=%d, id=%d] failed to bind. Error: %d.", params->id, clientparams->id, err);
			continue;
		}

		if (CreateIoCompletionPort((HANDLE)client, completionPort, (ULONG_PTR)clientparams, 0) == NULL) {
			int err = WSAGetLastError();
			Logger::log("Client[thread=%d, id=%d] failed to create IO completion port. Error: %d.", params->id, clientparams->id, err);
			continue;
		}

		GUID guidConnectEx = WSAID_CONNECTEX;
		LPFN_CONNECTEX ConnectEx;
		DWORD dwBytes = 0;
		if (WSAIoctl(client, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidConnectEx, sizeof(guidConnectEx),
				&ConnectEx, sizeof(ConnectEx), &dwBytes, NULL, NULL) == SOCKET_ERROR) {
			int err = WSAGetLastError();
			Logger::log("Client[thread=%d, id=%d] failed to get ConnectEx. Error: %d.", params->id, clientparams->id, err);
			continue;
		}

		DWORD dwSent = 0;
		OVERLAPPED ol;
		memset(&ol, 0, sizeof(ol));
		Logger::log("Client[thread=%d, id=%d, sid=%d] connecting...", params->id, clientparams->id, client);
		if (ConnectEx(client, (sockaddr *)&addr, sizeof(addr), (PVOID)handshaking, handshakingLen, &dwSent, &ol) == SOCKET_ERROR) {
			int err = WSAGetLastError();
			Logger::log("Client[thread=%d, id=%d] failed to connect server. Error: %d.", params->id, clientparams->id, err);
			continue;
		}
	}

	DWORD transferred;
	clientParams *clientparams;
	LPOVERLAPPED lpoverlapped;
	int connections = 0;
	while (true) {
		Logger::log("...");
		BOOL status = GetQueuedCompletionStatus(completionPort, &transferred, (PULONG_PTR)&clientparams, (LPOVERLAPPED *)&lpoverlapped, INFINITE);
		if (status == FALSE) {
			int err = WSAGetLastError();
			Logger::log("Failed to get queued completion status. Error: %d.", err);
			if (err == ERROR_NETNAME_DELETED) {
				close(clientparams, NULL);
				continue;
			}
		}
		if (lpoverlapped == NULL) {
			Logger::log("Failed to get overlapped.");
			close(clientparams, NULL);
			continue;
		}

		IOParams *ioparams = CONTAINING_RECORD(lpoverlapped, IOParams, overlapped);
		if (ioparams->client == NULL) {
			ioparams = new IOParams(clientparams);
		}

		Logger::log("Got status of client[thread=%d, id=%d, sid=%d], transferred=%ld, iotype=%d.", params->id, clientparams->id, clientparams->client, transferred, ioparams->type);
		switch (ioparams->type) {
		case NONE:
			Logger::log("Client[thread=%d, id=%d, sid=%d] connected.", params->id, clientparams->id, clientparams->client);
			if (postRecv(ioparams) == false) {
				close(clientparams, ioparams);
			}
			break;
		case RECV:
			if (ioparams->client->connected == true) {
				if (ioparams->pendding == false) {
					memset(ioparams->tempbuf.buf, 0, MAX_BUFFER_LEN);
					ioparams->tempbuf.len = 0;
				}
				wsabufncpy(&ioparams->tempbuf, &ioparams->recvbuf, transferred);
			}

			ioparams->pendding = transferred <= 2;
			if (transferred <= 2) {
				Logger::log("Client[thread=%d, id=%d, sid=%d] received %ld bytes data, pendding...", params->id, clientparams->id, clientparams->client, transferred);
				if (postRecv(ioparams) == false) {
					close(clientparams, ioparams);
				}
				break;
			}

			if (ioparams->client->connected == false) {
				ioparams->client->connected = true;
				Logger::log("Client[thread=%d, id=%d, sid=%d] about to join channel.", params->id, clientparams->id, clientparams->client);
				if (postJoin(ioparams) == false) {
					close(clientparams, ioparams);
					break;
				}
			} else if (ioparams->client->joined == false) {
				WSDecoder::decode(&ioparams->tempbuf, transferred);
				Logger::log("Client[thread=%d, id=%d, sid=%d] got identity message: \n%s", params->id, clientparams->id, clientparams->client, ioparams->tempbuf.buf);

				ioparams->client->joined = true;
				connections++;
				/*if (connections >= params->connections) {
					Logger::log("Client[thread=%d, id=%d, sid=%d] about to send message.", params->id, clientparams->id, clientparams->client);
					if (postSend(ioparams) == false) {
						close(clientparams, ioparams);
						break;
					}
				}*/
				if (postRecv(ioparams) == false) {
					close(clientparams, ioparams);
				}
			} else {
				WSDecoder::decode(&ioparams->tempbuf, transferred);
				Logger::log("Client[thread=%d, id=%d, sid=%d] received %ld bytes message: \n%s", params->id, clientparams->id, clientparams->client, ioparams->tempbuf.len, ioparams->tempbuf.buf);
				ioparams->received++;

				if (postRecv(ioparams) == false) {
					close(clientparams, ioparams);
				}
			}
			break;
		case JOIN:
			Logger::log("Client[thread=%d, id=%d, sid=%d] joining channel.", params->id, clientparams->id, clientparams->client);
			if (postRecv(ioparams) == false) {
				close(clientparams, ioparams);
			}
			break;
		case SEND:
			Logger::log("Client[thread=%d, id=%d, sid=%d] sent message.", params->id, clientparams->id, clientparams->client);
			if (postRecv(ioparams) == false) {
				close(clientparams, ioparams);
			}
			break;
		default:
			Logger::log("Client[thread=%d, id=%d, sid=%d] got an unknown I/O type.", params->id, clientparams->id, clientparams->client);
			close(clientparams, ioparams);
			break;
		}
	}

	return 0;
}

void wsabufncpy(WSABUF *dst, WSABUF *src, DWORD len) {
	int n = len;
	char *d = dst->buf + dst->len;
	char *s = src->buf;
	while (n--) {
		*d++ = *s++;
	}
	dst->len += len;
}

bool postRecv(IOParams *ioparams) {
	memset(ioparams->recvbuf.buf, 0, MAX_BUFFER_LEN);
	ioparams->recvbuf.len = MAX_BUFFER_LEN;
	ioparams->type = RECV;

	DWORD dwBytes = 0, dwFlags = 0;
	if (WSARecv(ioparams->client->client, &ioparams->recvbuf, 1, &dwBytes, &dwFlags, &ioparams->overlapped, NULL) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			Logger::log("Client[id=%d] failed to post receiving data. Error: %d.", ioparams->client->id, err);
			return false;
		}
	}
	return true;
}

bool postJoin(IOParams *ioparams) {
	memset(ioparams->sendbuf.buf, 0, MAX_BUFFER_LEN);
	ioparams->sendbuf.len = 0;
	ioparams->type = JOIN;

	WSEncoder::encode(&ioparams->sendbuf, "{\"cmd\":\"join\",\"channel\":{\"id\":1}}");

	DWORD dwBytes = 0, dwFlags = 0;
	if (WSASend(ioparams->client->client, &ioparams->sendbuf, 1, &dwBytes, dwFlags, &ioparams->overlapped, NULL) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			Logger::log("Client[id=%d] failed to send joining data. Error: %d.", ioparams->client->id, err);
			return false;
		}
	}
	return true;
}

bool postSend(IOParams *ioparams) {
	memset(ioparams->sendbuf.buf, 0, MAX_BUFFER_LEN);
	ioparams->sendbuf.len = 0;
	ioparams->type = SEND;

	WSEncoder::encode(&ioparams->sendbuf, "{\"cmd\":\"message\",\"text\":\"hello all!\",\"type\":\"multi\",\"channel\":{\"id\":1}}");

	DWORD dwBytes = 0, dwFlags = 0;
	if (WSASend(ioparams->client->client, &ioparams->sendbuf, 1, &dwBytes, dwFlags, &ioparams->overlapped, NULL) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			Logger::log("Client[id=%d] failed to send message. Error: %d.", ioparams->client->id, err);
			return false;
		}
	}
	return true;
}

bool postClose(IOParams *ioparams, uint16_t code, char *explain) {
	memset(ioparams->sendbuf.buf, 0, MAX_BUFFER_LEN);
	ioparams->sendbuf.len = 0;
	ioparams->type = CLOSE;

	char data[MAX_BUFFER_LEN];
	data[0] = code >> 8;
	data[1] = code;
	if (explain != NULL) {
		strcat(data, explain);
	}
	WSEncoder::encode(&ioparams->sendbuf, data, 1, 8);

	DWORD dwBytes = 0, dwFlags = 0;
	if (WSASend(ioparams->client->client, &ioparams->sendbuf, 1, &dwBytes, dwFlags, &ioparams->overlapped, NULL) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			Logger::log("Client[id=%d] failed to send message. Error: %d.", ioparams->client->id, err);
			return false;
		}
	}
	return true;
}

void close(clientParams *clientparams, IOParams *ioparams) {
	if (ioparams != NULL) {
		delete ioparams;
	}
	closesocket(clientparams->client);
	delete clientparams;
}


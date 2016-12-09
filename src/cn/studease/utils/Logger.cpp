/*
 * Logger.cpp
 *
 *  Created on: 2016Äê8ÔÂ24ÈÕ
 *      Author: ambition
 */

#include <iostream>
#include "Logger.h"
#include <winsock2.h>
#include <windows.h>

using namespace std;

void
Logger::log(const char *format, ...) {
	__builtin_va_list argv;
	__builtin_va_start(argv, format);
	__mingw_vprintf(format, argv);
	printf("\n");
	__builtin_va_end(argv );
}

void
Logger::err(const char *format, ...) {
	__builtin_va_list argv;
	__builtin_va_start(argv, format);
	__mingw_vprintf(format, argv);
	printf(" Error: %d.\n", (int) GetLastError());
	__builtin_va_end(argv );
}

void
Logger::wsaerr(const char *format, ...) {
	__builtin_va_list argv;
	__builtin_va_start(argv, format);
	__mingw_vprintf(format, argv);
	printf(" Error: %d.\n", WSAGetLastError());
	__builtin_va_end(argv );
}

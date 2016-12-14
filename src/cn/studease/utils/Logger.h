/*
 * Logger.h
 *
 *  Created on: 2016Äê8ÔÂ24ÈÕ
 *      Author: ambition
 */

#ifndef LOGGER_H_
#define LOGGER_H_

class Logger {
public:
	static void debug(const char *format, ...);
	static void log(const char *format, ...);
	static void err(const char *format, ...);
	static void wsaerr(const char *format, ...);
};

#endif /* LOGGER_H_ */

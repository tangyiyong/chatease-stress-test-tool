# chatease-stress-test-tool

> [[domain] http://studease.cn](http://studease.cn)

> [[source] https://github.com/studease/chatease-stress-test-tool](https://github.com/studease/chatease-stress-test-tool)

This is a win32 command line websocket stress test tool based on IOCP, specified but not only for chatease-server.


## Build & Run

> Install GCC, such as [MinGW-w64](https://sourceforge.net/projects/mingw-w64/). And add environment params.

> As this is an eclipse project, please append '-lws2_32 -lkernel32' to the building command line pattern, and also '-static-libgcc -static-libstdc++' if you want to run it on the other machines.

> Run it in the command line terminal. For example, 'chatease-stress-tool.exe -c 1'.


## Parameters

* **-a** Address, default 127.0.0.1.

* **-p** Port, default 80.

* **-t** Threads, default 1.

* **-c** Connections, default 1.


## Software License

MIT.

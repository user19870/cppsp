#if defined(_WIN32) || defined(_WIN64)

typedef unsigned int UINT;
typedef int BOOL;

extern "C" BOOL __stdcall SetConsoleOutputCP(UINT wCodePageID);
extern "C" BOOL __stdcall SetConsoleCP(UINT wCodePageID);

 #else
//have no requirement for other platforms theorically

 #endif
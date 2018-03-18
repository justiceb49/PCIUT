#ifndef DIALOGONEENTRYHPP
#define DIALOGONEENTRYHPP

#include <windows.h>

int OK(HWND, WORD, LONG);
int WMInitDialog(HWND, WORD, LONG);
int WMCommand(HWND, WORD, LONG);
int FAR PASCAL WidthEnterHndl(HWND, unsigned, unsigned, LONG);
extern "C" float oneEntry(HINSTANCE,
	HWND, char *, char *, float, float, float, int);

#endif

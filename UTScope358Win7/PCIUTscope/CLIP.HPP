#ifndef CLIPBOARDHPP
#define CLIPBOARDHPP

#include <windows.h>
//#include (string.h>
//#include "bm.hpp"

class Clipboard {

	HWND hWindow;
	int clipboard_open;
	void close();
	void reportError();
	char *string;

public:

	Clipboard(HWND);

	int copyText(char*);
	char *pasteText();
	int hasText();

	~Clipboard() {close();}
};

#endif

    

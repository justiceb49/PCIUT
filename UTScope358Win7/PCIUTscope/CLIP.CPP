#include "stdafx.h"
#include "clip.hpp"

Clipboard::Clipboard(HWND hwnd)
{
	hWindow = hwnd;
	OpenClipboard(hwnd);
	clipboard_open = TRUE;
	string = 0;
}

int Clipboard::copyText(char *string)
{
	// clear out old contrents of clipboard
	EmptyClipboard();

	// allocate memory for the copy operation
	HANDLE hMemory =
		GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,
			strlen(string) + 1);

	if(!hMemory) {
		reportError();
		return 0;
	}

	// copy the data into the clipboard
	LPSTR lpMemory = (LPSTR)GlobalLock(hMemory);
	while(*string)
		*lpMemory++ = *string++;
	lpMemory = '\0';

	GlobalUnlock(hMemory);

    return (int)SetClipboardData(CF_TEXT, hMemory);
}


char *Clipboard::pasteText()
{
	HANDLE hClipboard = GetClipboardData(CF_TEXT);
	if(!hClipboard)
		// no data available in the requested format
		return 0;

	// allocate memory for the copy operation
	DWORD size = GlobalSize(hClipboard) + 1;
	string = new char [size];
	if(!string) {
		reportError();
		return 0;
	}

	// get a pointer to the clipboard data
	LPSTR lpClipboard = (LPSTR)GlobalLock(hClipboard);

	// copy the data from the clipboard
	char * cp = string;
	while(*lpClipboard)
		*cp++ = *lpClipboard++;
	*cp = '\0';

	// unlock the clipboard memory
	GlobalUnlock(hClipboard);
	return string;
}


void Clipboard::reportError()
{
	// close clipboard and report the error
	close();
	static char *message =
		"Couldn't allocate sufficient memory "
		"for the clipboard operation.";
	MessageBox(hWindow, message, "Memory failure",
		MB_OK | MB_ICONEXCLAMATION);

	return;
}

int Clipboard::hasText()
{
	// see if the clipboard contains text
	return IsClipboardFormatAvailable(CF_TEXT);
}

void Clipboard::close()
{
	if(!clipboard_open)
		// clipboard has already been closed
		return;

	clipboard_open = FALSE;
	CloseClipboard();

	if(string)
		// delete any dynamic memory used in text pasting
		delete string;
}


#include <string.h>
#include "lib.h"

void displayInitDialog(HWND hwnd, long h)
{
}

LRESULT  displayHndl(HWND hwnd,
	unsigned iMessage, WPARAM wParam, LPARAM lParam)
{
	switch (iMessage) 
	{
		case WM_INITDIALOG:
			displayInitDialog(hwnd, lParam);
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
			case IDCANCEL:
				goto CLOSEDLG;
			}
			break;
	}
	return FALSE;
}


HWND  displayOpen(HINSTANCE hInstance,
	HWND masterHwnd, LPSTR msg, LPSTR cancelFlag,
	BYTE boxType, BYTE clr)
{
	HGLOBAL hMsg;
	struct display *lp;

	hMsg = GlobalAlloc(GMEM_MOVEABLE,	sizeof(display));
	if(! hMsg)
	{
		MessageBox(masterHwnd, "Cannot allocate memory!",
			NULL, MB_OK | MB_ICONHAND);
		return NULL;
	}
	lp = (struct display *)GlobalLock(hMsg);
	if(! lp)
	{
		MessageBox(masterHwnd, "Cannot lock memory!",
			NULL, MB_OK | MB_ICONHAND);
		return NULL;
	}

	strncpy((char*)lp->msg, msg, 79);
	lp->cancelFlag = cancelFlag;
	lp->boxType = boxType;
	lp->clr = clr;
	lp->percent = 0;

	GlobalUnlock(hMsg);

	if(displayCallBack == NULL)
    {
		FARPROC function = (FARPROC) displayHndl;
		displayCallBack = (DLGPROC) MakeProcInstance(function, hInstance);
    }

	HWND displayHwnd = CreateDialogParam(hInstance, msgBoxes[boxType],
		masterHwnd, displayCallBack, (long)hMsg);

		
	if(displayHwnd == NULL) {
		MessageBox(masterHwnd, "Cannot open wait message box!",
			NULL, MB_OK);
		return NULL;
	}

	ndisplay++;
	return displayHwnd;
}


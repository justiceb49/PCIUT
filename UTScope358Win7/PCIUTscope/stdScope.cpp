// stdScope.cpp : Defines the entry point for the application.
// 1.02 6/5/2011 1. Add average feature, 2. Remove S/N drop down list and show S/N on the title bar
// 2.01 6/6/2011 Add B-scan
//		4/17/2015 changed initFile from stdScope.ini to PCIUTScope.ini
// 2.02 2/17/2018 1) Moved ducer mode code form downloadParameters to downloadWaveformParameters
//		2) moved downloadParameters from init dialog to readParameters
//		3) added a line:	Parms(SETRECEIVERCHANNEL, cardIndex, rxChan[wave], 0);

#include "stdafx.h"
#include "resource.h"
#include "io.h"
#include "stdio.h"
#include "stdlib.h"
#include "UT.h"
#include "clip.hpp"

#define MAX_LOADSTRING	100

HWND paramOpen(HINSTANCE, HWND);
int RetrieveFilepathName(HWND hwnd, char* filePath, int open, char* comments);
void setMainTitle();									
void readParameters(HWND, char *);
void saveParameters(HWND, char *);
void paramClose(HWND);
void	getCardInfo();
float unitConvert(int num);
void  displayOpen(HINSTANCE hInstance, HWND hwnd);
void	downloadWaveformParameters(int wave);
void	getWaveforms();
int getTotalBoards();
void showStatus();
void drawGates(HWND hwnd, HDC hMemoryDC, double xratio, double yratio);
float getSampleRateHz();
HWND openbscan(HINSTANCE hInstance, HWND masterHwnd);
void  qfftmenuOpen(HINSTANCE hInstance, HWND masterHwnd, RECT* lprect);

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];								// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];								// The title bar text
HWND hWnd;
HMODULE hModule=NULL;    // handle to DLL module
char *menuname={"UT_MENU"};
char *parmExt={"PAM Files(*.PAM)|*.PAM||"}, filePath[256]={""}, 
	*initFile={"PCIUTScope.ini"}, gstr[256], gstr2[256],
	modelName[20], *mainText={"main"};
int totalCards=1, cardIndex=0, timerID=0, totalWaveform=1, optionByte1, optionByte2, 
	vUnit=1,
	cursorType=0, leftBttnDn=0, hOffset=0, zoomFactor=100, cursor1, cursor2;
BYTE byteDataBuffer[MAX_WAVEFORM][32000];
float 	vUnitConvert[2]={(float)1.0, (float)2.55};

HCURSOR cursors[8];

extern HWND paramHwnd;
extern int waveformLength, waveIndex, memorySize, totalChan, hunit;
extern char parameterChanged, pause, *dllName, *functionName, *hunitDigit[],
	*hunitLabel[];

ParmsProc	Parms = NULL;

COLORREF	clr[17]={
	RGB(0,0,0),			// BLACK
	RGB(0,255,0),		// GREEN
	RGB(0,255,255),		// CYAN
	RGB(255,255,0),		// YELLOW
    RGB(70,70,255),		// BLUE
	RGB(255,0,255),		// MAGENTA
	RGB(255,128,0),		// ORANGE
	RGB(255,0,0),		// RED
	RGB(180,0,0),		// DARK RED
	RGB(0,180,0),		// DARK GREEN
	RGB(180,180,0),		// BROWN
	RGB(0,0,150),		// DARK BLUE
	RGB(180,0,180),		// DARK MAGENTA
	RGB(0,180,180),		// DARK CYAN
	RGB(192,192,192),	// GREY
	RGB(128,128,128),	// DARK GREY
    RGB(255,255,255)};	// WHITE
HPEN	hColorPen[17];
HPEN	hColorDotPen[17];
HBRUSH	hColorBrush[17];


// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void	setMainTitle()							
{
	strcpy(gstr, szTitle);
	if(filePath[0])
		strcat(gstr, filePath);
	SetWindowText(hWnd, gstr);
}

void	copyDataToClipboard(HWND hwnd)
{
	char *strptr;
	long	strsize, j;
	float interval=1000000.0/getSampleRateHz();

	int x1 = (int)(cursor1 / (0.01 * zoomFactor) + hOffset);
	int x2 = (int)(cursor2 / (0.01 * zoomFactor) + hOffset);

	long totaldata = x2 - x1 + 1;

	strsize = 16 * totaldata + 30; 		// spaces for data + spaces for LF & CR

	strptr = new char [strsize];

	if(!strptr) {
		MessageBox(hwnd, "Not enough memory!", "UT Scope", MB_OK);
		return;
	}

	Clipboard cb(hwnd);

	sprintf(strptr, "%d\r\n\0", totaldata);

	for(j=x1; j<=x2; j++) 
	{
		sprintf(gstr, "%.3f\t%u\r\n\0", interval*(j-x1), byteDataBuffer[waveIndex][j]);
		strcat(strptr, gstr);
	}

	sprintf(gstr, "%f\0", 0.000001 * getSampleRateHz());
	strcat(strptr, gstr);

	cb.copyText(strptr);
	delete strptr;
}

void updateInputStatus()
{
	int amp1, amp2;
	if(!paramHwnd)
		return;

	int x1 = (int)(hOffset + cursor1 / (0.01 * zoomFactor));
	if(x1 < waveformLength)
	{
		amp1 = (int)((float)byteDataBuffer[waveIndex][x1] / vUnitConvert[vUnit]);
		sprintf(gstr, "%d", amp1);
		if(vUnit)
			strcat(gstr, "%");
		SetDlgItemText(paramHwnd, 302, gstr);
	
		sprintf(gstr, hunitDigit[hunit], unitConvert(x1));
		strcat(gstr, hunitLabel[hunit]);
		SetDlgItemText(paramHwnd, 303, gstr);
	}
	else
	{
		SetDlgItemText(paramHwnd, 302, "");
		SetDlgItemText(paramHwnd, 303, "");
	}

	int x2 = (int)(hOffset + cursor2 / (0.01 * zoomFactor));
	if(x2 < waveformLength)
	{
		amp2 = (int)((float)byteDataBuffer[waveIndex][x2] / vUnitConvert[vUnit]);
		sprintf(gstr, "%d", amp2);
		if(vUnit)
			strcat(gstr, "%");
		SetDlgItemText(paramHwnd, 304, gstr);

		sprintf(gstr, hunitDigit[hunit], unitConvert(x2));
		strcat(gstr, hunitLabel[hunit]);
		SetDlgItemText(paramHwnd, 305, gstr);
	}
	else
	{
		SetDlgItemText(paramHwnd, 304, "");
		SetDlgItemText(paramHwnd, 305, "");
	}

	if(x1 < waveformLength && x2 < waveformLength)
	{
		sprintf(gstr, "%d", amp2-amp1);
		if(vUnit)
			strcat(gstr, "%");
		SetDlgItemText(paramHwnd, 306, gstr);

		sprintf(gstr, hunitDigit[hunit], unitConvert(x2-x1));
		strcat(gstr, hunitLabel[hunit]);
		SetDlgItemText(paramHwnd, 307, gstr);
	}
	else
	{
		SetDlgItemText(paramHwnd, 306, "");
		SetDlgItemText(paramHwnd, 307, "");
	}

	showStatus();
}

void FAR PASCAL dataTimerHandle(HWND hwnd,
	unsigned, unsigned, DWORD)
{
	if(pause)
		return;

	KillTimer(hwnd, timerID);
	timerID = 0;

	getWaveforms();

	InvalidateRect(hwnd, NULL, FALSE);
	UpdateWindow(hwnd);

	updateInputStatus();

	timerID = SetTimer(hwnd, 1, 10, (TIMERPROC)dataTimerHandle);
	if(timerID == 0)
		MessageBox(hwnd, "Cannot set display timer!", NULL, MB_OK);
}

void readPCIUTInformation()
{
	getCardInfo();

	Parms(GETMODELNUMBER, cardIndex, (WPARAM)modelName, 0);
	int j = strlen(modelName)-1;
	if(j>0 && (modelName[j] > '0'))
	{
		totalChan = 4;
	}
	GetPrivateProfileString(mainText, "parmFile", "", filePath, 255, initFile);
	if(filePath[0])
	{
		readParameters(hWnd, filePath);
		setMainTitle();
	}

	paramOpen(hInst, hWnd);

	timerID = SetTimer(hWnd, 1, 40, (TIMERPROC)dataTimerHandle);
	if(timerID == 0)
		MessageBox(hWnd, "Cannot set display timer!", NULL, MB_OK);
}

int initializeDevice(int msg)
{
	hModule = LoadLibrary(dllName);
	if(hModule == NULL)
		return 0;

	Parms = (ParmsProc)GetProcAddress(hModule, functionName);
	if(Parms == NULL)
	{
		MessageBox(hWnd, "Couldn't find dynamic link library!", NULL, MB_OK);
		return 0;
	}

	totalCards = getTotalBoards();
	if(totalCards < 1)
	{
		MessageBox(hWnd, "No UT instrument is available!", NULL, MB_OK);
		return 0;
	}

	readPCIUTInformation();
	
	return 1;
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_STDSCOPE, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	initializeDevice(1);
	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_STDSCOPE);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_STDSCOPE);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCSTR)IDC_STDSCOPE;
	wcex.lpszClassName	= szWindowClass;
//	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);
	wcex.hIconSm		= LoadIcon(hInstance, (LPCTSTR)IDI_SMALL);

	cursors[0] = LoadCursor(NULL, IDC_ARROW);
	cursors[1] = LoadCursor(hInstance, "IDC_CURSOR1");
	cursors[2] = LoadCursor(hInstance, "IDC_CURSOR1");

	return RegisterClassEx(&wcex);
}

COLORREF	CALLBACK getPenRGB(int j)
{
	return clr[j];
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    RECT r;

	hInst = hInstance; // Store instance handle in our global variable

 	GetPrivateProfileString(mainText, "wT", "0", gstr, 255, initFile);
	r.top = atoi(gstr);
	if(r.top < 0)
		r.top = 0;

	GetPrivateProfileString(mainText, "wL", "0", gstr, 255, initFile);
	r.left = atoi(gstr);
	if(r.left < 0)
		r.left = 0;

	GetPrivateProfileString(mainText, "wR", "1000", gstr, 255, initFile);
	r.right = atoi(gstr);
	if(r.right < 100)
		r.right = 100;

	GetPrivateProfileString(mainText, "wB", "430", gstr, 255, initFile);
	r.bottom = atoi(gstr);
	if(r.bottom < 100)
		r.bottom = 100;

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      r.left, r.top, r.right-r.left, r.bottom-r.top, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}

	GetClientRect(hWnd, &r);
	cursor1 = 1;
	cursor2 = r.right - 2;

	HDC hdc = GetDC(hWnd);

	for(int j=0; j<17; j++) 
	{
		hColorPen[j] = CreatePen(PS_SOLID, 1, clr[j]);
		hColorDotPen[j] = CreatePen(PS_DOT, 1, clr[j]);
		hColorBrush[j] = CreateSolidBrush(clr[j]);
	}

    ReleaseDC(NULL, hdc);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

int haveFileExt(HWND, char* pathStr)
{
	int len = strlen(pathStr) - 1;
	char s1[5] = {".PAM"};
	char s2[5] = {".pam"};
	char tmp[5];
	strcpy(tmp, &pathStr[len-3]);
	if((strcmp(s1, tmp)==0)||(strcmp(s2, tmp)==0))
		return 1;

	return 0;
}

void confirmSaveParameters(HWND hwnd)
{
	RECT r;

	GetWindowRect(hwnd, &r);

	if(r.top >= 0 && r.left >=0)
	{
		sprintf(gstr, "%d", r.top);
		WritePrivateProfileString(mainText, "wT", gstr, initFile);

		sprintf(gstr, "%d", r.left);
		WritePrivateProfileString(mainText, "wL", gstr, initFile);

		sprintf(gstr, "%d", r.right);
		WritePrivateProfileString(mainText, "wR", gstr, initFile);

		sprintf(gstr, "%d", r.bottom);
		WritePrivateProfileString(mainText, "wB", gstr, initFile);
	}
	if(filePath[0])
	{
		WritePrivateProfileString(mainText, "parmFile", filePath, initFile);
	}

	paramClose(hwnd);

	if(!parameterChanged)
		return;

	if(MessageBox(hwnd, "Parameters changed. Do you want to save the changes?",
		"UT Scope", MB_YESNO) == IDNO)
		return;

	saveParameters(hwnd, filePath);
}

void updateWaveform(HWND hwnd)
{
	InvalidateRect(hwnd, NULL, FALSE);
	UpdateWindow(hwnd);
}

#define CLOSECURSOR	10

void	handleMouseMove(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	int x = LOWORD(lParam);
	int y = HIWORD(lParam);

	if(leftBttnDn)
	{
		SetCursor(cursors[cursorType]);
		switch (cursorType)
		{
		case 1: // line cursor 1
			cursor1 = x;
			if(cursor1 >= cursor2)
				cursor1 = cursor2 - 1;
			if(pause)
			{
				updateWaveform(hwnd);
				updateInputStatus();
			}
			break;

		case 2: // line cursor 2
			cursor2 = x;
			if(cursor2 <= cursor1)
				cursor2 = cursor1 + 1;
			if(pause)
			{
				updateWaveform(hwnd);
				updateInputStatus();
			}
			break;
		}
	}
	else if(labs(x - cursor1) < CLOSECURSOR &&
		labs(x - cursor1) < labs(x - cursor2))
	{
		SetCursor(cursors[1]);
		cursorType = 1;
	}
	else if(labs(x - cursor2) < CLOSECURSOR)
	{
		SetCursor(cursors[2]);
		cursorType = 2;
	}
	else
	{
		SetCursor(cursors[0]);
		cursorType = 0;
	}
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	MSG msg;
	int wmId, wmEvent, j;
	PAINTSTRUCT ps;
	HDC hdc;
	RECT rc;
	char pathStr[256];
	BYTE *bptr;

	switch (message) 
	{
	case WM_CREATE:
		return 1;

	case WM_MOUSEMOVE:
		while(PeekMessage(&msg, hWnd, WM_MOUSEMOVE,WM_MOUSEMOVE,
			PM_REMOVE))
		{
			wParam = msg.wParam;
			lParam = msg.lParam;
		}
		handleMouseMove(hWnd, wParam, lParam);
		return 1;

	case WM_LBUTTONDOWN:
		if(cursorType > 0)
		{
			leftBttnDn = 1;
			SetCursor(cursors[cursorType]);
		}
		return 1;

	case WM_LBUTTONUP:
		leftBttnDn = 0;
		return 1;

	case WM_COMMAND:
		wmId    = LOWORD(wParam); 
		wmEvent = HIWORD(wParam); 
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
		   DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
		   break;
	
		case IDM_EXIT:
			confirmSaveParameters(hWnd);
			DestroyWindow(hWnd);
			break;

		case 101:	// open parameter file
			pathStr[0] = '#';		//set a flag of first char if canceled 
			strcpy(&pathStr[1], parmExt);
			//call DLL func for OPEN
			RetrieveFilepathName(hWnd, pathStr, 1, "Open Parameter File");

			if(pathStr[0] != '#')
			{
				//read file
				strcpy(filePath, pathStr);
				readParameters(hWnd, filePath);
				setMainTitle();									
			}
			break;

		case 102:	// save
			if(filePath)
			{
				saveParameters(hWnd, filePath);
				break;
			}

		case 103:	// save as
			pathStr[0] = '#'; 
			strcpy(&pathStr[1], parmExt);
			RetrieveFilepathName(hWnd, pathStr, 0, "Save Parameter File As");
	
			if(!haveFileExt(hWnd, pathStr))	// no extension?
				strcat(pathStr, ".PAM");	// add extension

			if(access(pathStr, 0) == 0) 
			{		// file exsits?
				if(MessageBox(hWnd, "File exists!\n\nDo you want to overwrite?",
						pathStr, MB_YESNO | MB_ICONQUESTION) != IDYES)
					break;
			}

			if(pathStr[0] != '#')   //if not cancel the dialog
			{
				strcpy(filePath, pathStr);
				saveParameters(hWnd, filePath);
				setMainTitle();                  
			}
			break;

		case 201:	// open parameter dialog
			paramOpen(hInst, hWnd);
			break;

		case 202:	// open display dialog
			displayOpen(hInst, hWnd);
			break;

		case 203:	// open B-scan dialog
			openbscan(hInst, NULL);
			break;

		case 207:	// Quick FFT
			qfftmenuOpen(hInst, hWnd, NULL);
			break;

		default:
		   return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_PAINT:
		if(!hModule)
			break;
		{
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		int totalData=waveformLength-hOffset, 
			ch;
		int traceColorIndex = 1;

		GetClientRect(hWnd, &rc);
		HDC	hMemoryDC = CreateCompatibleDC(hdc);

		HBITMAP 	hMemoryBitmap = CreateCompatibleBitmap(hdc,	rc.right, rc.bottom);
		SelectObject(hMemoryDC, hMemoryBitmap);
		SelectObject(hMemoryDC, hColorBrush[0]);
		Rectangle(hMemoryDC, 0, 0, rc.right, rc.bottom);

		double xratio=(double)0.01 * zoomFactor;
		double yratio = (double)(rc.bottom - 1) / 255;

		// draw red vertical line cursors
		SelectObject(hMemoryDC, hColorPen[7]);
		MoveToEx(hMemoryDC, cursor1, 0, NULL);
		LineTo(hMemoryDC, cursor1, rc.bottom);
		MoveToEx(hMemoryDC, cursor2, 0, NULL);
		LineTo(hMemoryDC, cursor2, rc.bottom);

		// draw dark blue grid lines
		SelectObject(hMemoryDC, hColorDotPen[11]);
		SetBkColor(hMemoryDC, RGB(0, 0, 0));
		SetROP2(hMemoryDC, R2_MERGEPEN);

		for(j=1; j<10; j++)
		{
			int n = j*rc.right/10;
			MoveToEx(hMemoryDC, n, 0, NULL);
			LineTo(hMemoryDC, n, rc.bottom);

			n = j * rc.bottom / 10;
			MoveToEx(hMemoryDC, 0, n, NULL);
			LineTo(hMemoryDC, rc.right, n);
		}

		drawGates(hWnd, hMemoryDC, xratio, yratio);

		for(ch=0; ch<totalWaveform; ch++)
		{
			SelectObject(hMemoryDC, hColorPen[(ch%16)+1]);

			bptr = (BYTE *)byteDataBuffer[ch];

			MoveToEx(hMemoryDC, 0, (int)(yratio * (255-bptr[hOffset])), NULL);
			for(j=1; j<totalData; j++)
			{
				int x = (int)(xratio * j);
				LineTo(hMemoryDC, x, (int)(yratio * (255-bptr[j+hOffset])));
				if(x > rc.right)
					break;
			}
		}
		
		BitBlt(hdc, 0, 0, rc.right, rc.bottom, hMemoryDC, 0, 0 , SRCCOPY);
		DeleteObject(hMemoryBitmap);
		DeleteDC(hMemoryDC);

		EndPaint(hWnd, &ps);
		}
		break;
	
	case WM_SIZE:
		GetClientRect(hWnd, &rc);
		if(cursor2 >= rc.right)
			cursor2 = rc.right - 1;
		if(cursor1 >= cursor2)
			cursor1 = cursor2 - 1;
		InvalidateRect(hWnd, NULL, FALSE);
		break;

	case WM_CLOSE:
		confirmSaveParameters(hWnd);

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

// Mesage handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
    return FALSE;
}


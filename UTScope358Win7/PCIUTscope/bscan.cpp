#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include "UT.h"
#include "oneentry.hpp"
#include "resource.h"
#include "clip.hpp"
#include <io.h>

#define MAXBSCANPOINT 99999

void writePrivateProfileFloat(char *sec,
			char *item, float num, char *file);
float getPrivateProfileFloat(char *sec,
			char *item, float num, char *file);
float getSampleRateHz();
HDC	GetPrinterHndl();
int RetrieveFilepathName(HWND hwnd, char* filePath, int open, char* comments);
float unitConvert(int num);

extern ParmsProc Parms;
HWND bscanHwnd=NULL;
char *bscanSection={"bscn"};
int	bscanType=0, bscanBase=0, totalBscanPoint=0, totalTimePoint=1000, currentElement, oldElement,
	bCursor=100;
float maxThick=0.1, totalDistance=10, motionRatio=0.0001, // unit/encoder count
		scanInterval=0.04, xratio;
BYTE amp[MAXBSCANPOINT], scanning=0;

extern float 	vUnitConvert[];
extern HPEN	hColorPen[];
extern HPEN	hColorDotPen[];
extern HBRUSH	hColorBrush[];
extern COLORREF	clr[17];
extern TCHAR szTitle[];								// The title bar text
extern HINSTANCE hInst;								// current instance
extern WORD tof[];

extern char gstr[], gstr2[256], *initFile, modelName[], filePath[], *titleBar, centeringDCoffset,
			parameterChanged;
extern int totalWaveform, postDelay, hOffset, zoomFactor, XencoderValue, cursor1, cursor2, nGate[], 
	cardIndex, waveIndex, vUnit;
extern BYTE byteDataBuffer[MAX_WAVEFORM][32000], amplitude[];

DLGPROC bscanCallBack;

void enableBscanButtons(int enable)
{
	EnableWindow(GetDlgItem(bscanHwnd, 200), enable);
	EnableWindow(GetDlgItem(bscanHwnd, 201), enable);
	EnableWindow(GetDlgItem(bscanHwnd, 202), enable);
	EnableWindow(GetDlgItem(bscanHwnd, 203), enable);

	EnableWindow(GetDlgItem(bscanHwnd, 100), enable);
	EnableWindow(GetDlgItem(bscanHwnd, 103), enable);

	EnableWindow(GetDlgItem(bscanHwnd, 51), enable);
	EnableWindow(GetDlgItem(bscanHwnd, 52), enable);
	EnableWindow(GetDlgItem(bscanHwnd, 2), enable);

	if(enable)
		SetDlgItemText(bscanHwnd, 50, "Start");
	else
		SetDlgItemText(bscanHwnd, 50, "Stop");
}

void updateBscan()
{
	RECT r;
	int x1, x2;

	if(oldElement < currentElement)
	{
		x1 = oldElement;
		x2 = currentElement;
	}
	else
	{
		x1 = currentElement;
		x2 = oldElement;
	}

	GetClientRect(bscanHwnd, &r);

	r.left = x1 * xratio;
	r.right = x2 * xratio;
	r.top = r.bottom - 128;

	InvalidateRect(bscanHwnd, &r, FALSE);
	UpdateWindow(bscanHwnd);
}

void	updateBscanReading(HWND hwnd)
{
	if(bCursor < 0 || bCursor >= totalBscanPoint)
		return;

	if(bscanBase)	// distance 
	{
		sprintf(gstr, "%f", scanInterval * bCursor);
	}
	else	// time
	{
		sprintf(gstr, "%d", bCursor);
	}
	SetDlgItemText(hwnd, 300, gstr);

	if(bscanType)	// thickness
	{
		sprintf(gstr, "%f", maxThick * amp[bCursor] / 255);
	}
	else
	{
		int tempamp = (int)((float)amp[bCursor] / vUnitConvert[vUnit]);
		sprintf(gstr, "%d", tempamp);
		if(vUnit)
			strcat(gstr, "%");
	}
	SetDlgItemText(hwnd, 301, gstr);
}

void processBscan()
{
	BYTE tempAmp;
	float thick;

	if(!scanning)
		return;

	if(bscanBase)	// distance Based
	{
		currentElement = (XencoderValue * motionRatio) / scanInterval;
	}
	else 	// time based
	{
		currentElement++;
	}

	if(bscanType)	// thickness
	{
		if(nGate[waveIndex] == 1)
			thick = unitConvert(tof[0]);
		else
			thick = unitConvert(tof[1]-tof[0]); // show the distance from gate 0
		thick = (float)(255.0 * thick / maxThick);
		if(thick < 0)
			tempAmp = 0;
		else if(thick > 255)
			tempAmp = 255;
		else
			tempAmp = (BYTE)thick;
	}
	else	// flaw
	{
		tempAmp = amplitude[0];
	}
	
	if(currentElement >= 0 && currentElement < totalBscanPoint)
	{
		amp[currentElement] = tempAmp;
	}
	else if(currentElement >= totalBscanPoint)
	{
		scanning = 0;
		oldElement = 0;
		currentElement = totalBscanPoint - 1;
		InvalidateRect(bscanHwnd, NULL, FALSE);
		enableBscanButtons(1);
	}

	if(labs(currentElement - oldElement) > 5)
	{
		updateBscan();
		oldElement = currentElement;
	}
}

void bscanClose(HWND)
{
	RECT r;

	if(IsWindow(bscanHwnd))
	{
		GetWindowRect(bscanHwnd, &r);	// save position
		sprintf(gstr, "%d", r.top);
		WritePrivateProfileString("win", "rT", gstr, initFile);
		sprintf(gstr, "%d", r.left);
		WritePrivateProfileString("win", "rL", gstr, initFile);

		DestroyWindow(bscanHwnd);	 /* Exits the dialog box	     */
		FreeProcInstance( (FARPROC) bscanCallBack);
		bscanHwnd = NULL;
	}
}

void readbscanNow(HWND hwnd, char *fileName)
{
	maxThick = getPrivateProfileFloat(bscanSection, "mThick", (float)0.5, fileName);
	totalDistance = getPrivateProfileFloat(bscanSection, "tDistance", (float)10, fileName);
	motionRatio = getPrivateProfileFloat(bscanSection, "mRatio", (float)0.01, fileName);
	scanInterval = getPrivateProfileFloat(bscanSection, "sInterval", (float)0.04, fileName);

	GetPrivateProfileString(bscanSection, "bType", "0", gstr, 20, fileName);
	bscanType = atoi(gstr);

	GetPrivateProfileString(bscanSection, "bBase", "0", gstr, 20, fileName);
	bscanBase = atoi(gstr);

	GetPrivateProfileString(bscanSection, "tPoint", "1000", gstr, 20, fileName);
	totalTimePoint = atoi(gstr);
}

bscanInitDialog(HWND hwnd)
{
	int j;

	SendDlgItemMessage(hwnd, 100, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 100, CB_ADDSTRING, 0, (long)"Flaw");
	SendDlgItemMessage(hwnd, 100, CB_ADDSTRING, 0, (long)"Thickness");
	SendDlgItemMessage(hwnd, 100, CB_SETCURSEL, bscanType, 0);

	SendDlgItemMessage(hwnd, 103, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 103, CB_ADDSTRING, 0, (long)"Time based");
	SendDlgItemMessage(hwnd, 103, CB_ADDSTRING, 0, (long)"Distance based");
	SendDlgItemMessage(hwnd, 103, CB_SETCURSEL, bscanBase, 0);

	if(bscanType)	// thickness
	{
		j = SW_SHOW;
		SetDlgItemText(hwnd, 2005, "Thickness:");
	}
	else
	{
		j = SW_HIDE;
		SetDlgItemText(hwnd, 2005, "Amplitude:");
	}
	
	ShowWindow(GetDlgItem(hwnd, 2000), j);
	ShowWindow(GetDlgItem(hwnd, 200), j);

	if(bscanBase)	// distance based
	{
		j = SW_SHOW;
		SetDlgItemText(hwnd, 2001, "Total Distance:");
		SetDlgItemText(hwnd, 2004, "Cursor Distance:");

		sprintf(gstr, "%f", totalDistance);
	}
	else
	{
		j = SW_HIDE;
		SetDlgItemText(hwnd, 2001, "Total Points:");
		SetDlgItemText(hwnd, 2004, "Cursor Point:");
		sprintf(gstr, "%d", totalTimePoint);
	}
	SetDlgItemText(hwnd, 201, gstr);
	
	ShowWindow(GetDlgItem(hwnd, 2002), j);
	ShowWindow(GetDlgItem(hwnd, 2003), j);
	ShowWindow(GetDlgItem(hwnd, 202), j);
	ShowWindow(GetDlgItem(hwnd, 203), j);

	sprintf(gstr, "%f", maxThick);
	SetDlgItemText(hwnd, 200, gstr);

	sprintf(gstr, "%f", motionRatio);
	SetDlgItemText(hwnd, 202, gstr);

	sprintf(gstr, "%f", scanInterval);
	SetDlgItemText(hwnd, 203, gstr);

	return TRUE;
}

void savebscanNow(HWND hwnd, char *fileName)
{
	writePrivateProfileFloat(bscanSection, "mThick", maxThick, fileName);
	writePrivateProfileFloat(bscanSection, "tDistance", totalDistance, fileName);
	writePrivateProfileFloat(bscanSection, "mRatio", motionRatio, fileName);
	writePrivateProfileFloat(bscanSection, "sInterval", scanInterval, fileName);

	sprintf(gstr, "%d", bscanType);
	WritePrivateProfileString(bscanSection, "bType", gstr, fileName);

	sprintf(gstr, "%d", bscanBase);
	WritePrivateProfileString(bscanSection, "bBase", gstr, fileName);

	sprintf(gstr, "%d", totalTimePoint);
	WritePrivateProfileString(bscanSection, "tPoint", gstr, fileName);
}

char *bscanExt={"B-scan Files(*.BSN)|*.BSN||"}; 

int haveBscanFileExt(HWND, char* pathStr)
{
	int len = strlen(pathStr) - 1;
	char s1[5] = {".BSN"};
	char s2[5] = {".bsn"};
	char tmp[5];
	strcpy(tmp, &pathStr[len-3]);
	if((strcmp(s1, tmp)==0)||(strcmp(s2, tmp)==0))
		return 1;

	return 0;
}

void saveBscanData(HWND hwnd)
{
	char pathStr[256];
	int dummy=0;

	pathStr[0] = '#'; 
	strcpy(&pathStr[1], bscanExt);
	RetrieveFilepathName(hwnd, pathStr, 0, "Save B-scan File As");
	
	if(!haveBscanFileExt(hwnd, pathStr))	// no extension?
		strcat(pathStr, ".BSN");	// add extension

	if(access(pathStr, 0) == 0) 
	{		// file exsits?
		if(MessageBox(hwnd, "File exists!\n\nDo you want to overwrite?",
				pathStr, MB_YESNO | MB_ICONQUESTION) != IDYES)
			return;
	}

	if(pathStr[0] == '#')   //if not cancel the dialog
	{
		return;
	}

	FILE *fptr = fopen(pathStr, "wb");
	if(fptr == NULL)
	{
		MessageBox(hwnd, "Cannot open B-scan file.", pathStr, MB_OK);
		return;
	}
	fwrite(bscanSection, 4, 1, fptr);	// file indicator
	fwrite(&bscanType, 4, 1, fptr);
	fwrite(&bscanBase, 4, 1, fptr);
	fwrite(&totalBscanPoint, 4, 1, fptr);
	fwrite(&totalTimePoint, 4, 1, fptr);
	fwrite(&maxThick, 4, 1, fptr);
	fwrite(&totalDistance, 4, 1, fptr);
	fwrite(&motionRatio, 4, 1, fptr);
	fwrite(&scanInterval, 4, 1, fptr);
	fwrite(&totalDistance, 4, 1, fptr);

	fwrite(&dummy, 4, 1, fptr);	// reserved
	fwrite(&dummy, 4, 1, fptr);
	fwrite(&dummy, 4, 1, fptr);
	fwrite(&dummy, 4, 1, fptr);
	fwrite(&dummy, 4, 1, fptr);

	fwrite(amp, totalBscanPoint, 1, fptr);

	fflush(fptr);
	fclose(fptr);
}

void readBscanData(HWND hwnd)
{
	char pathStr[256];
	int dummy;
	RECT r;

	pathStr[0] = '#';		//set a flag of first char if canceled 
	strcpy(&pathStr[1], bscanExt);
	//call DLL func for OPEN
	RetrieveFilepathName(hwnd, pathStr, 1, "Open B-scan File");

	if(pathStr[0] == '#')
		return;

	FILE *fptr = fopen(pathStr, "rb");
	if(fptr == NULL)
	{
		MessageBox(hwnd, "Cannot open B-scan file.", pathStr, MB_OK);
		return;
	}

	fread(gstr, 4, 1, fptr);	// file indicator
	gstr[4] = 0;

	if(strcmp(gstr, bscanSection))	// not equal
	{
		fclose(fptr);
		MessageBox(hwnd, "It is not a B-scan file.", pathStr, MB_OK);
		return;
	}

	fread(&bscanType, 4, 1, fptr);
	fread(&bscanBase, 4, 1, fptr);
	fread(&totalBscanPoint, 4, 1, fptr);
	fread(&totalTimePoint, 4, 1, fptr);
	fread(&maxThick, 4, 1, fptr);
	fread(&totalDistance, 4, 1, fptr);
	fread(&motionRatio, 4, 1, fptr);
	fread(&scanInterval, 4, 1, fptr);
	fread(&totalDistance, 4, 1, fptr);

	fread(&dummy, 4, 1, fptr);	// reserved
	fread(&dummy, 4, 1, fptr);
	fread(&dummy, 4, 1, fptr);
	fread(&dummy, 4, 1, fptr);
	fread(&dummy, 4, 1, fptr);

	if(totalBscanPoint > MAXBSCANPOINT)
		totalBscanPoint = MAXBSCANPOINT;
	fread(amp, totalBscanPoint, 1, fptr);

	fflush(fptr);
	fclose(fptr);

	oldElement = 0;
	currentElement = totalBscanPoint - 1;

	GetClientRect(hwnd, &r);
	xratio = (float)r.right / totalBscanPoint;
	
	bscanInitDialog(hwnd);

	InvalidateRect(hwnd, NULL, FALSE);
	enableBscanButtons(1);
}

bscanCommand(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	RECT r;
	int j;

	WORD id = LOWORD(wParam);
	if(HIWORD(wParam) == CBN_SELCHANGE) 
	{
		switch (id)
		{
		case 100:	// type
			bscanType = SendDlgItemMessage(hwnd, 100, CB_GETCURSEL, 0, 0);
			bscanInitDialog(hwnd);
			parameterChanged = 1;
			break;

		case 103:	// based
			bscanBase = SendDlgItemMessage(hwnd, 103, CB_GETCURSEL, 0, 0);
			bscanInitDialog(hwnd);
			parameterChanged = 1;
			break;
		}
	}
	else
	{
		switch (id)
		{
		case 200:	// max thickness
			maxThick = oneEntry(hInst, hwnd, "Maximum Thickness", "", 
				maxThick,
				(float)0.001, (float)200.0, 3);
			sprintf(gstr, "%f", maxThick);
			SetDlgItemText(hwnd, 200, gstr);
			parameterChanged = 1;
			break;

		case 201:	// total waveforms or total distance
			if(bscanBase)	// distance
			{
				totalDistance = oneEntry(hInst, hwnd, "Total Distance", "", 
					(float)totalDistance,
					(float)1.0, (float)99999.0, 3);
				sprintf(gstr, "%f", totalDistance);
			}
			else
			{
				totalTimePoint = (int)oneEntry(hInst, hwnd, "Total B-scan Points", "", 
					(float)totalTimePoint,
					(float)100.0, (float)MAXBSCANPOINT, 0);
				sprintf(gstr, "%d", totalTimePoint);
			}
			SetDlgItemText(hwnd, 201, gstr);
			parameterChanged = 1;
			break;

		case 202:	// motion ratio
			motionRatio = oneEntry(hInst, hwnd, "Motion Ratio", "unit/encoder count", 
				motionRatio,
				(float)0.0000001, (float)1.0, 3);
			sprintf(gstr, "%f", motionRatio);
			SetDlgItemText(hwnd, 202, gstr);
			parameterChanged = 1;
			break;

		case 203:	// interval
			scanInterval = oneEntry(hInst, hwnd, "Scan Interval", "", 
				scanInterval,
				(float)0.0000001, (float)1.0, 3);
			sprintf(gstr, "%f", scanInterval);
			SetDlgItemText(hwnd, 203, gstr);
			parameterChanged = 1;
			break;

		case 2:	// close
			bscanClose(hwnd);
			break;

		case 50:	// start
			if(scanning)
			{
				scanning = 0;
				oldElement = 0;
				currentElement = totalBscanPoint - 1;
				InvalidateRect(hwnd, NULL, FALSE);
				enableBscanButtons(1);
				break;
			}
			if(bscanBase)	// distance
			{
				totalBscanPoint = totalDistance / scanInterval;
				Parms(SETENCODERCOUNTER, cardIndex, 0, 0);	// reset encoder
			}
			else
				totalBscanPoint = totalTimePoint;
			if(totalBscanPoint > MAXBSCANPOINT)
				totalBscanPoint = MAXBSCANPOINT;
			currentElement = -1;
			oldElement = 0;
			scanning = 1;
			GetClientRect(hwnd, &r);
			xratio = (float)r.right / totalBscanPoint;
			enableBscanButtons(0);

			for(j=0; j<totalBscanPoint; j++)
				amp[j] = 0;
			InvalidateRect(hwnd, NULL, FALSE);
			UpdateWindow(hwnd);	// erase the old trace

			break;

		case 51:	// save b-scan
			saveBscanData(hwnd);
			break;

		case 52:	// read b-scan
			readBscanData(hwnd);
			break;
		}
	}

	return TRUE;
}

void bscanHscroll(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam)) 
	{
	case SB_LINEDOWN:
		bCursor++;
		break;

	case SB_LINEUP:
		bCursor--;
		break;
	}
	if(bCursor < 0)
		bCursor = 0;
	else if(bCursor >= totalBscanPoint)
		bCursor = totalBscanPoint;
	updateBscanReading(hwnd);
	InvalidateRect(hwnd, NULL, FALSE);
	UpdateWindow(hwnd);
}

LRESULT  bscanHndl(HWND hwnd,
	unsigned iMessage, WPARAM wParam, LPARAM lParam)
{
	RECT rc;
	int result=FALSE;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (iMessage) 
	{
	case WM_INITDIALOG:
		result = bscanInitDialog(hwnd);
		break;

	case WM_COMMAND:
		bscanCommand(hwnd, wParam, lParam);
        break;

	case WM_CLOSE:
		bscanClose(hwnd);
		break;

	case WM_LBUTTONDOWN:
		if(xratio != 0)
		{
			bCursor = (int)((float)LOWORD(lParam)/xratio); 
			updateBscanReading(hwnd);
			InvalidateRect(hwnd, NULL, FALSE);
			UpdateWindow(hwnd);	// erase the old trace
		}
		break;

	case WM_HSCROLL:
		bscanHscroll(hwnd, wParam, lParam);
		break;

	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rc);
		Rectangle(hdc, 0, rc.bottom-128, rc.right, rc.bottom);
		if(totalBscanPoint > 0)
		{
			SelectObject(hdc, hColorPen[0]);	// black color
			GetClientRect(hwnd, &rc);
			MoveToEx(hdc, 0, rc.bottom-(amp[0]>>1), NULL);
			for(int j=0; j<totalBscanPoint; j++)
				LineTo(hdc, j*xratio, rc.bottom-(amp[j]>>1));
			SelectObject(hdc, hColorPen[7]);	// red color
			MoveToEx(hdc, bCursor * xratio, rc.bottom-128, NULL);
			LineTo(hdc, bCursor * xratio, rc.bottom);
		}
		EndPaint(hwnd, &ps);
		break;
	}
	return result;
}

HWND openbscan(HINSTANCE hInstance, HWND masterHwnd)
{
	RECT r;

	if(bscanHwnd) 
	{
		BringWindowToTop(bscanHwnd);
    }
	else 
	{
		FARPROC function = (FARPROC) bscanHndl;
		bscanCallBack = (DLGPROC) MakeProcInstance(function, hInstance);
		bscanHwnd = CreateDialog(hInstance,
			"BSCAN", masterHwnd, bscanCallBack);
		if(bscanHwnd == NULL) 
		{
			MessageBox(masterHwnd, "Cannot open B-scan window!",
				NULL, MB_OK);
			return NULL;
        }

		GetWindowRect(bscanHwnd, &r);	// get position of this dialog
		r.right -= r.left;
		r.bottom-= r.top;
		GetPrivateProfileString("win", "rT", "0", gstr, 255, initFile);
		r.top = atoi(gstr);
		if(r.top < 0) r.top = 0;

		GetPrivateProfileString("win", "rL", "50", gstr, 255, initFile);
		r.left = atoi(gstr);
		if(r.left < 0) r.left = 0;
		MoveWindow(bscanHwnd, r.left, r.top, r.right, r.bottom, FALSE);

		ShowWindow(bscanHwnd, SW_SHOWNORMAL);
	}
	return bscanHwnd;
}


#include "stdafx.h"
#include <stdio.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>
#include "oneentry.hpp"

// child control IDs
#define ID_OLDVALUETEXT	100
#define ID_ENTRY1	101
#define ID_RANGE	102

char *strText={"STR"};

char lstr[128],
	*oneEntryDlg={"ONEENTRY"},
	*strEntryDlg={"STRENTRY"};

char	parmName[128], parmUnit[20];
int	decimalnum;
float	oldValue, newValue, rangeStart, rangeEnd;

int OK(HWND hwnd, WORD, LONG)
{
	GetDlgItemText(hwnd, ID_ENTRY1, parmName, sizeof(parmName) );

	if(strcmp(parmUnit, strText) == 0)
	{
		return TRUE;    	
    }
	else if(parmName[0] == 0)
		newValue = oldValue;
	else if(strcmp(parmUnit, "HEX") == 0)
	{
		sscanf(parmName, "%x", &decimalnum);
		newValue = (float)decimalnum;
    }
	else 
		newValue = (float)atof(parmName);

	if(newValue < rangeStart)
		newValue = rangeStart;
	else if(newValue > rangeEnd)
		newValue = rangeEnd;

	return TRUE;
}

int WMInitDialog(HWND hwnd, WORD, LONG)
{
	char str[80], st[80];

	sprintf(lstr, "%s %s", "Enter", parmName);

	if(strcmp(parmUnit, "HEX") == 0)	//equal
	{
		sprintf(str, "%X %s",	(int)oldValue, parmUnit);
		sprintf(st, "%X-%X %s", (int)rangeStart, (int)rangeEnd, parmUnit);
	}
	else if(strcmp(parmUnit, strText) == 0)	//equal
    {
		goto initDone;
	}
	else
	{
		switch (decimalnum) {
    	    case 0:
			sprintf(str, "%.0f %s",	oldValue, parmUnit);
			sprintf(st, "%s (%.0f - %.0f %s ):",
				"New Value", rangeStart, rangeEnd, parmUnit);
			break;

		case 1:
			sprintf(str, "%.1f %s",	oldValue, parmUnit);
			sprintf(st, "%s (%.1f - %.1f %s ):",
				"New Value", rangeStart, rangeEnd, parmUnit);
			break;

		case 2:
			sprintf(str, "%.2f %s",	oldValue, parmUnit);
			sprintf(st, "%s (%.2f - %.2f %s ):",
				"New Value", rangeStart, rangeEnd, parmUnit);
			break;

		default:
			sprintf(str, "%.3f %s",	oldValue, parmUnit);
			sprintf(st, "%s (%.3f - %.3f %s ):",
				"New Value", rangeStart, rangeEnd, parmUnit);
			break;
        }
	}

	SetDlgItemText(hwnd, ID_OLDVALUETEXT, str);
	SetDlgItemText(hwnd, 1002, st);

initDone:

	SetWindowText(hwnd, lstr);

	return TRUE;
}

int WMCommand(HWND hwnd, WORD wParam, LONG lParam)
{
	switch (LOWORD(wParam))
	{
	case IDOK:
	    OK(hwnd, wParam, lParam);

	    EndDialog(hwnd, TRUE);
	    return TRUE;

	case IDCANCEL:
		*parmName = 0;
	    newValue = oldValue;

	    EndDialog(hwnd, FALSE);
	    return TRUE;
	}

	return FALSE;
}

int FAR PASCAL WidthEnterHndl(HWND hwnd, unsigned iMessage,
				  unsigned wParam, LONG lParam)
{
	int result;

	switch (iMessage)
	{
	case WM_INITDIALOG:
	    result = WMInitDialog(hwnd, wParam, lParam);
	    return result;

	case WM_COMMAND:
	    result = WMCommand(hwnd, wParam, lParam);
	    return result;

	case WM_DESTROY:
	default:
		break;
	}
	return FALSE;
}

float oneEntry(HINSTANCE hInstance,
	HWND hwnd,
	char *name, char *unit, float oldval, float range1,
	float range2, int decimal)
{
	FARPROC lpProcEnter;

	strcpy(parmName, name);
	strcpy(parmUnit, unit);
	oldValue	= oldval;
    decimalnum	= decimal;
	rangeStart	= range1;
    rangeEnd	= range2;

	lpProcEnter = MakeProcInstance((FARPROC)WidthEnterHndl, hInstance);

	if(strcmp(parmUnit, strText) == 0)
    {
		DialogBox(hInstance, strEntryDlg, hwnd, (DLGPROC)lpProcEnter);
        strcpy(name, parmName);
    }
	else
		DialogBox(hInstance, oneEntryDlg, hwnd, (DLGPROC)lpProcEnter);
//		(int (pascal far*)(HWND, unsigned int,unsigned int,LONG))
//		WidthEnterHndl);

	return newValue;
}

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
#include "DSPASDK.H"
#include "oneentry.hpp"

void updateInputStatus();
void	copyDataToClipboard(HWND hwnd);
float unitConvert(int num);

HWND	paramHwnd = NULL;
extern ParmsProc Parms;

HANDLE threadHndl=NULL;
DWORD dataSetCounter;

BYTE amplitude[10];
WORD tof[10];
int reportItems, XencoderValue, YencoderValue, minVoltage, maxVoltage, minPulseWidth,
	indexLPF=0, samplingRate=100000000,
	nGate[256]={0,0,0,0,0,0,0,0,0,0,0,0},	// no gate
	gateIndex[256]={0,0,0,0,0,0,0,0,0,0,0,0}, 
	activePin[256]={0,0,0,0,0,0,0,0,0,0,0,0}, 
	activeLogic[256]={0,0,0,0,0,0,0,0,0,0,0,0}, 
	outputDuration[256]={1,1,1,1,1,1,1,1,1,1,1,1}, 
	gateType[256][10]={0,0,0,0,0,0,0,0,0,0,0,0}, 
	gateStart[256][10], gateLength[256][10], gateThreshold[256][10], 
	gateAlgorithm[256][10]={0,0,0,0,0,0,0,0,0,0,0,0},
	indexHPF=1, 
	indexSamplingRate=0, // highest sampling rate
	postDelay=0,		// default: 0 sample
	indexTrgSrc=1,	// soft
	waveIndex=0,
	memorySize=8188,
	damping[256],
	totalChan=1,
	hardwareRev=0,
	hunit=1,
	roundTrip=BST_CHECKED,
	waveformLength=1000;// default: 1000 samples

float	gain[256]={20, 20, 20, 20},
	velocity=(float)0.232;

char scanning=0, parameterChanged=0, pause=0, expendDlg=0,
	lpfLabel[8][20]={"All", "48 MHz", "28 MHz", "18 Mhz",
		"8.8 MHz", "7.5 MHz", "6.7 MHz", "5.9 MHz"},
	hpfLabel[4][20]={"4.8 MHz", "1.8 MHz", "0.8 MHz", "0.6 MHz"},
	*trgSrcLabel[4]={"+EXT TRIG", "Software", "-EXT TRIG", "Soft Trig"},
	*multiChanDamping[]={"500", "50"},
	*singleChanDamping[]={"620", "339", "202", "159", "60", "55", "50", "47"},
	*rateLabel[8]={"100MHz","50MHz", "25MHz", "12.5MHz", "6.25MHz", "3.125MHz","1.5725MHz", "781kHz"},
	*hunitLabel[4]={"sp", "us", "in", "mm"},
	*hunitLongLabel[4]={"sample", "micro second", "inch", "mm"},
	*hunitDigit[4]={"%.0f", "%.3f", "%.3f", "%.1f"},
	*algLabel[5]={"Max peak", "Min valley", "Rising Edge", "Falling Edge", "Signal loss"},
	*outpinLabel[17]={"None","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","10"},

	// device independent
	*section={"PCIUT"},
	*dllName={"PCIUTDLL.dll"}, //"PCIUTDLL.DLL", "dspASDK.dll", , "ExUT.DLL", "GiUT.DLL"},
	*functionName={"PCIUTParms"};	//"PCIUTParms", "Parms", , "ExUTParms", "GiUTParms"},

int pulseVoltage=40,
	dcOffset[256]={0, 0, 0, 0},
	txChan[256]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
	rxChan[256]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
	signal[256]={3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},	// 0=full, 1=+half, 2=-half, 3=RF
	transducerMode[256]={0, 0, 0, 0},
	pulseWidth[256]={50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50},
	pulseFrequency,
	pulsePolarity,
	pulseCycle, pulseType, voltageType;

extern int totalCards, cardIndex, optionByte1, optionByte2, totalWaveform, zoomFactor, hOffset,
	vUnit;
extern char gstr[], gstr2[256], *initFile, modelName[];
extern BYTE byteDataBuffer[MAX_WAVEFORM][32000];
extern HINSTANCE hInst;								// current instance
extern RECT originalWindowSize;
extern HWND hWnd;
extern HPEN	hColorPen[];
extern float 	vUnitConvert[];

int checkIntValue(int current, int min, int max)
{
	if(current < min)
		return min;
	if(current > max)
		return max;

	return current;
}

void drawGates(HWND hwnd, HDC hMemoryDC, double xratio, double yratio)
{
	int g, x, y, start0;

	SelectObject(hMemoryDC, hColorPen[14]);
	for(g=0; g<nGate[waveIndex]; g++)
	{
		y = (int)(yratio * (255-gateThreshold[waveIndex][g]));
		if(g && gateType[waveIndex][g])
			start0 = tof[0];
		else
			start0 = 0;
		x = (int)(xratio * (gateStart[waveIndex][g]-hOffset + start0));
		MoveToEx(hMemoryDC, x, y, NULL);
		x = x + (int)(xratio * gateLength[waveIndex][g]);
		LineTo(hMemoryDC, x, y);
	}
}

int getTotalBoards()
{
	int j=0;
	while(1)
	{
		Parms(GETSERIALNUMBER, j, (WPARAM)gstr, 0);
		if(j>0)
		{
			if(strcmp(gstr, gstr2) == 0)
				break;
		}
		j++;
		strcpy(gstr2, gstr);
	}
	return j;
}

void writePrivateProfileFloat(char *sec,
			char *item, float num, char *file)
{
	char str[20];
    long *longPtr;
    longPtr = (long *)&num;
	sprintf(str, "%10lx", *longPtr);
	WritePrivateProfileString(sec, item, str, file);
}

float getPrivateProfileFloat(char *sec,
			char *item, float num, char *file)
{
	char str[20];
    long *longPtr;
    longPtr = (long *)&num;
    GetPrivateProfileString(sec, item, "", str, 19, file);
	if(*str)
	    sscanf(str, "%lx", longPtr);

    return num;
}

int oldStatus=-1, IOstatus, oldXencoder=-99999, oldYencoder=-99999;
BYTE oldAmp0=0, oldAmp1=0;
WORD oldtof0=0, oldtof1=0;

void showStatus()
{
	int amp;

	if(oldXencoder != XencoderValue)
	{
		sprintf(gstr, "%d", XencoderValue);
		SetDlgItemText(paramHwnd, 308, gstr);
		oldXencoder = XencoderValue;
	}

	if(oldYencoder != YencoderValue)
	{
		sprintf(gstr, "%d", YencoderValue);
		SetDlgItemText(paramHwnd, 309, gstr);
		oldYencoder = YencoderValue;
	}

	if(nGate[waveIndex] > 0 && (oldAmp0 != amplitude[0] || oldtof0 != tof[0]))
	{
		amp = (int)((float)amplitude[0] / vUnitConvert[vUnit]);
		sprintf(gstr, "%d", amp);
		if(vUnit)
			strcat(gstr, "%");
		SetDlgItemText(paramHwnd, 310, gstr);

		sprintf(gstr, hunitDigit[hunit], unitConvert(tof[0]));
		strcat(gstr, hunitLabel[hunit]);
		SetDlgItemText(paramHwnd, 311, gstr);
	}
	else
	{
		SetDlgItemText(paramHwnd, 310, "");
		SetDlgItemText(paramHwnd, 311, "");
	}

	if(nGate[waveIndex] > 1 && (oldAmp1 != amplitude[1] || oldtof1 != tof[1]))
	{
		amp = (int)((float)amplitude[1] / vUnitConvert[vUnit]);
		sprintf(gstr, "%d", amp);
		if(vUnit)
			strcat(gstr, "%");
		SetDlgItemText(paramHwnd, 312, gstr);

		if(gateType[waveIndex][1])	// slave gate
			sprintf(gstr, hunitDigit[hunit], unitConvert(tof[1]-tof[0])); // show the distance from gate 0
		else
			sprintf(gstr, hunitDigit[hunit], unitConvert(tof[1]));
		strcat(gstr, hunitLabel[hunit]);
		SetDlgItemText(paramHwnd, 313, gstr);
	}
	else
	{
		SetDlgItemText(paramHwnd, 312, "");
		SetDlgItemText(paramHwnd, 313, "");
	}

	if(oldStatus == IOstatus)
		return;		// do not waste time to display the same thing

	for(int j=0; j<16; j++)
	{
		if(IOstatus & (1 << j))
			SetDlgItemText(paramHwnd, j+5000, "1");
		else
			SetDlgItemText(paramHwnd, j+5000, "0");
	}
	oldStatus = IOstatus;
}

void	getCardInfo()
{
	samplingRate= Parms(SETSAMPLINGRATE, cardIndex, 0, 0);
	memorySize  = Parms(GETMEMORYSIZE, cardIndex, 0, 0);
	optionByte1 = Parms(GETOPTIONBYTE1, cardIndex, 0, 0);
	optionByte2 = Parms(GETOPTIONBYTE2, cardIndex, 0, 0);
	hardwareRev = Parms(GETREVNUMBER, cardIndex, 2, 0);
	minVoltage  = Parms(GETVMIN, cardIndex, 0, 0);
	maxVoltage  = Parms(GETVMAX, cardIndex, 0, 0);
	minPulseWidth=Parms(GETMINPULSEWIDTH, cardIndex, 0, 0);
}

float getSampleRateMHz()
{
	return (float)(samplingRate / 1000000) / (1 << indexSamplingRate);
}

float unitConvert(int num) 
{
	float fnum;
	switch (hunit)
	{
	case 0:	// samples
		return (float)num;

	case 1:	// micro seconds
		return (float)num / getSampleRateMHz();

	case 2: // inch
	case 3:	// mm
		fnum = (float)num / getSampleRateMHz() * velocity;
		if(roundTrip == BST_CHECKED)
			fnum /= 2;
		if(hunit == 3)	// mm
			fnum *= (float)25.4;
		return fnum;
	}
	return 1.0;
}

void setHscrollBar(HWND hwnd)
{
	WORD hscrollBarPos;

	if(hOffset > waveformLength-10)
       	hOffset = waveformLength-10;
	else if(hOffset < 0)
		hOffset = 0;

	hscrollBarPos = (LONG)1000 * hOffset/ waveformLength;
	SetScrollPos(GetDlgItem(hwnd, 301),
		SB_CTL, hscrollBarPos, TRUE);

	sprintf(gstr, hunitDigit[hunit], unitConvert(hOffset));
	SetDlgItemText(hwnd, 119, gstr);
}

int getPulseWidthIndex(int w)
{
	int j, narrow;
	if(w == minPulseWidth)
		return 0;
	if(w <= 500)	// wide pulse
	{
		j = 161.67 *(w-15)/(760.8-w+15);
		narrow = 1;
	}
	else
	{
		j = ((float)w - 500.0) / 5.882;
		narrow = 0;
	}

	if(j < 0)
		return 0;
	if(j > 255)
		return 255;

	Parms(SETPULSEWIDTH , cardIndex, j, narrow);

    return j;
}

int getGainIndex(float g)
{
	int temp;
	if(g <= -20)
		g = -19.5;
	else if(g > 80)
		g = 80;
	temp = (int)
		(204 + g / 0.0978);
	return temp;
}

void checkDataByPointer()
{
	int dwDataLength, jj, g;
	DWORD *dwptr, *tempPtr;

	while(1)
	{
		do {
			dwptr = (DWORD*)Parms(ASDK_GETDATAPOINTER, 0, 0, 0);
			if(!dwptr)
				Sleep(1);	// give CPU a break to take care of other tasks
		} while (dwptr == NULL);

		if(*dwptr != dataSetCounter)	// data buffer is over written
		{
			sprintf(gstr, "My code is too slow. Counter: %d, Actual data: %d", dataSetCounter, *dwptr);
			SetWindowText(hWnd, gstr);
		}
		dwptr++;	// skip waveform counter
		IOstatus = *dwptr >> 16;
		dwptr++;	// skip inputs and indicator
		if(reportItems & ASDK_REPORTXENCODER)
		{
			XencoderValue = *dwptr; // Get X counter
			dwptr++;	// skip X encoder
		}

		if(reportItems & ASDK_REPORTYENCODER)
		{
			YencoderValue = *dwptr; // Get Y counter
			dwptr++;	// skip Y encoder
		}

		for(jj=0; jj<totalWaveform; jj++)
		{
			dwptr++;				// skip data type byte and test #

			dwDataLength = waveformLength >> 2; // convert to number in DWORD
			tempPtr = (DWORD *)byteDataBuffer[jj];

			for(int kk=0; kk<dwDataLength; kk++)
			{
				tempPtr[kk] = *dwptr;
				dwptr++;
			}
				
			if(reportItems | ASDK_REPORTCSCANDATA)
			{
				if(jj == waveIndex)
				{
					for(g=0; g<nGate[jj]; g++)
					{
						amplitude[g] = (BYTE)(*dwptr & 0x000000FF);
						tof[g] = (WORD)((*dwptr)>>16);
						dwptr++;
					}
				}
				else
					dwptr += nGate[jj];
			}
		}
		dataSetCounter++;
	}
}

// download the parameters for the first waveform
void	downloadParameters(HWND hwnd)
{
	int j, g;

	scanning = 0;

	reportItems = ASDK_REPORTXENCODER | ASDK_REPORTCSCANDATA | ASDK_REPORTYENCODER | ASDK_REPORTDIGITALINPUT;

	Parms(ASDK_STARTSCAN, 0, 0, 0);	// stop scan mode

	Parms(ASDK_SETNUMBERTEST, 0, totalWaveform, 0);

	// initialize global parameters
	Parms(SETIOPORT, cardIndex, 0, 0);
	Parms(SETSAMPLINGRATE, cardIndex, indexSamplingRate, 0);
	Parms(SETLOWPASSFILTER,	0, indexLPF, 0);
	Parms(SETHIGHPASSFILTER,0, indexHPF, 0);
	Parms(SETPULSEVOLTAGE2, cardIndex, pulseVoltage, 0);
	Parms(SETADTRIGGERSOURCE, cardIndex, indexTrgSrc, 0);

	// Set transducer mode
	for(j=0; j<totalWaveform; j++)
	{
		if(txChan[j] >= 0 && txChan[j] < 256)
		{
			Parms(SETCHANTRANSDUCERMODE, cardIndex, txChan[j], transducerMode[j]);
			Parms(SETCHANDAMPING, cardIndex, txChan[j], damping[j]);
		}
	}

//	Parms(SETDAMPING, cardIndex, damping[0], 0);
//	Parms(SETTRANSDUCERMODE, cardIndex, transducerMode[0], 0);

	if(optionByte2 & 0x20)	// both pulser types
	{
		Parms(SWITCHPULSETYPE, cardIndex, pulseType, voltageType);
		optionByte2 = Parms(GETOPTIONBYTE2, cardIndex, 0, 0);
	}

	if(optionByte2 & 1)	// tone burst pulser
	{
		Parms(SETPULSEWIDTH, cardIndex, pulseCycle, 0);	// half cycles
		Parms(SETTONEBURSTFREQUENCY, cardIndex, pulseFrequency, pulsePolarity);
	}

	for(j=0; j<totalWaveform; j++)
	{
		Parms(ASDK_SETBUFFERLENGTH,	0, j, waveformLength);	// # of samples
		Parms(ASDK_SETREC,			0, j, signal[j]);	// rectification
		Parms(ASDK_SETCUR,			0, j, 0);	// DAC off
		Parms(ASDK_SETDLY,			0, j, postDelay);	// delay
		Parms(ASDK_SETGAIN,			0, j, getGainIndex(gain[j]) );
		Parms(ASDK_SETRXN,			0, j, rxChan[j]);	// receiver chan
		Parms(ASDK_SETTXN,			0, j, txChan[j]);	// transmit chan
		int n = 511 + dcOffset[j] / 5;
		Parms(ASDK_SETDCOFFSET,		0, j, n);	// center
		if((optionByte2 & 1) == 0)	// regular pulser
		{	// pulse width
			Parms(ASDK_SETPULSEWIDTH,	0, j, getPulseWidthIndex(pulseWidth[j]) );	// pulse width
		}
		Parms(ASDK_SETNGATE,		nGate[j], j, 0);	// # of gates
		Parms(ASDK_SETASCAN,		1, j, 0);			// include A-scan data
		Parms(ASDK_SETOUTPUTPINS,	activePin[j] ? (1<<(activePin[j]-1)) : 0, j, activeLogic[j]);
		Parms(ASDK_SETOUTPUTDURATION, outputDuration[j], j, 0);

		for(g=0; g<nGate[j]; g++)
		{
			Parms(ASDK_SETGATESTART,	g, j, gateStart[j][g]);
			Parms(ASDK_SETGATELENGTH,	g, j, gateLength[j][g]);
			if(gateType[j][g] && g)
				Parms(ASDK_SETGATEALGORITHM,	g, j, gateAlgorithm[j][g] | 0x80);
			else
				Parms(ASDK_SETGATEALGORITHM,	g, j, gateAlgorithm[j][g]);
			Parms(ASDK_SETGATETHRESHOLD,	g, j, gateThreshold[j][g]);
		}
	}

	if(indexTrgSrc == 0 || indexTrgSrc == 2)
	{
		Parms(ASDK_STARTSCAN, 0,	// not used
			5,		// exteranl trigger scan mode
			reportItems);
	}
	else
	{
		Parms(ASDK_STARTSCAN, 20,	// 20 Hz
			1,		// time base scan mode
			reportItems);
	}
	dataSetCounter = 0;
	scanning = 1;

	threadHndl = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)checkDataByPointer,
		NULL, 0, NULL);

	if (threadHndl == NULL)
	{
		MessageBox(hWnd, "Could not start the thread!", NULL, MB_OK);
   		return;
	}

	SetThreadPriority(threadHndl, THREAD_PRIORITY_HIGHEST);
}

void getWaveforms()
{
}

void restartScan(HWND hwnd)
{
	scanning = 0;
	TerminateThread(threadHndl, 0);
	downloadParameters(hwnd);
}

void readParameters(HWND hwnd, char *fileName)
{
	int ch, g;
	char wnum[10];

	GetPrivateProfileString(section, "lpf", "0", gstr, 255, fileName);
	indexLPF = atoi(gstr);

	GetPrivateProfileString(section, "hpf", "3", gstr, 255, fileName);
	indexHPF = atoi(gstr);

	GetPrivateProfileString(section, "sr", "0", gstr, 255, fileName);
	indexSamplingRate = atoi(gstr);

	GetPrivateProfileString(section, "post", "0", gstr, 255, fileName);
	postDelay = atoi(gstr);

	GetPrivateProfileString(section, "length", "1000", gstr, 255, fileName);
	waveformLength = atoi(gstr);

	GetPrivateProfileString(section, "nWave", "1", gstr, 255, fileName);
	totalWaveform = atoi(gstr);

	GetPrivateProfileString(section, "freq", "40", gstr, 25, fileName);
	pulseFrequency = atoi(gstr);

	GetPrivateProfileString(section, "Polar", "0", gstr, 25, fileName);
	pulsePolarity = atoi(gstr);

	GetPrivateProfileString(section, "pType", "0", gstr, 25, fileName);
	pulseType = atoi(gstr);

	GetPrivateProfileString(section, "vType", "0", gstr, 25, fileName);
	voltageType = atoi(gstr);

	GetPrivateProfileString(section, "Cycle", "4", gstr, 25, fileName);
	pulseCycle = atoi(gstr);

	for(ch=0; ch<totalWaveform; ch++)
	{
		sprintf(gstr2, "tx%d", ch);
		GetPrivateProfileString(section, gstr2, "0", gstr, 25, fileName);
		txChan[ch] = atoi(gstr);

		sprintf(gstr2, "rx%d", ch);
		GetPrivateProfileString(section, gstr2, "0", gstr, 25, fileName);
		rxChan[ch] = atoi(gstr);

		sprintf(gstr2, "g%d", ch);
		GetPrivateProfileString(section, gstr2, "30", gstr, 25, fileName);
		gain[ch] = (float)atof(gstr);

		sprintf(gstr2, "dc%d", ch);
		GetPrivateProfileString(section, gstr2, "0", gstr, 25, fileName);
		dcOffset[ch] = atoi(gstr);

		sprintf(gstr2, "mode%d", ch);
		GetPrivateProfileString(section, gstr2, "0", gstr, 25, fileName);
		transducerMode[ch] = atoi(gstr);

		sprintf(gstr2, "width%d", ch);
		GetPrivateProfileString(section, gstr2, "30", gstr, 50, fileName);
		pulseWidth[ch] = atoi(gstr);

		sprintf(gstr2, "sig%d", ch);
		GetPrivateProfileString(section, gstr2, "0", gstr, 25, fileName);
		signal[ch] = atoi(gstr);

		sprintf(wnum, "w%d", ch);

		GetPrivateProfileString(wnum, "ng", "0", gstr, 25, fileName);
		nGate[ch] = atoi(gstr);
		GetPrivateProfileString(wnum, "ad", "0", gstr, 25, fileName);
		outputDuration[ch] = atoi(gstr);
		GetPrivateProfileString(wnum, "ap", "0", gstr, 25, fileName);
		activePin[ch] = atoi(gstr);
		GetPrivateProfileString(wnum, "al", "0", gstr, 25, fileName);
		activeLogic[ch] = atoi(gstr);

		for(g=0; g<nGate[ch]; g++)
		{
			sprintf(gstr2, "ag%d", g);
			GetPrivateProfileString(wnum, gstr2, "0", gstr, 25, fileName);
			gateAlgorithm[ch][g] = atoi(gstr);

			sprintf(gstr2, "gt%d", g);
			GetPrivateProfileString(wnum, gstr2, "0", gstr, 25, fileName);
			gateType[ch][g] = atoi(gstr);

			sprintf(gstr2, "gs%d", g);
			GetPrivateProfileString(wnum, gstr2, "0", gstr, 25, fileName);
			gateStart[ch][g] = atoi(gstr);

			sprintf(gstr2, "gl%d", g);
			GetPrivateProfileString(wnum, gstr2, "0", gstr, 25, fileName);
			gateLength[ch][g] = atoi(gstr);

			sprintf(gstr2, "gth%d", g);
			GetPrivateProfileString(wnum, gstr2, "0", gstr, 25, fileName);
			gateThreshold[ch][g] = atoi(gstr);
		}
	}

	GetPrivateProfileString(section, "volt", "40", gstr, 255, fileName);
	pulseVoltage = atoi(gstr);

	GetPrivateProfileString(section, "tsrc", "1", gstr, 255, fileName);
	indexTrgSrc = atoi(gstr);

	GetPrivateProfileString(section, "z", "100", gstr, 255, fileName);
	zoomFactor = atoi(gstr);

	GetPrivateProfileString(section, "hunit", "1", gstr, 2, fileName);
	hunit = atoi(gstr);

	GetPrivateProfileString(section, "vunit", "1", gstr, 2, fileName);
	vUnit = atoi(gstr);

	velocity = getPrivateProfileFloat(section, "velo", (float)0.232, fileName);

	parameterChanged = 0;
}

void	saveParameters(HWND hwnd, char *fileName)
{
	int g, ch;
	char wnum[10];

	sprintf(gstr, "%d", indexLPF);
	WritePrivateProfileString(section, "lpf", gstr, fileName);

	sprintf(gstr, "%d", indexHPF);
	WritePrivateProfileString(section, "hpf", gstr, fileName);

	sprintf(gstr, "%d", indexSamplingRate);
	WritePrivateProfileString(section, "sr", gstr, fileName);

	sprintf(gstr, "%d", postDelay);
	WritePrivateProfileString(section, "post", gstr, fileName);

	sprintf(gstr, "%d", waveformLength);
	WritePrivateProfileString(section, "length", gstr, fileName);

	sprintf(gstr, "%d", totalWaveform);
	WritePrivateProfileString(section, "nWave", gstr, fileName);

	sprintf(gstr, "%d", pulseFrequency);
	WritePrivateProfileString(section, "freq", gstr, fileName);

	sprintf(gstr, "%d", pulsePolarity);
	WritePrivateProfileString(section, "Polar", gstr, fileName);

	sprintf(gstr, "%d", pulseType);
	WritePrivateProfileString(section, "pType", gstr, fileName);

	sprintf(gstr, "%d", voltageType);
	WritePrivateProfileString(section, "vType", gstr, fileName);

	sprintf(gstr, "%d", pulseCycle);
	WritePrivateProfileString(section, "Cycle", gstr, fileName);

	for(ch=0; ch<totalWaveform; ch++)
	{
		sprintf(gstr, "%d", rxChan[ch]);
		sprintf(gstr2, "rx%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", txChan[ch]);
		sprintf(gstr2, "tx%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%.1f", gain[ch]);
		sprintf(gstr2, "g%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", dcOffset[ch]);
		sprintf(gstr2, "dc%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", transducerMode[ch]);
		sprintf(gstr2, "mode%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", pulseWidth[ch]);
		sprintf(gstr2, "width%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", signal[ch]);
		sprintf(gstr2, "sig%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(wnum, "w%d", ch);

		sprintf(gstr, "%d", nGate[ch]);
		WritePrivateProfileString(wnum, "ng", gstr, fileName);
		sprintf(gstr, "%d", outputDuration[ch]);
		WritePrivateProfileString(wnum, "ad", gstr, fileName);
		sprintf(gstr, "%d", activePin[ch]);
		WritePrivateProfileString(wnum, "ap", gstr, fileName);
		sprintf(gstr, "%d", activeLogic[ch]);
		WritePrivateProfileString(wnum, "al", gstr, fileName);

		for(g=0; g<nGate[ch]; g++)
		{
			sprintf(gstr2, "ag%d", g);
			sprintf(gstr, "%d", gateAlgorithm[ch][g]);
			WritePrivateProfileString(wnum, gstr2, gstr, fileName);

			sprintf(gstr2, "gt%d", g);
			sprintf(gstr, "%d", gateType[ch][g]);
			WritePrivateProfileString(wnum, gstr2, gstr, fileName);

			sprintf(gstr2, "gs%d", g);
			sprintf(gstr, "%d", gateStart[ch][g]);
			WritePrivateProfileString(wnum, gstr2, gstr, fileName);

			sprintf(gstr2, "gl%d", g);
			sprintf(gstr, "%d", gateLength[ch][g]);
			WritePrivateProfileString(wnum, gstr2, gstr, fileName);

			sprintf(gstr2, "gth%d", g);
			sprintf(gstr, "%d", gateThreshold[ch][g]);
			WritePrivateProfileString(wnum, gstr2, gstr, fileName);
		}
	}

	sprintf(gstr, "%d", pulseVoltage);
	WritePrivateProfileString(section, "volt", gstr, fileName);

	sprintf(gstr, "%d", indexTrgSrc);
	WritePrivateProfileString(section, "rsrc", gstr, fileName);

	sprintf(gstr, "%d", zoomFactor);
	WritePrivateProfileString(section, "z", gstr, fileName);

	sprintf(gstr, "%d", hunit);
	WritePrivateProfileString(section, "hunit", gstr, fileName);

	sprintf(gstr, "%d", vUnit);
	WritePrivateProfileString(section, "vunit", gstr, fileName);

	writePrivateProfileFloat(section, "velo", velocity, fileName);

	parameterChanged = 0;
}

void initNumWaveform(HWND hwnd)
{
	SendDlgItemMessage(hwnd, 118, CB_RESETCONTENT, 0, 0);
	for(int j=0; j<totalWaveform; j++)
	{
		sprintf(gstr, "%d", j+1);
		SendDlgItemMessage(hwnd, 118, CB_ADDSTRING, 0, (long)gstr);
	}
	if(waveIndex >= totalWaveform)
		waveIndex = totalWaveform - 1;
	SendDlgItemMessage(hwnd, 118, CB_SETCURSEL, waveIndex, 0);
}

void 	initGateParameters(HWND hwnd)
{
	int j, gate = gateIndex[waveIndex];

	sprintf(gstr, "%d", nGate[waveIndex]);
	SetDlgItemText(hwnd, 122, gstr);

	SendDlgItemMessage(hwnd, 123, CB_RESETCONTENT, 0, 0);	// gate #
	for(j=1; j<=nGate[waveIndex]; j++)
	{
		sprintf(gstr, "%d", j);
		SendDlgItemMessage(hwnd, 123, CB_ADDSTRING, 0, (long)gstr);
	}
	SendDlgItemMessage(hwnd, 123, CB_SETCURSEL, gateIndex[waveIndex], 0);

	sprintf(gstr, "%d", outputDuration[waveIndex]);
	SetDlgItemText(hwnd, 129, gstr);
	SendDlgItemMessage(hwnd, 127, CB_SETCURSEL, activePin[waveIndex], 0);
	SendDlgItemMessage(hwnd, 128, CB_SETCURSEL, activeLogic[waveIndex], 0);
	
	if(gate)	// gates 1, 2, 3, ...
	{
		EnableWindow(GetDlgItem(hwnd, 120), TRUE);
	}
	else	// gate 0
	{
		EnableWindow(GetDlgItem(hwnd, 120), FALSE);
		gateType[waveIndex][gate] = 0;
	}

	SendDlgItemMessage(hwnd, 120, CB_SETCURSEL, gateType[waveIndex][gate], 0);
	SendDlgItemMessage(hwnd, 124, CB_SETCURSEL, gateAlgorithm[waveIndex][gate], 0);

	sprintf(gstr, "%d", gateStart[waveIndex][gate]);
	SetDlgItemText(hwnd, 125, gstr);
	sprintf(gstr, "%d", gateLength[waveIndex][gate]);
	SetDlgItemText(hwnd, 126, gstr);
	sprintf(gstr, "%d", gateThreshold[waveIndex][gate]);
	SetDlgItemText(hwnd, 130, gstr);
}

void	initializeWaveformParameter(HWND hwnd)
{
	initNumWaveform(hwnd);

	sprintf(gstr, "%.1f", (double)gain[waveIndex]);
	SetDlgItemText(hwnd, 109, gstr);	

	sprintf(gstr, "%d", dcOffset[waveIndex]);
	SetDlgItemText(hwnd, 110, gstr);

	if(optionByte2 & 1)	// tone burst pulser
	{
		sprintf(gstr, "%d", pulseCycle);
		SetDlgItemText(hwnd, 117, gstr);

		sprintf(gstr, "%d kHz", pulseFrequency);
		SetDlgItemText(hwnd, 3001, gstr);

		SetDlgItemText(hwnd, 10102, "Half cycles");

		SendDlgItemMessage(hwnd, 3002, CB_SETCURSEL, pulsePolarity, 0);
		EnableWindow(GetDlgItem(hwnd, 3001), TRUE);
		EnableWindow(GetDlgItem(hwnd, 3003), TRUE);
		EnableWindow(GetDlgItem(hwnd, 3002), TRUE);
		EnableWindow(GetDlgItem(hwnd, 3004), TRUE);
	}
	else
	{
		EnableWindow(GetDlgItem(hwnd, 3001), FALSE);
		EnableWindow(GetDlgItem(hwnd, 3003), FALSE);
		EnableWindow(GetDlgItem(hwnd, 3002), FALSE);
		EnableWindow(GetDlgItem(hwnd, 3004), FALSE);

		sprintf(gstr, "%d ns", pulseWidth[waveIndex]);
		SetDlgItemText(hwnd, 117, gstr);

		SetDlgItemText(hwnd, 10102, "Pulse width");
	}

	SendDlgItemMessage(hwnd, 114, CB_SETCURSEL, transducerMode[waveIndex], 0);

	SendDlgItemMessage(hwnd, 1101, CB_SETCURSEL, signal[waveIndex], 0);

	sprintf(gstr, "%d", txChan[waveIndex]+1);
	SetDlgItemText(hwnd, 121, gstr);	
	sprintf(gstr, "%d", rxChan[waveIndex]+1);
	SetDlgItemText(hwnd, 111, gstr);	

	setHscrollBar(hwnd);

	initGateParameters(hwnd);
}

void initPulseType(HWND hwnd)
{
	if(optionByte2 & 0x20)	// both pulser types are installed
	{
		if(optionByte2 & 1)	// tone burst pulser type
		{
			SendDlgItemMessage(hwnd, 107, CB_SETCURSEL, voltageType, 0);
			EnableWindow(GetDlgItem(hwnd, 107), TRUE);
		}
		else
		{
			SendDlgItemMessage(hwnd, 107, CB_SETCURSEL, 0, 0);
			EnableWindow(GetDlgItem(hwnd, 107), FALSE);
		}
	}
	else
	{
		if(optionByte2 & 1)	// tone burst pulser type
		{
			SendDlgItemMessage(hwnd, 107, CB_SETCURSEL, 1, 0);	// +/- volt
			SendDlgItemMessage(hwnd, 131, CB_SETCURSEL, 1, 0);	// Tone burst pulser
		}
		else
		{
			SendDlgItemMessage(hwnd, 107, CB_SETCURSEL, 0, 0);	// - volt
			SendDlgItemMessage(hwnd, 131, CB_SETCURSEL, 0, 0);	// square pulser
		}
		EnableWindow(GetDlgItem(hwnd, 107), FALSE);
		EnableWindow(GetDlgItem(hwnd, 131), FALSE);
	}
}

int daccurve[3000];

paramInitDialog(HWND hwnd)
{
	int j;

	paramHwnd = hwnd;

	SendDlgItemMessage(hwnd, 101, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 102, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 103, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 106, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 108, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 1103, CB_RESETCONTENT, 0, 0);

	for(j=0; j<3; j++)
	{
		SendDlgItemMessage(hwnd, 108, CB_ADDSTRING, 0, (long)trgSrcLabel[j]);
	}
	for(j=0; j<4; j++)
	{
		SendDlgItemMessage(hwnd, 102, CB_ADDSTRING, 0, (long)hpfLabel[j]);
	}
	for(j=0; j<8; j++)
	{
		SendDlgItemMessage(hwnd, 103, CB_ADDSTRING, 0, (long)lpfLabel[j]);
	}
	if(totalChan > 1)
	{
		for(j=0; j<2; j++)
			SendDlgItemMessage(hwnd, 1103, CB_ADDSTRING, 0, (long)multiChanDamping[j]);
	}
	else
	{
		for(j=0; j<8; j++)
			SendDlgItemMessage(hwnd, 1103, CB_ADDSTRING, 0, (long)singleChanDamping[j]);
	}
	SendDlgItemMessage(hwnd, 102, CB_SETCURSEL, indexHPF, 0);
	SendDlgItemMessage(hwnd, 103, CB_SETCURSEL, indexLPF, 0);
	SendDlgItemMessage(hwnd, 108, CB_SETCURSEL, indexTrgSrc, 0);
	SendDlgItemMessage(hwnd, 1103, CB_SETCURSEL, damping[waveIndex], 0);

	for(j=0; j<totalCards; j++)
	{
		Parms(GETSERIALNUMBER, 0, (DWORD)gstr, 0);
		SendDlgItemMessage(hwnd, 106, CB_ADDSTRING, 0, (long)gstr);
	}
	SendDlgItemMessage(hwnd, 106, CB_SETCURSEL, cardIndex, 0);
	
	SendDlgItemMessage(hwnd, 107, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 107, CB_ADDSTRING, 0, (long)"-");
	SendDlgItemMessage(hwnd, 107, CB_ADDSTRING, 0, (long)"+/-");

	SendDlgItemMessage(hwnd, 131, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 131, CB_ADDSTRING, 0, (long)"Square");
	SendDlgItemMessage(hwnd, 131, CB_ADDSTRING, 0, (long)"Tone burst");
	SendDlgItemMessage(hwnd, 131, CB_SETCURSEL, pulseType, 0);	// square pulser

	initPulseType(hwnd);

	for(j=0; j<8; j++)
	{
		SendDlgItemMessage(hwnd, 101, CB_ADDSTRING, 0, (long)rateLabel[j]);
	}
	SendDlgItemMessage(hwnd, 101, CB_SETCURSEL, indexSamplingRate, 0);

	Parms(GETSERIALNUMBER, cardIndex, (unsigned int)gstr, 0);
	SetDlgItemText(hwnd, 107, gstr);	
	
	sprintf(gstr, "%d", totalWaveform);
	SetDlgItemText(hwnd, 116, gstr);	

	sprintf(gstr, "%d", postDelay);
	SetDlgItemText(hwnd, 104, gstr);

	sprintf(gstr, "%d", waveformLength);
	SetDlgItemText(hwnd, 105, gstr);

	sprintf(gstr, "%d", zoomFactor);
	SetDlgItemText(hwnd, 113, gstr);

	sprintf(gstr, hunitDigit[hunit], unitConvert(hOffset));
	SetDlgItemText(hwnd, 119, gstr);

	SendDlgItemMessage(hwnd, 114, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 114, CB_ADDSTRING, 0, (long)"Single");
	SendDlgItemMessage(hwnd, 114, CB_ADDSTRING, 0, (long)"Dual");

	SendDlgItemMessage(hwnd, 3002, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 3002, CB_ADDSTRING, 0, (long)"Negative");
	SendDlgItemMessage(hwnd, 3002, CB_ADDSTRING, 0, (long)"Positive");

	SendDlgItemMessage(hwnd, 1101, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 1101, CB_ADDSTRING, 0, (long)"Full");
	SendDlgItemMessage(hwnd, 1101, CB_ADDSTRING, 0, (long)"+Half");
	SendDlgItemMessage(hwnd, 1101, CB_ADDSTRING, 0, (long)"-Half");
	SendDlgItemMessage(hwnd, 1101, CB_ADDSTRING, 0, (long)"RF");

	// gate parameters
	SendDlgItemMessage(hwnd, 120, CB_RESETCONTENT, 0, 0);	// gate type
	SendDlgItemMessage(hwnd, 120, CB_ADDSTRING, 0, (long)"Fixed");
	SendDlgItemMessage(hwnd, 120, CB_ADDSTRING, 0, (long)"Slave");

	SendDlgItemMessage(hwnd, 124, CB_RESETCONTENT, 0, 0);	// gate algorithm
	for(j=0; j<5; j++)
		SendDlgItemMessage(hwnd, 124, CB_ADDSTRING, 0, (long)algLabel[j]);
	
	SendDlgItemMessage(hwnd, 127, CB_RESETCONTENT, 0, 0);	// gate output pin
	for(j=0; j<9; j++)
	{
		SendDlgItemMessage(hwnd, 127, CB_ADDSTRING, 0, (long)outpinLabel[j]);
	}

	SendDlgItemMessage(hwnd, 128, CB_RESETCONTENT, 0, 0);	// output logic
	SendDlgItemMessage(hwnd, 128, CB_ADDSTRING, 0, (long)"Low");
	SendDlgItemMessage(hwnd, 128, CB_ADDSTRING, 0, (long)"High");

	SetScrollRange(GetDlgItem(hwnd, 301), SB_CTL, 0, 1000, FALSE);

	// portA=output, portB=input
	Parms(CONFIGUREIOPORT, cardIndex, 1, 0);
	Parms(SETIOPORT, cardIndex, 0, 0);	// set port A

	updateInputStatus();

	downloadParameters(hwnd);
	initializeWaveformParameter(hwnd);

	sprintf(gstr, "%d V", pulseVoltage);
	SetDlgItemText(hwnd, 115, gstr);	

	strcpy(gstr, "Parameters - ");
	strcat(gstr, modelName);
	sprintf(gstr2, " / Driver Rev %d", hardwareRev);
	strcat(gstr, gstr2);
	SetWindowText(hwnd, gstr);

/*
//***********************
	for(j=0; j<1000; j++)
	{
		daccurve[j] = 50+j*1.2;
		if(daccurve[j] >= 500)
			daccurve[j] = 500;
	}
	j = Parms(SETDACTABLE, 0, (WPARAM)daccurve, 1000);
	if(j<0)
		MessageBox(hwnd, "wrong dac curve", "", MB_OK);
Parms(SETDACENABLE, 0, (WPARAM)1, 1);
//******************
// test ***********
*/
	return TRUE;
}

DLGPROC paramCallBack;

void paramClose(HWND)
{
	RECT r;

	if(IsWindow(paramHwnd))
	{
		GetWindowRect(paramHwnd, &r);	// save position
		sprintf(gstr, "%d", r.top);
		WritePrivateProfileString("win", "wT", gstr, initFile);
		sprintf(gstr, "%d", r.left);
		WritePrivateProfileString("win", "wL", gstr, initFile);

		DestroyWindow(paramHwnd);	 /* Exits the dialog box	     */
		FreeProcInstance( (FARPROC) paramCallBack);
		paramHwnd = NULL;
	}
	Parms(ASDK_STARTSCAN, 0, 0, 0);	// stop scan mode
}

int paramCommand(HWND hwnd, WPARAM wParam, LONG lParam)
{
	int j, bit=0;

	WORD id = LOWORD(wParam);

	if(id >= 217 && id <= 224)	// set output pins
	{
		for(j=217; j<=224; j++)
		{
			bit = bit << 1;
			if(SendDlgItemMessage(hwnd, j, BM_GETCHECK, 0, 0))
				bit |= 1;
		}
		Parms(SETIOPORT, cardIndex, bit, 0);	// set port A
		return 0;
	}
	if(id >= 6000 && id <= 6007)
	{
		j = id - 6000;
		GetDlgItemText(hwnd, id, gstr, 2);
		if(*gstr == '0')
		{
			Parms(SETIOPORT, cardIndex, 1<<j, 3);	// turn on bit
			SetDlgItemText(hwnd, id, "1");
		}
		else
		{
			Parms(SETIOPORT, cardIndex, 1<<j, 2);	// turn off bit
			SetDlgItemText(hwnd, id, "0");
		}
	}

	if(HIWORD(wParam) == CBN_SELCHANGE) 
	{
		switch(id)
		{
		case 101:	// sample rate
			indexSamplingRate = SendDlgItemMessage(hwnd, 101,
				CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 103:	// LPF
			indexLPF = SendDlgItemMessage(hwnd, 103, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 102:	// HPF
			indexHPF = SendDlgItemMessage(hwnd, 102, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 106:	// BOARD #
			cardIndex = SendDlgItemMessage(hwnd, 106, CB_GETCURSEL, 0, 0);
			paramInitDialog(hwnd);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 108:
			indexTrgSrc = SendDlgItemMessage(hwnd, 108, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;
			
		case 118:	// waveform #
			waveIndex = SendDlgItemMessage(hwnd, 118, CB_GETCURSEL, 0, 0);
			initializeWaveformParameter(hwnd);
			parameterChanged = 1;
			break;

		case 114:	// single/dual
			transducerMode[waveIndex] = SendDlgItemMessage(hwnd, 114, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 120:	// gate type
			gateType[waveIndex][gateIndex[waveIndex]] = SendDlgItemMessage(hwnd, 120, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 123:	// gate index
			gateIndex[waveIndex] = SendDlgItemMessage(hwnd, 123, CB_GETCURSEL, 0, 0);
			initGateParameters(hwnd);
			parameterChanged = 1;
			break;

		case 124:	// gate algorithm
			gateAlgorithm[waveIndex][gateIndex[waveIndex]] = SendDlgItemMessage(hwnd, 124, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 127:	// active pin
			activePin[waveIndex] = SendDlgItemMessage(hwnd, 127, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 128:	// active logic
			activeLogic[waveIndex] = SendDlgItemMessage(hwnd, 128, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 1103:
			damping[waveIndex] = SendDlgItemMessage(hwnd, 1103, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 1101:	// signal
			signal[waveIndex] = SendDlgItemMessage(hwnd, 1101, CB_GETCURSEL, 0, 0);
			restartScan(hwnd);
			parameterChanged = 1;
			break;

		case 3002:	// pos/neg
			pulsePolarity = SendDlgItemMessage(hwnd, 3002, CB_GETCURSEL, 0, 0);
			parameterChanged = 1;
			restartScan(hwnd);
			break;

		case 107:	// voltage type
			voltageType = SendDlgItemMessage(hwnd, 107, CB_GETCURSEL, 0, 0);
			parameterChanged = 1;
			restartScan(hwnd);
			break;

		case 131:	// pulse type
			pulseType = SendDlgItemMessage(hwnd, 131, CB_GETCURSEL, 0, 0);
			Parms(SWITCHPULSETYPE, cardIndex, pulseType, voltageType);
			optionByte2 = Parms(GETOPTIONBYTE2, cardIndex, 0, 0);
			parameterChanged = 1;
	
			restartScan(hwnd);
			initPulseType(hwnd);
			initializeWaveformParameter(hwnd);	// change between half cycle and width
			break;
		}
	}
	switch (id)
	{
	case 104:	// trigger delay
		j = postDelay =
			(int)oneEntry(hInst, hwnd, "Trigger Delay", "samples", 
			(float)postDelay,
			(float)0, (float)32767, 0);
		sprintf(gstr, "%d", postDelay);
		SetDlgItemText(hwnd, 104, gstr);	
		restartScan(hwnd);
		parameterChanged = 1;
		break;

	case 105:	// waveform length
		j = waveformLength =
			(int)oneEntry(hInst, hwnd, "Waveform Length", "samples", 
			(float)waveformLength,
			(float)16, (float)memorySize, 0);
		sprintf(gstr, "%d", waveformLength);
		SetDlgItemText(hwnd, 105, gstr);	
		restartScan(hwnd);
		parameterChanged = 1;
		break;

	case 109:	// gain
		gain[waveIndex] =
			oneEntry(hInst, hwnd, "Gain", "dB", 
			(float)gain[waveIndex],
			(float)-12.0, (float)80.0, (int)1);
		sprintf(gstr, "%.1f", (double)gain[waveIndex]);
		SetDlgItemText(hwnd, 109, gstr);	
		parameterChanged = 1;
		Parms(ASDK_SETGAIN,	0, waveIndex, getGainIndex(gain[waveIndex]) );
		break;

	case 110:	// DC offset
		j = dcOffset[waveIndex] =
			(int)oneEntry(hInst, hwnd, "DC Offset", "mV", 
			(float)dcOffset[waveIndex],
			(float)-2500.0, (float)2500.0, 0);
		sprintf(gstr, "%d", dcOffset[waveIndex]);
		SetDlgItemText(hwnd, 110, gstr);
		parameterChanged = 1;
		j = 511 + j / 5;
		Parms(ASDK_SETDCOFFSET,	0, waveIndex, j);
		break;

	case 112:	// copy selected data to clipboard
		copyDataToClipboard(hwnd);
		break;

	case 113:	// Zoom
		zoomFactor =
			(int)oneEntry(hInst, hwnd, "Zoom Factor", "%", 
			(float)zoomFactor,
			(float)5, (float)200, 0);
		sprintf(gstr, "%d", zoomFactor);
		SetDlgItemText(hwnd, 113, gstr);
		parameterChanged = 1;
		break;

	case 115:	// volts
		j = pulseVoltage =
			(int)oneEntry(hInst, hwnd, "Pulse Voltage", "V", 
			(float)pulseVoltage,
			(float)minVoltage, (float)maxVoltage, 0);
		sprintf(gstr, "%d V", pulseVoltage);
		SetDlgItemText(hwnd, 115, gstr);	
		parameterChanged = 1;
		Parms(SETPULSEVOLTAGE2, cardIndex, pulseVoltage, 0);
		break;

	case 116:	// # of waveforms
		totalWaveform =
			(int)oneEntry(hInst, hwnd, "Number of Waveforms", "", 
			(float)totalWaveform,
			(float)1, (float)MAX_WAVEFORM, 0);
		restartScan(hwnd);

		sprintf(gstr, "%d", totalWaveform);
		SetDlgItemText(hwnd, 116, gstr);
		initNumWaveform(hwnd);
		break;

	case 117:	// width or cycles
		if(optionByte2 & 1)	// half cycles for tone burst pulser
		{
			j = pulseCycle =
				(int)oneEntry(hInst, hwnd, "# of Half Cycles", "", 
				(float)pulseCycle,
				(float)1.0, (float)32.0, 0);
			sprintf(gstr, "%d", pulseCycle);
			SetDlgItemText(hwnd, 117, gstr);
		}
		else // pulse width for single pulse pulser
		{
			int max = 500;
			if(optionByte1 & 2)	// wide pulse
				max = 2000;
			j = pulseWidth[waveIndex] =
				(int)oneEntry(hInst, hwnd, "Pulse Width", "ns", 
				(float)pulseWidth[waveIndex],
				(float)minPulseWidth, (float)max, 0);
			sprintf(gstr, "%d ns", pulseWidth[waveIndex]);
			SetDlgItemText(hwnd, 117, gstr);
		}
		restartScan(hwnd);
		parameterChanged = 1;
		break;

	case 3001:	// frequency
		pulseFrequency =
			(int)oneEntry(hInst, hwnd, "Frequency", "kHz", 
			(float)pulseFrequency,
			(float)20.0, (float)8000.0, 0);
		sprintf(gstr, "%d kHz", pulseFrequency);
		SetDlgItemText(hwnd, 3001, gstr);
		parameterChanged = 1;
		restartScan(hwnd);
		break;

	case 119:	// horizontal offset
		hOffset =
			(int)oneEntry(hInst, hwnd, "Offset", "sample", 
			(float)hOffset,
			(float)0.0, (float)waveformLength, 0);
		setHscrollBar(hwnd);
		break;

	case 111:	// RX channel
		rxChan[waveIndex] =
			(int)oneEntry(hInst, hwnd, "RX Channel", "", 
			(float)rxChan[waveIndex]+1,
			(float)1.0, (float)16.0, 0) - 1;
		sprintf(gstr, "%d", rxChan[waveIndex]+1);
		SetDlgItemText(hwnd, 111, gstr);	
		parameterChanged = 1;
		restartScan(hwnd);
		break;

	case 121:	// TX channel
		txChan[waveIndex] =
			(int)oneEntry(hInst, hwnd, "TX Channel", "", 
			(float)txChan[waveIndex]+1,
			(float)1.0, (float)16.0, 0) - 1;
		sprintf(gstr, "%d", txChan[waveIndex]+1);
		SetDlgItemText(hwnd, 121, gstr);	
		parameterChanged = 1;
		restartScan(hwnd);
		break;

	case 122:	// # of gates
		nGate[waveIndex] =	(int)oneEntry(hInst, hwnd, "Total Number of Gates", "", 
			(float)nGate[waveIndex],
			(float)1.0, (float)10, 0);
		initGateParameters(hwnd);
		restartScan(hwnd);
		parameterChanged = 1;
		break;

	case 125:	// gate start
		gateStart[waveIndex][gateIndex[waveIndex]] =
			(int)oneEntry(hInst, hwnd, "Gate Start", "sample", 
			(float)gateStart[waveIndex][gateIndex[waveIndex]],
			(float)0.0, (float)0x3FFF, 0);
		Parms(ASDK_SETGATESTART, gateIndex[waveIndex], waveIndex, gateStart[waveIndex][gateIndex[waveIndex]]);
		sprintf(gstr, "%d", gateStart[waveIndex][gateIndex[waveIndex]]);
		SetDlgItemText(hwnd, 125, gstr);
		parameterChanged = 1;
		break;

	case 126:	// gate length
		gateLength[waveIndex][gateIndex[waveIndex]] =
			(int)oneEntry(hInst, hwnd, "Gate Start", "sample", 
			(float)gateLength[waveIndex][gateIndex[waveIndex]],
			(float)0.0, (float)0x3FFF, 0);
		sprintf(gstr, "%d", gateLength[waveIndex][gateIndex[waveIndex]]);
		SetDlgItemText(hwnd, 126, gstr);
		Parms(ASDK_SETGATELENGTH, gateIndex[waveIndex], waveIndex, gateLength[waveIndex][gateIndex[waveIndex]]);
		parameterChanged = 1;
		break;

	case 129:	// active time (duration)
		outputDuration[waveIndex] =	(int)oneEntry(hInst, hwnd, "Active Time of Output Pin", "millisecond", 
			(float)outputDuration[waveIndex],
			(float)1.0, (float)4095.0, 0);
		sprintf(gstr, "%d", outputDuration[waveIndex]);
		SetDlgItemText(hwnd, 129, gstr);
		restartScan(hwnd);
		parameterChanged = 1;
		break;

	case 130:	// gate threshold
		gateThreshold[waveIndex][gateIndex[waveIndex]] =
			(int)oneEntry(hInst, hwnd, "Gate Start", "sample", 
			(float)gateThreshold[waveIndex][gateIndex[waveIndex]],
			(float)0.0, (float)255.0, 0);
		sprintf(gstr, "%d", gateThreshold[waveIndex][gateIndex[waveIndex]]);
		SetDlgItemText(hwnd, 130, gstr);
		Parms(ASDK_SETGATETHRESHOLD, gateIndex[waveIndex], waveIndex, gateThreshold[waveIndex][gateIndex[waveIndex]]);
		parameterChanged = 1;
		break;

	case 225:	// pause
		pause = (char)SendDlgItemMessage(hwnd, 225, BM_GETCHECK, 0, 0);
		if(pause)
		{
			scanning = 0;
			TerminateThread(threadHndl, 0);
		}
		else
			downloadParameters(hwnd);

		break;

	case 308:	// set X encoder
		j =
			(int)oneEntry(hInst, hwnd, "X Encoder Counter", "", 
			(float)XencoderValue,
			(float)-8388608.0, (float)8388607, 0);
		Parms(SETENCODERCOUNTER, cardIndex, 0, j);
		break;

	case 309:	// set Y encoder
		j =
			(int)oneEntry(hInst, hwnd, "Y Encoder Counter", "", 
			(float)XencoderValue,
			(float)-8388608.0, (float)8388607, 0);
		Parms(SETENCODERCOUNTER, cardIndex, 1, j);
		break;

	case IDOK:
	case IDCANCEL:
		paramClose(hwnd);
		break;

	}

    return FALSE;
}
    
void WMhscroll(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam)) 
	{
	case SB_LINEDOWN:
		hOffset = (LONG)hOffset + 10;
		break;

	case SB_LINEUP:
		hOffset = (LONG)hOffset - 10;
		break;

	case SB_PAGEUP:
		hOffset = (LONG)hOffset - 490;
		break;

	case SB_PAGEDOWN:
		hOffset = (LONG)hOffset + 490;
		break;

	case SB_THUMBTRACK:
        hOffset = (LONG)HIWORD(wParam) * waveformLength / 1000;
		break;
	}
	int hscrollBarPos = (LONG)1000 * hOffset/ waveformLength;
	SetScrollPos(GetDlgItem(hwnd, 301),
		SB_CTL, hscrollBarPos, TRUE);

	sprintf(gstr, hunitDigit[hunit], unitConvert(hOffset));
	SetDlgItemText(hwnd, 119, gstr);
}

int WMvscroll(HWND hwnd, WORD wParam, LONG lParam)
{
	int j, id = GetDlgCtrlID((HWND)lParam);

	if(LOWORD(wParam) != SB_LINEDOWN && LOWORD(wParam) != SB_LINEUP)
    		return FALSE;

	switch (id)
	{
	case 1206:	// DC offset
		if(LOWORD(wParam) == SB_LINEDOWN)
		{
			if(dcOffset[waveIndex] > -2495)
				dcOffset[waveIndex] -= 5;
		}
		else
		{
			if(dcOffset[waveIndex] < 2495)
				dcOffset[waveIndex] += 5;
		}

		sprintf(gstr, "%d", dcOffset[waveIndex]);
		SetDlgItemText(hwnd, 110, gstr);
		parameterChanged = 1;
		j = 511 + dcOffset[waveIndex] / 5;
		Parms(ASDK_SETDCOFFSET,	0, waveIndex, j);

		break;

	case 1204:	// gain
		if(LOWORD(wParam) == SB_LINEDOWN)
		{
			if(gain[waveIndex] > -20)
				gain[waveIndex] -= (float)0.0978;
		}
		else
		{
			if(gain[waveIndex] < 80)
				gain[waveIndex] += (float)0.0978;
		}

		sprintf(gstr, "%.1f", (double)gain[waveIndex]);
		SetDlgItemText(hwnd, 109, gstr);	
		parameterChanged = 1;
		Parms(ASDK_SETGAIN,	0, waveIndex, getGainIndex(gain[waveIndex]) );	// gain: 40 dB

		break;
	}
	return 0;
}

LRESULT  paramHndl(HWND hwnd,
	unsigned iMessage, WPARAM wParam, LPARAM lParam)
{
	int result=FALSE;

	switch (iMessage) 
	{
		case WM_INITDIALOG:
			result = paramInitDialog(hwnd);
			break;

		case WM_COMMAND:
			paramCommand(hwnd, wParam, lParam);
            break;

		case WM_HSCROLL:
			WMhscroll(hwnd, wParam, lParam);
			break;

		case WM_VSCROLL:
			WMvscroll(hwnd, wParam, lParam);
			break;

		case WM_CLOSE:
			paramClose(hwnd);
			break;
	}
	return result;
}

int totalBoards;

HWND paramOpen(HINSTANCE hInstance, HWND masterHwnd)
{
	RECT r;

	if(paramHwnd) 
	{
		BringWindowToTop(paramHwnd);
    }
	else 
	{
		FARPROC function = (FARPROC) paramHndl;
		paramCallBack = (DLGPROC) MakeProcInstance(function, hInstance);
		paramHwnd = CreateDialog(hInstance,
			"DSPPARAMETER", masterHwnd, paramCallBack);
		if(paramHwnd == NULL) 
		{
			MessageBox(masterHwnd, "Cannot open paramemter menu!",
				NULL, MB_OK);
			return NULL;
        }

		GetWindowRect(paramHwnd, &r);	// get position of this dialog
		r.right -= r.left;
		r.bottom-= r.top;
		GetPrivateProfileString("win", "wT", "440", gstr, 255, initFile);
		r.top = atoi(gstr);
		if(r.top < 0) r.top = 0;

		GetPrivateProfileString("win", "wL", "0", gstr, 255, initFile);
		r.left = atoi(gstr);
		if(r.left < 0) r.left = 0;
		MoveWindow(paramHwnd, r.left, r.top, r.right, r.bottom, FALSE);

		ShowWindow(paramHwnd, SW_SHOWNORMAL);
	}
	return paramHwnd;
}

void displayInitDialog(HWND hwnd, long h)
{
	int j;

	SendDlgItemMessage(hwnd, 100, CB_RESETCONTENT, 0, 0);
	for(j=0; j<4; j++)
	{
		SendDlgItemMessage(hwnd, 100, CB_ADDSTRING, 0, (long)hunitLongLabel[j]);
	}
	SendDlgItemMessage(hwnd, 100, CB_SETCURSEL, hunit, 0);

	SendDlgItemMessage(hwnd, 103, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 103, CB_ADDSTRING, 0, (long)"value");
	SendDlgItemMessage(hwnd, 103, CB_ADDSTRING, 0, (long)"percent");
	SendDlgItemMessage(hwnd, 103, CB_SETCURSEL, vUnit, 0);

	sprintf(gstr, "%f in/us", velocity);
	SetDlgItemText(hwnd, 101, gstr);

	SendDlgItemMessage(hwnd, 102, BM_SETCHECK, roundTrip, 0);
}

LRESULT  displayHndl(HWND hwnd,
	unsigned iMessage, WPARAM wParam, LPARAM lParam)
{
	float flt;

	switch (iMessage) 
	{
		case WM_INITDIALOG:
			displayInitDialog(hwnd, lParam);
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
			case IDOK:
				hunit = SendDlgItemMessage(hwnd, 100, CB_GETCURSEL, 0, 0);
				vUnit = SendDlgItemMessage(hwnd, 103, CB_GETCURSEL, 0, 0);
				GetDlgItemText(hwnd, 101, gstr, 20);
				flt = (float)
					atof(gstr);
				if(flt > 0)
					velocity = flt;
				else
					MessageBox(hwnd, "Invalid velocity value!", gstr, MB_OK);

				roundTrip = SendDlgItemMessage(hwnd, 102, BM_GETCHECK, 0, 0);
				parameterChanged = 1;
				
			case IDCANCEL:
			    EndDialog(hwnd, TRUE);
				return TRUE;
			}
			break;

	case WM_DESTROY:
	default:
		break;
	}
	return FALSE;
}


void  displayOpen(HINSTANCE hInstance, HWND hwnd)
{
	FARPROC lpProcEnter;

	lpProcEnter = MakeProcInstance((FARPROC)displayHndl, hInstance);

	DialogBox(hInstance, "DISPLAY", hwnd, (DLGPROC)lpProcEnter);
}


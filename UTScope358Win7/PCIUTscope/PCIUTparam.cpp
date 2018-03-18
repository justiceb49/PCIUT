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

void updateInputStatus();
void	copyDataToClipboard(HWND hwnd);
float unitConvert(int num);
WORD  gatePeak(WORD thresh,WORD start,WORD len,WORD* tof,short int alg, BYTE *buff);
void paramClose(HWND);
void savebscanNow(HWND hwnd, char *rFile);
void readbscanNow(HWND hwnd, char *rFile);
void processBscan();

HWND	paramHwnd = NULL;
extern ParmsProc Parms;
extern COLORREF	clr[17];

BYTE amplitude[10];
WORD tof[10], outStatus=0, IOconfig=0;
int reportItems, XencoderValue, YencoderValue, minPulseWidth, 
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
//	rectifier=3,	// RF
	memorySize=8188,
	damping[256],
	totalChan=1,
	hardwareRev=0,
	hunit=1,
	roundTrip=BST_CHECKED,
	audioAlarm=BST_CHECKED,
	outputPin=BST_CHECKED,
	virtualLED=BST_CHECKED,
	copyThickness=BST_UNCHECKED,
	copyInterval=500,
	displayType=0,
	average=1,
	minVoltage, maxVoltage,
	waveformLength=1000;// default: 1000 samples

float	gain[256]={20, 20, 20, 20},
	velocity=(float)0.232, thickness=1, density=7.8;

char parameterChanged=0, pause=0, expendDlg=0,
	lpfLabel[8][20]={"All", "48 MHz", "28 MHz", "18 Mhz",
		"8.8 MHz", "7.5 MHz", "6.7 MHz", "5.9 MHz"},
	hpfLabel[4][20]={"4.8 MHz", "1.8 MHz", "0.8 MHz", "0.6 MHz"},
	*trgSrcLabel[3]={"+EXT TRIG", "Software", "-EXT TRIG"},
	*multiChanDamping[]={"500", "50"},
	*singleChanDamping[]={"620", "339", "202", "159", "60", "55", "50", "47"},
	*hunitLabel[4]={"sp", "us", "in", "mm"},
	*velocityLabel[4]={"sp", "us", "in/us", "mm/us"},
	*hunitLongLabel[4]={"sample", "micro second", "inch", "mm"},
	*hunitDigit[4]={"%.0f", "%.3f", "%.3f", "%.1f"},
	*velocityDigit[4]={"%.0f", "%.3f", "%.8f", "%.8f"},
	*algLabel[5]={"Max peak", "Min valley", "Rising Edge", "Falling Edge", "Signal loss"},
	*outpinLabel[17]={"None","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","10"},
	serialNumber[30], modelNumber[30],
	// device independent
	*section={"PCIUT"},
	*dllName={"PCIUTDLL.DLL"}, //"PCIUTDLL.DLL", "dspASDK.dll", , "ExUT.DLL", "GiUT.DLL"},
	*functionName={"PCIUTParms"};	//"PCIUTParms", "DSPUTParms", , "ExUTParms", "GiUTParms"},

int pulseVoltage=40,
	dcOffset[256]={0, 0, 0, 0},
	txChan[256]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
	rxChan[256]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
	signal[256]={3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},	// 0=full, 1=+half, 2=-half, 3=RF
	transducerMode[256]={0, 0, 0, 0},
	pulseWidth[256]={50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50},
	pulseFrequency[256],
	pulsePolarity[256],
	pulseCycle[256];

extern int totalCards, cardIndex, optionByte1, optionByte2, totalWaveform, zoomFactor, hOffset,
	vUnit, timerID;
extern char gstr[], gstr2[256], *initFile, modelName[];
extern BYTE byteDataBuffer[MAX_WAVEFORM][32000];
extern HINSTANCE hInst;								// current instance
extern RECT originalWindowSize;
extern HWND hWnd;
extern HPEN	hColorPen[];
extern float 	vUnitConvert[];

int milliSecond()
{
	struct _timeb tb;
	_ftime(&tb);
	return tb.millitm;
}

int milliSecond2()
{
	struct _timeb tb;
	_ftime(&tb);
	return tb.time*1000 + tb.millitm;
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

float getSampleRateHz()
{
	return (float)samplingRate / (1 << indexSamplingRate);
}

float convertToVelocity(int num) 
{
	float fnum;
	switch (hunit)
	{
	case 2: // inch
	case 3:	// mm
		fnum = thickness / ((float)num / (getSampleRateHz() / 1000000.0));
		if(roundTrip == BST_CHECKED)
			fnum *= 2;
		if(hunit == 3)	// mm
			fnum *= (float)2.54;
		return fnum;
	}
	return 1.0;
}

int oldStatus=-1, IOstatus, oldXencoder=-99999, oldYencoder=-99999, oldCopyTime=0;

void drawGates(HWND hwnd, HDC hMemoryDC, double xratio, double yratio)
{
	int g, x, x2, y, start0, outpin=0;

	HBITMAP hbitmap = LoadBitmap (hInst, MAKEINTRESOURCE(IDB_LONGLED));
	HDC hMemBmp = CreateCompatibleDC(NULL);
	SelectObject(hMemBmp, hbitmap);

	SelectObject(hMemoryDC, hColorPen[14]);
	SetTextColor(hMemoryDC, clr[14]);
	for(g=0; g<nGate[waveIndex]; g++)
	{
		y = (int)(yratio * (255-gateThreshold[waveIndex][g]));
		if(g && gateType[waveIndex][g])
			start0 = tof[0];
		else
			start0 = 0;
		x = (int)(xratio * (gateStart[waveIndex][g]-hOffset + start0));
		x2 = x + (int)(xratio * gateLength[waveIndex][g]);
		if(virtualLED && amplitude[g] && (milliSecond() > 300))
		{
			StretchBlt(hMemoryDC, //destination
					x, y-11,
					x2-x, 11,  //output size
					hMemBmp,      // source
					0, 0,
					x2-x, 11,  //source size
					SRCCOPY);
		}
		else
		{
			MoveToEx(hMemoryDC, x, y, NULL);
			LineTo(hMemoryDC, x2, y);
		}
		sprintf(gstr, "%d", g+1);
		TextOut(hMemoryDC, x, y+1, gstr, strlen(gstr));

		if(amplitude[g])
		{
			outpin = outpin | (1 << g);
			if(outputPin)
				outStatus = outStatus & (0xFF ^(1<<g));	// clean up the pins
		}
	}

	DeleteDC(hMemBmp);
	DeleteObject (hbitmap);

	if(outputPin)
	{
		Parms(SETIOPORT, cardIndex, 0, outStatus | outpin);	// set port A
	}
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

BYTE oldAmp0=0, oldAmp1=0;
WORD oldtof0=0, oldtof1=0;
extern WORD maxAmp;

void showStatus()
{
	int amp, showgate=0, temp;
	WORD t;
	float velo;

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
		showgate++;
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
		showgate++;
	}
	else
	{
		SetDlgItemText(paramHwnd, 312, "");
		SetDlgItemText(paramHwnd, 313, "");
	}

	if(showgate == nGate[waveIndex])
	{
		if(displayType == 1)	// velocity
		{
			if(hunit < 2)	// samples and us
				strcpy(gstr, "");
			else
			{
				strcpy(gstr, "Velocity = ");
				if(nGate[waveIndex] == 1)
					velo = convertToVelocity(tof[0]);
				else if(nGate[waveIndex] == 2)
					velo = convertToVelocity(tof[1]-tof[0]);

				sprintf(gstr2, velocityDigit[hunit], velo);
				strcat(gstr, gstr2);
				strcat(gstr, velocityLabel[hunit]);

				strcat(gstr, "        Young\'s modulus = ");
				sprintf(gstr2, "%f", velo*velo * density);
				strcat(gstr, gstr2);
				strcat(gstr, " Gpa");
			}
		}
		else if(displayType == 2)	// damping
		{
			if(nGate[waveIndex] < 2)
				strcpy(gstr, "Please set the total number of gates to 2 or higher.");
			else if(hunit < 2)
				strcpy(gstr, "Please select the horizontal unit to inch or mm.");
			else
			{
				amp = gatePeak(127, gateStart[waveIndex][0],	gateLength[waveIndex][0], &t, 0, byteDataBuffer[waveIndex])	// max peak
					- (maxAmp - gatePeak(127, gateStart[waveIndex][0], gateLength[waveIndex][0], &t, 1, byteDataBuffer[waveIndex]));	// min valley

				temp = gatePeak(127, gateStart[waveIndex][1], gateLength[waveIndex][1], &t, 0, byteDataBuffer[waveIndex])		// max peak
					- (maxAmp - gatePeak(127, gateStart[waveIndex][1], gateLength[waveIndex][1], &t, 1, byteDataBuffer[waveIndex]));	// min valley
				if(amp <= 0 || temp <=0)
					strcpy(gstr, "Amplitude is 0. Please adjust the gate position.");
				else
				{
					velo = (float)(10.0 * log10((double)amp / temp) / thickness);
					if(hunit == 3)	// mm
						velo *= 25.4;
					sprintf(gstr, "Damping Coefficient: %f dB\/%s", velo, hunitLabel[hunit]);
				}
			}
		}
		else	// thickness
		{
			strcpy(gstr, "Thickness = ");
			if(nGate[waveIndex] == 1)
				sprintf(gstr2, hunitDigit[hunit], unitConvert(tof[0]));
			else if(nGate[waveIndex] == 2)
				sprintf(gstr2, hunitDigit[hunit], unitConvert(tof[1]-tof[0])); // show the distance from gate 0
			strcat(gstr, gstr2);
			strcat(gstr, hunitLabel[hunit]);
			if(copyThickness)
			{
				temp = milliSecond2();
				if(labs(temp - oldCopyTime) >= copyInterval)
				{
					oldCopyTime = temp;
					Clipboard cb(hWnd);
					cb.copyText(gstr2);
				}
			}
		}
		SetDlgItemText(paramHwnd, 10218, gstr);
	}
	else
		SetDlgItemText(paramHwnd, 10218, "");

	processBscan();

	IOstatus = Parms(SETIOPORT, cardIndex, 0, 1);

	if(oldStatus == IOstatus)
		return;		// do not waste time to display the same thing

	for(int j=0; j<8; j++)
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
	memorySize= Parms(GETMEMORYSIZE, cardIndex, 0, 0);
	optionByte1 = Parms(GETOPTIONBYTE1, cardIndex, 0, 0);
	optionByte2 = Parms(GETOPTIONBYTE2, cardIndex, 0, 0);
	minPulseWidth=Parms(GETMINPULSEWIDTH, cardIndex, 0, 0);
	samplingRate =Parms(GETINFO, cardIndex, 0, 1) * 1000000;
}

float unitConvert(int num) 
{
	float fnum;
	switch (hunit)
	{
	case 0:	// samples
		return (float)num;

	case 1:	// micro seconds
		return (float)num / (getSampleRateHz() / 1000000.0);

	case 2: // inch
	case 3:	// mm
		fnum = (float)num / (getSampleRateHz()  / 1000000.0)* velocity;
		if(roundTrip == BST_CHECKED)
			fnum /= 2;
		if(hunit == 3)	// mm
			fnum *= (float)25.4;
		return fnum;
	}
	return 1.0;
}

int getVoltageIndex()
{
	int j;
	if(maxVoltage < minVoltage)
		j = (pulseVoltage-minVoltage) * 255 / (maxVoltage - minVoltage);
	else
		j = (pulseVoltage-40) * 255 / (300 - 40);
	if(j < 0)
		j = 0;
	else if(j > 255)
		j = 255;

	return j;
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

//	Parms(SETPULSEWIDTH , cardIndex, j, narrow);

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

void	downloadWaveformParameters(int wave)
{
	int j;

	if(totalChan > 1)
	{
		Parms(SETCHANDAMPING, cardIndex, txChan[wave], damping[wave]);
		Parms(SETCHANTRANSDUCERMODE, cardIndex, txChan[wave], transducerMode[wave]);
	}
	else
	{
		Parms(SETDAMPING, cardIndex, damping[wave], 0);
		Parms(SETTRANSDUCERMODE, cardIndex, transducerMode[wave], 0);
	}

	if(optionByte2 & 1)	// tone burst pulser
	{
		Parms(SETPULSEWIDTH, cardIndex, pulseCycle[wave], 0);	// half cycles
		Parms(SETTONEBURSTFREQUENCY, cardIndex, pulseFrequency[wave], pulsePolarity[wave]);
	}
	else	// regular pulser
	{	// pulse width
		Parms(SETPULSEWIDTH, cardIndex, getPulseWidthIndex(pulseWidth[wave]), 1);
	}

	Parms(SETGAIN, cardIndex, getGainIndex(gain[wave]), 0);

	j = 511 + dcOffset[wave] / 5;
	Parms(SETDCOFFSET, cardIndex, j, 0);

	Parms(SETPULSERCHANNEL, cardIndex, txChan[wave], 0);
	Parms(SETRECEIVERCHANNEL, cardIndex, rxChan[wave], 0);
	Parms(SETRECTIFIER, cardIndex, signal[wave], 0);
}

// download the parameters for the first waveform
void	downloadParameters(HWND hwnd)
{
	Parms(SETSAMPLINGRATE, cardIndex, indexSamplingRate, 0);

	Parms(SETLOWPASSFILTER, cardIndex, indexLPF, 0);

//	Parms(SETRECTIFIER, cardIndex, rectifier, 0);
	Parms(SETHIGHPASSFILTER, cardIndex, indexHPF, 0);

	Parms(SETBUFFERLENGTH, cardIndex, waveformLength, 0);

	Parms(SETTRIGGERDELAY, cardIndex, postDelay, 0);

	// need to be change to use actual value V or ns
	Parms(SETPULSEVOLTAGE, cardIndex, getVoltageIndex(), 0);

	Parms(SETADTRIGGERSOURCE, cardIndex, indexTrgSrc, 0);

	downloadWaveformParameters(0);
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

		sprintf(gstr2, "damp%d", ch);
		GetPrivateProfileString(section, gstr2, "0", gstr, 25, fileName);
		damping[ch] = atoi(gstr);

		sprintf(gstr2, "width%d", ch);
		GetPrivateProfileString(section, gstr2, "30", gstr, 50, fileName);
		pulseWidth[ch] = atoi(gstr);

		sprintf(gstr2, "freq%d", ch);
		GetPrivateProfileString(section, gstr2, "5000", gstr, 25, fileName);
		pulseFrequency[ch] = atoi(gstr);

		sprintf(gstr2, "sig%d", ch);
		GetPrivateProfileString(section, gstr2, "0", gstr, 25, fileName);
		signal[ch] = atoi(gstr);

		sprintf(gstr2, "Polar%d", ch);
		GetPrivateProfileString(section, gstr2, "0", gstr, 25, fileName);
		pulsePolarity[ch] = atoi(gstr);

		sprintf(gstr2, "Cycle%d", ch);
		GetPrivateProfileString(section, gstr2, "2", gstr, 25, fileName);
		pulseCycle[ch] = atoi(gstr);

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

	GetPrivateProfileString(section, "z", "100", gstr, 255, fileName);
	zoomFactor = atoi(gstr);

	GetPrivateProfileString(section, "hoffset", "0", gstr, 255, fileName);
	hOffset = atoi(gstr);
	
	GetPrivateProfileString(section, "hunit", "1", gstr, 2, fileName);
	hunit = atoi(gstr);

	GetPrivateProfileString(section, "vunit", "1", gstr, 2, fileName);
	vUnit = atoi(gstr);

	velocity = getPrivateProfileFloat(section, "velo", (float)0.232, fileName);
	thickness = getPrivateProfileFloat(section, "thick", (float)1.0, fileName);
	density = getPrivateProfileFloat(section, "den", (float)7.8, fileName);
	displayType = (int)getPrivateProfileFloat(section, "distype", (float)0.0, fileName);
	average = (int)getPrivateProfileFloat(section, "aver", (float)1.0, fileName);
	roundTrip = (int)getPrivateProfileFloat(section, "rtrip", (float)0.0, fileName);
	audioAlarm = (int)getPrivateProfileFloat(section, "alarm", (float)0.0, fileName);
	outputPin = (int)getPrivateProfileFloat(section, "opin", (float)0.0, fileName);
	virtualLED = (int)getPrivateProfileFloat(section, "vled", (float)0.0, fileName);
	copyThickness = (int)getPrivateProfileFloat(section, "cthick", (float)0.0, fileName);
	copyInterval = (int)getPrivateProfileFloat(section, "cinterval", (float)500.0, fileName);

	readbscanNow(hwnd, fileName);
	parameterChanged = 0;

	downloadParameters(hwnd);
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

		sprintf(gstr, "%d", damping[ch]);
		sprintf(gstr2, "damp%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", pulseWidth[ch]);
		sprintf(gstr2, "width%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", pulseFrequency[ch]);
		sprintf(gstr2, "freq%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", signal[ch]);
		sprintf(gstr2, "sig%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", pulsePolarity[ch]);
		sprintf(gstr2, "Polar%d", ch);
		WritePrivateProfileString(section, gstr2, gstr, fileName);

		sprintf(gstr, "%d", pulseCycle[ch]);
		sprintf(gstr2, "Cycle%d", ch);
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

	sprintf(gstr, "%d", zoomFactor);
	WritePrivateProfileString(section, "z", gstr, fileName);

	sprintf(gstr, "%d", hOffset);
	WritePrivateProfileString(section, "hoffset", gstr, fileName);

	sprintf(gstr, "%d", hunit);
	WritePrivateProfileString(section, "hunit", gstr, fileName);

	sprintf(gstr, "%d", vUnit);
	WritePrivateProfileString(section, "vunit", gstr, fileName);

	writePrivateProfileFloat(section, "velo", velocity, fileName);
	writePrivateProfileFloat(section, "thick", thickness, fileName);
	writePrivateProfileFloat(section, "den", density, fileName);
	writePrivateProfileFloat(section, "distype", displayType, fileName);
	writePrivateProfileFloat(section, "aver", average, fileName);
	writePrivateProfileFloat(section, "rtrip", roundTrip, fileName);
	writePrivateProfileFloat(section, "alarm", audioAlarm, fileName);
	writePrivateProfileFloat(section, "opin", outputPin, fileName);
	writePrivateProfileFloat(section, "vled", virtualLED, fileName);
	writePrivateProfileFloat(section, "cthick", copyThickness, fileName);
	writePrivateProfileFloat(section, "cinterval", copyInterval, fileName);

	savebscanNow(hwnd, fileName);

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
		sprintf(gstr, "%d", pulseCycle[waveIndex]);
		SetDlgItemText(hwnd, 117, gstr);

		sprintf(gstr, "%d kHz", pulseFrequency[waveIndex]);
		SetDlgItemText(hwnd, 3001, gstr);

		SetDlgItemText(hwnd, 10102, "Half cycles");

		SendDlgItemMessage(hwnd, 3002, CB_SETCURSEL, pulsePolarity[waveIndex], 0);
	}
	else
	{
		DestroyWindow(GetDlgItem(hwnd, 3001));
		DestroyWindow(GetDlgItem(hwnd, 3003));
		DestroyWindow(GetDlgItem(hwnd, 3002));
		DestroyWindow(GetDlgItem(hwnd, 3004));

		sprintf(gstr, "%d ns", pulseWidth[waveIndex]);
		SetDlgItemText(hwnd, 117, gstr);
	}

	SendDlgItemMessage(hwnd, 114, CB_SETCURSEL, transducerMode[waveIndex], 0);
	SendDlgItemMessage(hwnd, 1103, CB_SETCURSEL, damping[waveIndex], 0);

	SendDlgItemMessage(hwnd, 1101, CB_SETCURSEL, signal[waveIndex], 0);

	sprintf(gstr, "%d", txChan[waveIndex]+1);
	SetDlgItemText(hwnd, 121, gstr);	
	sprintf(gstr, "%d", rxChan[waveIndex]+1);
	SetDlgItemText(hwnd, 111, gstr);	

	setHscrollBar(hwnd);

	initGateParameters(hwnd);
}

int daccurve[3000];

paramInitDialog(HWND hwnd)
{
	int j;

	paramHwnd = hwnd;

	SendDlgItemMessage(hwnd, 101, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 102, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 103, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 108, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 1103, CB_RESETCONTENT, 0, 0);

	for(j=0; j<4; j++)
	{
		SendDlgItemMessage(hwnd, 102, CB_ADDSTRING, 0, (long)hpfLabel[j]);
	}
	for(j=0; j<8; j++)
	{
		SendDlgItemMessage(hwnd, 103, CB_ADDSTRING, 0, (long)lpfLabel[j]);
	}
	for(j=0; j<3; j++)
	{
		SendDlgItemMessage(hwnd, 108, CB_ADDSTRING, 0, (long)trgSrcLabel[j]);
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

	
	for(j=0; j<7; j++)
	{
		sprintf(gstr, "%.3f", 0.000001 * getSampleRateHz()/(1<<j));
		SendDlgItemMessage(hwnd, 101, CB_ADDSTRING, 0, (long)gstr);
	}
	SendDlgItemMessage(hwnd, 101, CB_SETCURSEL, indexSamplingRate, 0);

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

	// port 0=input
	Parms(CONFIGUREIOPORT, cardIndex, 0, 1);//IOconfig);	// configure I/O
	Parms(SETIOPORT, cardIndex, 0, outStatus);	// set port A

	updateInputStatus();

	initializeWaveformParameter(hwnd);

	sprintf(gstr, "%d V", pulseVoltage);
	SetDlgItemText(hwnd, 115, gstr);	

	strcpy(gstr, "Parameters - ");
	strcat(gstr, modelName);

	strcat(gstr, " / ");
	Parms(GETSERIALNUMBER, 0, (DWORD)gstr2, 0);
	strcat(gstr, gstr2);
	strcpy(serialNumber, gstr2);

	Parms(GETMODELNUMBER, 0, (DWORD)modelNumber, 0);

	hardwareRev = Parms(GETREVNUMBER, cardIndex, 0, 0);

	sprintf(gstr2, " / Driver Rev %d", hardwareRev);
	strcat(gstr, gstr2);
	SetWindowText(hwnd, gstr);

	sprintf(gstr, "%d", average);
	SetDlgItemText(hwnd, 106, gstr);

	minVoltage = Parms(GETVMIN, cardIndex, 0, 0);
	maxVoltage = Parms(GETVMAX, cardIndex, 0, 0);

//	downloadParameters(hwnd);

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
}

int paramCommand(HWND hwnd, WPARAM wParam, LONG lParam)
{
	int j, bit=0;

	WORD id = LOWORD(wParam);

	if(id >= 6000 && id <= 6007)
	{
		j = id - 6000;
		GetDlgItemText(hwnd, id, gstr, 2);
		if(*gstr == '0')
		{
			outStatus = outStatus | (1<<j);
			SetDlgItemText(hwnd, id, "1");
		}
		else
		{
			outStatus = outStatus & (0xFF ^ (1<<j));
			SetDlgItemText(hwnd, id, "0");
		}
		Parms(SETIOPORT, cardIndex, 0, outStatus);	// set outputs
	}

	if(id >= 6008 && id <= 60015)
	{
		j = id - 6008;
		GetDlgItemText(hwnd, id, gstr, 2);
		if(*gstr == 'I')
		{
			IOconfig = IOconfig | (1<<j);
			SetDlgItemText(hwnd, id, "O");
		}
		else
		{
			IOconfig = IOconfig & (0xFF ^ (1<<j));
			SetDlgItemText(hwnd, id, "I");
		}
		Parms(CONFIGUREIOPORT, cardIndex, 0, IOconfig);	// configure I/O
	}

	if(HIWORD(wParam) == CBN_SELCHANGE) 
	{
		switch(id)
		{
		case 101:	// sample rate
			indexSamplingRate = SendDlgItemMessage(hwnd, 101,
				CB_GETCURSEL, 0, 0);
			Parms(SETSAMPLINGRATE, cardIndex, indexSamplingRate, 0);
			parameterChanged = 1;
			break;

		case 103:	// LPF
			indexLPF = SendDlgItemMessage(hwnd, 103, CB_GETCURSEL, 0, 0);
			Parms(SETLOWPASSFILTER, cardIndex, indexLPF, 0);
			parameterChanged = 1;
			break;

		case 102:	// HPF
			indexHPF = SendDlgItemMessage(hwnd, 102, CB_GETCURSEL, 0, 0);
			Parms(SETHIGHPASSFILTER, cardIndex, indexHPF, 0);
			parameterChanged = 1;
			break;

		case 118:	// waveform #
			waveIndex = SendDlgItemMessage(hwnd, 118, CB_GETCURSEL, 0, 0);
			initializeWaveformParameter(hwnd);
			parameterChanged = 1;
			break;

		case 114:	// single/dual
			j = transducerMode[waveIndex] = SendDlgItemMessage(hwnd, 114, CB_GETCURSEL, 0, 0);
			if(totalChan > 1)
			{
				Parms(SETCHANTRANSDUCERMODE, cardIndex, txChan[waveIndex], j);
			}
			else
			{
				Parms(SETTRANSDUCERMODE, cardIndex, j, 0);
			}
			parameterChanged = 1;
			break;

		case 120:	// gate type
			gateType[waveIndex][gateIndex[waveIndex]] = SendDlgItemMessage(hwnd, 120, CB_GETCURSEL, 0, 0);
			parameterChanged = 1;
			break;

		case 123:	// gate index
			gateIndex[waveIndex] = SendDlgItemMessage(hwnd, 123, CB_GETCURSEL, 0, 0);
			initGateParameters(hwnd);
			parameterChanged = 1;
			break;

		case 124:	// gate algorithm
			gateAlgorithm[waveIndex][gateIndex[waveIndex]] = SendDlgItemMessage(hwnd, 124, CB_GETCURSEL, 0, 0);
			parameterChanged = 1;
			break;

		case 127:	// active pin
			activePin[waveIndex] = SendDlgItemMessage(hwnd, 127, CB_GETCURSEL, 0, 0);
			parameterChanged = 1;
			break;

		case 128:	// active logic
			activeLogic[waveIndex] = SendDlgItemMessage(hwnd, 128, CB_GETCURSEL, 0, 0);
			parameterChanged = 1;
			break;

		case 1103:
			damping[waveIndex] = SendDlgItemMessage(hwnd, 1103, CB_GETCURSEL, 0, 0);
			if(totalChan > 1)
			{
				Parms(SETCHANDAMPING, cardIndex, txChan[waveIndex], damping[waveIndex]);
			}
			else
			{
				Parms(SETDAMPING, cardIndex, damping[waveIndex], 0);
			}
			parameterChanged = 1;
			break;

		case 1101:	// signal
			signal[waveIndex] = SendDlgItemMessage(hwnd, 1101, CB_GETCURSEL, 0, 0);
			Parms(SETRECTIFIER, cardIndex, signal[waveIndex], 0);
			parameterChanged = 1;
			break;

		case 3002:	// pos/neg
			pulsePolarity[waveIndex] = SendDlgItemMessage(hwnd, 3002, CB_GETCURSEL, 0, 0);
			parameterChanged = 1;
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
		Parms(SETTRIGGERDELAY, cardIndex, postDelay, 0);
		parameterChanged = 1;
		break;

	case 105:	// waveform length
		j = waveformLength =
			(int)oneEntry(hInst, hwnd, "Waveform Length", "samples", 
			(float)waveformLength,
			(float)16, (float)memorySize, 0);
		sprintf(gstr, "%d", waveformLength);
		SetDlgItemText(hwnd, 105, gstr);
		Parms(SETBUFFERLENGTH, cardIndex, waveformLength, 0);
		parameterChanged = 1;
		break;

	case 106:	// average
		average = (int)oneEntry(hInst, hwnd, "Average", "", 
			(float)average,
			(float)1, (float)200, 0);
		sprintf(gstr, "%d", average);
		SetDlgItemText(hwnd, 106, gstr);
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
		Parms(SETPULSEVOLTAGE, cardIndex, getVoltageIndex(), 0);
		break;

	case 116:	// # of waveforms
		totalWaveform =
			(int)oneEntry(hInst, hwnd, "Number of Waveforms", "", 
			(float)totalWaveform,
			(float)1, (float)8, 0);

		sprintf(gstr, "%d", totalWaveform);
		SetDlgItemText(hwnd, 116, gstr);
		initNumWaveform(hwnd);
		break;

	case 117:
		if(optionByte2 & 1)	// half cycles for tone burst pulser
		{
			j = pulseCycle[waveIndex] =
				(int)oneEntry(hInst, hwnd, "# of Half Cycles", "", 
				(float)pulseCycle[waveIndex],
				(float)1.0, (float)32.0, 0);
			sprintf(gstr, "%d", pulseCycle[waveIndex]);
			SetDlgItemText(hwnd, 117, gstr);
		}
		else // pulse width for single pulse pulser
		{
			j = pulseWidth[waveIndex] =
				(int)oneEntry(hInst, hwnd, "Pulse Width", "ns", 
				(float)pulseWidth[waveIndex],
				(float)minPulseWidth, (float)640.0, 0);
			sprintf(gstr, "%d ns", pulseWidth[waveIndex]);
			SetDlgItemText(hwnd, 117, gstr);
		}
		parameterChanged = 1;
		break;

	case 3001:	// frequency
		pulseFrequency[waveIndex] =
			(int)oneEntry(hInst, hwnd, "Frequency", "kHz", 
			(float)pulseFrequency[waveIndex],
			(float)20.0, (float)12500.0, 0);
		sprintf(gstr, "%d kHz", pulseFrequency[waveIndex]);
		SetDlgItemText(hwnd, 3001, gstr);
		parameterChanged = 1;
		break;

	case 119:	// horizontal offset
		hOffset =
			(int)oneEntry(hInst, hwnd, "Offset", "sample", 
			(float)hOffset,
			(float)0.0, (float)waveformLength, 0);
		setHscrollBar(hwnd);
		parameterChanged = 1;
		break;

	case 111:	// RX channel
		rxChan[waveIndex] =
			(int)oneEntry(hInst, hwnd, "RX Channel", "", 
			(float)rxChan[waveIndex]+1,
			(float)1.0, (float)32.0, 0) - 1;
		sprintf(gstr, "%d", rxChan[waveIndex]+1);
		SetDlgItemText(hwnd, 111, gstr);	
		parameterChanged = 1;
		break;

	case 121:	// TX channel
		txChan[waveIndex] =
			(int)oneEntry(hInst, hwnd, "TX Channel", "", 
			(float)txChan[waveIndex]+1,
			(float)1.0, (float)32.0, 0) - 1;
		sprintf(gstr, "%d", txChan[waveIndex]+1);
		SetDlgItemText(hwnd, 121, gstr);	
		parameterChanged = 1;
		break;

	case 122:	// # of gates
		nGate[waveIndex] =	(int)oneEntry(hInst, hwnd, "Total Number of Gates", "", 
			(float)nGate[waveIndex],
			(float)1.0, (float)10, 0);
		initGateParameters(hwnd);
		parameterChanged = 1;
		break;

	case 125:	// gate start
		gateStart[waveIndex][gateIndex[waveIndex]] =
			(int)oneEntry(hInst, hwnd, "Gate Start", "sample", 
			(float)gateStart[waveIndex][gateIndex[waveIndex]],
			(float)0.0, (float)0x3FFF, 0);
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
		parameterChanged = 1;
		break;

	case 129:	// active time (duration)
		outputDuration[waveIndex] =	(int)oneEntry(hInst, hwnd, "Active Time of Output Pin", "millisecond", 
			(float)outputDuration[waveIndex],
			(float)1.0, (float)4095.0, 0);
		sprintf(gstr, "%d", outputDuration[waveIndex]);
		SetDlgItemText(hwnd, 129, gstr);
		parameterChanged = 1;
		break;

	case 130:	// gate threshold
		gateThreshold[waveIndex][gateIndex[waveIndex]] =
			(int)oneEntry(hInst, hwnd, "Gate Start", "sample", 
			(float)gateThreshold[waveIndex][gateIndex[waveIndex]],
			(float)0.0, (float)255.0, 0);
		sprintf(gstr, "%d", gateThreshold[waveIndex][gateIndex[waveIndex]]);
		SetDlgItemText(hwnd, 130, gstr);
		parameterChanged = 1;
		break;

	case 225:	// pause
		pause = (char)SendDlgItemMessage(hwnd, 225, BM_GETCHECK, 0, 0);
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
	parameterChanged = 1;
}

int WMvscroll(HWND hwnd, WORD wParam, LONG lParam)
{
	int id = GetDlgCtrlID((HWND)lParam);

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
			"PARAMETER", masterHwnd, paramCallBack);
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

void	getGateInfo()
{
	int g, tmpTof=0;

	for(g=0; g<nGate[waveIndex]; g++)
	{
		if(g && gateType[waveIndex][g])	// slave gate
			tmpTof = tof[0];
		else
			tmpTof = 0;

		amplitude[g] = (BYTE)gatePeak(gateThreshold[waveIndex][g], gateStart[waveIndex][g]+tmpTof,
			gateLength[waveIndex][g], &tof[g], gateAlgorithm[waveIndex][g],
			byteDataBuffer[waveIndex]);
	}	
}

int averageBuffer[32000];
BYTE averageTemp[32000];

void	getWaveforms()
{
	int n, j = Parms(ISDATAREADY, 0, 0, 0);
	BYTE *chptr;

	if(j < 0)	// disconnected
		return;

	if(j == 1)
	{
		for(int ch=0; ch<totalWaveform; ch++)
		{
			if(average > 1)
			{
				n = average;
				
				for(j=0; j<waveformLength; j++)
					averageBuffer[j] = 0;

				while(n >= 1)
				{
					Parms(GETDATA, cardIndex, (WPARAM)averageTemp, waveformLength);
					for(j=0; j<waveformLength; j++)
						averageBuffer[j] += averageTemp[j];
					n--;
	
					Parms(SOFTWARETRIGGER, cardIndex, 0, 0);	// trigger
					while(!Parms(ISDATAREADY, 0, 0, 0));
				}
				chptr = byteDataBuffer[ch];
				for(j=0; j<waveformLength; j++, chptr++)
					*chptr = averageBuffer[j]/average;
			}
			else
//				Parms(GETDATA, cardIndex, (WPARAM)byteDataBuffer[(ch+totalWaveform-1)%totalWaveform], waveformLength);
				Parms(GETDATA, cardIndex, (WPARAM)byteDataBuffer[ch], waveformLength);
			
			if(ch < totalWaveform - 1)
			{
				downloadWaveformParameters(ch + 1);		// download parameters for next waveform
				Parms(SOFTWARETRIGGER, cardIndex, 0, 0);	// trigger

				while(!Parms(ISDATAREADY, 0, 0, 0));
			}
		}
	}

	XencoderValue = Parms(GETENCODERCOUNTER, cardIndex, 0, 0);
	YencoderValue = Parms(GETENCODERCOUNTER, cardIndex, 1, 0);
	downloadWaveformParameters(0);
	getGateInfo();
	Parms(SOFTWARETRIGGER, cardIndex, 0, 0);	// trigger
}

int oldHunit;

void showVelocity(HWND hwnd)
{
	if(oldHunit == 3)	// mm
	{
		sprintf(gstr, "%f m/s", velocity * 25400);	// convert in/us to m/s
		sprintf(gstr2, "%f mm", thickness * 2.54);	// convert in/us to m/s
	}
	else
	{
		sprintf(gstr, "%f in/us", velocity);
		sprintf(gstr2, "%f in", thickness);
	}
	SetDlgItemText(hwnd, 101, gstr);
	SetDlgItemText(hwnd, 104, gstr2);
}

void displayInitDialog(HWND hwnd, long h)
{
	int j;

	oldHunit = hunit;
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

	SendDlgItemMessage(hwnd, 105, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwnd, 105, CB_ADDSTRING, 0, (long)"Thickness");
	SendDlgItemMessage(hwnd, 105, CB_ADDSTRING, 0, (long)"Velocity");
	SendDlgItemMessage(hwnd, 105, CB_ADDSTRING, 0, (long)"Damping Coefficient");
	SendDlgItemMessage(hwnd, 105, CB_SETCURSEL, displayType, 0);

	sprintf(gstr2, "%f g/cm^3", density);
	SetDlgItemText(hwnd, 106, gstr2);

	sprintf(gstr2, "%d", copyInterval);
	SetDlgItemText(hwnd, 111, gstr2);

	showVelocity(hwnd);

	SendDlgItemMessage(hwnd, 102, BM_SETCHECK, roundTrip, 0);
	SendDlgItemMessage(hwnd, 107, BM_SETCHECK, audioAlarm, 0);
	SendDlgItemMessage(hwnd, 108, BM_SETCHECK, outputPin, 0);
	SendDlgItemMessage(hwnd, 109, BM_SETCHECK, virtualLED, 0);
	SendDlgItemMessage(hwnd, 110, BM_SETCHECK, copyThickness, 0);
}

LRESULT  displayHndl(HWND hwnd,
	unsigned iMessage, WPARAM wParam, LPARAM lParam)
{
	float flt, flt2;

	switch (iMessage) 
	{
		case WM_INITDIALOG:
			displayInitDialog(hwnd, lParam);
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
			case 100:
				if(HIWORD(wParam) == CBN_SELCHANGE)
				{
					oldHunit = SendDlgItemMessage(hwnd, 100, CB_GETCURSEL, 0, 0);
					showVelocity(hwnd);
				}
				break;

			case IDOK:
				hunit = SendDlgItemMessage(hwnd, 100, CB_GETCURSEL, 0, 0);
				vUnit = SendDlgItemMessage(hwnd, 103, CB_GETCURSEL, 0, 0);
				displayType = SendDlgItemMessage(hwnd, 105, CB_GETCURSEL, 0, 0);

				GetDlgItemText(hwnd, 101, gstr, 20);
				flt = (float)atof(gstr);
				if(flt > 0)
				{
					velocity = flt;
					if(hunit == 3)	// mm
						velocity /= 25400;	// convert m/s to in/us
				}
				else
					MessageBox(hwnd, "Invalid velocity value!", gstr, MB_OK);

				GetDlgItemText(hwnd, 104, gstr, 20);
				flt = (float)atof(gstr);
				if(flt > 0)
				{
					thickness = flt;
					if(hunit == 3)	// mm
						thickness /= 2.54;	// convert m/s to in/us
				}
				else
					MessageBox(hwnd, "Invalid thickness value!", gstr, MB_OK);

				GetDlgItemText(hwnd, 106, gstr, 20);
				flt = (float)atof(gstr);
				if(flt > 0)
				{
					density = flt;
				}
				else
					MessageBox(hwnd, "Invalid density value!", gstr, MB_OK);

				roundTrip = SendDlgItemMessage(hwnd, 102, BM_GETCHECK, 0, 0);
				outputPin  = SendDlgItemMessage(hwnd, 108, BM_GETCHECK, 0, 0);
				virtualLED = SendDlgItemMessage(hwnd, 109, BM_GETCHECK, 0, 0);
				copyThickness = SendDlgItemMessage(hwnd, 110, BM_GETCHECK, 0, 0);
				GetDlgItemText(hwnd, 111, gstr, 20);
				copyInterval = atoi(gstr);
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


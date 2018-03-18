#ifndef UTH
#define UTH

// list of mode
#define ISDATAREADY			1000	// wParam=
#define SOFTWARETRIGGER		1001	// wParam=
#define SETSAMPLINGRATE		1002	// wParam: index of sampling rate
	// 0=100MHz, 1=50MHz, 2=25MHz, 3=12.5MHz, 4=6.25MHz, 7=External clock
#define SETDACENABLE		1003	// wParam: 0=DAC off, 1=DAC on
#define SETLOWPASSFILTER	1004	// wParam: 
	//0=No Filter, 1=48 MHz, 2=28 MHz, 3=18 Mhz, 4=8.8 MHz, 5=7.5 MHz, 
	//6=6.7 MHz, 7=5.9 MHz
#define SETHIGHPASSFILTER	1005	// wParam:
	//0=4.8 MHz, 1=1.8 MHz, 2=0.8 MHz, 3=0.6 MHz
#define SETRECTIFIER		1006	// wParam:
	//0=Full, 1=+half, 2=-half, 3=RF
#define SETDAMPING			1007	// wParam:
	//0=620 ohms, 1=339, 2=202, 3=159, 4=60, 5=55, 6=50, 7=47
#define SETTRANSDUCERMODE	1008	// wParam:
	//0=pulse/echo, 1=through transmission
#define SETBUFFERLENGTH		1009	
	// wParam: buffer length 16 to 16382 in step of 4 
#define SETTRIGGERDELAY		1010	
	// wParam: trigger delay 2 to 32764 in step of 2
#define SETPULSEVOLTAGE		1011
	// wParam: 0 - 255
#define SETPULSEWIDTH		1012
	// wParam: 0 - 255
#define SETGAIN				1013
	// wParam: 0 - 1023
#define SETDCOFFSET			1014
	// wParam: 0 - 1023
#define SETDACTABLE			1015
	// wParam: (unsigned char *); lParam: number of data
#define SETADTRIGGERSOURCE	1016
	// wParam: 0=+EXT TRIG (rising edge), 1=software trigger
	// 2=-EXT TRIG (falling edge)
#define GETENCODERCOUNTER	1017
	// wParam: 0=X axis, 1=Y axis, 2=Z axis, 3=W axis
#define SETENCODERCOUNTER	1018
	// wParam: 0=X axis, 1=Y axis, 2=Z axis, 3=W axis
	// lParam: counter value: -8388608 to 8388607
#define GETDATA				1019
	// wParam: (unsigned char *); lParam: number of data
#define RESETCOUNTER		1020
	// wParam: 
#define RESETMEMORY			1021
	// wParam: 
#define SETOFFSETADJUSTMENT	1022
	// wParam: 0=change value and download to the card
	//	1=save value as default for current signal and sampling rate
	// lParam: adjust value -127 to 127
#define GETMODELNUMBER		1023
	// wParam: (char *); 
#define GETSERIALNUMBER		1024
	// wParam: (char *); 
#define GETMEMORYSIZE		1025
	// wParam: 
#define GETMINPULSEWIDTH	1026
	// wParam: 
#define SETMCHANDAMPING		1027
	// wParam: (char *); Value of each element: 0=500 ohms, 1=50 ohms
	// lParam: number of channels starting from channel 1
#define SETMCHANTRANSDUCERMODE	1028
	// wParam: (char *); Value of each element: 0=single, 1=dual transducers
	// lParam: number of channels starting from channel 1
#define SETALLPARAMETERS	1029
	// wParam: (struct DSPUT3100 *)
	// lParam: number of channels starting from channel 1
#define SETPULSERCHANNEL	1030
	// wParam: index of channel; 0=Channel 1, 1=Channel 2...
#define SETRECEIVERCHANNEL	1031
	// wParam: index of channel; 0=Channel 1, 1=Channel 2...
#define SETCHANDAMPING		1032
	// wParam: channel number: 0=channel 1
	// lParam: 0=500 ohms, 1=50 ohms
#define SETCHANTRANSDUCERMODE	1033
	// wParam: channel number: 0=channel 1
	// lParam: 0=single, 1=dual transducers
#define GETOPTIONBYTE1		1034
	// wParam: not used
	// lParam: not used
#define GETOPTIONBYTE2		1035
	// wParam: not used
	// lParam: not used
#define GETREVNUMBER		1036
	// wParam: not used
	// lParam: not used
#define SETLOGAMP			1037
	// wParam: 1=On, or 0=Off
	// lParam: not used
#define SETPRF				1038
	// wParam: 0=internal trigger Off, 10, 20, 30,...5000 Hz
	// lParam: not used
	// Note: This command is for DSPPR300 card only, not for DSPUT3100.
#define SETIOPORT			1039
	// wParam: output value
	// lParam: 0=port 0; 1=port 1
	// return: current status
#define CONFIGUREIOPORT		1040
	// wParam: 0=set port 0 to input port, 1=set port 0 to output port, default: 1
	// lParam: 0=set port 1 to input port, 1=set port 1 to output port, default: 0
#define GET14BITADC			1041
	// wParam: 1=for the 1st 14-bit ADC, 2=for the 2nd 14-bit ADC, 3=for both
	// lParam: not used
	// return: -1 if wParam is out of range.
	//			-2 if specifed 14-bit ADC is not installed.
	//	when wParam==1, return value is 14-bit ADC value from 1st ADC
	//	when wParam==2, return value is 14-bit ADC value from 2nd ADC
	//	when wParam==3, the two MSB bytes are 14-bit ADC value from 2nd ADC
	//		and the two LSB bytes are 14-bit ADC value from 1st ADC
#define GET9BITDATA				1042
	// wParam: (short int *); lParam: number of data
	// return: -2 if wParam is NULL; -3 if 9-bit resolution is not installed on the
	//			board
#define GET14BITADCINPUTRANGE	1043
	// wParam: 1 for minimum value, 2 for maximum value 
	// lParam: 1 for the 1st converter, and 2 for the 2nd converter 
	// return: minimum value if wParam==1, maximum value if wParam==2, or 
	//			-1 if wParam or lParam are out of range
	//			-2 if specifed 14-bit ADC is not installed.
#define GETVMIN	1044
	// wParam: not used
	// lParam: not used
	// return: minimum pulse voltage
#define GETVMAX	1045
	// wParam: not used
	// lParam: not used
	// return: maximum pulse voltage
#define SETTONEBURSTFREQUENCY	1046
	// wParam: Frequency in kHz from 20 to 10,000
	// lParam: Polarity: 0 = Positive, 1 = Negative
#define SETPULSEVOLTAGE2		1050
	// wParam: volts
	// lParam: not used
	// return: actual voltage set

#define GETMINPULSEVOLTAGE		1051
	// same as mode = 1044

#define GETMAXPULSEVOLTAGE		1052
	// same as mode = 1045

#define SWITCHPULSETYPE			1053
	// wParam: 0 for the square pulser, or 1 for the tone burst pulser.
	// lParam:	0 for negative voltage pulse -300V , or 1 for bipolar pulse +/-150V


#define INITIALIZE			5000

//typedef int (*ParmsProc)(int, int, unsigned int, int);

#define MAXUTBOARD		8
#define MAX_WAVEFORM	16

#if defined(__cplusplus)
extern "C" {
#endif  //  __cplusplus

// card: 0 to ?
__declspec(dllexport) int DSPUTParms(int mode, int card, unsigned int wParam, int lParam);

#ifdef NOTVB
typedef int (*ParmsProc)(int, int, unsigned int, int);

#else
typedef int (_stdcall *ParmsProc)(int, int, unsigned int, int);

#endif

#if defined(__cplusplus)
}
#endif  //  __cplusplus


#endif

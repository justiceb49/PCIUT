#include "stdafx.h"

int algGap=2, gapEnd;

WORD	 findMaxpeak(WORD thresh, WORD start,
	WORD len, WORD *tof, BYTE *buff)
{
	short int ch=0, index=0;

	buff += start;
	for(short int j=0; j<len; j++)
	{
		if(*buff >= thresh && *buff > ch)
		{
			index = j;
            ch = *buff;
        }
		buff++;
	}

	gapEnd = j;	//ver 561
	*tof = index + start;
	return ch;
}

WORD maxAmp=255;

WORD	 findMinipeak(WORD thresh, WORD start,
	WORD len, WORD *tof, BYTE *buff)
{
	short int ch=maxAmp, index=0;

	buff += start;
	for(short int j=0; j<len; j++)
	{
		if(*buff <= thresh && *buff < ch)
		{
			index = j;
			ch = *buff;
//			if(!ch)	// ver 567
//				ch = 1;
		}
		buff++;
	}

	*tof = index + start;
//	return ch;	ver 573
	return maxAmp - ch;
}

WORD	 findMaxedge(WORD thresh, WORD start,
	WORD len, WORD *tof, BYTE *buff)
{
	buff += start;
	for(short int j=0; j<len; j++)
	{
		if(*buff >= thresh)
		{
			*tof = j + start;
			gapEnd = j;	//ver 361
			if(thresh == 0)
				return 1;
			return *buff;
		}
		buff++;
	}

	gapEnd = j;	//ver 561
	*tof = start;
	return 0;
}

WORD	 findMiniedge(WORD thresh, WORD start,
	WORD len, WORD *tof, BYTE *buff)
{
	buff += start;
	for(short int j=0; j<len; j++)
	{
		if(*buff <= thresh)
		{
			*tof = j + start;
			gapEnd = j;	//ver 361
//		    return *buff;	ver 573
		    return maxAmp - *buff;
		}
        buff++;
	}

	gapEnd = j;	//ver 561
	*tof = start;
    return 0;
}

WORD  gatePeak(WORD thresh,
	WORD start, WORD len, WORD *tof,short int alg, BYTE *buff)
{
	switch (alg)
	{
	case 0:
		return findMaxpeak(thresh, start, len, tof, buff);

	case 1:
		return findMinipeak(thresh, start, len, tof, buff);

	case 2:
		return findMaxedge(thresh, start, len, tof, buff);

	case 3:
		return findMiniedge(thresh, start, len, tof, buff);

	case 4:
		alg = findMaxpeak(thresh, start, len, tof, buff);
		if(alg > thresh)
			alg = 0;
		else
			alg = 255 - alg;
		return alg;
	}

	*tof = 0;
	return 0;
}


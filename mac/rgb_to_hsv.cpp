/*
 *  rgb_to_hsv.cpp
 *  valourut
 *
 *  Created by Joel Lehtonen on 14.5.2011.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "rgb_to_hsv.h"

// Got this example from http://www.nunosantos.net/archives/114 .
// No license mentioned but trivial implementation from Wikipedia article.

void HSVtoRGB( int *r, int *g, int *b, int h, int s, int v )
{
	int f;
	long p, q, t;
	
	if( s == 0 )
	{
		*r = *g = *b = v;
		return;
	}
	
	f = ((h%60)*255)/60;
	h /= 60;
	
	p = (v * (256 - s))/256;
	q = (v * ( 256 - (s * f)/256 ))/256;
	t = (v * ( 256 - (s * ( 256 - f ))/256))/256;
	
	switch( h ) {
		case 0:
			*r = v;
			*g = t;
			*b = p;
			break;
		case 1:
			*r = q;
			*g = v;
			*b = p;
			break;
		case 2:
			*r = p;
			*g = v;
			*b = t;
			break;
		case 3:
			*r = p;
			*g = q;
			*b = v;
			break;
		case 4:
			*r = t;
			*g = p;
			*b = v;
			break;
		default:
			*r = v;
			*g = p;
			*b = q;
			break;
	}
}

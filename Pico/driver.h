
// This file is part of the PicoDrive Megadrive Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

// Drive filler file for ym2612.c
#undef INLINE
#define INLINE __inline

#define CLIB_DECL 
#define INTERNAL_TIMER

// Callbacks from fm.c
int YM2612UpdateReq(int nChip);
int AY8910_set_clock(int nChip,int nClock);
//int Log(int nType,char *szText,...);
extern void *errorlog;

#ifndef __GNUC__
#pragma warning (disable:4100)
#pragma warning (disable:4244)
#pragma warning (disable:4245)
#pragma warning (disable:4710)
#endif


// This file is part of the PicoDrive Megadrive Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "PicoInt.h"

#ifdef _WIN32_WCE

#pragma warning(disable:4514)
#pragma warning(push)
#pragma warning(disable:4201)
#include <windows.h>
#pragma warning(pop)

static float Period=0.0f;
static LARGE_INTEGER TimeStart={0,0};

int ProfileInit()
{
  LARGE_INTEGER freq={0,0};

  QueryPerformanceFrequency(&freq);

  Period =(float)freq.HighPart*4294967296.0f;
  Period+=(float)freq.LowPart;

  if (Period>=1.0f) Period=1.0f/Period;
  return 0;
}

int ProfileStart()
{
  QueryPerformanceCounter(&TimeStart);

  return 0;
}

float ProfileTime()
{
  LARGE_INTEGER end={0,0};
  int ticks=0;
  float seconds=0.0f;

  QueryPerformanceCounter(&end);

  ticks=end.LowPart-TimeStart.LowPart;
  seconds=(float)ticks*Period;

  return seconds;
}

#endif



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


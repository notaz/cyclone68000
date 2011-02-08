
// This file is part of the PicoDrive Megadrive Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "stdafx.h"

int WaveRate=0;
int WaveLen=0; // Length of each buffer in samples
short *WaveDest=NULL; // Destination to render sound

static HWAVEOUT WaveOut=NULL;
static short *WaveBuf=NULL; // Wave double-buffer
static WAVEHDR WaveHeader[2]; // WAVEHDR for each buffer
static int WavePlay=0; // Next buffer side to play

int WaveInit()
{
  WAVEFORMATEX wfx;
  WAVEHDR *pwh=NULL;

  if (WaveOut) return 0; // Already initted

  memset(&wfx,0,sizeof(wfx));
  memset(&WaveHeader,0,sizeof(WaveHeader));

  wfx.wFormatTag=WAVE_FORMAT_PCM;
  wfx.nChannels=2; // stereo
  wfx.nSamplesPerSec=WaveRate; // sample rate
  wfx.wBitsPerSample=16;
  // Calculate bytes per sample and per second
  wfx.nBlockAlign=(unsigned short)( (wfx.wBitsPerSample>>3)*wfx.nChannels );
  wfx.nAvgBytesPerSec=wfx.nSamplesPerSec*wfx.nBlockAlign;

  waveOutOpen(&WaveOut,WAVE_MAPPER,&wfx,0,NULL,CALLBACK_NULL);

  // Allocate both buffers
  WaveBuf=(short *)malloc(WaveLen<<3);
  if (WaveBuf==NULL) return 1;
  memset(WaveBuf,0,WaveLen<<3);

  // Make WAVEHDRs for both buffers
  pwh=WaveHeader+0;
  pwh->lpData=(char *)WaveBuf;
  pwh->dwBufferLength=WaveLen<<2;
  pwh->dwLoops=1;

  pwh=WaveHeader+1;
  *pwh=WaveHeader[0]; pwh->lpData+=WaveLen<<2;

  // Prepare the buffers
  waveOutPrepareHeader(WaveOut,WaveHeader,  sizeof(WAVEHDR));
  waveOutPrepareHeader(WaveOut,WaveHeader+1,sizeof(WAVEHDR));

  // Queue both buffers:
  WavePlay=0;
  WaveHeader[0].dwFlags|=WHDR_DONE;
  WaveHeader[1].dwFlags|=WHDR_DONE;
  WaveUpdate();
  return 0;
}

int WaveExit()
{
  WAVEHDR *pwh=NULL;
  int i=0;

  if (WaveOut) waveOutReset(WaveOut);

  for (i=0;i<2;i++)
  {
    pwh=WaveHeader+i;
    if (pwh->lpData) waveOutUnprepareHeader(WaveOut,pwh,sizeof(*pwh));
  }
  memset(WaveHeader,0,sizeof(WaveHeader));

  free(WaveBuf); WaveBuf=NULL; WaveLen=0;

  if (WaveOut) waveOutClose(WaveOut);  WaveOut=NULL;
  return 0;
}

int WaveUpdate()
{
  WAVEHDR *pwh=NULL; int i=0;
  int Last=-1;

  for (i=0;i<2;i++)
  {
    pwh=WaveHeader+WavePlay;
    if (pwh->lpData==NULL) return 1; // Not initted

    if (pwh->dwFlags&WHDR_DONE)
    {
      // This buffer has finished - start it playing again
      WaveDest=(short *)pwh->lpData;
      SndRender();
      WaveDest=NULL;
      
      waveOutWrite(WaveOut,pwh,sizeof(*pwh));
      Last=WavePlay; // Remember the last buffer we played
    }

    WavePlay++; WavePlay&=1;
  }

  if (Last>=0) WavePlay=Last^1; // Next buffer to play is the other one
  return 0;
}

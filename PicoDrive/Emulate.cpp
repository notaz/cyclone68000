
// This file is part of the PicoDrive Megadrive Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "stdafx.h"

TCHAR RomName[260]={0};
static unsigned char *RomData=NULL;
static unsigned int RomSize=0;

static unsigned int LastSecond=0;
static int FramesDone=0;
static int FramesPerSecond=60;

struct Target Targ;

static int TargetInit()
{
  RECT rect={0,0,0,0};
  int height=0;

  memset(&Targ,0,sizeof(Targ));

  height=168;

  ClientToScreen(FrameWnd,&Targ.point);

  GetClientRect(FrameWnd,&rect);
  // Find out where the top of the screen should go:
  rect.top=(rect.bottom-height)>>1;
  if (rect.top<0) rect.top=0;
  rect.bottom=rect.top+height;

  Targ.view=rect; // Save the view rectangle (client coordinates)

  Targ.offset=Targ.view.top+Targ.point.y;

  return 0;
}

static int TargetUpdate()
{
  // Need to repaint the view rectangle:
  GetUpdateRect(FrameWnd,&Targ.update,0);

  Targ.top   =Targ.update.top   +Targ.point.y;
  Targ.bottom=Targ.update.bottom+Targ.point.y;

  return 0;
}

int EmulateInit()
{
  FILE *f=NULL;

  EmulateExit(); // Make sure exited

  TargetInit(); // Find out where to put the screen

  PicoInit();

  // Load cartridge
  f=_wfopen(RomName,L"rb"); if (f==NULL) return 1;
  PicoCartLoad(f,&RomData,&RomSize);
  fclose(f);

  PicoCartInsert(RomData,RomSize);

  LastSecond=GetTickCount(); FramesDone=0;

  return 0;
}

void EmulateExit()
{
  // Remove cartridge
  PicoCartInsert(NULL,0);
  if (RomData) free(RomData); RomData=NULL; RomSize=0;
  
  PicoExit();
}

// Callback for scanline data:
static int EmulateScan(unsigned int scan,unsigned short *sdata)
{
  int len=0;
  unsigned short *ps=NULL,*end=NULL;
  unsigned char *pd=NULL;
  int xpitch=0;

  if ((scan&3)==1) return 0;
  scan+=scan<<1; scan>>=2; // Reduce size to 75%

  scan+=Targ.offset;
  if ((int)scan< Targ.top) return 0; // Out of range
  if ((int)scan>=Targ.bottom) return 0; // Out of range

  pd=Targ.screen+scan*GXDisp.cbyPitch;

  len=240;
  xpitch=GXDisp.cbxPitch;
  ps=sdata; end=ps+320;

  // Reduce 4 pixels into 3
  do
  {
    *(unsigned short *)pd=ps[0]; pd+=xpitch;
    *(unsigned short *)pd=(unsigned short)((ps[1]+ps[2])>>1); pd+=xpitch;
    *(unsigned short *)pd=ps[3]; pd+=xpitch;
    ps+=4;
  }
  while (ps<end);

  return 0;
}

static int DoFrame()
{
  int pad=0,i=0,ks=0;
  char map[8]={0,1,2,3,5,6,4,7}; // u/d/l/r/b/c/a/start

  for (i=0;i<8;i++)
  {
    ks=GetAsyncKeyState(Config.key[map[i]]);
    if (ks) pad|=1<<i;
  }
  PicoPad[0]=pad;

  PicoFrame();
  return 0;
}

static int DrawFrame()
{
  // Now final frame is drawn:
  InvalidateRect(FrameWnd,&Targ.view,0);

  if (Main3800) Targ.screen=(unsigned char *)0xac0755a0; // The real 3800 screen address
  else          Targ.screen=(unsigned char *)GXBeginDraw();

  if (Targ.screen==NULL) return 1;

  TargetUpdate();

  PicoScan=EmulateScan; // Setup scanline callback
  DoFrame();
  PicoScan=NULL;


  
  if (Main3800==0) GXEndDraw();

  Targ.screen=NULL;

  ValidateRect(FrameWnd,&Targ.update);

  if (PicoStatus[0])
  {
    // Print the status of the 68000:
    HDC hdc=GetDC(FrameWnd);
    RECT rect={0,220, 240,260};
    TCHAR status[128]={0};

    wsprintf(status,L"%.120S",PicoStatus);

    FillRect(hdc,&rect,(HBRUSH)GetStockObject(WHITE_BRUSH));
    SetBkMode(hdc,TRANSPARENT);

    DrawText(hdc,status,lstrlen(status),&rect,0);
    ReleaseDC(FrameWnd,hdc);
  }

  return 0;
}

int EmulateFrame()
{
  int i=0,need=0;
  int time=0,frame=0;

  if (RomData==NULL) return 1;

  // Speed throttle:
  time=GetTickCount()-LastSecond; // This will be about 0-1000 ms
  frame=time*FramesPerSecond/1000;
  need=frame-FramesDone;
  FramesDone=frame;

  if (FramesPerSecond>0)
  {
    // Carry over any >60 frame count to one second
    while (FramesDone>=FramesPerSecond) { FramesDone-=FramesPerSecond; LastSecond+=1000; }
  }

  if (need<=0) { Sleep(2); return 1; }
  if (need>4) need=4; // Limit frame skipping

  for (i=0;i<need-1;i++) DoFrame(); // Frame skip if needed

  DrawFrame();
  return 0;
}

int SndRender()
{
//  int p=0;

  PsndRate=WaveRate;
  PsndLen=WaveLen;
  PsndOut=WaveDest;

  DrawFrame();
  // Convert to signed:
//  for (p=0;p<PsndLen<<1;p++) PsndOut[p]+=0x8000;

  PsndRate=PsndLen=0;
  PsndOut=NULL;

  return 0;
}

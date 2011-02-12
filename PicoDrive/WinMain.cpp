
// This file is part of the PicoDrive Megadrive Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "stdafx.h"
#include <stdarg.h>

static FILE *DebugFile=NULL;
int Main3800=0;
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPTSTR,int)
{
  MSG msg; int ret=0;
  TCHAR device[260];

  memset(&msg,0,sizeof(msg));
  memset(device,0,sizeof(device));

  // Check if this program is running already:
  FrameWnd=FindWindow(APP_TITLE,NULL);
  if (FrameWnd!=NULL) { SetForegroundWindow(FrameWnd); return 0; }

  DeleteFile(L"zout.txt");

  SystemParametersInfo(SPI_GETOEMINFO,sizeof(device)>>1,device,0);
  if (_wcsicmp(device,L"compaq ipaq h3800")==0) Main3800=1;

  FrameInit();

  ConfigInit();
  ConfigLoad();

  WaveRate=44100; WaveLen=735;
  WaveInit();

  for(;;)
  {
    ret=PeekMessage(&msg,NULL,0,0,PM_REMOVE);
    if (ret)
    {
      if (msg.message==WM_QUIT) break;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else
    {
      EmulateFrame();
      //WaveUpdate();
      Sleep(1);
    }
  }

  WaveExit();
  EmulateExit();

  ConfigSave();

  DestroyWindow(FrameWnd);

  if (DebugFile) fclose(DebugFile);
  DebugFile=NULL;
  return 0;
}

extern "C" int dprintf(char *Format, ...)
{
  va_list VaList=NULL;
  va_start(VaList,Format);

  if (DebugFile==NULL) DebugFile=fopen("zout.txt","wt");
  if (DebugFile) vfprintf(DebugFile,Format,VaList);

  va_end(VaList);
  return 0;
}

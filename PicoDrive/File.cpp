
// This file is part of the PicoDrive Megadrive Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "stdafx.h"

// Loading roms, loading and saving states etc...

int FileLoadRom()
{
  OPENFILENAME ofn;

  memset(&ofn,0,sizeof(ofn));
  memset(&RomName,0,sizeof(RomName));

  ofn.lStructSize=sizeof(ofn);
  ofn.hwndOwner=FrameWnd;
  ofn.hInstance=GetModuleHandle(NULL);
  ofn.lpstrFile=RomName;
  ofn.nMaxFile=260;
  ofn.lpstrDefExt=L"bin";
  ofn.lpstrFilter=L"Rom Files\0*.bin;*.gen;*.smd\0\0";

  GetOpenFileName(&ofn);

  UpdateWindow(FrameWnd);

  // Open new rom:
  if (RomName[0]) EmulateInit();

  return 0;
}

int FileState(int load)
{
  OPENFILENAME ofn;
  WCHAR name[260]={0};

  if (load==0) wcscpy(name,L"State.mds");

  memset(&ofn,0,sizeof(ofn));
  ofn.lStructSize=sizeof(ofn);
  ofn.hwndOwner=FrameWnd;
  ofn.hInstance=GetModuleHandle(NULL);
  ofn.lpstrFile=name;
  ofn.nMaxFile=sizeof(name)>>1;
  ofn.lpstrDefExt=L"mds";
  ofn.lpstrFilter=L"MD State Files\0*.mds\0\0";

  if (load) GetOpenFileNameW(&ofn);
  else      GetSaveFileNameW(&ofn);
  UpdateWindow(FrameWnd);

  if (name[0]==0) return 1;

  if (PmovFile) fclose(PmovFile);

  PmovFile=_wfopen(name,load ? L"rb":L"wb");
  if (PmovFile==NULL) return 1;
  
  PmovAction=load?6:5;
  PmovState(); // Save the state

  return 0;
}

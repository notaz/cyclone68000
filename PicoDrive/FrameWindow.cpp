
// This file is part of the PicoDrive Megadrive Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "stdafx.h"

HWND FrameWnd=NULL;
struct GXDisplayProperties GXDisp;
struct GXKeyList GXKey;

// Window procedure for frame window
static LRESULT CALLBACK FrameProc(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam)
{
  switch (Msg)
  {
    case WM_COMMAND:
    switch (LOWORD(wParam))
    {
      case IDOK: case IDCANCEL: SendMessage(hWnd,WM_CLOSE,0,0); break;

      case ID_LOADROM: FileLoadRom(); break;
      case ID_OPTIONS_GRAB: DebugScreenGrab(); break;
      case ID_OPTIONS_SAVE: FileState(0); break;
      case ID_OPTIONS_LOAD: FileState(1); break;
    }
    return 0;

    case WM_KILLFOCUS: GXSuspend(); return 0;
    case WM_SETFOCUS: GXResume(); return 0;

    case WM_CLOSE: PostQuitMessage(0); return 0;

    case WM_DESTROY:
      GXCloseInput();
      GXCloseDisplay();
      FrameWnd=NULL; // Blank window handle
    return 0;
  }

  return DefWindowProc(hWnd,Msg,wParam,lParam);
}

static int GxInit()
{
  GXOpenDisplay(FrameWnd,GX_FULLSCREEN);
  GXOpenInput();
  GXDisp=GXGetDisplayProperties();
  GXKey=GXGetDefaultKeys(GX_NORMALKEYS);

  // The real layout of the 3800:
  if (Main3800) { GXDisp.cbxPitch=-640; GXDisp.cbyPitch=2; }

  return 0;
}

int FrameInit()
{
  WNDCLASS wc;
  SHMENUBARINFO mbi;
  TCHAR title[128]={0};
  RECT rect={0,0,0,0};

  memset(&wc,0,sizeof(wc));
  memset(&mbi,0,sizeof(mbi));

  // Register the Frame window class
  wc.lpfnWndProc=FrameProc;
  wc.hInstance=GetModuleHandle(NULL);
  wc.lpszClassName=APP_TITLE;
  wc.hbrBackground=(HBRUSH)CreateSolidBrush(0x404040);
  RegisterClass(&wc);

  FrameWnd=CreateWindowEx(WS_EX_CAPTIONOKBTN,APP_TITLE,APP_TITLE,WS_VISIBLE,
    CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
    NULL,NULL,wc.hInstance,NULL);
  
  wsprintf(title,APP_TITLE L" v%x.%.3x",PicoVer>>12,PicoVer&0xfff);
  SetWindowText(FrameWnd,title);

  // Show SIP
  mbi.cbSize=sizeof(mbi);
  mbi.hwndParent=FrameWnd;
  mbi.nToolBarId=IDR_MENUBAR1;
  mbi.hInstRes=wc.hInstance;
  SHCreateMenuBar(&mbi);

  // Resize Frame to avoid the SIP
  GetWindowRect(FrameWnd,&rect);
  MoveWindow(FrameWnd, rect.left,rect.top, rect.right-rect.left,rect.bottom-rect.top-26, 1);

  GxInit();

  FileLoadRom();
  return 0;
}

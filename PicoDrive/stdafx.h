
// This file is part of the PicoDrive Megadrive Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#pragma warning(disable:4514)
#pragma warning(push)
#pragma warning(disable:4201)
#include <windows.h>
#pragma warning(pop)

#include <aygshell.h>
#include <commdlg.h>
#include <gx.h>

#include "resource.h"

#include "../Pico/Pico.h"

#define APP_TITLE L"PicoDrive"

// ----------------------------------------------------------

struct Target
{
  unsigned char *screen;
  POINT point; // Screen to client point
  RECT view,update;
  int offset; // Amount to add onto scanline
  int top,bottom; // Update rectangle in screen coordinates
};

// Config.cpp
struct Config
{
  int key[8];
};
extern struct Config Config;

int ConfigInit();
int ConfigSave();
int ConfigLoad();

// Debug.cpp
int DebugShowRam();
int DebugScreenGrab();

// Emulate.cpp
extern struct Target Targ;
extern TCHAR RomName[260];
int EmulateInit();
void EmulateExit();
int EmulateFrame();
int SndRender();

// File.cpp
int FileLoadRom();
int FileState(int load);

// FrameWindow.cpp
extern HWND FrameWnd;
extern struct GXDisplayProperties GXDisp;
extern struct GXKeyList GXKey;
extern int FrameShowRam;
int FrameInit();

// Wave.cpp
extern int WaveRate;
extern int WaveLen; // Length of each buffer in samples
extern short *WaveDest; // Destination to render sound
int WaveInit();
int WaveExit();
int WaveUpdate();

// WinMain.cpp
extern "C" int dprintf(char *Format, ...);
extern int Main3800;

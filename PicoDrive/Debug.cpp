
// This file is part of the PicoDrive Megadrive Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "stdafx.h"

static int ScreenNum=0;

int DebugScreenGrab()
{
  unsigned char *screen=NULL;
  FILE *file=NULL;
  unsigned char *line=NULL;
  int x=0,y=0;
  char filename[64];
  unsigned char head[0x12];

  memset(filename,0,sizeof(filename));
  memset(head,0,sizeof(head));

  // Allocate memory for one line
  line=(unsigned char *)malloc(GXDisp.cxWidth*3); if (line==NULL) return 1;
  memset(line,0,GXDisp.cxWidth*3);

  // Get pointer to screen:
  screen=(unsigned char *)GXBeginDraw(); if (screen==NULL) { free(line); return 1; }

  // Open screenshot file:
  for (;;)
  {
    sprintf(filename,"\\Screen%.3d.tga",ScreenNum);

    // Does this file exist?
    file=fopen(filename,"rb"); if (file==NULL) break; // No - use this
    
    // Exists, try next file
    fclose(file); ScreenNum++;
    if (ScreenNum>=1000) break;
  }

  // Use this filename
  file=fopen(filename,"wb"); if (file==NULL) { GXEndDraw(); free(line); return 1; }

  head[0x02]=0x02; //?
  head[0x0c]=(unsigned char) GXDisp.cxWidth;
  head[0x0d]=(unsigned char)(GXDisp.cxWidth>>8);
  head[0x0e]=(unsigned char) GXDisp.cyHeight;
  head[0x0f]=(unsigned char)(GXDisp.cyHeight>>8);
  head[0x10]=24; // Number of bits per pixel

  // Write header:
  fwrite(head,1,sizeof(head),file);

  for (y=0;y<(int)GXDisp.cyHeight;y++)
  {
    unsigned char *ps=NULL,*pd=NULL;
    int ry=0;
    int pix=0;

    ry=GXDisp.cyHeight-y-1;
    ps=screen+ry*GXDisp.cbyPitch;
    pd=line;

    // Copy pixel to our line buffer
    for (x=0;x<(int)GXDisp.cxWidth; x++,ps+=GXDisp.cbxPitch,pd+=3)
    {
      pix=*(unsigned short *)ps;

      pd[0]=(unsigned char)((pix&0x001f)<<3); // Red
      pd[1]=(unsigned char)((pix&0x07e0)>>3); // Green
      pd[2]=(unsigned char)((pix&0xf800)>>8); // Blue
    }

    fwrite(line,1,GXDisp.cxWidth*3,file);
  }

  fclose(file); file=NULL;
  
  GXEndDraw();
  free(line);

  return 0;
}

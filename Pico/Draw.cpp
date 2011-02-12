
// This file is part of the PicoDrive Megadrive Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "PicoInt.h"
#ifndef __GNUC__
#pragma warning (disable:4706) // Disable assignment with conditional
#endif

int (*PicoScan)(unsigned int num,unsigned short *data)=NULL;

// Line colour indices - in the format 00ppcccc pp=palette, cccc=colour
static unsigned short HighCol[32+320+8]; // Gap for 32 column, and messy border on right
static int Scanline=0; // Scanline

int PicoMask=0xfff; // Mask of which layers to draw

static int TileNorm(unsigned short *pd,int addr,unsigned int *pal)
{
  unsigned int pack=0; unsigned int t=0;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=pack&0x0000f000; if (t) { t=pal[t>>12]; pd[0]=(unsigned short)t; }
    t=pack&0x00000f00; if (t) { t=pal[t>> 8]; pd[1]=(unsigned short)t; }
    t=pack&0x000000f0; if (t) { t=pal[t>> 4]; pd[2]=(unsigned short)t; }
    t=pack&0x0000000f; if (t) { t=pal[t    ]; pd[3]=(unsigned short)t; }
    t=pack&0xf0000000; if (t) { t=pal[t>>28]; pd[4]=(unsigned short)t; }
    t=pack&0x0f000000; if (t) { t=pal[t>>24]; pd[5]=(unsigned short)t; }
    t=pack&0x00f00000; if (t) { t=pal[t>>20]; pd[6]=(unsigned short)t; }
    t=pack&0x000f0000; if (t) { t=pal[t>>16]; pd[7]=(unsigned short)t; }
    return 0;
  }

  return 1; // Tile blank
}

static int TileFlip(unsigned short *pd,int addr,unsigned int *pal)
{
  unsigned int pack=0; unsigned int t=0;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=pack&0x000f0000; if (t) { t=pal[t>>16]; pd[0]=(unsigned short)t; }
    t=pack&0x00f00000; if (t) { t=pal[t>>20]; pd[1]=(unsigned short)t; }
    t=pack&0x0f000000; if (t) { t=pal[t>>24]; pd[2]=(unsigned short)t; }
    t=pack&0xf0000000; if (t) { t=pal[t>>28]; pd[3]=(unsigned short)t; }
    t=pack&0x0000000f; if (t) { t=pal[t    ]; pd[4]=(unsigned short)t; }
    t=pack&0x000000f0; if (t) { t=pal[t>> 4]; pd[5]=(unsigned short)t; }
    t=pack&0x00000f00; if (t) { t=pal[t>> 8]; pd[6]=(unsigned short)t; }
    t=pack&0x0000f000; if (t) { t=pal[t>>12]; pd[7]=(unsigned short)t; }
    return 0;
  }
  return 1; // Tile blank
}

struct TileStrip
{
  int nametab; // Position in VRAM of name table (for this tile line)
  int line;    // Line number in pixels 0x000-0x3ff within the virtual tilemap 
  int hscroll; // Horizontal scroll value in pixels for the line
  int xmask;   // X-Mask (0x1f - 0x7f) for horizontal wraparound in the tilemap
  int high;    // High or low tiles
};

static int WrongPri=0; // 1 if there were tiles which are the wrong priority

static int DrawStrip(struct TileStrip ts)
{
  int tilex=0,dx=0,ty=0;
  int blank=-1; // The tile we know is blank

  WrongPri=0;

  // Draw tiles across screen:
  tilex=(-ts.hscroll)>>3;
  ty=(ts.line&7)<<1; // Y-Offset into tile
  for (dx=((ts.hscroll-1)&7)+1; dx<328; dx+=8,tilex++)
  {
    int code=0,addr=0,zero=0;
    unsigned int *pal=NULL;

    code=Pico.vram[ts.nametab+(tilex&ts.xmask)];
    if (code==blank) continue;
    if ((code>>15)!=ts.high) { WrongPri=1; continue; }

    // Get tile address/2:
    addr=(code&0x7ff)<<4;
    if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

    pal=Pico.highpal+((code>>9)&0x30);

    if (code&0x0800) zero=TileFlip(HighCol+24+dx,addr,pal);
    else             zero=TileNorm(HighCol+24+dx,addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  return 0;
}

static int DrawLayer(int plane,int high)
{
  struct PicoVideo *pvid=&Pico.video;
  static char shift[4]={5,6,6,7}; // 32,64 or 128 sized tilemaps
  struct TileStrip ts;

  // Work out the TileStrip to draw

  // Get vertical scroll value:
  int vscroll=Pico.vsram[plane];

  int htab=pvid->reg[13]<<9; // Horizontal scroll table address
  if ( pvid->reg[11]&2)     htab+=Scanline<<1; // Offset by line
  if ((pvid->reg[11]&1)==0) htab&=~0xf; // Offset by tile
  htab+=plane; // A or B

  // Get horizontal scroll value
  ts.hscroll=Pico.vram[htab&0x7fff];

  // Work out the name table size: 32 64 or 128 tiles (0-3)
  int width=pvid->reg[16];
  int height=(width>>4)&3; width&=3;

  ts.xmask=(1<<shift[width ])-1; // X Mask in tiles
  int ymask=(8<<shift[height])-1; // Y Mask in pixels

  // Find name table:
  if (plane==0) ts.nametab=(pvid->reg[2]&0x38)<< 9; // A
  else          ts.nametab=(pvid->reg[4]&0x07)<<12; // B

  // Find the line in the name table
  ts.line=(vscroll+Scanline)&ymask;
  ts.nametab+=(ts.line>>3)<<shift[width];

  ts.high=high;

  DrawStrip(ts);
  return 0;
}

static int DrawWindow(int high)
{
  struct PicoVideo *pvid=&Pico.video;
  struct TileStrip ts;

  ts.line=Scanline;
  ts.hscroll=0;

  // Find name table line:
  if (Pico.video.reg[12]&1)
  {
    ts.nametab=(pvid->reg[3]&0x3c)<<9; // 40-cell mode
    ts.nametab+=(ts.line>>3)<<6;
  }
  else
  {
    ts.nametab=(pvid->reg[3]&0x3e)<<9; // 32-cell mode
    ts.nametab+=(ts.line>>3)<<5;
  }

  ts.xmask=0x3f;
  ts.high=high;

  DrawStrip(ts);
  return 0;
}

static int DrawSprite(int sy,unsigned short *sprite,int high)
{
  int sx=0,width=0,height=0;
  int row=0,code=0;
  unsigned int *pal=NULL;
  int tile=0,delta=0;
  int i=0;

  code=sprite[2];
  if ((code>>15)!=high) { WrongPri=1; return 0; } // Wrong priority

  height=sprite[1]>>8;
  width=(height>>2)&3; height&=3;
  width++; height++; // Width and height in tiles
  if (Scanline>=sy+(height<<3)) return 0; // Not on this line after all

  row=Scanline-sy; // Row of the sprite we are on
  pal=Pico.highpal+((code>>9)&0x30); // Get palette pointer
  if (code&0x1000) row=(height<<3)-1-row; // Flip Y

  tile=code&0x7ff; // Tile number
  tile+=row>>3; // Tile number increases going down
  delta=height; // Delta to increase tile by going right
  if (code&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

  tile<<=4; tile+=(row&7)<<1; // Tile address
  delta<<=4; // Delta of address

  sx=(sprite[3]&0x1ff)-0x78; // Get X coordinate + 8

  for (i=0; i<width; i++,sx+=8,tile+=delta)
  {
    if (sx<=0 || sx>=328) continue; // Offscreen

    tile&=0x7fff; // Clip tile address
    if (code&0x0800) TileFlip(HighCol+24+sx,tile,pal);
    else             TileNorm(HighCol+24+sx,tile,pal);
  }

  return 0;
}

static int DrawAllSprites(int high)
{
  struct PicoVideo *pvid=&Pico.video;
  int table=0;
  int i=0,link=0;
  unsigned char spin[80]; // Sprite index

  WrongPri=0;

  table=pvid->reg[5]&0x7f;
  if (pvid->reg[12]&1) table&=0x7e; // Lowest bit 0 in 40-cell mode
  table<<=8; // Get sprite table address/2

  for (;;)
  {
    unsigned short *sprite=NULL;
    
    spin[i]=(unsigned char)link;
    sprite=Pico.vram+((table+(link<<2))&0x7ffc); // Find sprite

    // Find next sprite
    link=sprite[1]&0x7f;
    if (link==0 || i>=79) break; // End of sprites
    i++;
  }

  // Go through sprites backwards:
  for ( ;i>=0; i--)
  {
    unsigned short *sprite=NULL;
    int sy=0;
    
    sprite=Pico.vram+((table+(spin[i]<<2))&0x7ffc); // Find sprite

    sy=(sprite[0]&0x1ff)-0x80; // Get Y coordinate

    if (Scanline>=sy && Scanline<sy+32) DrawSprite(sy,sprite,high); // Possibly on this line
  }

  return 0;
}

static void BackFill()
{
  unsigned int back=0;
  unsigned int *pd=NULL,*end=NULL;

  // Start with a blank scanline (background colour):
  back=Pico.video.reg[7]&0x3f;
  back=Pico.highpal[back];
  back|=back<<16;

  pd= (unsigned int *)(HighCol+32);
  end=(unsigned int *)(HighCol+32+320);

  do { pd[0]=pd[1]=pd[2]=pd[3]=back; pd+=4; } while (pd<end);
}

static int DrawDisplay()
{
  int win=0,edge=0,full=0;
  int bhigh=1,ahigh=1,shigh=1;

  // Find out if the window is on this line:
  win=Pico.video.reg[0x12];
  edge=(win&0x1f)<<3;

  if (win&0x80) { if (Scanline>=edge) full=1; }
  else          { if (Scanline< edge) full=1; }

  if (PicoMask&0x04) { DrawLayer(1,0); bhigh=WrongPri; }
  if (PicoMask&0x08) { if (full) DrawWindow(0); else DrawLayer(0,0);  ahigh=WrongPri; }
  if (PicoMask&0x10) { DrawAllSprites(0); shigh=WrongPri; }
  
  if (bhigh) if (PicoMask&0x20) DrawLayer(1,1);
  if (ahigh) if (PicoMask&0x40) { if (full) DrawWindow(1); else DrawLayer(0,1); }
  if (shigh) if (PicoMask&0x80) DrawAllSprites(1);
  return 0;
}

static int UpdatePalette()
{
  int c=0;

  // Update palette:
  for (c=0;c<64;c++) Pico.highpal[c]=(unsigned short)PicoCram(Pico.cram[c]);
  Pico.m.dirtyPal=0;

  return 0;
}

static int Overlay()
{
  int col=0,x=0;

  if (PmovAction==0) return 0;
  if (Scanline>=4) return 0;

  if (PmovAction&1) col =0x00f;
  if (PmovAction&2) col|=0x0f0;
  col=PicoCram(col);

  for (x=0;x<4;x++) HighCol[32+x]=(unsigned short)col;

  return 0;
}

static int Skip=0;


int PicoLine(int scan)
{
  if (Skip>0) { Skip--; return 0; } // Skip rendering lines

  Scanline=scan;

  if (Pico.m.dirtyPal) UpdatePalette();

  // Draw screen:
  if (PicoMask&0x02) BackFill();
  if (Pico.video.reg[1]&0x40) DrawDisplay();

  Overlay();

  if (Pico.video.reg[12]&1)
  {
    Skip=PicoScan(Scanline,HighCol+32); // 40-column mode
  }
  else
  {
    // Crop, centre and return 32-column mode
    memset(HighCol,    0,64); // Left border
    memset(HighCol+288,0,64); // Right border
    Skip=PicoScan(Scanline,HighCol);
  }

  return 0;
}


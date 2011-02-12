
// Cyclone 68000 Emulator - Header File

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#ifdef __cplusplus
extern "C" {
#endif

extern int CycloneVer; // Version number of library

struct Cyclone
{
  unsigned int d[8];   // [r7,#0x00]
  unsigned int a[8];   // [r7,#0x20]
  unsigned int pc;     // [r7,#0x40] Memory Base+PC
  unsigned char srh;   // [r7,#0x44] Status Register high (T_S__III)
  unsigned char xc;    // [r7,#0x45] Extend flag (____??X?)
  unsigned char flags; // [r7,#0x46] Flags (ARM order: ____NZCV)
  unsigned char irq;   // [r7,#0x47] IRQ level
  unsigned int osp;    // [r7,#0x48] Other Stack Pointer (USP/SSP)
  unsigned int vector; // [r7,#0x50] IRQ vector (temporary)
  int pad1[3];
  int cycles;          // [r7,#0x5c]
  int membase;         // [r7,#0x60] Memory Base (ARM address minus 68000 address)
  unsigned int   (*checkpc)(unsigned int pc); // [r7,#0x64] - Called to recalc Memory Base+pc
  unsigned char  (*read8  )(unsigned int a);  // [r7,#0x68]
  unsigned short (*read16 )(unsigned int a);  // [r7,#0x6c]
  unsigned int   (*read32 )(unsigned int a);  // [r7,#0x70]
  void (*write8 )(unsigned int a,unsigned char  d); // [r7,#0x74]
  void (*write16)(unsigned int a,unsigned short d); // [r7,#0x78]
  void (*write32)(unsigned int a,unsigned int   d); // [r7,#0x7c]
  unsigned char  (*fetch8 )(unsigned int a);  // [r7,#0x80]
  unsigned short (*fetch16)(unsigned int a);  // [r7,#0x84]
  unsigned int   (*fetch32)(unsigned int a);  // [r7,#0x88]
};

void CycloneRun(struct Cyclone *pcy);

#ifdef __cplusplus
} // End of extern "C"
#endif

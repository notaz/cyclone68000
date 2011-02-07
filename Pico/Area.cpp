
#include "PicoInt.h"

int (*PicoAcb)(struct PicoArea *)=NULL; // Area callback for each block of memory
FILE *PmovFile=NULL;
int PmovAction=0;

// Scan one variable and callback
static void ScanVar(void *data,int len,char *name)
{
  struct PicoArea pa={NULL,0,NULL};
  pa.data=data; pa.len=len; pa.name=name;
  if (PicoAcb) PicoAcb(&pa);
}

#define SCAN_VAR(x,y) ScanVar(&x,sizeof(x),y);
#define SCANP(x)      ScanVar(&Pico.x,sizeof(Pico.x),#x);

// Pack the cpu into a common format:
static int PackCpu(unsigned char *cpu)
{
  unsigned int pc=0;

#ifdef EMU_A68K
  memcpy(cpu,M68000_regs.d,0x40);
  pc=M68000_regs.pc;
  *(unsigned char *)(cpu+0x44)=(unsigned char)M68000_regs.ccr;
  *(unsigned char *)(cpu+0x45)=(unsigned char)M68000_regs.srh;
  *(unsigned int  *)(cpu+0x48)=M68000_regs.isp;
#endif

#ifdef EMU_C68K
  memcpy(cpu,PicoCpu.d,0x40);
  pc=PicoCpu.pc-PicoCpu.membase;
  *(unsigned char *)(cpu+0x44)=PicoCpu.ccr;
  *(unsigned char *)(cpu+0x45)=PicoCpu.srh;
  *(unsigned int  *)(cpu+0x48)=PicoCpu.osp;
#endif

  *(unsigned int *)(cpu+0x40)=pc;
  return 0;
}

static int UnpackCpu(unsigned char *cpu)
{
  unsigned int pc=0;

  pc=*(unsigned int *)(cpu+0x40);

#ifdef EMU_A68K
  memcpy(M68000_regs.d,cpu,0x40);
  M68000_regs.pc=pc;
  M68000_regs.ccr=*(unsigned char *)(cpu+0x44);
  M68000_regs.srh=*(unsigned char *)(cpu+0x45);
  M68000_regs.isp=*(unsigned int  *)(cpu+0x48);
#endif

#ifdef EMU_C68K
  memcpy(PicoCpu.d,cpu,0x40);
  PicoCpu.membase=0;
  PicoCpu.pc =PicoCpu.checkpc(pc); // Base pc
  PicoCpu.ccr=*(unsigned char *)(cpu+0x44);
  PicoCpu.srh=*(unsigned char *)(cpu+0x45);
  PicoCpu.osp=*(unsigned int  *)(cpu+0x48);
#endif
  return 0;
}

// Scan the contents of the virtual machine's memory for saving or loading
int PicoAreaScan(int action,int *pmin)
{
  unsigned char cpu[0x60];

  memset(&cpu,0,sizeof(cpu));

  if (action&4)
  {
    // Scan all the memory areas:
    SCANP(ram) SCANP(vram) SCANP(zram) SCANP(cram) SCANP(vsram)

    // Pack, scan and unpack the cpu data:
    PackCpu(cpu);
    SekInit();
    PicoMemInit();
    SCAN_VAR(cpu,"cpu")
    UnpackCpu(cpu);

    SCAN_VAR(Pico.m    ,"misc")
    SCAN_VAR(Pico.video,"video")
    SCAN_VAR(Pico.s    ,"sound")

    Pico.m.scanline=0;
    if (action&2)
    {
      memset(Pico.highpal,0,sizeof(Pico.highpal));
      Pico.m.dirtyPal=1; // Recalculate palette
    }
  }

  if (pmin) *pmin=0x0021;

  return 0;
}

// ---------------------------------------------------------------------------
// Helper code to save/load to a file handle

// Area callback for each piece of Megadrive memory, read or write to the file:
static int StateAcb(struct PicoArea *pa)
{
  if (PmovFile==NULL) return 1;

  if ((PmovAction&3)==1) fwrite(pa->data,1,pa->len,PmovFile);
  if ((PmovAction&3)==2) fread (pa->data,1,pa->len,PmovFile);
  return 0;
}

// Save or load the state from PmovFile:
int PmovState()
{
  int minimum=0;
  unsigned char head[32];

  memset(head,0,sizeof(head));

  // Find out minial compatible version:
  PicoAreaScan(PmovAction&0xc,&minimum);  

  memcpy(head,"Pico",4);
  *(unsigned int *)(head+0x8)=PicoVer;
  *(unsigned int *)(head+0xc)=minimum;

  // Scan header:
  if (PmovAction&1) fwrite(head,1,sizeof(head),PmovFile);
  if (PmovAction&2) fread (head,1,sizeof(head),PmovFile);

  // Scan memory areas:
  PicoAcb=StateAcb;
  PicoAreaScan(PmovAction,NULL);
  PicoAcb=NULL;
  return 0;
}

int PmovUpdate()
{
  int ret=0;
  if (PmovFile==NULL) return 1;
  
  if ((PmovAction&3)==0) return 0;

  PicoPad[1]=0; // Make sure pad #2 is blank
  if (PmovAction&1) ret=fwrite(PicoPad,1,2,PmovFile);
  if (PmovAction&2) ret=fread (PicoPad,1,2,PmovFile);

  if (ret!=2)
  {
    // End of file
    fclose(PmovFile); PmovFile=NULL;
    PmovAction=0;
  }

  return 0;
}

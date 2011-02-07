
#include "PicoInt.h"

static void Byteswap(unsigned char *data,int len)
{
  int i=0;

  if (len<2) return; // Too short

  do
  {
    unsigned short *pd=(unsigned short *)(data+i);
    int value=*pd; // Get 2 bytes

    value=(value<<8)|(value>>8); // Byteswap it
    *pd=(unsigned short)value; // Put 2b ytes
    i+=2;
  }  
  while (i+2<=len);
}

// Interleve a 16k block and byteswap
static int InterleveBlock(unsigned char *dest,unsigned char *src)
{
  int i=0;
  for (i=0;i<0x2000;i++) dest[(i<<1)  ]=src[       i]; // Odd
  for (i=0;i<0x2000;i++) dest[(i<<1)+1]=src[0x2000+i]; // Even
  return 0;
}

// Decode a SMD file
static int DecodeSmd(unsigned char *data,int len)
{
  unsigned char *temp=NULL;
  int i=0;

  temp=(unsigned char *)malloc(0x4000);
  if (temp==NULL) return 1;
  memset(temp,0,0x4000);

  // Interleve each 16k block and shift down by 0x200:
  for (i=0; i+0x4200<=len; i+=0x4000)
  {
    InterleveBlock(temp,data+0x200+i); // Interleve 16k to temporary buffer
    memcpy(data+i,temp,0x4000); // Copy back in
  }

  free(temp);
  return 0;
}

int PicoCartLoad(FILE *f,unsigned char **prom,unsigned int *psize)
{
  unsigned char *rom=NULL; int size=0;
  if (f==NULL) return 1;

  fseek(f,0,SEEK_END); size=ftell(f); fseek(f,0,SEEK_SET);

  size=(size+3)&~3; // Round up to a multiple of 4

  // Allocate space for the rom plus padding
  rom=(unsigned char *)malloc(size+4);
  if (rom==NULL) { fclose(f); return 1; }
  memset(rom,0,size+4);

  fread(rom,1,size,f); // Load up the rom
  fclose(f);

  // Check for SMD:
  if ((size&0x3fff)==0x200) { DecodeSmd(rom,size); size-=0x200; } // Decode and byteswap SMD
  else Byteswap(rom,size); // Just byteswap
  
  if (prom)  *prom=rom;
  if (psize) *psize=size;

  return 0;
}

// Insert/remove a cartridge:
int PicoCartInsert(unsigned char *rom,unsigned int romsize)
{
  // Make sure movie playing/recording is stopped:
  if (PmovFile) fclose(PmovFile);
  PmovFile=NULL; PmovAction=0;

  memset(&Pico,0,sizeof(Pico)); // Blank Pico state
  Pico.rom=rom;
  Pico.romsize=romsize;
  PicoReset();

  return 0;
}

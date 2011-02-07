
#include "PicoInt.h"

#ifdef MSOUND
extern "C"
{
int YM2612UpdateReq(int) { return 0; }
void *errorlog=NULL;
}
#endif

int PsndRate=0,PsndLen=0;
short *PsndOut=NULL;

// An operator is a single sine wave generator
struct Operator
{
  unsigned short angle; // 0-0xffff
  unsigned short freq; // Converted frequency
  unsigned char key;
  unsigned char state; // 1=attack, 2=decay, 3=sustain, 4=release
  int vol;
  int delta; // Change in volume per sample
  int limit; // Next volume limit
};

struct Channel
{
  struct Operator op[4]; // 4 operators for the channel
  unsigned short note; // Written to 0xa4 and 0xa0
};

static struct Channel Chan[8];

unsigned char PicoSreg[0x200];

static int WriteReg(int side,int a,int d)
{
  struct Channel *pc=NULL;

  PicoSreg[(side<<8)|a]=(unsigned char)d;

  if (a==0x28)
  {
    pc=Chan+(d&7);
    // Key On/Key Off
    if (d&0xf0) pc->op[0].state=1; // Attack
    else        pc->op[0].state=4; // Release

    return 0;
  }

  // Find channel:
  pc=Chan+(a&3); if (side) pc+=4;

  if ((a&0xf0)==0xa0)
  {
    int n=0,f=0,mult=2455;

    if (PsndRate>0) mult=44100*2455/PsndRate;

    if (a&4) pc->note =(unsigned short)(d<<8);
    else     pc->note|=d;

    // Convert to an actual frequency:
    n=pc->note; f=(n&0x7ff)<<((n>>11)&7);

    pc->op[0].freq=(unsigned short)((f*mult)>>16);
    return 0;
  }

  return 0;
}

int PsndReset()
{
  int i=0;
  memset(&Chan,0,sizeof(Chan));
  memset(PicoSreg,0,sizeof(PicoSreg));

// Change Sine wave into a square wave
  for (i=0x00; i<0x080; i++) Sine[i]= 0x2000;
  for (i=0x80; i<0x100; i++) Sine[i]=-0x2000;

  return 0;
}

int PsndFm(int a,int d)
{
  int side=0;

#ifdef MSOUND
  YM2612Write(0,a&3,(unsigned char)d);
#endif

  a&=3; side=a>>1; // Which side: channels 0-2 or 3-5

  if (a&1) WriteReg(side,Pico.s.fmsel[side],d); // Write register
  else     Pico.s.fmsel[side]=(unsigned char)d; // Select register

  return 0;
}

static void BlankSound(short *dest,int len)
{
  short *end=NULL;

  end=dest+(len<<1);

  // Init sound to silence:
  do { *dest=0; dest++; } while (dest<end);
}

// Add to output and clip:
static inline void AddClip(short *targ,int d)
{
  int total=*targ+d;
  if ((total+0x8000)&0xffff0000)
  {
    if (total>0) total=0x7fff; else total=-0x8000;
  }
  *targ=(short)total;
}

static void OpNewState(int c)
{
  struct Operator *op=Chan[c].op;
  int off=0;

  off=((c&4)<<6)|(c&3);

  switch (op->state)
  {
    case 1:
    {
      // Attack:
      int ar=PicoSreg[0x50|off];
      ar&=0x1f; if (ar) ar+=0x10;
      op->delta=ar<<7; op->limit=0x1000000; break;
    }
    case 2:
    {
      // Decay:
      int d1r=PicoSreg[0x60|off];
      d1r&=0x1f; if (d1r) d1r+=0x10;
      op->delta=-(d1r<<5); op->limit=0;
    }
    break;
    case 3:
    {
      // Sustain:
      int d2r=0,rr=0;
      
      d2r=PicoSreg[0x70|off];
      d2r&=0x1f; if (d2r) d2r+=0x10;
      rr =PicoSreg[0x80|off];
      rr>>=4;

      op->delta=-(d2r<<5); op->limit=rr<<20;
    }
    break;
    case 4:
      // Release:
      int rr=PicoSreg[0x80|off];
      rr&=0x0f; rr<<=1; rr+=0x10;
      op->delta=-(rr<<5); op->limit=0;
    break;
  }
}

// Merely adding this code in seems to bugger up the N-Gage???
static void UpdateOp(int c)
{
  struct Operator *op=Chan[c].op;

  op->angle=(unsigned short)(op->angle+op->freq);
  op->vol+=op->delta;

  switch (op->state)
  {
    case 1:
      if (op->vol>=op->limit) { op->vol=op->limit; op->state++; OpNewState(c); }
    break;
    case 2: case 3: // Decay/Sustain
      if (op->vol< op->limit) { op->vol=op->limit; op->state++; OpNewState(c); }
    break;
    case 4:
      if (op->vol< op->limit) { op->vol=op->limit; }
    break;
  }
}

static void AddChannel(int c,short *dest,int len)
{
  struct Channel *pc=Chan+c;
  struct Operator *op=pc->op;
  short *end=NULL;

  // Work out volume delta for this operator:
  OpNewState(c);

  end=dest+len;
  do
  {
    int d=0;
    d=Sine[(op->angle>>8)&0xff]>>2;

    d*=(op->vol>>8); d>>=16;

    // Add to output:
    AddClip(dest,d);
    UpdateOp(c);

    dest++;
  }
  while (dest<end);
}

int PsndRender()
{
  int i=0;

  if (PsndOut==NULL) return 1; // Not rendering sound

#ifdef MSOUND
  if (PicoOpt&1)
  {
    short *wave[2]={NULL,NULL};
    wave[0]=PsndOut;
    wave[1]=PsndOut+PsndLen;
    YM2612UpdateOne(0,wave,PsndLen);
  }
#endif

  if ((PicoOpt&1)==0)
  {
    BlankSound(PsndOut,PsndLen);
    for (i=0;i<7;i++)
    {
      if (i!=3) AddChannel(i,PsndOut,PsndLen);
    }
  }

  return 0;
}

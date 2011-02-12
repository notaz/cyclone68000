
// This file is part of the Cyclone 68000 Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "app.h"

// --------------------- Opcodes 0x0100+ ---------------------
// Emit a Btst (Register) opcode 0000nnn1 00aaaaaa
int OpBtstReg(int op)
{
  int use=0;
  int type=0,sea=0,tea=0;
  int size=0;

  type=(op>>6)&3;
  // Get source and target EA
  sea=(op>>9)&7;
  tea=op&0x003f;
  if (tea<0x10) size=2; // For registers, 32-bits

  if ((tea&0x38)==0x08) return 1; // movep

  // See if we can do this opcode:
  if (EaCanRead(tea,0)==0) return 1;
  if (type>0)
  {
    if (EaCanWrite(tea)==0) return 1;
  }

  use=OpBase(op);
  use&=~0x0e00; // Use same handler for all registers
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;
  if (tea<0x10) Cycles+=2;
  if (type>0) Cycles+=4;

  ot("  mov r10,#1\n");

  EaCalc (0,0x0e00,sea,0);
  EaRead (0,     0,sea,0);
  ot("  bic r9,r9,#0x40000000 ;@ Blank Z flag\n");
  ot("  mov r10,r10,lsl r0 ;@ Make bit mask\n");
  ot("\n");

  EaCalc(11,0x003f,tea,size);
  EaRead(11,     0,tea,size);
  ot("  tst r0,r10 ;@ Do arithmetic\n");
  ot("  orreq r9,r9,#0x40000000 ;@ Get Z flag\n");
  ot("\n");

  if (type>0)
  {
    if (type==1) ot("  eor r1,r0,r10 ;@ Toggle bit\n");
    if (type==2) ot("  bic r1,r0,r10 ;@ Clear bit\n");
    if (type==3) ot("  orr r1,r0,r10 ;@ Set bit\n");
    ot("\n");
    EaWrite(11,   1,tea,size);
  }
  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x0800+ ---------------------
// Emit a Btst/Bchg/Bclr/Bset (Immediate) opcode 00001000 ttaaaaaa nn
int OpBtstImm(int op)
{
  int type=0,sea=0,tea=0;
  int use=0;
  int size=0;

  type=(op>>6)&3;
  // Get source and target EA
  sea=   0x003c;
  tea=op&0x003f;
  if (tea<0x10) size=2; // For registers, 32-bits

  // See if we can do this opcode:
  if (EaCanRead(tea,0)==0) return 1;
  if (type>0)
  {
    if (EaCanWrite(tea)==0) return 1;
  }

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;
  if (type<3 && tea<0x10) Cycles+=2;
  if (type>0) Cycles+=4;

  ot("  mov r10,#1\n");
  ot("\n");
  EaCalc ( 0,0x0000,sea,0);
  EaRead ( 0,     0,sea,0);
  ot("  bic r9,r9,#0x40000000 ;@ Blank Z flag\n");
  ot("  mov r10,r10,lsl r0 ;@ Make bit mask\n");
  ot("\n");

  EaCalc (11,0x003f,tea,size);
  EaRead (11,     0,tea,size);
  ot("  tst r0,r10 ;@ Do arithmetic\n");
  ot("  orreq r9,r9,#0x40000000 ;@ Get Z flag\n");
  ot("\n");

  if (type>0)
  {
    if (type==1) ot("  eor r1,r0,r10 ;@ Toggle bit\n");
    if (type==2) ot("  bic r1,r0,r10 ;@ Clear bit\n");
    if (type==3) ot("  orr r1,r0,r10 ;@ Set bit\n");
    ot("\n");
    EaWrite(11,   1,tea,size);
  }

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4000+ ---------------------
int OpNeg(int op)
{
  // 01000tt0 xxeeeeee (tt=negx/clr/neg/not, xx=size, eeeeee=EA)
  int type=0,size=0,ea=0,use=0;

  type=(op>>9)&3;
  ea  =op&0x003f;
  size=(op>>6)&3; if (size>=3) return 1;

  switch (type)
  {
    case 1: case 2: case 3: break;
    default: return 1; // todo
  }

  // See if we can do this opcode:
  if (EaCanRead (ea,size)==0) return 1;
  if (EaCanWrite(ea     )==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=size<2?4:6;

  EaCalc (10,0x003f,ea,size);

  if (type!=1) EaRead (10,0,ea,size); // Don't need to read for 'clr'
  if (type==1) ot("\n");

  if (type==1)
  {
    ot(";@ Clear:\n");
    ot("  mov r1,#0\n");
    ot("  mov r9,#0x40000000 ;@ NZCV=0100\n");
    ot("\n");
  }

  if (type==2)
  {
    ot(";@ Neg:\n");
    ot("  rsbs r1,r0,#0\n");
    OpGetFlags(1,1);
    ot("\n");
  }

  if (type==3)
  {
    ot(";@ Not:\n");
    ot("  mvn r1,r0\n");
    ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");
    OpGetFlags(0,0);
    ot("\n");
  }

  EaWrite(10,     1,ea,size);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4840+ ---------------------
// Swap, 01001000 01000nnn swap Dn
int OpSwap(int op)
{
  int ea=0,use=0;

  ea=op&7;
  use=op&~0x0007; // Use same opcode for all An

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc (10,0x0007,ea,2);
  EaRead (10,     0,ea,2);

  ot("  mov r1,r0,ror #16\n");
  ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");
  OpGetFlags(0,0);

  EaWrite(10,     1,8,2);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4a00+ ---------------------
// Emit a Tst opcode, 01001010 xxeeeeee
int OpTst(int op)
{
  int sea=0;
  int size=0,use=0;

  sea=op&0x003f;
  size=(op>>6)&3; if (size>=3) return 1;

  // See if we can do this opcode:
  if (EaCanWrite(sea)==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc ( 0,0x003f,sea,size);
  EaRead ( 0,     0,sea,size);

  ot("  adds r0,r0,#0 ;@ Defines NZ, clears CV\n");
  ot("  mrs r9,cpsr ;@ r9=flags\n");
  ot("\n");

  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x4880+ ---------------------
// Emit an Ext opcode, 01001000 1x000nnn
int OpExt(int op)
{
  int ea=0;
  int size=0,use=0;
  int shift=0;

  ea=op&0x0007;
  size=(op>>6)&1;
  shift=32-(8<<size);

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc (10,0x0007,ea,size+1);
  EaRead (10,     0,ea,size+1);

  ot("  mov r0,r0,asl #%d\n",shift);
  ot("  adds r0,r0,#0 ;@ Defines NZ, clears CV\n");
  ot("  mrs r9,cpsr ;@ r9=flags\n");
  ot("  mov r1,r0,asr #%d\n",shift);
  ot("\n");

  EaWrite(10,     1,ea,size+1);

  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x50c0+ ---------------------
// Emit a Set cc opcode, 0101cccc 11eeeeee
int OpSet(int op)
{
  int cc=0,ea=0;
  int size=0,use=0;
  char *cond[16]=
  {
    "al","", "hi","ls","cc","cs","ne","eq",
    "vc","vs","pl","mi","ge","lt","gt","le"
  };

  cc=(op>>8)&15;
  ea=op&0x003f;

  if ((ea&0x38)==0x08) return 1; // dbra, not scc
  
  // See if we can do this opcode:
  if (EaCanWrite(ea)==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=8;

  if (ea<0x10) Cycles=4;

  ot("  mov r1,#0\n");

  if (cc!=1)
  {
    ot(";@ Is the condition true?\n");
    if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000 ;@ Invert carry for hi/ls\n");
    ot("  msr cpsr_flg,r9 ;@ ARM flags = 68000 flags\n");
    if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000 ;@ Invert carry for hi/ls\n");
    ot("  mvn%s r1,r1\n",cond[cc]);
  }

  if (ea<0x10) ot("  sub%s r5,r5,#2 ;@ Extra cycles\n",cond[cc]);
  ot("\n");

  EaCalc (0,0x003f, ea,size);
  EaWrite(0,     1, ea,size);

  OpEnd();
  return 0;
}

// Emit a Asr/Lsr/Roxr/Ror opcode
static int EmitAsr(int op,int type,int dir,int count,int size,int usereg)
{
  char pct[8]="";
  int shift=32-(8<<size);

  if (count>=1) sprintf(pct,"#%d",count); // Fixed count

  if (count<0)
  {
    ot("  mov r2,r8,lsr #9 ;@ Get 'n'\n");
    ot("  and r2,r2,#7\n\n"); strcpy(pct,"r2");
  }

  if (usereg)
  {
    ot(";@ Use Dn for count:\n");
    ot("  ldr r2,[r7,r2,lsl #2]\n");
    ot("  and r2,r2,#63\n");
    ot("\n");
  }

  // Take 2*n cycles:
  if (count<0) ot("  sub r5,r5,r2,asl #1 ;@ Take 2*n cycles\n\n");
  else Cycles+=count<<1;

  if (type<2)
  {
    // Asr/Lsr
    if (dir==0 && size<2)
    {
      ot(";@ For shift right, also copy to lowest bits (to get carry bit):\n");
      ot("  orr r0,r0,r0,lsr #%d\n",32-(8<<size));
      ot("\n");
    }

    ot(";@ Shift register:\n");
    if (type==0) ot("  movs r0,r0,%s %s\n",dir?"asl":"asr",pct);
    if (type==1) ot("  movs r0,r0,%s %s\n",dir?"lsl":"lsr",pct);
    OpGetFlags(0,1);
    ot("\n");

    if (size<2)
    {
      ot(";@ Check if result is zero:\n");
      ot("  movs r2,r0,lsr #%d\n",shift);
      ot("  orreq r9,r9,#0x40000000\n");
      ot("\n");
    }
  }

  // --------------------------------------
  if (type==2)
  {
    // Roxr
    int wide=8<<size;

    if (shift) ot("  mov r0,r0,lsr #%d ;@ Shift down\n",shift);

    ot(";@ Rotate register through X:\n");
    if (strcmp("r2",pct)!=0) { ot("  mov r2,%s\n",pct); strcpy(pct,"r2"); } // Get into register

    if (count!=8)
    {
      ot(";@ Reduce r2 until <0:\n");
      ot("Reduce_%.4x%s\n",op,ms?"":":");
      ot("  subs r2,r2,#%d\n",wide+1);
      ot("  bpl Reduce_%.4x\n",op);
      ot("  add r2,r2,#%d ;@ Now r2=0-%d\n",wide+1,wide);
      ot("\n");
    }

    if (dir) ot("  rsb r2,r2,#%d ;@ Reverse direction\n",wide+1);

    ot(";@ Rotate bits:\n");
    ot("  mov r3,r0,lsr r2 ;@ Get right part\n");
    ot("  rsb r2,r2,#%d\n",wide+1);
    ot("  movs r0,r0,lsl r2 ;@ Get left part\n");
    ot("  orr r0,r3,r0 ;@ r0=Rotated value\n");

    ot(";@ Insert X bit into r2-1:\n");
    ot("  ldrb r3,[r7,#0x45]\n");
    ot("  sub r2,r2,#1\n");
    ot("  and r3,r3,#2\n");
    ot("  mov r3,r3,lsr #1\n");
    ot("  orr r0,r0,r3,lsl r2\n");
    ot("\n");

    if (shift) ot("  movs r0,r0,lsl #%d ;@ Shift up and get correct NC flags\n",shift);
    OpGetFlags(0,1);
    ot("\n");
  }

  // --------------------------------------
  if (type==3)
  {
    // Ror
    if (size<2)
    {
      ot(";@ Mirror value in whole 32 bits:\n");
      if (size<=0) ot("  orr r0,r0,r0,lsr #8\n");
      if (size<=1) ot("  orr r0,r0,r0,lsr #16\n");
      ot("\n");
    }

    ot(";@ Rotate register:\n");
    if (count<0)
    {
      if (dir) ot("  rsb r2,%s,#32\n",pct);
      ot("  movs r0,r0,ror %s\n",pct);
    }
    else
    {
      int ror=count;
      if (dir) ror=32-ror;
      if (ror&31) ot("  movs r0,r0,ror #%d\n",ror);
    }

    if (dir)
    {
      ot(";@ Get carry bit from bit 0:\n");
      ot("  mov r9,#0\n");
      ot("  ands r2,r0,#1\n");
      ot("  orrne r9,r9,#0x20000000\n");
    }
    else
    {
      OpGetFlags(0,0);
    }
    ot("\n");

  }
  // --------------------------------------
  
  return 0;
}

// Emit a Asr/Lsr/Roxr/Ror opcode - 1110cccd xxuttnnn
// (ccc=count, d=direction xx=size extension, u=use reg for count, tt=type, nnn=register Dn)
int OpAsr(int op)
{
  int ea=0,use=0;
  int count=0,dir=0;
  int size=0,usereg=0,type=0;

  ea=0;
  count =(op>>9)&7;
  dir   =(op>>8)&1;
  size  =(op>>6)&3;
  if (size>=3) return 1; // todo Asr EA
  usereg=(op>>5)&1;
  type  =(op>>3)&3;

  if (usereg==0) count=((count-1)&7)+1; // because ccc=000 means 8

  // Use the same opcode for target registers:
  use=op&~0x0007;

  // As long as count is not 8, use the same opcode for all shift counts::
  if (usereg==0 && count!=8) { use|=0x0e00; count=-1; }
  if (usereg) { use&=~0x0e00; count=-1; } // Use same opcode for all Dn

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=size<2?6:8;

  EaCalc(10,0x0007, ea,size);
  EaRead(10,     0, ea,size,1);

  EmitAsr(op,type,dir,count, size,usereg);

  EaWrite(10,    0, ea,size,1);

  OpEnd();

  return 0;
}

// Asr/l/Ror/l etc EA - 11100ttd 11eeeeee 
int OpAsrEa(int op)
{
  int use=0,type=0,dir=0,ea=0,size=1;

  type=(op>>9)&3;
  dir =(op>>8)&1;
  ea  = op&0x3f;

  if (ea<0x10) return 1;
  // See if we can do this opcode:
  if (EaCanRead(ea,0)==0) return 1;
  if (EaCanWrite(ea)==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=8;

  EaCalc (10,0x003f,ea,size);
  EaRead (10,     0,ea,size,1);

  EmitAsr(op,type,dir,1, size,0);

  ot(";@ Save shifted value back to EA:\n");
  ot("  mov r1,r0\n");
  EaWrite(10,     1,ea,size,1);

  OpEnd();
  return 0;
}

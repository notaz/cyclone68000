
// This file is part of the Cyclone 68000 Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "app.h"

// --------------------- Opcodes 0x1000+ ---------------------
// Emit a Move opcode, 00xxdddd ddssssss
int OpMove(int op)
{
  int sea=0,tea=0;
  int size=0,use=0;
  int movea=0;

  // Get source and target EA
  sea = op&0x003f;
  tea =(op&0x01c0)>>3;
  tea|=(op&0x0e00)>>9;

  if (tea>=8 && tea<0x10) movea=1;

  // Find size extension
  switch (op&0x3000)
  {
    default: return 1;
    case 0x1000: size=0; break;
    case 0x3000: size=1; break;
    case 0x2000: size=2; break;
  }

  if (movea && size<1) return 1; // movea.b is invalid

  // See if we can do this opcode:
  if (EaCanRead (sea,size)==0) return 1;
  if (EaCanWrite(tea     )==0) return 1;

  use=OpBase(op);
  if (tea<0x38) use&=~0x0e00; // Use same handler for register ?0-7
  
  if (tea>=0x18 && tea<0x28 && (tea&7)==7) use|=0x0e00; // Specific handler for (a7)+ and -(a7)

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc(0,0x003f,sea,size);
  EaRead(0,     0,sea,size);

  ot("  adds r1,r0,#0 ;@ Defines NZ, clears CV\n");

  if (movea==0)  ot("  mrs r9,cpsr ;@ r9=NZCV flags\n");
  ot("\n");

  if (movea) size=2; // movea always expands to 32-bits

  EaCalc (0,0x0e00,tea,size);
  EaWrite(0,     1,tea,size);

  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x41c0+ ---------------------
// Emit an Lea opcode, 0100nnn1 11aaaaaa
int OpLea(int op)
{
  int use=0;
  int sea=0,tea=0;

  sea= op&0x003f;
  tea=(op&0x0e00)>>9; tea|=8;

  if (EaCanRead(sea,-1)==0) return 1; // See if we can do this opcode:

  use=OpBase(op);
  use&=~0x0e00; // Also use 1 handler for target ?0-7
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc (1,0x003f,sea,0); // Lea
  EaCalc (0,0x0e00,tea,2);
  EaWrite(0,     1,tea,2);

  if (Amatch)
  {
    // Correct?
         if (sea< 0x18) Cycles+=4;
    else if (sea==0x30) Cycles+=12;
    else Cycles+=8;
  }

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x40c0+ ---------------------

// Pack our flags into r1, in SR/CCR register format
// trashes r0,r2
void OpFlagsToReg(int high)
{
  ot("  mov r1,r9,lsr #28   ;@ ____NZCV\n");
  ot("  eor r0,r1,r1,ror #1 ;@ Bit 0=C^V\n");
  ot("  tst r0,#1           ;@ 1 if C!=V\n");
  ot("  eorne r1,r1,#3      ;@ ____NZVC\n");
  ot("\n");
  ot("  ldrb r0,[r7,#0x45]  ;@ X bit\n");
  if (high) ot("  ldrb r2,[r7,#0x44]  ;@ Include SR high\n");
  ot("  and r0,r0,#0x02\n");
  if (high) ot("  orr r1,r1,r2,lsl #8\n");
  ot("  orr r1,r1,r0,lsl #3 ;@ ___XNZVC\n");
  ot("\n");
}

// Convert SR/CRR register in r0 to our flags
// trashes r0,r1
void OpRegToFlags(int high)
{
  ot("  eor r1,r0,r0,ror #1 ;@ Bit 0=C^V\n");
  ot("  mov r2,r0,lsr #3    ;@ r2=___XN\n");
  ot("  tst r1,#1           ;@ 1 if C!=V\n");
  ot("  eorne r0,r0,#3      ;@ ___XNZCV\n");
  ot("  strb r2,[r7,#0x45]  ;@ Store X bit\n");
  ot("  mov r9,r0,lsl #28   ;@ r9=NZCV...\n");

  if (high)
  {
    ot("  mov r0,r0,ror #8\n");
    ot("  strb r0,[r7,#0x44] ;@ Store SR high\n");
  }
  ot("\n");
}

static void SuperCheck(int op)
{
  ot("  ldrb r0,[r7,#0x44] ;@ Get SR high\n");
  ot("  tst r0,#0x20 ;@ Check we are in supervisor mode\n");
  ot("  beq WrongMode%.4x ;@ No\n",op);
  ot("\n");
}

static void SuperEnd(int op)
{
  ot("WrongMode%.4x%s\n",op,ms?"":":");
  ot(";@ todo - cause an exception\n");
  OpEnd();
}

// Move SR opcode, 01000tt0 11aaaaaa move to SR
int OpMoveSr(int op)
{
  int type=0,ea=0;
  int use=0,size=1;

  type=(op>>9)&3;
  ea=op&0x3f;

  switch(type)
  {
    case 0: case 1:
      if (EaCanWrite(ea)==0) return 1; // See if we can do this opcode:
    break;

    default: return 1; // todo

    case 2: case 3:
      if (EaCanRead(ea,size)==0) return 1; // See if we can do this opcode:
    break;
  }

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);
       if (type==0) Cycles=8;
  else if (type==1) Cycles=6;
  else Cycles=12;

  if (Amatch && ea==0x3c) Cycles-=4; // Correct?

  if (type==0 || type==3) SuperCheck(op);

  if (type==0 || type==1)
  {
    OpFlagsToReg(type==0);
    EaCalc (0,0x003f,ea,size);
    EaWrite(0,     1,ea,size);
  }

  if (type==2 || type==3)
  {
    EaCalc(0,0x003f,ea,size);
    EaRead(0,     0,ea,size);
    OpRegToFlags(type==3);
    if (type==3) CheckInterrupt();
  }

  OpEnd();

  if (type==0 || type==3) SuperEnd(op);

  return 0;
}


// Ori/Andi/Eori $nnnn,sr 0000t0t0 01111100
int OpArithSr(int op)
{
  int type=0,ea=0;
  int use=0,size=0;

  type=(op>>9)&5; if (type==4) return 1;
  size=(op>>6)&1;
  ea=0x3c;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=16;

  SuperCheck(op);

  EaCalc(0,0x003f,ea,size);
  EaRead(0,    10,ea,size);

  OpFlagsToReg(size);
  if (type==0) ot("  orr r0,r1,r10\n");
  if (type==1) ot("  and r0,r1,r10\n");
  if (type==5) ot("  eor r0,r1,r10\n");
  OpRegToFlags(size);
  if (size) CheckInterrupt();

  OpEnd();
  SuperEnd(op);

  return 0;
}

// --------------------- Opcodes 0x4850+ ---------------------
// Emit an Pea opcode, 01001000 01aaaaaa
int OpPea(int op)
{
  int use=0;
  int ea=0;

  ea=op&0x003f; if (ea<0x10) return 1; // Swap opcode
  if (EaCanRead(ea,-1)==0) return 1; // See if we can do this opcode:

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=20;

  EaCalc (1,0x003f, ea,0);
  ot("\n");
  ot("  ldr r0,[r7,#0x3c]\n");
  ot("  sub r0,r0,#4 ;@ Predecrement A7\n");
  ot("  str r0,[r7,#0x3c] ;@ Save A7\n");
  ot("\n");
  MemHandler(1,2); // Write 32-bit
  ot("\n");

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4880+ ---------------------
// Emit a Movem opcode, 01001d00 1xeeeeee regmask
int OpMovem(int op)
{
  int size=0,ea=0,cea=0,dir=0;
  int use=0,decr=0,change=0;

  size=((op>>6)&1)+1;
  ea=op&0x003f;
  dir=(op>>10)&1; // Direction

  if (ea<0x10 || ea>0x39) return 1; // Invalid EA
  if ((ea&0x38)==0x18 || (ea&0x38)==0x20) change=1;
  if ((ea&0x38)==0x20) decr=1; // -(An), bitfield is decr

  // See if we can do this opcode:
  if (EaCanWrite(ea)==0) return 1;

  cea=ea; if (change) cea=0x10;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  ot("  stmdb sp!,{r9} ;@ Push r9\n");
  ot("  ldrh r11,[r4],#2 ;@ r11=register mask\n");

  ot("\n");
  ot(";@ Get the address into r9:\n");
  EaCalc(9,0x003f,cea,size);

  ot(";@ r10=Register Index*4:\n");
  if (decr) ot("  mov r10,#0x3c ;@ order reversed for -(An)\n");
  else      ot("  mov r10,#0\n");
  
  ot("\n");
  ot("MoreReg%.4x%s\n",op, ms?"":":");

  ot("  tst r11,#1\n");
  ot("  beq SkipReg%.4x\n",op);
  ot("\n");

  if (decr) ot("  sub r9,r9,#%d ;@ Pre-decrement address\n",1<<size);

  if (dir)
  {
    ot("  ;@ Copy memory to register:\n",1<<size);
    EaRead (9,0,ea,size);
    ot("  str r0,[r7,r10] ;@ Save value into Dn/An\n");
  }
  else
  {
    ot("  ;@ Copy register to memory:\n",1<<size);
    ot("  ldr r1,[r7,r10] ;@ Load value from Dn/An\n");
    EaWrite(9,1,ea,size);
  }

  if (decr==0) ot("  add r9,r9,#%d ;@ Post-increment address\n",1<<size);

  ot("  sub r5,r5,#%d ;@ Take some cycles\n",2<<size);
  ot("\n");
  ot("SkipReg%.4x%s\n",op, ms?"":":");
  ot("  movs r11,r11,lsr #1;@ Shift mask:\n");
  ot("  add r10,r10,#%d ;@ r10=Next Register\n",decr?-4:4);
  ot("  bne MoreReg%.4x\n",op);
  ot("\n");

  if (change)
  {
    ot(";@ Write back address:\n");
    EaCalc (0,0x0007,8|(ea&7),2);
    EaWrite(0,     9,8|(ea&7),2);
  }

  ot("  ldmia sp!,{r9} ;@ Pop r9\n");
  ot("\n");

       if (ea<0x10) { }
  else if (ea<0x18) Cycles=16; // (a0)
  else if (ea<0x20) Cycles= 0; // (a0)+ ?
  else if (ea<0x28) Cycles= 8; //-(a0)
  else if (ea<0x30) Cycles=24; // ($3333,a0)
  else if (ea<0x38) Cycles=28; // ($33,a0,d3.w*2)
  else if (ea<0x39) Cycles=24; // $3333.w
  else if (ea<0x3a) Cycles=28; // $33333333.l

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4e60+ ---------------------
// Emit a Move USP opcode, 01001110 0110dnnn move An to/from USP
int OpMoveUsp(int op)
{
  int use=0,dir=0;

  dir=(op>>3)&1; // Direction
  use=op&~0x0007; // Use same opcode for all An

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  ot("  ldrb r0,[r7,#0x44] ;@ Get SR\n");
  ot("  tst r0,#0x20 ;@ Check we are in supervisor mode\n");
  ot("  beq WrongMode%.4x ;@ No\n",op);
  ot("\n");

  if (dir)
  {
    EaCalc (0,0x0007,8,2);
    ot("  ldr r1,[r7,#0x48] ;@ Get from USP\n\n");
    EaWrite(0,     1,8,2);
  }
  else
  {
    EaCalc (0,0x0007,8,2);
    EaRead (0,     0,8,2);
    ot("  str r0,[r7,#0x48] ;@ Put in USP\n\n");
  }
    
  OpEnd();

  ot("WrongMode%.4x%s\n",op,ms?"":":");
  ot(";@ todo - cause an exception\n");
  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x7000+ ---------------------
// Emit a Move Quick opcode, 0111nnn0 dddddddd  moveq #dd,Dn
int OpMoveq(int op)
{
  int use=0;

  use=op&0xf100; // Use same opcode for all values
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  ot("  movs r0,r8,asl #24\n");
  ot("  and r1,r8,#0x0e00\n");
  ot("  mov r0,r0,asr #24 ;@ Sign extended Quick value\n");
  ot("  mrs r9,cpsr ;@ r9=NZ flags\n");
  ot("  str r0,[r7,r1,lsr #7] ;@ Store into Dn\n");
  ot("\n");

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0xc140+ ---------------------
// Emit a Exchange opcode:
// 1100ttt1 01000sss  exg ds,dt
// 1100ttt1 01001sss  exg as,at
// 1100ttt1 10001sss  exg as,dt
int OpExg(int op)
{
  int use=0,type=0;

  type=op&0xf8;

  if (type!=0x40 && type!=0x48 && type!=0x88) return 1; // Not an exg opcode

  use=op&0xf1f8; // Use same opcode for all values
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=6;

  ot("  and r10,r8,#0x0e00 ;@ Find T register\n");
  ot("  and r11,r8,#0x000f ;@ Find S register\n");
  if (type==0x48) ot("  orr r10,r10,#0x1000 ;@ T is an address register\n");
  ot("\n");
  ot("  ldr r0,[r7,r10,lsr #7] ;@ Get T\n");
  ot("  ldr r1,[r7,r11,lsl #2] ;@ Get S\n");
  ot("\n");
  ot("  str r0,[r7,r11,lsl #2] ;@ T->S\n");
  ot("  str r1,[r7,r10,lsr #7] ;@ S->T\n");  
  ot("\n");

  OpEnd();
  
  return 0;
}

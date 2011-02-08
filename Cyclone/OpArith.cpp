
// This file is part of the Cyclone 68000 Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "app.h"

// --------------------- Opcodes 0x0000+ ---------------------
// Emit an Ori/And/Sub/Add/Eor/Cmp Immediate opcode, 0000ttt0 00aaaaaa
int OpArith(int op)
{
  int type=0,size=0;
  int sea=0,tea=0;
  int use=0;

  // Get source and target EA
  type=(op>>9)&7; if (type==4 || type>=7) return 1;
  size=(op>>6)&3; if (size>=3) return 1;
  sea=   0x003c;
  tea=op&0x003f;

  // See if we can do this opcode:
  if (EaCanRead(tea,size)==0) return 1;
  if (type!=6 && EaCanWrite(tea)==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc(10,0x0000, sea,size);
  EaRead(10,    10, sea,size,1);

  EaCalc(11,0x003f, tea,size);
  EaRead(11,     0, tea,size,1);

  ot(";@ Do arithmetic:\n");

  if (type==0) ot("  orr r1,r0,r10\n");
  if (type==1) ot("  and r1,r0,r10\n");
  if (type==2) ot("  subs r1,r0,r10 ;@ Defines NZCV\n");
  if (type==3) ot("  adds r1,r0,r10 ;@ Defines NZCV\n");
  if (type==5) ot("  eor r1,r0,r10\n");
  if (type==6) ot("  cmp r0,r10 ;@ Defines NZCV\n");

  if (type<2 || type==5) ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n"); // 0,1,5

  if (type< 2) OpGetFlags(0,0); // Ori/And
  if (type==2) OpGetFlags(1,1); // Sub: Subtract/X-bit
  if (type==3) OpGetFlags(0,1); // Add: X-bit
  if (type==5) OpGetFlags(0,0); // Eor
  if (type==6) OpGetFlags(1,0); // Cmp: Subtract
  ot("\n");

  if (type!=6)
  {
    EaWrite(11, 1, tea,size,1);
  }

  // Correct cycles:
  if (type==6)
  {
    if (size>=2 && tea<0x10) Cycles+=2;
  }
  else
  {
    if (size>=2) Cycles+=4;
    if (tea>=0x10) Cycles+=4;
    if (Amatch && type==1 && size>=2 && tea<0x10) Cycles-=2;
  }

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x5000+ ---------------------
int OpAddq(int op)
{
  // 0101nnnt xxeeeeee (nnn=#8,1-7 t=addq/subq xx=size, eeeeee=EA)
  int num=0,type=0,size=0,ea=0;
  int use=0;
  char count[16]="";
  int shift=0;

  num =(op>>9)&7; if (num==0) num=8;
  type=(op>>8)&1;
  size=(op>>6)&3; if (size>=3) return 1;
  ea  = op&0x3f;

  // See if we can do this opcode:
  if (EaCanRead (ea,size)==0) return 1;
  if (EaCanWrite(ea     )==0) return 1;

  use=op; if (ea<0x38) use&=~7;
  if ((ea&0x38)==0x08) { size=2; use&=~0xc0; } // Every addq #n,An is 32-bit

  if (num!=8) use|=0x0e00; // If num is not 8, use same handler
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);
  Cycles=ea<8?4:8;
  if (size>=2 && ea!=8) Cycles+=4;

  EaCalc(10,0x003f, ea,size);
  EaRead(10,     0, ea,size,1);

  shift=32-(8<<size);

  if (num!=8)
  {
    int lsr=9-shift;

    if (lsr>=0) ot("  mov r2,r8,lsr #%d ;@ Get quick value\n", lsr);
    else        ot("  mov r2,r8,lsl #%d ;@ Get quick value\n",-lsr);

    ot("  and r2,r2,#0x%.4x\n",7<<shift);
    ot("\n");
    strcpy(count,"r2");
  }

  if (num==8) sprintf(count,"#0x%.4x",8<<shift);

  if (type==0) ot("  adds r1,r0,%s\n",count);
  if (type==1) ot("  subs r1,r0,%s\n",count);

  if ((ea&0x38)!=0x08) OpGetFlags(type,1);
  ot("\n");

  EaWrite(10,     1, ea,size,1);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x8000+ ---------------------
// 1t0tnnnd xxeeeeee (tt=type:or/sub/and/add xx=size, eeeeee=EA)
int OpArithReg(int op)
{
  int use=0;
  int type=0,size=0,dir=0,rea=0,ea=0;

  type=(op>>12)&5;
  rea =(op>> 9)&7;
  dir =(op>> 8)&1;
  size=(op>> 6)&3; if (size>=3) return 1;
  ea  = op&0x3f;

  if (dir && ea<0x10) return 1; // addx/subx opcode

  // See if we can do this opcode:
  if (dir==0 && EaCanWrite(rea)==0) return 1;
  if (dir    && EaCanWrite( ea)==0) return 1;

  use=OpBase(op);
  use&=~0x0e00; // Use same opcode for Dn
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  ot(";@ Get r10=EA r11=EA value\n");
  EaCalc(10,0x003f, ea,size);
  EaRead(10,    11, ea,size,1);
  ot(";@ Get r0=Register r1=Register value\n");
  EaCalc( 0,0x0e00,rea,size);
  EaRead( 0,     1,rea,size,1);

  ot(";@ Do arithmetic:\n");
  if (type==0) ot("  orr  ");
  if (type==1) ot("  subs ");
  if (type==4) ot("  and  ");
  if (type==5) ot("  adds ");
  if (dir) ot("r1,r11,r1\n");
  else     ot("r1,r1,r11\n");

  if ((type&1)==0) ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");

  OpGetFlags(type==1,type&1); // 1==subtract
  ot("\n");

  ot(";@ Save result:\n");
  if (dir) EaWrite(10, 1, ea,size,1);
  else     EaWrite( 0, 1,rea,size,1);

  if (size==1 && ea>=0x10) Cycles+=4;
  if (size>=2) { if (ea<0x10) Cycles+=4; else Cycles+=2; }

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x80c0+ ---------------------
int OpMul(int op)
{
  // Div/Mul: 1m00nnns 11eeeeee (m=Mul, nnn=Register Dn, s=signed, eeeeee=EA)
  int type=0,rea=0,sign=0,ea=0;
  int use=0;

  type=(op>>14)&1; // div/mul
  rea =(op>> 9)&7;
  sign=(op>> 8)&1;
  ea  = op&0x3f;

  // See if we can do this opcode:
  if (EaCanRead(ea,1)==0) return 1;

  use=OpBase(op);
  use&=~0x0e00; // Use same for all registers
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=type?70:133;

  EaCalc(10,0x003f, ea, 1);
  EaRead(10,    10, ea, 1);

  EaCalc (0,0x0e00,rea, 2);
  EaRead (0,     2,rea, 2);

  if (type==0)
  {
    ot("  cmp r10,#0\n");
    ot("  moveq r10,#1 ;@ Divide by zero\n");
    ot("\n");
    
    if (sign)
    {
      ot("  mov r11,#0 ;@ r11 = 1 if the result is negative\n");
      ot("  eorlt r11,r11,#1\n");
      ot("  rsblt r10,r10,#0 ;@ Make r10 positive\n");
      ot("\n");
      ot("  cmp r2,#0\n");
      ot("  eorlt r11,r11,#1\n");
      ot("  rsblt r2,r2,#0 ;@ Make r2 positive\n");
      ot("\n");
    }

    ot(";@ Divide r2 by r10\n");
    ot("  mov r3,#0\n");
    ot("  mov r1,r10\n");
    ot("\n");
    ot(";@ Shift up divisor till it's just less than numerator\n");
    ot("Shift%.4x%s\n",op,ms?"":":");
    ot("  cmp r1,r2,lsr #1\n");
    ot("  movls r1,r1,lsl #1\n");
    ot("  bcc Shift%.4x\n",op);
    ot("\n");

    ot("Divide%.4x%s\n",op,ms?"":":");
    ot("  cmp r2,r1\n");
    ot("  adc r3,r3,r3 ;@ Double r3 and add 1 if carry set\n");
    ot("  subcs r2,r2,r1\n");
    ot("  teq r1,r10\n");
    ot("  movne r1,r1,lsr #1\n");
    ot("  bne Divide%.4x\n",op);
    ot("\n");

    if (sign)
    {
      ot("  tst r11,r11\n");
      ot("  rsbne r3,r3,#0 ;@ Negate if result is negative\n");
    }

    ot("  mov r11,r2 ;@ Remainder\n");

    ot("  adds r1,r3,#0 ;@ Defines NZ, clears CV\n");
    OpGetFlags(0,0);

    ot("  mov r1,r1,lsl #16 ;@ Clip to 16-bits\n");
    ot("  mov r1,r1,lsr #16\n");
    ot("  orr r1,r1,r11,lsl #16 ;@ Insert remainder\n");
  }

  if (type==1)
  {
    char *shift="asr";

    ot(";@ Get 16-bit signs right:\n");
    if (sign==0) { ot("  mov r10,r10,lsl #16\n"); shift="lsr"; }
    ot("  mov r2,r2,lsl #16\n");

    if (sign==0) ot("  mov r10,r10,lsr #16\n");
    ot("  mov r2,r2,%s #16\n",shift);
    ot("\n");

    ot("  mul r1,r2,r10\n");
    ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");
    OpGetFlags(0,0);

    if (Amatch && ea==0x3c) Cycles-=4;
  }
  ot("\n");

  EaWrite(0,     1,rea, 2);


  OpEnd();

  return 0;
}

// Get X Bit into carry - trashes r2
static int GetXBit(int subtract)
{
  ot(";@ Get X bit:\n");
  ot("  ldrb r2,[r7,#0x45]\n");
  if (subtract) ot("  mvn r2,r2,lsl #28 ;@ Invert it\n");
  else          ot("  mov r2,r2,lsl #28\n");
  ot("  msr cpsr_flg,r2 ;@ Get into Carry\n");
  ot("\n");
  return 0;
}

// --------------------- Opcodes 0x8100+ ---------------------
// 1t00ddd1 0000asss - sbcd/abcd Ds,Dd or -(As),-(Ad)
int OpAbcd(int op)
{
  int use=0;
  int type=0,sea=0,addr=0,dea=0;
  
  type=(op>>14)&1;
  dea =(op>> 9)&7;
  addr=(op>> 3)&1;
  sea = op     &7;

  if (addr) { sea|=0x20; dea|=0x20; }

  use=op&~0x0e07; // Use same opcode for all registers
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=6;

  EaCalc( 0,0x0007, sea,0);
  EaRead( 0,    10, sea,0,1);
  EaCalc(11,0x0e00, dea,0);
  EaRead(11,     1, dea,0,1);

  ot("  ldrb r2,[r7,#0x45] ;@ Get X bit\n");
  ot("  tst r2,#2\n");
  ot("  addne r10,r10,#0x01000000 ;@ Add carry bit\n");

  if (type)
  {
    ot(";@ Add units into r2:\n");
    ot("  and r2,r1, #0x0f000000\n");
    ot("  and r0,r10,#0x0f000000\n");
    ot("  add r2,r2,r0\n");
    ot("  cmp r2,#0x0a000000\n");
    ot("  addpl r1,r1,#0x06000000 ;@ Decimal adjust units\n");
    ot("  add r1,r1,r10 ;@ Add BCD\n");
    ot("  mov r0,r1,lsr #24\n");
    ot("  cmp r0,#0xa0\n");
    ot("  addpl r1,r1,#0x60000000 ;@ Decimal adjust tens\n");
    OpGetFlags(0,1);
  }
  else
  {
    ot(";@ Sub units into r2:\n");
    ot("  and r2,r1, #0x0f000000\n");
    ot("  and r0,r10,#0x0f000000\n");
    ot("  subs r2,r2,r0\n");
    ot("  submi r1,r1,#0x06000000 ;@ Decimal adjust units\n");
    ot("  subs r1,r1,r10 ;@ Subtract BCD\n");
    ot("  submis r1,r1,#0x60000000 ;@ Decimal adjust tens\n");
    OpGetFlags(1,1);
  }
  ot("\n");

  EaWrite(11,     1, dea,0,1);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x90c0+ ---------------------
// Suba/Cmpa/Adda 1tt1nnnx 11eeeeee (tt=type, x=size, eeeeee=Source EA)
int OpAritha(int op)
{
  int use=0;
  int type=0,size=0,sea=0,dea=0;

  // Suba/Cmpa/Adda/(invalid):
  type=(op>>13)&3; if (type>=3) return 1;

  size=(op>>8)&1; size++;
  dea=(op>>9)&7; dea|=8; // Dest=An
  sea=op&0x003f; // Source

  // See if we can do this opcode:
  if (EaCanRead(sea,size)==0) return 1;

  use=OpBase(op);
  use&=~0x0e00; // Use same opcode for An
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;
  EaCalc ( 0,0x003f, sea,size);
  EaRead ( 0,    10, sea,size);

  EaCalc ( 0,0x0e00, dea,2);
  EaRead ( 0,     1, dea,2);

  if (type==0) ot("  sub r1,r1,r10\n");
  if (type==1) ot("  cmp r1,r10 ;@ Defines NZCV\n");
  if (type==1) OpGetFlags(1,0); // Get Cmp flags
  if (type==2) ot("  add r1,r1,r10\n");
  ot("\n");
  
  EaWrite( 0,     1, dea,2);

  if (Amatch && sea==0x3c) Cycles-=size<2?4:8; // Correct?
  if (size>=2) { if (sea<0x10) Cycles+=4; else Cycles+=2; }

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x9100+ ---------------------
// Emit a Subx/Addx opcode, 1t01ddd1 zz000sss addx.z Ds,Dd
int OpAddx(int op)
{
  int use=0;
  int type=0,size=0,dea=0,sea=0;

  type=(op>>12)&5;
  dea =(op>> 9)&7;
  size=(op>> 6)&3; if (size>=3) return 1;
  sea = op&0x3f;

  // See if we can do this opcode:
  if (EaCanRead(sea,size)==0) return 1;
  if (EaCanWrite(dea)==0) return 1;

  use=OpBase(op);
  use&=~0x0e00; // Use same opcode for Dn
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=8;

  ot(";@ Get r10=EA r11=EA value\n");
  EaCalc( 0,0x003f,sea,size);
  EaRead( 0,    11,sea,size,1);
  ot(";@ Get r0=Register r1=Register value\n");
  EaCalc( 0,0x0e00,dea,size);
  EaRead( 0,     1,dea,size,1);

  ot(";@ Do arithmetic:\n");
  GetXBit(type==1);

  if (type==5 && size<2)
  {
    ot(";@ Make sure the carry bit will tip the balance:\n");
    if (size==0) ot("  ldr r2,=0x00ffffff\n");
    else         ot("  ldr r2,=0x0000ffff\n");
    ot("  orr r11,r11,r2\n");
    ot("\n");
  }

  if (type==1) ot("  sbcs r1,r1,r11\n");
  if (type==5) ot("  adcs r1,r1,r11\n");
  OpGetFlags(type==1,1); // subtract
  ot("\n");

  ot(";@ Save result:\n");
  EaWrite( 0, 1, dea,size,1);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0xb000+ ---------------------
// Emit a Cmp/Eor opcode, 1011rrrt xxeeeeee (rrr=Dn, t=cmp/eor, xx=size extension, eeeeee=ea)
int OpCmpEor(int op)
{
  int rea=0,eor=0;
  int size=0,ea=0,use=0;

  // Get EA and register EA
  rea=(op>>9)&7;
  eor=(op>>8)&1;
  size=(op>>6)&3; if (size>=3) return 1;
  ea=op&0x3f;

  // See if we can do this opcode:
  if (EaCanRead(ea,size)==0) return 1;
  if (eor && EaCanWrite(ea)==0) return 1;

  use=OpBase(op);
  use&=~0x0e00; // Use 1 handler for register d0-7
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=eor?8:4;

  ot(";@ Get EA into r10 and value into r0:\n");
  EaCalc (10,0x003f,  ea,size);
  EaRead (10,     0,  ea,size,1);

  ot(";@ Get register operand into r1:\n");
  EaCalc (1 ,0x0e00, rea,size);
  EaRead (1,      1, rea,size,1);

  ot(";@ Do arithmetic:\n");
  if (eor==0) ot("  cmp r1,r0\n");
  if (eor)
  {
    ot("  eor r1,r0,r1\n");
    ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");
  }

  OpGetFlags(eor==0,0); // Cmp like subtract
  ot("\n");

  if (size>=2) Cycles+=4; // Correct?
  if (ea==0x3c) Cycles-=4;

  if (eor) EaWrite(10, 1,ea,size,1);

  OpEnd();
  return 0;
}


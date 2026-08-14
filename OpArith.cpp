
// This file is part of the Cyclone 68000 Emulator

// Copyright (c) 2004,2011 FinalDave (emudave (at) gmail.com)
// Copyright (c) 2005-2011 Gražvydas "notaz" Ignotas (notasas (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/


#include "app.h"

// --------------------- Opcodes 0x0000+ ---------------------
// Emit an Ori/And/Sub/Add/Eor/Cmp Immediate opcode, 0000ttt0 ssaaaaaa
int OpArith(int op)
{
  int type=0,size=0;
  int sea=0,tea=0;
  int use=0;
  const char *shiftstr="";

  // Get source and target EA
  type=(op>>9)&7; if (type==4 || type>=7) return 1;
  size=(op>>6)&3; if (size>=3) return 1;
  sea=   0x003c;
  tea=op&0x003f;

  // See if we can do this opcode:
  if (EaCanRead(tea,size)==0) return 1;
  if (EaCanWrite(tea)==0 || EaAn(tea)) return 1;

  use=OpBase(op,size);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op, sea, tea); Cycles=4;

  // imm must be read first
  EaCalcRead(-1,10,sea,size,0,earwt_msb_dont_care);
  EaCalcRead((type!=6)?11:-1,0,tea,size,0x003f,earwt_msb_dont_care);

  if (size<2) shiftstr=(char *)(size?",asl #16":",asl #24");
  if (size<2) ot("  mov r10,r10,asl #%i\n",size?16:24);

  ot(";@ Do arithmetic:\n");

  if (type==0) ot("  orrs r1,r10,r0%s\n",shiftstr);
  if (type==1) ot("  ands r1,r10,r0%s\n",shiftstr);
  if (type==2||type==6)
               ot("  rsbs r1,r10,r0%s ;@ Defines NZCV\n",shiftstr);
  if (type==3) ot("  adds r1,r10,r0%s ;@ Defines NZCV\n",shiftstr);
  if (type==5) ot("  eors r1,r10,r0%s\n",shiftstr);

  if (type< 2) OpGetFlagsNZ(1); // Ori/And
  if (type==2) OpGetFlags(1,1); // Sub: Subtract/X-bit
  if (type==3) OpGetFlags(0,1); // Add: X-bit
  if (type==5) OpGetFlagsNZ(1); // Eor
  if (type==6) OpGetFlags(1,0); // Cmp: Subtract
  ot("\n");

  if (type!=6)
  {
    EaWrite(11, 1, tea,size,0x003f,earwt_shifted_up);
  }

  // Correct cycles:
  if (type==6)
  {
    if (size>=2 && tea<0x10) Cycles+=2;
  }
  else
  {
    if (size>=2) Cycles+=4;
    if (tea>=8)  Cycles+=4;
  }

  OpEnd(sea,tea);

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
  if (EaCanWrite(ea)     ==0) return 1;
  if (size == 0 && EaAn(ea) ) return 1;

  use=OpBase(op,size,1);

  if (num!=8) use|=0x0e00; // If num is not 8, use same handler
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,ea);
  Cycles=ea<8?4:8;
  if(size>=2) Cycles=ea<0x10?8:12;

  if (size>0 && (ea&0x38)==0x08) size=2; // addq.w #n,An is also 32-bit

  EaCalcRead(11,0,ea,size,0x003f,earwt_msb_dont_care);

  shift=32-(8<<size);

  if (num!=8)
  {
    int lsr=9-shift;

    ot("  and r2,r8,#0x0e00 ;@ Get quick value\n");

    if (lsr>=0) sprintf(count,"r2,lsr #%d",  lsr);
    else        sprintf(count,"r2,lsl #%d", -lsr);

    ot("\n");
  }
  else
  {
    sprintf(count,"#0x%.4x",8<<shift);
  }

  if (size<2)  ot("  mov r0,r0,asl #%d\n\n",size?16:24);

  if (type==0) ot("  adds r1,r0,%s\n",count);
  if (type==1) ot("  subs r1,r0,%s\n",count);

  if ((ea&0x38)!=0x08) OpGetFlags(type,1);
  ot("\n");

  EaWrite(11, 1, ea,size,0x003f,earwt_shifted_up);

  OpEnd(ea);

  return 0;
}

// --------------------- Opcodes 0x8000+ ---------------------
// 1t0tnnnd xxeeeeee (tt=type:or/sub/and/add xx=size, eeeeee=EA)
int OpArithReg(int op)
{
  int use=0;
  int type=0,size=0,dir=0,rea=0,ea=0;
  const char *asl="";
  const char *strop=0;

  type=(op>>12)&5;
  rea =(op>> 9)&7;
  dir =(op>> 8)&1; // er,re
  size=(op>> 6)&3; if (size>=3) return 1;
  ea  = op&0x3f;

  if (dir && ea<0x10) return 1; // addx/subx opcode

  // See if we can do this opcode:
  if (dir==0 && EaCanRead (ea,size)==0) return 1;
  if (dir    && EaCanWrite(ea)==0)      return 1;
  if ((size==0||!(type&1))&&EaAn(ea))   return 1;

  use=OpBase(op,size);
  use&=~0x0e00; // Use same opcode for Dn
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,ea); Cycles=4;

  EaCalcRead(dir?11:-1,0,ea,size,0x003f,earwt_msb_dont_care);

  EaCalcRead(dir?-1:11,1,rea,size,0x0e00,earwt_msb_dont_care);

  ot(";@ Do arithmetic:\n");
  if (type==0) strop = "orrs";
  if (type==1) strop = (char *) (dir ? "subs" : "rsbs");
  if (type==4) strop = "ands";
  if (type==5) strop = "adds";

  if (size==0) asl=",asl #24";
  if (size==1) asl=",asl #16";

  if (size<2) ot("  mov r0,r0%s\n",asl);
  ot("  %s r1,r0,r1%s\n",strop,asl);

  if (type&1) OpGetFlags(type==1,type&1); // add/subtract
  else        OpGetFlagsNZ(1);
  ot("\n");

  ot(";@ Save result:\n");
  if (size<2) ot("  mov r1,r1,asr #%d\n",size?16:24);
  if (dir) EaWrite(11, 1, ea,size,0x003f,earwt_msb_dont_care);
  else     EaWrite(11, 1,rea,size,0x0e00,earwt_msb_dont_care);

  if(rea==ea) {
    if(ea<8) Cycles=(size>=2)?8:4; else Cycles+=(size>=2)?26:14;
  } else if(dir) {
    Cycles+=4;
    if(size>=2) Cycles+=4;
  } else {
    if(size>=2) {
      Cycles+=2;
      if(ea<0x10||ea==0x3c) Cycles+=2;
    }
  }

  OpEnd(ea);

  return 0;
}

// --------------------- Opcodes 0x80c0+ ---------------------
static void UnrolledDiv()
{
  ot("  rsbs r0,r1,#0 ;@ negate divisor and clear carry\n");
  ot("  sbc r10,r1,r1,lsr #1 ;@ r10=(divisor<<15)-1\n");
  ot(";@ calculate number of skipped initial iterations\n");
#if HAVE_ARMv5
  // Use the difference in approximated log2 to determine skipped iterations,
  // but round down by subtracting an extra 1.
  // This rounded result may be less than 0, which is handled below.
  ot("  orr r3,r2,r1,lsr #16 ;@ make sure the rounded difference is at most 15\n");
  ot("  clz r3,r3 ;@ leading zeros of dividend, clamped to clz(divisor)+16\n");
  ot("  clz r1,r1 ;@ leading zeros of divisor\n");
  ot("  sbcs r3,r3,r1 ;@ subtract and round down (carry is still clear)\n");
  ot("  mov r3,r3,lsl #1 ;@ each skipped iteration has 2 penalty cycle pairs\n");
  ot(";@ add branch offset (12 bytes per iteration)\n");
  ot("  add r1,r3,r3,lsl #1\n");
  ot("  addhi pc,pc,r1,lsl #1 ;@ fallthrough if max iterations\n");
  ot("  mov r3,#0 ;@ saturate negative cycles to 0\n");
#else
  ot("  mov r3,#0 ;@ number of additional cycle pairs\n");
  // Resolve only to an even number of skipped iterations, because
  // determining bit 0 would be just as expensive as the skipped iteration
  for (int shift=8; shift!=1; shift>>=1)
  {
    ot("  cmp r2,r1,lsr #%d+1\n",shift);
    ot("  addlo r3,r3,#%d*2 ;@ each skipped iteration has 2 penalty cycle pairs\n",shift);
    if (shift!=2) ot("  movlo r1,r1,lsr #%d\n",shift);
  }
  ot("\n");
  ot(";@ add branch offset (12 bytes per iteration)\n");
  ot("  adds r1,r3,r3,lsl #1\n");
  ot("  addne pc,pc,r1,lsl #1 ;@ fallthrough if max iterations\n");
  ot("  nop ;@ padding for pc-relative offset\n");
#endif
  ot("\n");
  ot(";@ For each iteration:\n");
  ot(";@ Shift remainder into upper bits and compare the pre-shifted divisor to it.\n");
  ot(";@ This resets carry if non-restoring, and sets overflow if remainder MSB was 1.\n");
  ot(";@ Then, conditionally shift and add the negated divisor, setting an upper quotient bit.\n");
  ot(";@ Finally, add 0, 1, or 2 penalty cycle pairs based on the overflow and carry flags.\n");
  for (int shift=0; shift<16; shift++)
  {
    ot("  cmp r10,r2,lsl #%d\n",shift);
    ot("  addcc r2,r2,r0,lsr #%d+1\n",shift);
    // Final iteration has a fixed cycle length
    if (shift!=15) ot("  adcvc r3,r3,#1\n");
  }
  ot("\n");
  ot("  sub r5,r5,r3,lsl #1 ;@ Count penalty cycle pairs\n");
}

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
  if (EaCanRead(ea,1)==0||EaAn(ea)) return 1;

  use=OpBase(op,1);
  use&=~0x0e00; // Use same for all registers
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  // Output common unrolled divide loop function before first DIVU opcode
  if (op == 0x80c0)
  {
#if !INLINE_UNROLLED_DIV
    ot(";@ ---------- Common unrolled division subroutine ----------\n");
    ot(";@ Fully unrolled unsigned division, with accurate cycle counting.\n");
    ot(";@ Assumes divide-by-zero and unsigned overflow checks have passed.\n");
    ot(";@ Inputs: r2=dividend, r1=divisor<<16\n");
    ot(";@ Output: r2=(quotient<<16)|remainder\n");
    ot(";@ Destroys: r0,r1,r3,r10\n");
    ot(";@ r5 is adjusted with the per-iteration penalty cycles.\n");
    ot("DivideCommon%s\n",ms?"":":");
    UnrolledDiv();
    ot("  bx lr\n");
    ot("\n");
#endif
  }

  OpStart(op,ea,0,1);
  if(type) Cycles=38;
  else     Cycles=sign?16:10;

  EaCalcRead(-1,0,ea,1,0x003f,earwt_msb_dont_care);

  EaCalc(11,0x0e00,rea, 2);
  EaRead(11,     2,rea, 2,0x0e00);

  ot("  movs r1,r0,asl #16\n");

  if (type==0) // div
  {
    // the manual says C is always cleared, but neither Musashi nor FAME do that
    //ot("  bic r10,r10,#0x20000000 ;@ always clear C\n");
    ot("  beq divzero%.4x ;@ division by zero\n",op);
    ot("\n");
    
    if (sign)
    {
      ot("  ands r12,r2,#0x80000000 ;@ r12 = odd parity if the result is negative\n");
      ot("  submi r5,r5,#2\n");
      ot("  rsbmi r2,r2,#0 ;@ Make r2 positive\n");
      ot("\n");
      ot("  tst r1,r1\n");
      ot("  orrmi r12,r12,#0x40000000\n");
      ot("  rsbmi r1,r1,#0 ;@ Make r1 positive\n");
    }
    ot("\n");

    ot(";@ Overflow?\n");
    ot("  cmp r2,r1\n");
    ot("  movhs r10,#0x90000000 ;@ set overflow/negative flags\n");
    ot("  bhs endofop%.4x ;@ overflow!\n",op);
    ot("\n");

    ot("  sub r5,r5,#%d ;@ Minimum cycles divide loop can take\n",sign?74:66);
#if INLINE_UNROLLED_DIV
    UnrolledDiv();
#else
    ot("  bl DivideCommon ;@ Divide r2 by (r1>>16)\n");
#endif
    ot(";@r2==(quotient<<16)|remainder\n");
    ot("\n");

    if (sign)
    {
      // sign correction
      ot("  mov r3,r2,lsr #16\n");
      ot("  cmn r12,r12\n");
      ot("  rsbvs r3,r3,#0 ;@ negate if quotient is negative\n");
      ot("  subvs r5,r5,#2\n");
      ot("  rsbcs r2,r2,#0 ;@ negate the remainder if dividend was negative\n");
      ot("  subcs r5,r5,#2\n");
      ot("\n");

      ot("  movs r1,r3,lsl #16 ;@ set flags based on quotient\n");
      OpGetFlagsNZ(1);
      // signed overflow check
      ot("  cmp r3,r1,asr #16 ;@ signed overflow?\n");
      ot("  movne r10,#0x90000000 ;@ set overflow/negative flags\n");
      ot("  bne endofop%.4x ;@ overflow!\n",op);
      ot("\n");

      ot("  mov r1,r1,lsr #16\n");
      ot("  orr r1,r1,r2,lsl #16 ;@ Insert remainder\n");
    }
    else
    {
      ot("  mov r1,r2,ror #16 ;@ swap quotient and remainder\n");
      ot("  movs r2,r1,lsl #16 ;@ set flags based on quotient\n");
      OpGetFlagsNZ(2);
    }
    ot("\n");
  }

  if (type==1)
  {
    ot(";@ Calculate cycles needed: 2*(#bits set in multiplier engine mask)\n");
    if (sign) ot("  eor r0,r1,r1,lsl #1\n");
    ot("  mov r12,#0x0F000000 ;@ count top 16 bits, the O(1) way\n");
    ot("  orr r12,r12,r12,lsr #8 ;@ r12 = 0x0F0F0000\n");
    ot("  eor r10,r12,r12,lsl #2 ;@ r10 = 0x33330000\n");
    ot("  eor r3,r10,r10,lsl #1  ;@ r3  = 0x55550000\n");
    ot("  and r3,r3,r%d,lsr #1\n",sign?0:1);
    ot("  sub r0,r%d,r3\n",sign?0:1);
    ot("  and r3,r10,r0,lsr #2\n");
    ot("  and r0,r10,r0\n");
    ot("  add r0,r0,r3\n");
    ot("  add r0,r0,r0,lsr #4\n");
    ot("  and r0,r12,r0\n");
    ot("  add r0,r0,r0,lsl #8\n");
    ot("  sub r5,r5,r0,lsr #23 ;@ cycles -= 2*bitcount(mask)\n");

    ot(";@ Get 16-bit signs right:\n");
    ot("  mov r0,r1,%s #16\n",sign?"asr":"lsr");
    if (sign) SignExtend(2,2,1);
    else      ZeroExtend(2,2,1);
    ot("\n");

    ot("  muls r1,r2,r0\n");
    OpGetFlagsNZ(1);
  }
  ot("\n");

  EaWrite(11, 1,rea, 2,0x0e00,earwt_shifted_up);

  if (type==0) ot("endofop%.4x%s\n",op,ms?"":":");
  opend_op_changes_cycles=1;
  OpEnd(ea);

  if (type==0) // div
  {
    ot("divzero%.4x%s\n",op,ms?"":":");
    ot("  mov r0,#5 ;@ Divide by zero\n");
    ot("  bl Exception\n");
    Cycles+=34-6;
    OpEnd(ea);
    ot("\n");
  }

  return 0;
}

// Get X Bit into carry - trashes r2
int GetXBit(int subtract)
{
  ot(";@ Get X bit:\n");
  ot("  ldr r2,[r7,#0x4c]\n");
  if (subtract) ot("  mvn r2,r2 ;@ Invert it\n");
  ot("  tst r2,r2,lsl #3 ;@ Get into Carry\n");
  ot("\n");
  return 0;
}

// --------------------- Opcodes 0x8100+ ---------------------
// 1t00ddd1 0000asss - sbcd/abcd Ds,Dd or -(As),-(Ad)
int OpAbcd(int op)
{
  int use=0;
  int type=0,sea=0,mem=0,dea=0;
  
  type=(op>>14)&1; // sbcd/abcd
  dea =(op>> 9)&7;
  mem =(op>> 3)&1;
  sea = op     &7;

  if (mem) { sea|=0x20; dea|=0x20; }

  use=op&~0x0e07; // Use same opcode for all registers..
  if (sea==0x27) use|=0x0007; // ___x.b -(a7)
  if (dea==0x27) use|=0x0e00; // ___x.b -(a7)
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,sea,dea); Cycles=6;

  if (mem)
  {
    ot(";@ Get src/dest EA vals\n");
    EaCalc (0,0x000f, sea,0,earwt_msb_dont_care);
    EaRead (0,     6, sea,0,0x000f,earwt_msb_dont_care);
    EaCalcRead(11,0,dea,0,0x0e00,earwt_msb_dont_care);
  }
  else
  {
    ot(";@ Get src/dest reg vals\n");
    EaCalcRead(-1,6,sea,0,0x0007,earwt_msb_dont_care);
    EaCalcRead(11,0,dea,0,0x0e00,earwt_msb_dont_care);
  }

  ot("  bic r10,r10,#0xb1000000 ;@ clear all flags except old Z\n");

  ot("  ldr r1,[r7,#0x4c] ;@ Get X bit\n");
  ot("  and r2,r0,#0x0f\n");
  ot("  movs r12,r1,lsl #3 ;@ X into carry\n");
  ot("  and r1,r6,#0x0f\n");
  if (type)
  {
    // abcd
    ot("  adc r1,r2,r1\n");
    ot("  cmp r1,#9\n");

    ot("  and r0,r0,#0xf0\n");
    ot("  and r6,r6,#0xf0\n");
    ot("  add r1,r1,r0\n");
    ot("  add r1,r1,r6\n");
    ot("  mov r12,r1\n");
    ot("  addhi r12,r12,#6 ;@ Decimal adjust units\n");
    ot("  tst r1,#0x80\n");
    ot("  orreq r10,r10,#0x10000000 ;@ Undefined V behavior\n");
    ot("  cmp r12,#0x9f\n");
    ot("  orrhi r10,r10,#0x20000000 ;@ C\n");
    ot("  subhi r12,r12,#0xa0\n");
    ot("  movs r0,r12,lsl #24\n");
    ot("  bicpl r10,r10,#0x10000000 ;@ Undefined V behavior part II\n");
  }
  else
  {
    // sbcd
    ot("  mov r12,#0 ;@ corf\n");
    ot("  adc r1,r1,#0\n");
    ot("  sub r1,r2,r1\n");
    ot("  cmp r1,#0x0f\n");
    ot("  movhi r12,#6\n");

    ot("  and r0,r0,#0xf0\n");
    ot("  and r6,r6,#0xf0\n");
    ot("  add r1,r1,r0\n");
    ot("  sub r1,r1,r6\n");
    ot("  tst r1,#0x80\n");
    ot("  orrne r10,r10,#0x10000000 ;@ Undefined V behavior\n");
    ot("  cmp r1,r12\n");
    ot("  orrlt r10,r10,#0x20000000 ;@ C\n");
    ot("  cmp r1,#0xff\n");
    ot("  addhi r1,r1,#0xa0\n");
    ot("  sub r12,r1,r12\n");
    ot("  movs r0,r12,lsl #24\n");
    ot("  bicmi r10,r10,#0x10000000 ;@ Undefined V behavior part II\n");
  }

  ot("  orrmi r10,r10,#0x80000000 ;@ Undefined N behavior\n");
  ot("  bicne r10,r10,#0x40000000 ;@ Z flag\n");
  ot("  str r10,[r7,#0x4c] ;@ Save X bit\n");
  ot("\n");

  EaWrite(11, 0, dea,0,0x0e00,earwt_shifted_up);

  ot("  ldr r6,[r7,#0x54]\n");
  OpEnd(sea,dea);

  return 0;
}

// 01001000 00eeeeee - nbcd <ea>
int OpNbcd(int op)
{
  int use=0;
  int ea=0;
  
  ea=op&0x3f;

  if(EaCanWrite(ea)==0||EaAn(ea)) return 1;

  use=OpBase(op,0);
  if(op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,ea); Cycles=6;
  if(ea >= 8)  Cycles+=2;

  EaCalcRead(11,0,ea,0,0x003f,earwt_zero_extend);

  // this is rewrite of Musashi's code
  ot("  ldr r2,[r7,#0x4c]\n");
  ot("  bic r10,r10,#0xb0000000 ;@ clear all flags, except Z\n");
  ot("  and r2,r2,#0x20000000\n");
  ot("  rsb r1,r0,#0 ;@ do arithmetic\n");
  ot("  subs r1,r1,r2,lsr #29 ;@ X\n");

  ot("  beq finish%.4x\n",op);
  ot("\n");

  ot("  movs r1,r1,lsl #24\n");
  ot("  orrmi r10,r10,#0x10000000 ;@ Undefined V behavior\n");
  ot("  orr r2,r1,r0,lsl #24\n");
  ot("  tst r2,#0x0f000000\n");
  ot("  andeq r1,r1,#0xf0000000\n");
  ot("  orreq r1,r1,#0x06000000\n");
  ot("  adds r1,r1,#0x9a000000\n");
  ot("  bicmi r10,r10,#0x10000000 ;@ Undefined V behavior part II\n");
  ot("  orrmi r10,r10,#0x80000000 ;@ Undefined N behavior\n");
  ot("  bicne r10,r10,#0x40000000 ;@ Z\n");
  ot("  orr r10,r10,#0x20000000 ;@ C\n");
  ot("\n");

  EaWrite(11, 1, ea,0,0x3f,earwt_shifted_up);

  ot("finish%.4x%s\n",op,ms?"":":");
  ot("  str r10,[r7,#0x4c] ;@ Save X\n");
  ot("\n");

  ot("  ldr r6,[r7,#0x54]\n");
  OpEnd(ea);

  return 0;
}

// --------------------- Opcodes 0x90c0+ ---------------------
// Suba/Cmpa/Adda 1tt1nnnx 11eeeeee (tt=type, x=size, eeeeee=Source EA)
int OpAritha(int op)
{
  int use=0;
  int type=0,size=0,sea=0,dea=0;
  const char *asr="";

  // Suba/Cmpa/Adda/(invalid):
  type=(op>>13)&3; if (type>=3) return 1;

  size=(op>>8)&1; size++;
  dea=(op>>9)&7; dea|=8; // Dest=An
  sea=op&0x003f; // Source

  // See if we can do this opcode:
  if (EaCanRead(sea,size)==0) return 1;

  use=OpBase(op,size);
  use&=~0x0e00; // Use same opcode for An
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,sea); Cycles=(size==2)?6:8;
  if(size==2&&(sea<0x10||sea==0x3c)) Cycles+=2;
  if(type==1) Cycles=6;

  // EA calculation order defines how situations like  suba.w (A0)+, A0 get handled.
  // different emus act differently in this situation, I couldn't fugure which is right behaviour.
  //if (type == 1)
  {
    EaCalcRead(-1,0,sea,size,0x003f,earwt_msb_dont_care);
    EaCalcRead(type!=1?11:-1,1,dea,2,0x0e00,earwt_msb_dont_care);
  }
#if 0
  else
  {
    EaCalcRead(type!=1?11:-1,1,dea,2,0x0e00,earwt_msb_dont_care);
    EaCalcRead(-1,0,sea,size,0x003f,earwt_msb_dont_care);
  }
#endif

  if (size<2) ot("  mov r0,r0,asl #%d\n\n",size?16:24);
  if (size<2) asr=(char *)(size?",asr #16":",asr #24");

  if (type==0) ot("  sub r1,r1,r0%s\n",asr);
  if (type==1) ot("  cmp r1,r0%s ;@ Defines NZCV\n",asr);
  if (type==1) OpGetFlags(1,0); // Get Cmp flags
  if (type==2) ot("  add r1,r1,r0%s\n",asr);
  ot("\n");

  if (type!=1) EaWrite(11, 1, dea,2,0x0e00);

  OpEnd(sea);

  return 0;
}

// --------------------- Opcodes 0x9100+ ---------------------
// Emit a Subx/Addx opcode, 1t01ddd1 zz00rsss addx.z Ds,Dd
int OpAddx(int op)
{
  int use=0;
  int type=0,size=0,dea=0,sea=0,mem=0;
  const char *asl="";

  type=(op>>14)&1;
  dea =(op>> 9)&7;
  size=(op>> 6)&3; if (size>=3) return 1;
  sea = op&7;
  mem =(op>> 3)&1;

  // See if we can do this opcode:
  if (EaCanRead(sea,size)==0) return 1;
  if (EaCanWrite(dea)==0) return 1;

  if (mem) { sea+=0x20; dea+=0x20; }

  use=op&~0x0e07; // Use same opcode for Dn
  if (size==0&&sea==0x27) use|=0x0007; // ___x.b -(a7)
  if (size==0&&dea==0x27) use|=0x0e00; // ___x.b -(a7)
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,sea,dea); Cycles=4;
  if(size>=2)   Cycles+=4;
  if(sea>=0x10) Cycles+=2;

  if (mem)
  {
    ot(";@ Get src/dest EA vals\n");
    EaCalc (0,0x000f, sea,size,earwt_shifted_up);
    EaRead (0,     6, sea,size,0x000f,earwt_shifted_up);
    EaCalcRead(11,0,dea,size,0x0e00,earwt_msb_dont_care);
  }
  else
  {
    ot(";@ Get src/dest reg vals\n");
    EaCalcRead(-1,6,sea,size,0x0007,earwt_msb_dont_care);
    EaCalcRead(11,0,dea,size,0x0e00,earwt_msb_dont_care);
    if (size<2) ot("  mov r6,r6,asl #%d\n\n",size?16:24);
  }

  if (size<2) asl=(char *)(size?",asl #16":",asl #24");

  ot(";@ Do arithmetic:\n");
  GetXBit(type==0);

  if (type==1 && size<2)
  {
    ot(";@ Make sure the carry bit will tip the balance:\n");
    ot("  mvn r2,#0\n");
    ot("  orr r6,r6,r2,lsr #%i\n",(size==0)?8:16);
    ot("\n");
  }

  if (type==0) ot("  rscs r1,r6,r0%s\n",asl);
  if (type==1) ot("  adcs r1,r6,r0%s\n",asl);
  ot("  orr r3,r10,#0xb0000000 ;@ for old Z\n");
  OpGetFlags(type==0,1,0); // subtract
  if (size<2) {
    ot("  movs r2,r1,lsr #%i\n", size?16:24);
    ot("  orreq r10,r10,#0x40000000 ;@ add potentially missed Z\n");
  }
  ot("  andeq r10,r10,r3 ;@ fix Z\n");
  ot("\n");

  ot(";@ Save result:\n");
  EaWrite(11, 1, dea,size,0x0e00,earwt_shifted_up);

  ot("  ldr r6,[r7,#0x54]\n");
  OpEnd(sea,dea);

  return 0;
}

// --------------------- Opcodes 0xb000+ ---------------------
// Emit a Cmp/Eor opcode, 1011rrrt xxeeeeee (rrr=Dn, t=cmp/eor, xx=size extension, eeeeee=ea)
int OpCmpEor(int op)
{
  int rea=0,eor=0;
  int size=0,ea=0,use=0;
  const char *asl="";

  // Get EA and register EA
  rea=(op>>9)&7;
  eor=(op>>8)&1;
  size=(op>>6)&3; if (size>=3) return 1;
  ea=op&0x3f;

  if (eor && (ea>>3) == 1) return 1; // not a valid mode for eor

  // See if we can do this opcode:
  if (EaCanRead(ea,size)==0) return 1;
  if (eor && EaCanWrite(ea)==0) return 1;
  if (EaAn(ea)&&(eor||size==0)) return 1;

  use=OpBase(op,size);
  use&=~0x0e00; // Use 1 handler for register d0-7
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,ea); Cycles=4;
  if(eor) {
    if(ea>8)     Cycles+=4;
    if(size>=2)  Cycles+=4;
  } else {
    if(size>=2)  Cycles+=2;
  }

  ot(";@ Get EA into r11 and value into r0:\n");
  EaCalcRead(eor?11:-1,0,ea,size,0x003f,earwt_msb_dont_care);

  ot(";@ Get register operand into r1:\n");
  EaCalcRead(-1,1,rea,size,0x0e00,earwt_msb_dont_care);

  if (size<2) ot("  mov r0,r0,asl #%d\n\n",size?16:24);
  if (size<2) asl=(char *)(size?",asl #16":",asl #24");

  ot(";@ Do arithmetic:\n");
  if (eor)
  {
    ot("  eors r1,r0,r1%s\n",asl);
    OpGetFlagsNZ(1);
  }
  else
  {
    ot("  rsbs r1,r0,r1%s\n",asl);
    OpGetFlags(1,0); // Cmp like subtract
  }
  ot("\n");

  if (eor) EaWrite(11, 1,ea,size,0x003f,earwt_shifted_up);

  OpEnd(ea);
  return 0;
}

// Emit a Cmpm opcode, 1011ddd1 xx001sss (rrr=Adst, xx=size extension, sss=Asrc)
int OpCmpm(int op)
{
  int size=0,sea=0,dea=0,use=0;
  const char *asl="";

  // get size, get EAs
  size=(op>>6)&3; if (size>=3) return 1;
  sea=(op&7)|0x18;
  dea=(op>>9)&0x3f;

  use=op&~0x0e07; // Use 1 handler for all registers..
  if (size==0&&sea==0x1f) use|=0x0007; // ..except (a7)+
  if (size==0&&dea==0x1f) use|=0x0e00;
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,sea); Cycles=4;

  ot(";@ Get src operand into r11:\n");
  EaCalc (0,0x0007, sea,size,earwt_shifted_up);
  EaRead (0,    11, sea,size,0x0007,earwt_shifted_up);

  ot(";@ Get dst operand into r0:\n");
  EaCalcRead(-1,0,dea,size,0x0e00,earwt_msb_dont_care);

  if (size<2) asl=(char *)(size?",asl #16":",asl #24");

  ot("  rsbs r0,r11,r0%s\n",asl);
  OpGetFlags(1,0); // Cmp like subtract
  ot("\n");

  OpEnd(sea);
  return 0;
}


// Emit a Chk opcode, 0100ddd1 x0eeeeee (rrr=Dn, x=size extension, eeeeee=ea)
int OpChk(int op)
{
  int rea=0;
  int size=0,ea=0,use=0;

  // Get EA and register EA
  rea=(op>>9)&7;
  if((op>>7)&1)
       size=1; // word operation
  else size=2; // long
  ea=op&0x3f;

  if (EaAn(ea)) return 1; // not a valid mode
  if (size!=1)  return 1; // 000 variant only supports word

  // See if we can do this opcode:
  if (EaCanRead(ea,size)==0) return 1;

  use=OpBase(op,size);
  use&=~0x0e00; // Use 1 handler for register d0-7
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,ea); Cycles=10;

  ot(";@ Get value into r0:\n");
  EaCalcRead(-1,0,ea,size,0x003f,earwt_msb_dont_care);

  ot(";@ Get register operand into r1:\n");
  EaCalcRead(-1,1,rea,size,0x0e00,earwt_msb_dont_care);

  if (size<2) ot("  movs r1,r1,asl #%d\n\n",size?16:24);
  else        ot("  adds r1,r1,#0 ;@ Define flags\n");

  ot(";@ get flags, including undocumented ones\n");
  OpGetFlagsNZ(1);

  ot(";@ is reg negative?\n");
  ot("  bmi chktrapneg%.4x\n",op);

  ot(";@ Do arithmetic:\n");
  if (size<2) ot("  cmp r1,r0,asl #%d\n",size?16:24);
  else        ot("  cmp r1,r0\n");
  ot("  bgt chktrap%.4x\n",op);

  OpEnd(ea);

  ot("chktrapneg%.4x%s ;@ CHK negative exception:\n",op,ms?"":":");
  ot(";@ Delayed negative trap only if !(N||V), which is !N for a negative subtrahend\n");
  if (size<2) ot("  rsbs r0,r1,r0,asl #%d\n",size?16:24);
  else        ot("  cmp r0,r1\n");
  ot("  subpl r5,r5,#2\n");
  ot("chktrap%.4x%s ;@ CHK exception:\n",op,ms?"":":");
  ot("  mov r0,#6\n");
  ot("  bl Exception\n");
  Cycles+=34-6;
  opend_op_changes_cycles=1;
  OpEnd(ea);

  return 0;
}

// vim:ts=2:sw=2:expandtab

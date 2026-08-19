
// This file is part of the Cyclone 68000 Emulator

// Copyright (c) 2004,2011 FinalDave (emudave (at) gmail.com)
// Copyright (c) 2005-2011 Gražvydas "notaz" Ignotas (notasas (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/


#include "app.h"

// trashes r0
const char *TestCond(int m68k_cc, int invert)
{
  const char *cond="";
  const char *icond="";

  // ARM: NZCV
  switch (m68k_cc)
  {
    case 0x00: // T
    case 0x01: // F
      break;
    case 0x02: // hi
      ot("  tst r10,#0x60000000 ;@ hi: !C && !Z\n");
      cond="eq", icond="ne";
      break;
    case 0x03: // ls
      ot("  tst r10,#0x60000000 ;@ ls: C || Z\n");
      cond="ne", icond="eq";
      break;
    case 0x04: // cc
      ot("  tst r10,#0x20000000 ;@ cc: !C\n");
      cond="eq", icond="ne";
      break;
    case 0x05: // cs
      ot("  tst r10,#0x20000000 ;@ cs: C\n");
      cond="ne", icond="eq";
      break;
    case 0x06: // ne
      ot("  tst r10,#0x40000000 ;@ ne: !Z\n");
      cond="eq", icond="ne";
      break;
    case 0x07: // eq
      ot("  tst r10,#0x40000000 ;@ eq: Z\n");
      cond="ne", icond="eq";
      break;
    case 0x08: // vc
      ot("  tst r10,#0x10000000 ;@ vc: !V\n");
      cond="eq", icond="ne";
      break;
    case 0x09: // vs
      ot("  tst r10,#0x10000000 ;@ vs: V\n");
      cond="ne", icond="eq";
      break;
    case 0x0a: // pl
      ot("  tst r10,r10 ;@ pl: !N\n");
      cond="pl", icond="mi";
      break;
    case 0x0b: // mi
      ot("  tst r10,r10 ;@ mi: N\n");
      cond="mi", icond="pl";
      break;
    case 0x0c: // ge
      ot("  teq r10,r10,lsl #3 ;@ ge: N == V\n");
      cond="pl", icond="mi";
      break;
    case 0x0d: // lt
      ot("  teq r10,r10,lsl #3 ;@ lt: N != V\n");
      cond="mi", icond="pl";
      break;
    case 0x0e: // gt
      ot("  eor r0,r10,r10,lsl #3 ;@ gt: !Z && N == V\n");
      ot("  orrs r0,r0,r10,lsl #1\n");
      cond="pl", icond="mi";
      break;
    case 0x0f: // le
      ot("  eor r0,r10,r10,lsl #3 ;@ le: Z || N != V\n");
      ot("  orrs r0,r0,r10,lsl #1\n");
      cond="mi", icond="pl";
      break;
    default:
      printf("invalid m68k_cc: %x\n", m68k_cc);
      exit(1);
      break;
  }
  return invert?icond:cond;
}

// Emit a Btst/Bchg/Bclr/Bset opcode
static void EmitBtst(int type,int mem)
{
  if (mem) {
    ot("  and r2,r11,#7  ;@ mem - do mod 8\n");  // size always 0
    ot("\n");
  } else if (type) {
    ot("  movs r2,r2,lsl #27 ;@ reg - do mod 32\n"); // size always 2
    ot("  submi r5,r5,#2 ;@ extra cycles\n");
    ot("  mov r2,r2,lsr #27\n");
    ot("\n");
  }

#if !HAVE_ARMv6T2
  ot("  mov r1,#1\n");
  ot("  bic r10,r10,#0x40000000 ;@ Clear Z flag\n");
  ot("  tst r1,r0,ror r2 ;@ Test bit\n");
  ot("  orreq r10,r10,#0x40000000 ;@ Get Z flag\n");
  ot("\n");
#endif

  if (type>0)
  {
#if HAVE_ARMv6T2
    ot("  mov r1,#1\n");
#endif
    if (type==1) ot("  eor r1,r0,r1,lsl r2 ;@ Toggle bit\n");
    if (type==2) ot("  bic r1,r0,r1,lsl r2 ;@ Clear bit\n");
    if (type==3) ot("  orr r1,r0,r1,lsl r2 ;@ Set bit\n");
    ot("\n");
  }

#if HAVE_ARMv6T2
  ot("  mvn r0,r0,ror r2 ;@ Shift to bit 0 and invert\n");
  ot("  bfi r10,r0,#30,#1 ;@ Replace Z flag\n");
  ot("\n");
#endif
}

// --------------------- Opcodes 0x0100+ ---------------------
// Emit a Btst (Register) opcode 0000nnn1 ttaaaaaa
int OpBtstReg(int op)
{
  int use=0;
  int type=0,sea=0,tea=0;
  int size=0;

  type=(op>>6)&3; // Btst/Bchg/Bclr/Bset
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

  use=OpBase(op,size);
  use&=~0x0e00; // Use same handler for all registers
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,tea,0,tea<0x10);

  if(type==1||type==3) {
    Cycles=8;
    if(size>=2) Cycles-=2;
  } else {
    Cycles=type?6:4;
    if(size>=2) Cycles+=2;
    if(type==0 && tea==0x3c) Cycles+=2;
    if(type==2 && tea>=0x10) Cycles+=2;
  }

  EaCalcRead(-1,(size==0)?11:2,sea,0,0x0e00,earwt_msb_dont_care);

  EaCalcRead((type>0)?8:-1,0,tea,size,0x003f,earwt_msb_dont_care);

  EmitBtst(type,size==0);

  if (type>0)
    EaWrite(8,1,tea,size,0x003f,earwt_msb_dont_care);

  opend_op_changes_cycles=tea<0x10;
  OpEnd(tea);

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
  if (EaCanRead(tea,0)==0||EaAn(tea)||tea==0x3c) return 1;
  if (type>0)
  {
    if (EaCanWrite(tea)==0) return 1;
  }

  use=OpBase(op,size);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,sea,tea,tea<0x10);

  ot("\n");

  EaCalcRead(-1,(size==0)?11:2,sea,0,0,earwt_msb_dont_care);

  if(type==1||type==3) {
    Cycles=10;
  } else {
    Cycles=type?10:8;
    if(size>=2) Cycles+=2;
  }
  if(type && tea>=0x10) Cycles+=2;

  EaCalcRead((type>0)?8:-1,0,tea,size,0x003f,earwt_msb_dont_care);

  EmitBtst(type,size==0);

  if (type>0)
  {
    EaWrite(8, 1,tea,size,0x003f,earwt_msb_dont_care);
#if CYCLONE_FOR_GENESIS && !MEMHANDLERS_CHANGE_CYCLES
    // this is a bit hacky (device handlers might modify cycles)
    if (tea==0x38||tea==0x39)
      ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
#endif
  }

  opend_op_changes_cycles=tea<0x10;
  OpEnd(sea,tea);

  return 0;
}

// --------------------- Opcodes 0x4000+ ---------------------
int OpNeg(int op)
{
  // 01000tt0 xxeeeeee (tt=negx/clr/neg/not, xx=size, eeeeee=EA)
  int type=0,size=0,ea=0,use=0;
  EaRWType wtype=earwt_msb_dont_care;

  type=(op>>9)&3;
  ea  =op&0x003f;
  size=(op>>6)&3; if (size>=3) return 1;

  // See if we can do this opcode:
  if (EaCanRead (ea,size)==0||EaAn(ea)) return 1;
  if (EaCanWrite(ea     )==0) return 1;

  use=OpBase(op,size);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,ea); Cycles=size<2?4:6;
  if(ea >= 0x10)  Cycles*=2;

  if (type==1) EaCalc (11,0x003f,ea,size,earwt_msb_dont_care);
#if HAVE_ARMv6
  else if (type==3) EaCalcRead (11,0,ea,size,0x003f,earwt_sign_extend);
#endif
  else EaCalcRead (11,0,ea,size,0x003f,earwt_msb_dont_care); // Don't need to read for 'clr' (or do we, for a dummy read?)

  if (type==0)
  {
    ot(";@ Negx:\n");
    GetXBit(1);
    if(size!=2) ot("  mov r0,r0,asl #%i\n",size?16:24);
    ot("  rscs r1,r0,#0 ;@ do arithmetic\n");
    ot("  orr r3,r10,#0xb0000000 ;@ for old Z\n");
    OpGetFlags(1,1,0);
    if(size!=2) {
      ot("  movs r1,r1,lsr #%i\n",size?16:24);
      ot("  orreq r10,r10,#0x40000000 ;@ possily missed Z\n");
    }
    ot("  andeq r10,r10,r3 ;@ fix Z\n");
    ot("\n");
    wtype=earwt_zero_extend;
  }

  if (type==1)
  {
    ot(";@ Clear:\n");
    ot("  mov r1,#0\n");
    ot("  mov r10,#0x40000000 ;@ NZCV=0100\n");
    ot("\n");
    wtype=earwt_zero_extend;
  }

  if (type==2)
  {
    ot(";@ Neg:\n");
    if(size!=2) ot("  mov r0,r0,asl #%i\n",size?16:24);
    ot("  rsbs r1,r0,#0\n");
    OpGetFlags(1,1);
    wtype=earwt_shifted_up;
    ot("\n");
  }

  if (type==3)
  {
    ot(";@ Not:\n");
#if HAVE_ARMv6
    wtype=earwt_sign_extend;
#else
    if(size!=2) {
      ot("  mov r0,r0,asl #%i\n",size?16:24);
      ot("  mvns r1,r0,asr #%i\n",size?16:24);
    }
    else
#endif
      ot("  mvns r1,r0\n");
    OpGetFlagsNZ(1);
    ot("\n");
  }

  if (type==1) eawrite_check_addrerr=1;
  EaWrite(11, 1,ea,size,0x003f,wtype);

  OpEnd(ea);

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

  EaCalc (11,0x0007,ea,2,earwt_shifted_up);
  EaRead (11,     0,ea,2,0x0007,earwt_shifted_up);

  ot("  movs r1,r0,ror #16\n");
  OpGetFlagsNZ(1);

  EaWrite(11,     1,8,2,0x0007,earwt_shifted_up);

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
  if (EaCanWrite(sea)==0||EaAn(sea)) return 1;

  use=OpBase(op,size);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,sea); Cycles=4;

  EaCalc (0,0x003f,sea,size,earwt_shifted_up);
  EaRead (0,     0,sea,size,0x003f,earwt_shifted_up,1);

  OpGetFlagsNZ(0);
  ot("\n");

  OpEnd(sea);
  return 0;
}

// --------------------- Opcodes 0x4880+ ---------------------
// Emit an Ext opcode, 01001000 1x000nnn
int OpExt(int op)
{
  int ea=0;
  int size=0,use=0;

  ea=op&0x0007;
  size=(op>>6)&1;

  use=OpBase(op,size);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalcRead (11,     1,ea,size,0x0007,earwt_sign_extend,1,1);

  OpGetFlagsNZ(1);
  ot("\n");

  EaWrite(11,     1,ea,size+1,0x0007,earwt_msb_dont_care,1);

  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x50c0+ ---------------------
// Emit a Set cc opcode, 0101cccc 11eeeeee
int OpSet(int op)
{
  int cc=0,ea=0;
  int size=0,use=0,changed_cycles=0;
  const char *cond;

  cc=(op>>8)&15;
  ea=op&0x003f;

  if ((ea&0x38)==0x08) return 1; // dbra, not scc

  // See if we can do this opcode:
  if (EaCanWrite(ea)==0) return 1;

  use=OpBase(op,size);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  changed_cycles=ea<8 && cc>=2;
  OpStart(op,ea,0,changed_cycles); Cycles=8;
  if (ea<8) Cycles=4;

  switch (cc)
  {
    case 0x00: // T
      ot("  mov r1,#0xff\n"); // size is always 0
      if (ea<8) Cycles+=2;
      break;
    case 0x01: // F
      ot("  mov r1,#0x00\n");
      break;
    default:
      ot("  mov r1,#0x00\n");
      cond=TestCond(cc);
      ot("  mov%s r1,#0xff\n",cond); // size is always 0
      if (ea<8) ot("  sub%s r5,r5,#2 ;@ Extra cycles\n",cond);
      break;
  }

  ot("\n");

  eawrite_check_addrerr=1;
  EaCalc (0,0x003f, ea,size,earwt_zero_extend);
  EaWrite(0,     1, ea,size,0x003f,earwt_zero_extend);

  opend_op_changes_cycles=changed_cycles;
  OpEnd(ea,0);
  return 0;
}

// Emit a Asr/Lsr/Roxr/Ror opcode
static int EmitAsr(int op,int type,int dir,int count,int size,int usereg,EaRWType eatype)
{
  char pct[13]=""; // count
  int wide=8<<size;
  int shift=32-wide;

  if (count>=1) sprintf(pct,"#%d",count); // Fixed count

  if (usereg)
  {
    ot(";@ Use Dn for count:\n");
#if HAVE_ARMv6T2
    // Offset shifted left by 2 has reduced latency on some newer CPUs
    ot("  ubfx r2,r8,#9,#3\n");
    ot("  ldr r2,[r7,r2,lsl #2]\n");
#else
    ot("  and r2,r8,#0x0e00\n");
    ot("  ldr r2,[r7,r2,lsr #7]\n");
#endif
    ot("  and r2,r2,#63\n");
    ot("\n");
    strcpy(pct,"r2");
  }
  else if (count<0)
  {
#if HAVE_ARMv6T2
    ot("  ubfx r2,r8,#9,#3 ;@ Get 'n'\n");
#else
    ot("  mov r2,r8,lsr #9 ;@ Get 'n'\n");
    ot("  and r2,r2,#7\n\n");
#endif
    strcpy(pct,"r2");
  }

  // Take 2*n cycles:
  if (count<0) ot("  sub r5,r5,r2,asl #1 ;@ Take 2*n cycles\n\n");
  else Cycles+=count<<1;

  if (type<2)
  {
    // Asr/Lsr
    int asl=(type==0&&dir);

    if (shift && (dir^(eatype==earwt_shifted_up))) {
      if (usereg||count<0||asl||(dir&&(count+shift==32))) {
        // register-based shifts, Asl, or Lsl by a total of 32 require pre-shift
        if (type==0) ot("  mov r0,r0,%s #%d\n",dir?"asl":"asr",shift);
        if (type==1) ot("  mov r0,r0,%s #%d\n",dir?"lsl":"lsr",shift);
      } else {
        // otherwise, combine the shift with the pre-shift
        sprintf(pct,"#%d",count+shift);
      }
    }

    if (!asl)
      ot("  adds r3,r3,#0 ;@ clear C and V, avoiding false register dependency on r0\n");
    else if (count!=1)
      ot("  adds r3,r0,#0 ;@ clear C and V, also save old value for V flag calculation\n");

    ot(";@ Shift register:\n");
    if (asl&&count==1)
      ot("  adds r0,r0,r0 ;@ includes V flag\n");
    else if (type==0)
      ot("  movs r0,r0,%s %s\n",dir?"asl":"asr",pct);
    else
      ot("  movs r0,r0,%s %s\n",dir?"lsl":"lsr",pct);

    OpGetFlags(0,!usereg);
    if (usereg) { // store X only if count is not 0
      ot("  cmp %s,#0 ;@ shifting by 0?\n",pct);
      ot("  strne r10,[r7,#0x4c] ;@ if not, Save X bit\n");
      if (type && dir==0 && size<2) {
        ot("  moveq r3,r0,lsr #%d\n",wide-1);
        ot("  orreq r10,r10,r3,lsl #31 ;@ if so, add missed N flag\n");
      }
    }
    ot("\n");

    if (asl&&count!=1) {
      ot(";@ calculate V flag (set if sign bit changes at anytime):\n");
      ot("  cmp r3,r0,asr %s\n", pct);
      ot("  orrne r10,r10,#0x10000000\n");
      ot("\n");
    }
  }

  // --------------------------------------
  if (type==2)
  {
    const char *sh_fwd=dir?"lsl":"lsr";
    const char *sh_rev=dir?"lsr":"lsl";
    char pct_rev[12]=""; // reverse count

    // Roxr
    if (count==8 && size==0) {
        count=1;
        dir^=1;
    }

    if(count == 1)
    {
      ot("  ldr r2,[r7,#0x4c] ;@ X bit\n");
      if(dir==0) {
        if(size!=2) {
          ot("  adds r0,r0,r0,%s #%i ;@ Clear V flag\n",eatype==earwt_shifted_up?"lsr":"lsl",shift);
          ot("  bic r0,r0,#0x%x\n", 1<<(32-wide));
        } else {
          ot("  adds r1,r1,#0 ;@ Clear V flag\n");
        }
        ot("  movs r2,r2,lsl #3 ;@ Get X bit into Carry\n");
        ot("  movs r0,r0,rrx\n");
        OpGetFlags(0,1);
      } else {
        if (size!=2) {
          if (eatype!=earwt_shifted_up)
            ot("  mov r0,r0,lsl #%i\n",shift);
          ot("  and r2,r2,#0x20000000 ;@ Isolate X bit\n");
          ot("  adds r0,r0,r2,lsr #29-%d ;@ Clear V flag\n",31-wide);
          ot("  movs r0,r0,lsl #1\n");
        } else {
          ot("  movs r2,r2,lsl #3 ;@ Get X bit into Carry\n");
          ot("  adcs r0,r0,r0\n");
        }
        OpGetFlags(0,1);
        if (size==2) ot("  bic r10,r10,#0x10000000 ;@ make sure V is clear\n");
      }
      if (size!=2 && eatype!=earwt_shifted_up)
        ot("  mov r0,r0,lsr #%d\n",shift);
      return 0;
    }

    if (usereg||count < 0)
      strcpy(pct_rev,"r1");
    else
      sprintf(pct_rev,"#%d",wide-count);

    ot("  ldr r3,[r7,#0x4c] ;@ X bit\n");

    if (usereg)
    {
      ot(";@ Reduce rotation amount modulo %d:\n",wide+1);
      if (size==2)
        ot("  subs r2,r2,#%d\n",wide+1);
      else
      {
        ot("  and r1,r2,#%d\n",wide-1);
        ot("  subs r2,r1,r2,lsr #%d\n",3+size);
      }
      ot("  addmi r2,r2,#%d ;@ Now r2=0-%d\n",wide+1,wide);
    }

    if (usereg||count < 0)
      ot("  rsbs r1,r2,#%d ;@ should also clear ARM V\n",wide);
    else if (!shift)
      ot("  adds r1,r1,#0 ;@ clear V flag\n");

    if (dir&&shift) ot("  mov r0,r0,lsl #%d ;@ shift value to upper bits\n",shift);
    ot("\n");

    ot(";@ Rotate bits:\n");
    ot("  movs r3,r3,lsl #3 ;@ Get X bit into Carry\n");
    if (dir) ot("  mov r3,r0,rrx ;@ Rotate X bit into reverse part\n");
    else     ot("  adc r3,r0,r0 ;@ Rotate X bit into reverse part, preserve V flag\n");

    if (shift) ot("  mov r0,r0,%s %s ;@ Shift forward part\n",sh_fwd,pct);
    else       ot("  movs r0,r0,%s %s ;@ Shift forward part, set C flag\n",sh_fwd,pct);

    if (shift) ot("  adds r0,r0,r3,%s %s ;@ Add reverse part, clear V flag\n",sh_rev,pct_rev);
    else       ot("  orrs r0,r0,r3,%s %s ;@ Orr reverse part, set flags\n",sh_rev,pct_rev);
    ot("\n");

    if (shift&& dir) ot("  movs r0,r0,asr #%d ;@ Shift down and get correct NC flags\n",shift);
    if (shift&&!dir) ot("  movs r1,r0,lsl #%d ;@ Shift up and get correct NC flags\n",shift);
    OpGetFlags(0,1);
    ot("\n");
  }

  // --------------------------------------
  if (type==3)
  {
    int flags_cleared=0;
    // Ror
    if (size<2)
    {
      ot(";@ Mirror value in whole 32 bits:\n");
      if (eatype==earwt_zero_extend) {
        if (size<=0) ot("  orr r0,r0,r0,lsl #8\n");
        if (size<=1) ot("  adds r0,r0,r0,lsl #16 ;@ first clear V and C\n");
        flags_cleared=1;
      } else { /*earwt_msb_dont_care*/
#if HAVE_ARMv6T2
        if (size<=0) ot("  bfi r0,r0,#8,#8\n");
        if (size<=1) ot("  bfi r0,r0,#16,#16\n");
#else
        ot("  mov r0,r0,lsl #%d\n",shift);
        if (size<=0) ot("  orr r0,r0,r0,lsr #8\n");
        if (size<=1) ot("  adds r0,r0,r0,lsr #16 ;@ first clear V and C\n");
        flags_cleared=1;
#endif
      }
      ot("\n");
    }

    ot(";@ Rotate register:\n");
    if (!dir && !flags_cleared)
      ot("  adds r1,r1,#0 ;@ first clear V and C\n"); // ARM does not clear C if rot count is 0
    if (count<0)
    {
      if (dir) {
        if (usereg) ot("  rsbs %s,%s,#0 ;@ clears V flag\n",pct,pct);
        else        ot("  rsb %s,%s,#33 ;@ rotate left by N-1, get carry for N\n",pct,pct);
      }
      ot("  movs r0,r0,ror %s\n",pct);
    }
    else
    {
      int ror=count;
      if (dir) ror=33-ror;
      if (ror&31) {
          ot("  movs r0,r0,ror #%d",ror);
          if (dir) ot(" ;@ rotate left by %d, get carry for %d",count-1,count);
          ot("\n");
      }
      else if (dir) ot("  cmn r0,r0 ;@ get carry for rotation left by 1\n");
    }

    if (dir && !usereg) ot("  adcs r0,r0,r0 ;@ rotate left by 1, setting flags\n");
    if (dir)
    {
      if (usereg)
      {
#if HAVE_ARMv6T2
        OpGetFlags(0,0);
        ot("  and r2,r0,r2,lsr #31 ;@ check non-zero rotation and bit 0 of result\n");
        ot("  bfi r10,r2,#29,#1 ;@ insert C flag\n");
#else
        OpGetFlagsNZ(0);
        ot("  tst r2,r0,lsl #31 ;@ check non-zero rotation and bit 0 of result\n");
        ot("  orrmi r10,r10,#0x20000000 ;@ set C flag\n");
#endif
      }
      else
      {
        OpGetFlags(0,0);
        ot("  bic r10,r10,#0x10000000 ;@ make sure V is clear\n");
      }
    }
    else
      OpGetFlags(0,0);
    ot("\n");

  }
  // --------------------------------------

  return 0;
}

// Emit a Asr/Lsr/Roxr/Ror opcode - 1110cccd xxuttnnn
// (ccc=count, d=direction(r,l) xx=size extension, u=use reg for count, tt=type, nnn=register Dn)
int OpAsr(int op)
{
  int ea=0,use=0;
  int count=0,dir=0;
  int size=0,usereg=0,type=0;
  EaRWType eatype=earwt_shifted_up;

  count =(op>>9)&7;
  dir   =(op>>8)&1;
  size  =(op>>6)&3;
  if (size>=3) return 1; // use OpAsrEa()
  usereg=(op>>5)&1;
  type  =(op>>3)&3;

  if (usereg==0) count=((count-1)&7)+1; // because ccc=000 means 8

  // Use the same opcode for target registers:
  use=op&~0x0007;

  // As long as count is not 8, use the same opcode for all shift counts:
  if (usereg==0 && count!=8 && !(count==1&&type==2)) { use|=0x0e00; count=-1; }
  if (usereg) { use&=~0x0e00; count=-1; } // Use same opcode for all Dn

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,ea,0,count<0); Cycles=size<2?6:8;

  // logical/arithmetic
  if (type<2) {
    if (dir==0) eatype=type?earwt_zero_extend:earwt_sign_extend;
    if (dir==1) eatype=type?earwt_msb_dont_care:earwt_shifted_up;
  }
  //Roxr/Roxl
  if (type==2)
    eatype=(dir^(size==0&&count==8))?earwt_msb_dont_care:earwt_zero_extend;
  //Ror/Rol
  if (type==3) eatype=earwt_zero_extend;

  EaCalcRead(11,     0, ea,size,0x0007,eatype);

  EmitAsr(op,type,dir,count, size,usereg,eatype);

  //Lsl/Asl result is shifted up
  if (type<2 && dir==1) eatype=earwt_shifted_up;
  //Ror/Rol result is mirrored across whole word
  if (type==3) eatype=earwt_msb_dont_care;

  EaWrite(11,    0, ea,size,0x0007,eatype);

  opend_op_changes_cycles = (count<0);
  OpEnd(ea,0);

  return 0;
}

// Asr/Lsr/Roxr/Ror etc EA - 11100ttd 11eeeeee
int OpAsrEa(int op)
{
  int use=0,type=0,dir=0,ea=0,size=1;
  EaRWType eatype=earwt_shifted_up;

  type=(op>>9)&3;
  dir =(op>>8)&1;
  ea  = op&0x3f;

  if (ea<0x10) return 1;
  // See if we can do this opcode:
  if (EaCanRead(ea,0)==0) return 1;
  if (EaCanWrite(ea)==0) return 1;

  use=OpBase(op,size);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op,ea); Cycles=6; // EmitAsr() will add 2

  // logical/arithmetic
  if (type<2) {
    if (dir==0) eatype=earwt_shifted_up;
    if (dir==1) eatype=type?earwt_msb_dont_care:earwt_shifted_up;
  }
  //Ror/Rol
  if (type==3) eatype=earwt_msb_dont_care;

  EaCalcRead(11,     0,ea,size,0x003f,eatype);

  EmitAsr(op,type,dir,1,size,0,eatype);

  //Lsr/Asr results are zero/sign extended
  if (type<2 && dir==0) eatype=type?earwt_zero_extend:earwt_sign_extend;
  //Lsl/Asl result is shifted up
  if (type<2 && dir==1) eatype=earwt_shifted_up;
  //Ror/Rol result is mirrored across whole word
  if (type==3) eatype=earwt_shifted_up;

  EaWrite(11,     0,ea,size,0x003f,eatype);

  OpEnd(ea);
  return 0;
}

int OpTas(int op, int gen_special)
{
  int ea=0;
  int use=0;

  ea=op&0x003f;

  // See if we can do this opcode:
  if (EaCanWrite(ea)==0 || EaAn(ea)) return 1;

  use=OpBase(op,0);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  if (!gen_special) OpStart(op,ea);
  else
    ot("Op%.4x_%s\n", op, ms?"":":");

  Cycles=4;
  if(ea>=8) Cycles+=6;

  EaCalc (11,0x003f,ea,0,earwt_shifted_up);
  EaRead (11,     1,ea,0,0x003f,earwt_shifted_up,1);

  OpGetFlagsNZ(1);
  ot("\n");

#if CYCLONE_FOR_GENESIS
  // the original Sega hardware ignores write-back phase (to memory only)
  if (ea < 0x10 || gen_special) {
#endif
    ot("  orr r1,r1,#0x80000000 ;@ set bit7\n");

    EaWrite(11,   1,ea,0,0x003f,earwt_shifted_up);
#if CYCLONE_FOR_GENESIS
  }
#endif

  OpEnd(ea);

#if (CYCLONE_FOR_GENESIS == 2)
  if (!gen_special && ea >= 0x10) {
    OpTas(op, 1);
  }
#endif

  return 0;
}


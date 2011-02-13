
// This file is part of the Cyclone 68000 Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "app.h"

static FILE *AsmFile=NULL;

static int CycloneVer=0x0069; // Version number of library
int *CyJump=NULL; // Jump table
int ms=0; // If non-zero, output in Microsoft ARMASM format
char *Narm[4]={ "b", "h","",""}; // Normal ARM Extensions for operand sizes 0,1,2
char *Sarm[4]={"sb","sh","",""}; // Sign-extend ARM Extensions for operand sizes 0,1,2
int Cycles=0; // Current cycles for opcode
int Amatch=1; // If one, try to match A68K timing
int Accu=-1; // Accuracy
int Debug=0; // Debug info

void ot(const char *format, ...)
{
  va_list valist;
  va_start(valist,format);
  if (AsmFile) vfprintf(AsmFile,format,valist);
  va_end(valist);
}

void ltorg()
{
  if (ms) ot("  LTORG\n");
  else    ot("  .ltorg\n");
}

static void PrintException()
{
  ot("  ;@ Cause an Exception - Vector in [r7,#0x50]\n");
  ot("  ldr r10,[r7,#0x60] ;@ Get Memory base\n");
  ot("  sub r1,r4,r10 ;@ r1 = Old PC\n");
  OpPush32();
  OpPushSr(1);
  ot("  ldr r0,[r7,#0x50] ;@ Get Vector\n");
  ot(";@ Read IRQ Vector:\n");
  MemHandler(0,2);
  ot("  add r0,r0,r10 ;@ r0 = Memory Base + New PC\n");
  ot("  mov lr,pc\n");
  ot("  ldr pc,[r7,#0x64] ;@ Call checkpc()\n");
  ot("  mov r4,r0\n");
  ot("\n");

  // todo - make Interrupt code use this function as well
}

// Trashes r0
void CheckInterrupt()
{
  ot(";@ CheckInterrupt:\n");
  ot("  ldrb r0,[r7,#0x47] ;@ Get IRQ level\n");
  ot("  tst r0,r0\n");
  ot("  blne DoInterrupt\n");
  ot("\n");
}

static void PrintFramework()
{
  ot(";@ --------------------------- Framework --------------------------\n");
  if (ms) ot("CycloneRun\n");
  else    ot("CycloneRun:\n");

  ot("  stmdb sp!,{r4-r11,lr}\n");

  ot("  mov r7,r0          ;@ r7 = Pointer to Cpu Context\n");
  ot("                     ;@ r0-3 = Temporary registers\n");
  ot("  ldrb r9,[r7,#0x46] ;@ r9 = Flags (NZCV)\n");
  ot("  ldr r6,=JumpTab    ;@ r6 = Opcode Jump table\n");
  ot("  ldr r5,[r7,#0x5c]  ;@ r5 = Cycles\n");
  ot("  ldr r4,[r7,#0x40]  ;@ r4 = Current PC + Memory Base\n");
  ot("                     ;@ r8 = Current Opcode\n");
  ot("  mov r9,r9,lsl #28  ;@ r9 = Flags 0xf0000000, cpsr format\n");
  ot("                     ;@ r10 = Source value / Memory Base\n");
  ot("\n");
  CheckInterrupt();
  ot(";@ Check if interrupt used up all the cycles:\n");
  ot("  subs r5,r5,#0\n");
  ot("  blt CycloneEndNoBack\n");

  OpFirst();
  ltorg();
  ot("\n");

  ot(";@ We come back here after execution\n");
  ot("CycloneEnd%s\n", ms?"":":");
  ot("  sub r4,r4,#2\n");
  ot("CycloneEndNoBack%s\n", ms?"":":");
  ot("  mov r9,r9,lsr #28\n");
  ot("  str r4,[r7,#0x40]  ;@ Save Current PC + Memory Base\n");
  ot("  str r5,[r7,#0x5c]  ;@ Save Cycles\n");
  ot("  strb r9,[r7,#0x46] ;@ Save Flags (NZCV)\n");
  ot("  ldmia sp!,{r4-r11,pc}\n");
  ot("\n");

  ot(";@ DoInterrupt - r0=IRQ number\n");
  ot("DoInterrupt%s\n", ms?"":":");
  ot("\n");
  ot("  ldrb r1,[r7,#0x44] ;@ Get SR high: T_S__III\n");
  ot("  and r1,r1,#7 ;@ Get interrupt mask\n");
  ot("  cmp r0,#6 ;@ irq>6 ?\n");
  ot("  cmple r0,r1 ;@ irq<=6: Is irq<=mask ?\n");
  ot("  movle pc,lr ;@ irq<=6 and mask, not allowed\n");
  ot("\n");
  ot("  ldr r10,[r7,#0x60] ;@ Get Memory base\n");
  ot("  mov r11,lr ;@ Preserve ARM return address\n");
  ot("  sub r1,r4,r10 ;@ r1 = Old PC\n");
  OpPush32();
  OpPushSr(1);
  ot(";@ Get IRQ Vector address:\n");
  ot("  ldrb r1,[r7,#0x47] ;@ IRQ\n");
  ot("  mov r0,r1,asl #2\n");
  ot("  add r0,r0,#0x60\n");
  ot(";@ Read IRQ Vector:\n");
  MemHandler(0,2);
  ot("  add r0,r0,r10 ;@ r0 = Memory Base + New PC\n");
  ot("  mov lr,pc\n");
  ot("  ldr pc,[r7,#0x64] ;@ Call checkpc()\n");
  ot("  mov r4,r0\n");
  ot("\n");
  ot(";@ todo - swap OSP and A7 if not in Supervisor mode\n");
  ot("  ldrb r0,[r7,#0x47] ;@ IRQ\n");
  ot("  orr r0,r0,#0x20 ;@ Supervisor mode + IRQ number\n");
  ot("  strb r0,[r7,#0x44] ;@ Put SR high\n");
  ot("\n");
  ot(";@ Clear irq:\n");
  ot("  mov r0,#0\n");
  ot("  strb r0,[r7,#0x47]\n");
  ot("  subs r5,r5,#%d ;@ Subtract cycles\n",46);
  ot("  mov pc,r11 ;@ Return\n");
  ot("\n");

  ot("Exception%s\n", ms?"":":");
  ot("\n");
  ot("  mov r11,lr ;@ Preserve ARM return address\n");
  PrintException();
  ot("  mov pc,r11 ;@ Return\n");
  ot("\n");
}

// ---------------------------------------------------------------------------
// Call Read(r0), Write(r0,r1) or Fetch(r0)
// Trashes r0-r3
int MemHandler(int type,int size)
{
  int func=0;
  func=0x68+type*0xc+(size<<2); // Find correct offset

  if (Debug&4) ot("  str r4,[r7,#0x40] ;@ Save PC\n");
  if (Debug&3) ot("  str r5,[r7,#0x5c] ;@ Save Cycles\n");

  ot("  mov lr,pc\n");
  ot("  ldr pc,[r7,#0x%x] ;@ Call ",func);

  // Document what we are calling:
  if (type==0) ot("read");
  if (type==1) ot("write");
  if (type==2) ot("fetch");

  if (type==1) ot("%d(r0,r1)",8<<size);
  else         ot("%d(r0)",   8<<size);
  ot(" handler\n");

  if (Debug&2) ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
  return 0;
}

static void PrintOpcodes()
{
  int op=0;
 
  printf("Creating Opcodes: [");

  ot(";@ ---------------------------- Opcodes ---------------------------\n");

  // Emit null opcode:
  ot("Op____%s ;@ Called if an opcode is not recognised\n", ms?"":":");
  OpStart(-1); Cycles=4; OpEnd(); //test

  ot("  b CycloneEnd\n\n");

  for (op=0;op<0x10000;op++)
  {
    if ((op&0xfff)==0) { printf("%x",op>>12); fflush(stdout); } // Update progress

    OpAny(op);
  }

  ot("\n");

  printf("]\n");
}

static void PrintJumpTable()
{
  int i=0,op=0,len=0;

  ot(";@ -------------------------- Jump Table --------------------------\n");
  ot("JumpTab%s\n", ms?"":":");

  len=0xfffe; // Hmmm, armasm 2.50.8684 messes up with a 0x10000 long jump table
  for (i=0;i<len;i++)
  {
    op=CyJump[i];

    if ((i&7)==0) ot(ms?"  dcd ":"  .long ");
    if (op<0) ot("Op____"); else ot("Op%.4x",op);
    
    if ((i&7)==7) ot(" ;@ %.4x\n",i-7);
    else if (i+1<len) ot(",");
  }

  ot("\n");
}

static int CycloneMake()
{
  char *name="Cyclone.s";
  
  // Open the assembly file
  if (ms) name="Cyclone.asm";
  AsmFile=fopen(name,"wt"); if (AsmFile==NULL) return 1;
  
  printf("Making %s...\n",name);

  ot("\n;@ Cyclone 68000 Emulator v%x.%.3x - Assembler Output\n\n",CycloneVer>>12,CycloneVer&0xfff);

  ot(";@ Copyright (c) 2011 FinalDave (emudave (at) gmail.com)\n\n");

  ot(";@ This code is licensed under the GNU General Public License version 2.0 and the MAME License.\n");
  ot(";@ You can choose the license that has the most advantages for you.\n\n");
  ot(";@ SVN repository can be found at http://code.google.com/p/cyclone68000/\n\n");

  CyJump=(int *)malloc(0x40000); if (CyJump==NULL) return 1;
  memset(CyJump,0xff,0x40000); // Init to -1

  if (ms)
  {
    ot("  area |.text|, code\n");
    ot("  export CycloneRun\n");
    ot("  export CycloneVer\n");
    ot("\n");
    ot("CycloneVer dcd 0x%.4x\n",CycloneVer);
  }
  else
  {
    ot("  .global CycloneRun\n");
    ot("  .global CycloneVer\n");
    ot("CycloneVer: .long 0x%.4x\n",CycloneVer);
  }
  ot("\n");

  PrintFramework();
  PrintOpcodes();
  PrintJumpTable();

  if (ms) ot("  END\n");

  fclose(AsmFile); AsmFile=NULL;

  printf("Assembling...\n");
  // Assemble the file
  if (ms) system("armasm Cyclone.asm");
  else    system("as -o Cyclone.o Cyclone.s");
  printf("Done!\n\n");

  free(CyJump);
  return 0;
}

int main()
{
  printf("\n  Cyclone 68000 Emulator v%x.%.3x - Core Creator\n\n",CycloneVer>>12,CycloneVer&0xfff);

  // Make GAS and ARMASM versions
  for (ms=0;ms<2;ms++) CycloneMake();
  return 0;
}

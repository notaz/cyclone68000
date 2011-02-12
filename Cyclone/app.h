
// This file is part of the Cyclone 68000 Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// Disa.c
#include "../Pico/Disa.h"

// Ea.cpp
int EaCalc(int a,int mask,int ea,int size);
int EaRead(int a,int v,int ea,int size,int top=0);
int EaCanRead(int ea,int size);
int EaWrite(int a,int v,int ea,int size,int top=0);
int EaCanWrite(int ea);

// Main.cpp
extern int *CyJump; // Jump table
extern int ms; // If non-zero, output in Microsoft ARMASM format
extern char *Narm[4]; // Normal ARM Extensions for operand sizes 0,1,2
extern char *Sarm[4]; // Sign-extend ARM Extensions for operand sizes 0,1,2
extern int Cycles; // Current cycles for opcode
extern int Amatch; // If one, try to match A68K timing
extern int Accu; // Accuracy
extern int Debug; // Debug info
void ot(char *format, ...);
void ltorg();
void CheckInterrupt();
int MemHandler(int type,int size);

// OpAny.cpp
int OpGetFlags(int subtract,int xbit);
void OpUse(int op,int use);
void OpFirst();
void OpStart(int op);
void OpEnd();
int OpBase(int op);
void OpAny(int op);

//----------------------
// OpArith.cpp
int OpArith(int op);
int OpLea(int op);
int OpAddq(int op);
int OpArithReg(int op);
int OpMul(int op);
int OpAbcd(int op);
int OpAritha(int op);
int OpAddx(int op);
int OpCmpEor(int op);

// OpBranch.cpp
void OpPush32();
void OpPushSr(int high);
int OpTrap(int op);
int OpLink(int op);
int OpUnlk(int op);
int Op4E70(int op);
int OpJsr(int op);
int OpBranch(int op);
int OpDbra(int op);

// OpLogic.cpp
int OpBtstReg(int op);
int OpBtstImm(int op);
int OpNeg(int op);
int OpSwap(int op);
int OpTst(int op);
int OpExt(int op);
int OpSet(int op);
int OpAsr(int op);
int OpAsrEa(int op);

// OpMove.cpp
int OpMove(int op);
int OpLea(int op);
void OpFlagsToReg(int high);
void OpRegToFlags(int high);
int OpMoveSr(int op);
int OpArithSr(int op);
int OpPea(int op);
int OpMovem(int op);
int OpMoveq(int op);
int OpMoveUsp(int op);
int OpExg(int op);

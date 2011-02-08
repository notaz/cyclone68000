
// -------------------- Pico Library --------------------

// Pico Megadrive Emulator Library - Header File

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#ifdef __cplusplus
extern "C" {
#endif

// Pico.cpp
extern int PicoVer;
extern int PicoOpt;
int PicoInit();
void PicoExit();
int PicoReset();
int PicoFrame();
extern int PicoPad[2]; // Joypads, format is SACB RLDU
extern int (*PicoCram)(int cram); // Callback to convert colour ram 0000bbb0 ggg0rrr0

// Area.cpp
struct PicoArea { void *data; int len; char *name; };
extern int (*PicoAcb)(struct PicoArea *); // Area callback for each block of memory
extern FILE *PmovFile;
extern int PmovAction;
// &1=for reading &2=for writing &4=volatile &8=non-volatile
int PicoAreaScan(int action,int *pmin);
// Save or load the state from PmovFile:
int PmovState();
int PmovUpdate();

// Cart.cpp
int PicoCartLoad(FILE *f,unsigned char **prom,unsigned int *psize);
int PicoCartInsert(unsigned char *rom,unsigned int romsize);

// Draw.cpp
extern int (*PicoScan)(unsigned int num,unsigned short *data);
extern int PicoMask; // Mask of which layers to draw

// Sek.cpp
extern char PicoStatus[];

// Sound.cpp
extern int PsndRate,PsndLen;
extern short *PsndOut;
extern unsigned char PicoSreg[];

// Utils.cpp
extern int PicuAnd;
int PicuQuick(unsigned short *dest,unsigned short *src);
int PicuShrink(unsigned short *dest,int destLen,unsigned short *src,int srcLen);
int PicuMerge(unsigned short *dest,int destLen,unsigned short *src,int srcLen);

#ifdef __cplusplus
} // End of extern "C"
#endif

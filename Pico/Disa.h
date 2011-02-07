
// Dave's Disa 68000 Disassembler

#ifdef __cplusplus
extern "C" {
#endif

#define CPU_CALL

extern unsigned int DisaPc;
extern char *DisaText; // Text buffer to write in

extern unsigned short (CPU_CALL *DisaWord)(unsigned int a);
int DisaGetEa(char *t,int ea,int size);

int DisaGet();

#ifdef __cplusplus
} // End of extern "C"
#endif


// Make a Sine table

#pragma warning (disable:4514)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PI 3.14159265358979

int main()
{
  int i=0;

  printf ("\nshort Sine[0x100]=\n");
  printf ("{\n");

  for (i=0;i<0x100;i++)
  {
    double fAng,fPos;
    int nPos;
    if ((i&7)==0) printf ("  ");
    
    fAng=(double)i/(double)0x100;
    fAng*=2*PI;
    fPos=sin(fAng)*(double)0x4000;
    nPos=(int)fPos;
    printf ("%+6d,",nPos);
    
    if ((i&7)==7) printf ("\n");
  }

  printf ("};\n");

  return 0;
}

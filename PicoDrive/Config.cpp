
#include "stdafx.h"

static char *ConfigName="\\PicoConfig.txt";
struct Config Config;

int ConfigInit()
{
  memset(&Config,0,sizeof(Config));

  Config.key[0]=VK_UP;
  Config.key[1]=VK_DOWN;
  Config.key[2]=VK_LEFT;
  Config.key[3]=VK_RIGHT;
  Config.key[4]=GXKey.vkC; // A
  Config.key[5]=GXKey.vkA; // B
  Config.key[6]=GXKey.vkB; // C
  Config.key[7]=GXKey.vkStart;

  return 0;
}

int ConfigSave()
{
  FILE *f=NULL;
  int i=0,max=0;

  // Open config file:
  f=fopen(ConfigName,"wt"); if (f==NULL) return 1;

  fprintf(f,"// PicoDrive Config File\n\n");

  fprintf(f,"// Keys: Up Down Left Right\n");
  fprintf(f,"// A B C Start\n\n");

  max=sizeof(Config.key)/sizeof(Config.key[0]);
  for (i=0;i<max;i++)
  {
    fprintf(f,"key,%d,0x%.2x\n",i,Config.key[i]);
  }

  fclose(f);

  return 0;
}

int ConfigLoad()
{
  FILE *f=NULL;
  char line[256];
  int i=0;

  memset(line,0,sizeof(line));

  // Open config file:
  f=fopen(ConfigName,"rt"); if (f==NULL) return 1;

  // Read through each line of the config file
  for (;;)
  {
    char *tok[3]={"","",""};
    if (fgets(line,sizeof(line)-1,f)==NULL) break;

    // Split line into tokens:
    for (i=0;i<3;i++)
    {
      tok[i]=strtok(i<1?line:NULL, ",\r\n");
      if (tok[i]==NULL) { tok[i]=""; break; }
    }

    if (_stricmp(tok[0],"key")==0)
    {
      // Key code
      int ta=0,tb=0;
      
      ta=strtol(tok[1],NULL,0);
      tb=strtol(tok[2],NULL,0);

      Config.key[ta&7]=tb;
    }
  }

  fclose(f);

  return 0;
}

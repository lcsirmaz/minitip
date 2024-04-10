/* yesno.c */

/***********************************************************************
* This code is part of MINITIP (a MINimal Information Theoretic Prover)
*
* Copyright (2016-2024) Laszlo Csirmaz, github.com/lcsirmaz/minitip
*
* This program is free, open-source software. You may redistribute it
* and/or modify under the terms of the GNU General Public License (GPL).
*
* There is ABSOLUTELY NO WARRANTY, use at your own risk.
*************************************************************************/


#include <readline/readline.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

/***********************************************************************
* Reading a single y/n answer from the terminal
*
*  int yesno(int force)
*    read a yes/no answer  If force!=0 insist on getting one of them
*  return value: 1 - yes; 0 - no.
*/

#define UNUSED __attribute__((unused))

/* readline completion for yes/no */
static char* yn_comp(const char *txt, int state)
{static int list_index=0; const char *name;
 static const char *yn[]={"yes","no",NULL};
    if(!state){ list_index=0; }
    while((name=yn[list_index])!=NULL){
       list_index++;
       if(strncasecmp(name,txt,strlen(txt))==0){
          return strdup(name);
       }
    }
    return (char*)NULL;
}
/* readline completion procedure */
static char **yesno_completion(const char *text, int start, UNUSED int end)
{char **matches=NULL;
  rl_attempted_completion_over=1; /* no filename extension */
  if(start==0){ matches=rl_completion_matches(text,yn_comp); }
  return matches;
}

/* first prompt: argument, next: enter y/n */
int yesno(int force,const char *fmt, ...)
{rl_completion_func_t *ocomp;
 char *line; int res=-1; char buff[100]; va_list ap;
 char *prompt;
    va_start(ap,fmt); vsnprintf(buff,90,fmt,ap); va_end(ap);
    strcat(buff," (y/n)? "); prompt=&buff[0];
    ocomp=rl_attempted_completion_function;
    rl_attempted_completion_function=yesno_completion;
    while(res<0){
        line=readline(prompt);
        prompt="Please enter 'y' or 'n' (y/n)? ";
        if(!line){printf("\n"); continue;}
        if(strlen(line)<5){
           if(line[0]=='y'||line[0]=='Y'){ res=1; }
           else if(line[0]=='n'||line[0]=='N'){ res=0; }
           else if(force==0){
              res=0; printf(" no\n");
           }
        }
        free(line);
    }
    rl_attempted_completion_function=ocomp;
    return res;
}

/* EOF */


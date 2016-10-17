/* yesno.c */

/***********************************************************************
* This code is part of MINITIP (a MINimal Information Theoretic Prover)
*
* Copyright (2016) Laszlo Csirmaz, Central European University, Budapest
*
* This program is free, open-source software. You may redistribute it
* and/or modify under the terms of the GNU General Public License (GPL).
*
* There is ABSOLUTELY NO WARRANTY, use at your own risk.
*************************************************************************/


#include <termios.h>
#include <stdio.h>
#include <unistd.h>

/***********************************************************************
* Reading a single y/n answer from the terminal
*
*  int yesno(void)
*    read a single character from the terminal and return it.
*/

static void set_mode(int getkey)
{static struct termios old,new;
    if(!getkey){
       tcsetattr(STDIN_FILENO,TCSANOW,&old);
       return;
    }
    tcgetattr(STDIN_FILENO,&old);
    new=old;
    new.c_lflag &= ~(ICANON);
    tcsetattr(STDIN_FILENO,TCSANOW,&new);
}

int yesno(void)
{int c;
    set_mode(1);
    fflush(stdout);
    while((c=getchar())<0);
    set_mode(0);
    printf("\n");
    return c;
}

/* EOF */


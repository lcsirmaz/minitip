/* yesno.c */

#include <termios.h>
#include <stdio.h>
#include <unistd.h>

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


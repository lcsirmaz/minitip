/* minitip.c (main file) */

/***********************************************************************
 * This code is part of MINITIP (a MINimal Information Theoretic Prover)
 *
 * Copyright (2016) Laszlo Csirmaz, Central European University, Budapest
 *
 * This program is free, open-source software. You may redistribute it
 * and/or modify under the terms of the GNU General Public License (GPL).
 *
 * There is ABSOLUTELY NO WARRANTY, use at your own risk.
 ************************************************************************/

/* Version and copyright */
#define VERSION_MAJOR	1
#define VERSION_MINOR	1
#define VERSION_SUB	3
#define VERSION_STRING	mkstringof(VERSION_MAJOR.VERSION_MINOR.VERSION_SUB)

#define COPYRIGHT	\
"Copyright (C) 2016 Laszlo Csirmaz, Central European University, Budapest"

/*----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minitip.h"
#include "parser.h"
#include "xassert.h"

/* forward declarations */
static void check_expression(const char *src, int with_constraints);
extern int yesno(void);
#define UNUSED __attribute__((unused)) 

/***********************************************************************
* Auxiliary variables
*
*  char *HISTORY_FILE
*     read from and save history to this file.
*  int done
*     has value zero until the "quit" command is encountered, which set
*     this variable to 1
*  syntax_style_t minitip_style
*     the actual style to parse expressions
*  char minitip_sepchar
*     the actual separator character
*  int cmdarg_position
*     store the position where the parsed string starts. When the parser
*     returns an error, the position is relative to the start of the string,
*     this variable is used to offset the starting position.
*/
static char *HISTORY_FILE = DEFAULT_HISTORY_FILE;
static int done=0;
static syntax_style_t minitip_style=minitip_INITIAL_STYLE;
static char minitip_sepchar=minitip_INITIAL_SEPCHAR;
static int cmdarg_position;

/***********************************************************************
* Batch processing
*  int batch_depth
*     recursion depth for batch files
*  char batch_line[minitip_MAX_LINE_LENGTH]
*     buffer to hold the next line in a batch file
*/
static int batch_depth = 0;
static char batch_line[minitip_MAX_LINE_LENGTH+5];

/***********************************************************************
* Constraints
*  int constraint_no
*     the number of active constraints
*  char *constraint_table[max_constraints]
*     the original form of the constraint. It is parsed freshly when
*     the constraint is required, as it contains variable names.
*/
#define max_constraints	minitip_MAX_CONSTR
static int constraint_no=0;
char *constraint_table[max_constraints];

/***********************************************************************
*  int cmp_s(char *s2, char *s1)
*   return 0 when s2 starts with s1 followed by a space or a tab; or
*     when s2 and s1 are equal; otherwise return 1.
*
*  int cmp_ns(char *s2, char *s1, int n)
*   return 1 when s2 starts with s1 followed by spaces and tabs up to
*     position n; otherwise return 0.
*/
static int cmp_s(const char*s2,const char*s1)
{   while(*s1 && *s1==*s2){s1++;s2++;}
    return (*s2==0 || *s2==' '|| *s2=='\t') ? 0 : 1;
}
static int cmp_ns(const char *s2, const char *s1, int n)
{   while(n>0 && *s1 && *s1==*s2){s1++;s2++;n--;}
    if(*s1) return 0; /* not an initial segment */
    while(n>0){
        if(*s2!=' ' && *s2!='\t') return 0;
        n--; s2++;
    }
    return 1;
}
/***********************************************************************
*  char *prepare_filename(char *rawstring)
*    create another string from the argument:
*    - replace heading ~/ by the value of the HOME variable
*    - if enclosed by ' or ", escape \' and \"
*    - delete leading and trailing spaces
*    return NULL if there was an error, otherwise use strdup()
*/
#define MAX_PATH_LENGTH	260
static char *prepare_filename(const char *raw)
{int i; char last; char buff[MAX_PATH_LENGTH+10];
    while(*raw==' ') raw++; /* skip leading spaces */
    last=0;
    if(*raw=='\''|| *raw=='"'){ last=*raw; raw++; }
    i=0;
    if(raw[0]=='~' && raw[1]=='/'){ /* home directory */
        char *home=getenv("HOME");
        if(home==NULL) return NULL; /* sorry, no home directory */
        strncpy(buff,home,MAX_PATH_LENGTH);buff[MAX_PATH_LENGTH]=0;
        i=strlen(buff); raw++;
    }
    while(*raw && *raw!=last){ /* unescape */
        int spaces;
        for(spaces=0; *raw==' '; spaces++,raw++);
        if(*raw==0) break;
        while(spaces>0){
            buff[i]=' '; if(i<MAX_PATH_LENGTH)i++; spaces--; 
        }
        if(last && *raw=='\\' && raw[1]==last) raw++;
        buff[i]=*raw; raw++;
        if(i<MAX_PATH_LENGTH) i++;
    }
    if(*raw!=last) return NULL; /* incorrectly escaped */
    buff[i]=0;
    if(i==0 || buff[i-1]=='/') return NULL; /* empty or ends with / */
    if(strlen(buff)>=MAX_PATH_LENGTH) return NULL;
    return strdup(buff);
}
/***********************************************************************
*  int not_regular_file(char *path)
*    check if the path is a regular file
*    return values: 1: not a regular file, 0: it is
*/
#include <sys/stat.h>
inline static int not_regular_file(const char *path)
{struct stat buf;
    return stat(path,&buf) || ! S_ISREG(buf.st_mode);
}

/***********************************************************************
*  int read_batch_line(FILE *f)
*     read the next line from f to batch_line[], ignoring 0, \n, \r;
*      use \b as backspace erasing until the beginning
*      replace \t by a space
*     return: -1: no more input; 1: line too long; 0: line read and
*      stored in batch_line[]
*/
static int read_batch_line(FILE *f)
{int pos, c;
    pos=0;
    while((c=getc(f))>=0) switch(c){
     case 0: case '\r': break;
     case '\b':   if(pos>0) pos--; break;
     case '\n':   batch_line[pos]=0; return 0;
     case '\t':   c=' '; /* and fall through */
     default:     batch_line[pos]=c; 
                  if(pos<minitip_MAX_LINE_LENGTH) pos++;
                  else return 1; /* too long line */
    }
    batch_line[pos]=0;
    return (c<0 && pos==0) ? -1 : 0; /* EOF or OK */
}

/***********************************************************************
*  readline and history interface */
#include <readline/readline.h>
#include <readline/history.h>

/***********************************************************************
* void store_if_not_new(char *line)
*
*   add line to the history only if it has at least 3 characters.
*   if the line is among the last 5 in the history, then before
*   storing, delete the last occurrence.
*/
static void store_if_not_new(const char *line)
{int i,total; HIST_ENTRY **hl;
    if(strlen(line)<3) return;
    /* do not store 'quit' */
    if(strncasecmp(line,"quit",4)==0) return;
    hl=history_list();
    if(hl){
      for(total=0;hl[total]!=NULL; total++);
      for(i=0; total>0 && i<5; i++){
        total--;
        if(hl[total] && strcmp(line,hl[total]->line)==0){
            remove_history(total); total=0;
        }
      }
    }
    add_history(line);
}

/***********************************************************************
*  struct COMMAND commands[]
*   strucutre containing available commands. An entry has the fields:
*    name        command name
*    func        function to be called which executes the command
*    pfile       0/1, whether allow filename expansion in the argument
*    pcomp       NULL or the function doing parameter completion
*    doc         short help for the command
*/

/* forward declarations: functions which execute the command */
typedef int com_func_t(const char*arg,const char*line);

static com_func_t 
  com_quit,	/* quit */
  com_help,	/* help */
  com_check,	/* check */
  com_add,	/* add restriction */
  com_list,	/* list restriction */
  com_del,	/* delete restriction */
  com_style,	/* style: change / help*/
  com_syntax,	/* formula syntax help */
  com_nocon,	/* check without constraints */
  com_diff,	/* difference of two expressions */
  com_about,	/* print license info */
  com_macro,	/* handle macros: del, list, define */
  com_batch;	/* execute commands from a file */

/* forward declarations: parameter completion */
static rl_compentry_func_t
  cmd_name,	/* help */
  pm_syntax,	/* syntax */
  pm_style,	/* style */
  pm_list,	/* list */
  pm_help,	/* offer 'help' */
  pm_macro;	/* macro */

/* the COMMAND structure */
typedef struct {
    char *name;         /* command string */
    com_func_t *func;   /* function which executes the command */
    int pfile;          /* allow filename completion in argument position */
    rl_compentry_func_t
        *pcomp;         /* argument completion function */
    char *doc;          /* short document */
} COMMAND;

static COMMAND commands[] = {
/* name     func     pfile pcomp        doc              */
{"quit",    com_quit,  0,  NULL,	"quit minitip" },
{"help",    com_help,  0,  cmd_name,	"display this text" },
{"?",       com_help,  0,  cmd_name,	"synonym for 'help'" },
{"check",   com_check, 0,  pm_help,	"check inequality with constraints" },
{"test",    com_check, 0,  pm_help,	"synonym for 'check'" },
{"xcheck",  com_nocon, 0,  pm_help,	"check without constraints" },
{"add",	    com_add,   0,  pm_help,	"add new constraint" },
{"list",    com_list,  0,  pm_list,	"list all or specified constraints: 3,5-7"},
{"del",	    com_del,   0,  pm_help,	"delete numbered constraint"},
{"zap",	    com_diff,  0,  pm_help,	"print missing entropy terms on RHS"},
{"macro",   com_macro, 0,  pm_macro,	"add, list, delete macros"},
{"run",     com_batch, 1,  NULL,	"execute commands from a file"},
{"style",   com_style, 0,  pm_style,	"show / change formula style"},
{"syntax",  com_syntax,0,  pm_syntax,	"how to enter entropy formulas"},
{"about",   com_about, 0,  NULL,	"history, license, author, etc"},
{NULL,      NULL,      0,  NULL,	NULL }
};

/***********************************************************************
* readline command and parameter completion
*
*  char *cmd_name(char *text, int state)
*    expands text to one of the command names in commands[]
*
*  char *pm_syntax(char *text, int state)
*     expands text to a "syntax" command argument
*
*  char *pm_style(char *text, int state)
*     expands text to a "style" command argument
*
*  char *pm_list(char *text, int state)
*     expands text to a "list" command argument
*
*  char *pm_macro(char *text, int state)
*     expands text to a "macro" command argument
*
*  char **cmd_completion(char *text, int start, int end)
*     the completion procedure: at the beginning (start==0) expands
*     to command word; otherwise check which command is issued, and
*     expand to an argument of that command
*/

/* returns the next matching command text*/
static char *cmd_name(const char*txt, int state)
{static int list_index=0; char *name;
    if(!state){ list_index=0; }
    while((name=commands[list_index].name)!=NULL){
         list_index++;
         if(strncmp(name,txt,strlen(txt))==0){
              return strdup(name);
         }
    }
    return (char*)NULL;
}
/* parameter completion for different commands */
static char *pm_syntax(const char *txt, int state)
{static int list_index=0; const char *name;
 static const char *syntaxargs[]={
  "style","variables","entropy","relation","zap","constraint","macro",NULL };
    if(!state){ list_index=0; }
    while((name=syntaxargs[list_index])!=NULL){
        list_index++;
        if(strncmp(name,txt,strlen(txt))==0){
            return strdup(name);
        }
    }
    return (char*)NULL;
}
static char *pm_style(const char *txt, int state)
{static int list_index=0; const char *name;
 static const char *styleargs[]={"simple","full","help",NULL };
    if(!state){ list_index=0; }
    while((name=styleargs[list_index])!=NULL){
        list_index++;
        if(strncmp(name,txt,strlen(txt))==0){
            return strdup(name);
        }
    }
    return (char*)NULL;
}
static char *pm_list( const char *txt, int state)
{static int list_index=0; const char *name;
 static const char *listargs[]={"1-10","all","help",NULL };
    if(!state){ list_index=0; }
    while((name=listargs[list_index])!=NULL){
        list_index++;
        if(strncmp(name,txt,strlen(txt))==0){
            return strdup(name);
        }
    }
    return (char*)NULL;
}
static char *pm_help(const char *txt, int state)
{static int list_index=0; const char *name;
 static const char *macroargs[]={"help",NULL };
    if(!state){ list_index=0; }
    while((name=macroargs[list_index])!=NULL){
        list_index++;
        if(strncmp(name,txt,strlen(txt))==0){
            return strdup(name);
        }
    }
    return (char*)NULL;

}
static char *pm_macro( const char *txt, int state)
{static int list_index=0; const char *name;
 static const char *macroargs[]={"add","list","delete","help",NULL };
    if(!state){ list_index=0; }
    while((name=macroargs[list_index])!=NULL){
        list_index++;
        if(strncmp(name,txt,strlen(txt))==0){
            return strdup(name);
        }
    }
    return (char*)NULL;
}
/* the readline completion procedure */
static char **cmd_completion(const char*text, int start, UNUSED int end)
{char **matches=NULL;
  rl_attempted_completion_over=1; /* that's all folks */
  if(start==0){
      matches=rl_completion_matches(text,cmd_name);
  } else { // check that this is the first argument after the command
      int j,cmd;
      for(j=0,cmd=-1;cmd<0 && commands[j].name;j++){
          if(cmp_ns(rl_line_buffer,commands[j].name,start)) cmd=j;
      }
      if(cmd>=0){
          if(commands[cmd].pfile) rl_attempted_completion_over=0;
          if(commands[cmd].pcomp)
             matches=rl_completion_matches(text,commands[cmd].pcomp);
      }
  }
  return matches;
}

/***********************************************************************
* void initialize_readline(void)
*
*  initialize the readline library: define the name, history file,
*  and supply the completion algorithm.
*/
static void initialize_readline(void)
{
  rl_readline_name="minitip";
  read_history(HISTORY_FILE);
  using_history();
  rl_attempted_completion_function = cmd_completion;
}

/***********************************************************************
* int execute_cmd(char *text, char *line)
*
*   execute the command given in 'text'
*   line==NULL for interactive usage; otherwise it contains the
*      original line to be printed in case of error
*   return value: 0  -- batch file can continue (no error) 
*       1  -- batch file should be aborted (some unrecoverable error)
*/
static int execute_cmd(char *text,const char *line)
{int i,j,len,cmd;
    xassert(text!=NULL);
    for(i=0;text[i]==' '|| text[i]=='\t';i++);
    if(text[i]==0 || (line && text[i]=='#')) // empty or comment line
        return 0;
    // remove white spaces from the end
    for(j=strlen(text)-1;j>=0 && (text[j]==' '|| text[j]=='\t');j--) text[j]=0;
    cmd=-1; //find the first match
    for(j=0;cmd<0 && commands[j].name; j++){
        len=strlen(commands[j].name);
        if(strncmp(text+i,commands[j].name,len)==0 && 
            (text[i+len]==0 || text[i+len]==' '||text[i+len]=='\t') ) cmd=j;
    }
    if(cmd>=0){ // execute the command which have been found
      for(i+=strlen(commands[cmd].name);text[i]==' '||text[i]=='\t';i++);
      cmdarg_position=i;
      return commands[cmd].func(text+i,line);
    }
    if(('a'<=text[i]&& text[i]<='z')||('A'<=text[i]&&text[i]<='Z')){
        if(line)printf("%s\n",line);
        printf(" Unknown command; use 'help' to get a list of commands\n");
        return 1;
    }
    // if no = sign, do nothing ...
    for(j=i;text[j] && text[j]!='='; j++);
    if(text[j]!='='){
        if(line)printf("%s\n",line);
        printf(" Unknown command; use 'help' to get a list of commands\n");
        return 1;
    }
    cmdarg_position=i;
    // is there another = nearby? if yes, this is a diff question
    // otherwise assume that it is an expression to be checked
    for(j=j+1;text[j]==' '||text[j]=='\t'; j++);
    return text[j]=='=' ? com_diff(text+i,line) : com_check(text+i,line);
}

/***********************************************************************
* error_message(char *original_line)
*
*   print out the error message on a constraint, relation, macro.
*   If original_line is set, it is printed first.
*   Uses cmdarg_position and the syntax_error structure to figure
*   out the place of the error.
*/
static void error_message(const char *orig)
{int i, pos=cmdarg_position;
 const char *err=syntax_error.harderrstr;
    if(err){ pos += syntax_error.harderrpos; }
    else { err=syntax_error.softerrstr; pos+=syntax_error.softerrpos; }
    if(orig) printf("%s\n",orig);
    else pos += strlen(minitip_PROMPT);
    for(i=0;i<pos;i++)printf("-"); printf("^\nERROR: %s\n",err);
    if(syntax_error.showexpression){
        printf(" ==> "); print_expression(); printf("\n"); 
    }
}

/***********************************************************************
* procedures executing particular commands
*
*   com_about(char *arg, char *line)    version and copyright
*   com_help (char *arg, char *line)    short help on commands
*   com_quit (char *arg, char *line)    quit, no arguments accepted
*   com_syntax(char *arg,char *line)    help on syntax with several topics
*   com_add  (char *arg, char *line)    add a new constraints
*   com_list (char *arg, char *line)    list all or some of the constraints
*   com_del  (char *arg, char *line)    delete all or a single constraint
*   com_style(char *arg, char *line)    report or change syntax style
*   com_macro(char *arg, char *line)    manipulate macros
*   com_batch(char *arg, char *line)    execute commands from a file
*   com_check(char *arg, char *line)    check relation with constraints
*   com_nocon(char *arg, char *line)    check relation without constraints
*   com_diff (char *arg, char *line)    print difference of two expressions
*/

/** ABOUT -- copyright and version **/
static int com_about(UNUSED const char *arg, const char *line)
{   if(!line) /* don't print in batch jobs */
      printf(
"This is 'minitip' Version " VERSION_STRING "\n\n"
"Minitip is a MINimal Information Theoretic Inequality Prover. This program\n"
"was written in C, uses editable command line input with history expansion;\n"
"extended syntax; macro facilities; online help; user friendly syntax checker,\n"
"and glpk (gnu linear programming kit) as LP solver.\n"
"The original ITIP software was developed by Raymond W. Yeung and Ying-On Yan,\n"
"runs under MATLAB and is avaiable at http://user-www.ie.cuhk.edu.hk/~ITIP\n"
"The stand alone version Xitip at http://xitip.epfl.ch has graphical interface\n"
"and runs both in Windows and Linux.\n\n"
"This program is free, open-source software. You may redistribute it and/or\n"
"modify under the terms of the GNU General Public License (GPL) as published\n"
"by the Free Software Foundation http://www.gnu.org/licenses/gpl.html\n"
"There is ABSOLUTELY NO WARRANTY, use at your own risk.\n"
COPYRIGHT
"\n");
    return 0; /* OK */
}

/** HELP -- give a comprehensive help on commands **/
static int com_help(const char *arg, const char *line)
{int i, printed=0;
    for(i=0;commands[i].name;i++){
        if(!*arg || cmp_s(arg,commands[i].name)==0){
            printf("%s\t\t%s\n",commands[i].name,commands[i].doc);
            printed++;
        }
    }
    if(!printed && !line){ /* give help only when not in batch job */
        printf(" No command matches '%s'. Possibilities are:\n",arg);
        for(i=0;commands[i].name;i++){
            printf(" %s%c",commands[i].name,printed%6==5?'\n':'\t');
            printed++;
        }
        printf("\n");
    }
    return 0; /* OK */
}

/** QUIT -- quit minitip **/
static int com_quit(const char *arg, const char *line)
{   if(line){ done=1; return 0; } /* batch */
    if(*arg==0){int c;
        printf("Save the commands to the history file %s (y/n)? ",HISTORY_FILE);
        c=yesno(); while(c!='y' && c!='n' && c!='Y' && c!='N'){
          printf("             please hit 'y' or 'n' (y/n)? ");
          c=yesno(); }
       done=1;
       if(c=='y'||c=='Y') write_history(HISTORY_FILE);
    }
    else printf("'quit' should not have any argument \n");
    return 0; /* OK */
}

/** SYNTAX  -- give help on syntax of different constructs **/
static int com_syntax(const char *argv, const char *line)
{   if(strncasecmp(argv,"style",3)==0){ // on style
        printf(
"Entropy expressions can be entered in two styles: 'full' or 'simple'.\n"
"FULL (standard) style\n"
"  Random variables are arbitrary identifiers, e.g., 'X12' or 'Winter'.\n"
"  Entropy and mutual information follows the standard: H(X1,Winter) is the\n"
"  joint entropy of 'X1' and 'Winter'; I(A,Winter;X1,Y|Z) is the conditional\n"
"  mutual information of 'A,Winter' and 'X1,Y' given 'Z'.\n"
"SIMPLE style\n"
"  Variables are single lower case letters (plus optional primes); write\n"
"  variables next to each other for their joint distribution. Any list such\n"
"  as 'abc' denotes its own entropy. In entropy expressions letters 'H' and\n"
"  'I' can be omitted, and optionally the enclosing brackets. Comma (default)\n"
"  or a specified character separates lists. With ',' as separator any of\n"
"  'I(ab,xy|z)', '(ab,xy|z)', or even 'ab,xy|z' denotes the conditional\n"
"  mutual information of 'a,b' and 'x,y' given 'z'.\n"
"Enter 'style full' or 'style simple [separator char]' to set the style.\n"
"Warning: changing the style deletes all stored constraints.\n"
"The present style is ");
        if(minitip_style==syntax_short)printf(
"SIMPLE using '%c' as separator.\n",minitip_sepchar);
        else printf("FULL (standard).\n");
        return 0;
    }
    if(strncasecmp(argv,"var",3)==0){ // give syntax of vars and var list
        printf( minitip_style==syntax_short ?
// simple style
"In simple style RANDOM VARIABLES are lower case letters from 'a' to 'z'\n"
"optionally followed by a sequence of primes such as \"a\", \"a'\", \"a''\".\n"
"Put variables next to each other for their joint distribution: \"ac'rs'\"\n" :
// full style
"In full style RANDOM VARIABLES are identifiers such as \"X\", \"Snow_fall\",\n"
"\"Winter\"; primes can be appended so you can use \"A'\" or \"Winter'\".\n"
"Variables are case sensitive, \"x\" and \"X\" are different variables.\n"
"A variable sequence is a list of variables separated by commas, such as\n"
"\"X', y,Y\" or \"Snow_fall, Winter\".\n");
        return 0;
    }
    if(strncasecmp(argv,"entrop",3)==0){ // entropy syntax
        if(minitip_style==syntax_short) printf(
// simple style
"In simple style with separating character '%1$c' an ENTROPY EXPRESSION can be\n"
"abbreviated to minimal:\n"
"a) entropy of the variable sequence 'a,c,r,s' is entered as\n"
"     H(acrs)     or  acrs\n"
"b) conditional entropy H(a,b|r,s) can be written as\n"
"     H(ab|rs)    or  (ab|rs)    or  ab|rs\n"
"c) mutual information I(a,b;r,s) is\n"
"     I(ab%1$crs)    or  (ab%1$crs)    or  ab%1$crs\n"
"d) conditional mutual information I(a,b;r,s|c,d) is\n"
"     I(ab%1$crs|cd) or  (ab%1$crs|cd) or  ab%1$crs|cd\n"
"e) the Ingleton expression  -(a%1$cb)+(a%1$cb|c)+(a%1$cb|d)+(c%1$cd) is\n"
"     [a%1$cb%1$cc%1$cd]\n"
"f) invocation of the macro X() with arguments 'ab', 'c' and 'a' is\n"
"     X(ab%1$cc|a)\n",minitip_sepchar);
        else printf(
// full style
"In full style an ENTROPY EXPRESSION follows the standard notation:\n"
" a) H(W,S)       entropy of the joint distribution of 'W' and 'S'\n"
" b) H(W|F)       conditional entropy\n"
" c) I(W,F;S)     mutual information of 'W,F' and 'S'\n"
" d) I(W,F;S|day) conditional mutual information\n"
" e) [A;B;C;D]    shorthand for the Ingleton expression\n"
"                     -I(A;B)+I(A;B|C)+I(A;B|D)+I(C;D)\n"
" f) X(A,B;C|A)   invocation of the macro X() with three arguments.\n");
        return 0;
    }
    if(strncasecmp(argv,"relation",3)==0){ //formula description
        printf(
"A RELATION compares two linear compositions of entropy expressions:\n");
        if(minitip_style==syntax_short) printf(
// simple style
"       I(x%1$cy) +3H(a)-1.234* I(x%1$ca|z) >= 0\n"
"       -1.234*(x|y) - 12.234*(a%1$cb|h) <= -2bxy\n"
"       (b%1$cd|a'c)+(b%1$cc|a')=(b%1$ccd|a')\n",minitip_sepchar);
        else printf(
// full style
"       I(X;Y)+3 H(A) -1.234 I(X;A|Z) >= 0\n"
"       +1.234*H(X|Y) - 12.234*I(A;B|H) <= -2H(B,X,Y)\n"
"       I(X;Y1|Z,Y2)+I(X;Y2|Z)=I(X;Y1,Y2|Z)\n");
        printf(
"The '*' sign between the constant and the entropy expression is optional.\n"
"Only '=' (equal), '>=' (greater or equal) and '<=' (less or equal) can be\n"
"used. Any side (but not both) can be zero as in the above example.\n");
        return 0;
    }
    if(strncmp(argv,"zap",3)==0){ // zap
        printf(
"Calculate the missing terms on the right hand side of two entropy formulas\n"
"connected by '=='. Leave the right hand side empty to print the formula as\n"
"a composition of entropies. Example: 'zap ");
        if(minitip_style==syntax_short) printf(
// simple style
"(a%1$cb|c)+(b%1$cc|a)+(c%1$ca|b) =='\n",minitip_sepchar);
        else printf(
// full style
"I(A;B|C)+I(B;C|A)+I(C;A|B) =='\n");
        return 0;
    }
    if(strncasecmp(argv,"const",3)==0){ // constraint
        printf(
"A CONSTRAINT can be a RELATION (two linear entropy formulas compared by '=',\n"
"'<=' or '>='), or one of the following shorthands:\n"
"         {vars} : {vars}\n"
"expressing that the first list is determined by the second one;\n"
"         {vars} .  {vars} .  {vars} .   ...\n"
"  or     {vars} || {vars} || {vars} ||  ...\n"
"expressing that they are totally independent;\n"
"         {vars} /  {vars} /  {vars} /  ...\n"
"  or     {vars} -> {vars} -> {vars} -> ...\n"
"expressing that they form a Markov chain.\n");
        printf( minitip_style!=syntax_full ?
"A variable list is a sequence of lower case letters from 'a' to 'z'.\n" : 
"A variable list is a sequence of random variables (identifiers)\n"
"separated by commas, such that \"Winter, Fall, Summer\"\n");
        return 0;
    }
    if(strncasecmp(argv,"macro",4)==0){ // macro
        printf(
"MACROS are shorthands in entropy formulas. An unmeaningful definition is\n");
        if(minitip_style==syntax_short) printf(
//simple style
"    macro T(x%1$cy|t%1$cz) = 3(tx%1$cy|z)+2(x%1$cty|z)+(t%1$cz|xy)\n",minitip_sepchar);
        else printf(
//full style
"    macro T(X;Y|Z1;Z2) = 3I(Z1,X;Y|Z2)+2I(X;Y,Z2|Z1)+H(X,Y|Z1,Z2)\n");
        printf(
"Only variables in the argument list can be used in the formula. Macros in the\n"
"definition are expanded. Macros are stored and printed out in raw format using\n"
"only entropies. Macro name is an upper case letter from 'A' to 'Z'. Arguments\n"
"can be separated by '%c' or '|', and must match when using the macro.\n"
"\n"
"Use 'macro add', 'macro list', 'macro delete' to add, list, or delete macros.\n"
"The 'add' keyword can be omitted.\n",minitip_sepchar);
       return 0;
    }
    if(!line) /* only if online */
    printf(
"Please enter one of the following topics:\n"
"  style      -- choose between \"simple\" and \"full\" style\n"
"  variables  -- random variables and sequences of random variables\n"
"  entropy    -- entropy expression syntax and shorthands\n"
"  relation   -- compare two linear entropy formulas\n"
"  zap        -- calculate missing terms on the right hand side\n"
"  constraint -- how to add constraints\n"
"  macro      -- add, list delete macros\n");
    return 0; /* OK */
}

/** ADD  -- add a constraint **/
static int com_add(const char* line,const char *orig)
{int i;
    if(*line==0 || strcmp(line,"help")==0){ // empty line, help
        if(!orig) printf("add a new constraint, which can be\n"
        "- two linear entropy formulas compared by '=', '<=' or '>='; or\n"
        "- functional dependency; total independence; or a Markov chain.\n");
        return 0;
    }
    // check if it is there ...
    for(i=0;i<constraint_no;i++) if(strcmp(line,constraint_table[i])==0){
        printf("This constraint is #%d, no need to add again\n",i+1);
        return 1; /* abort */
    }
    if(parse_constraint(line,0)){ // some error
        error_message(orig);
        return 1; /* abort */
    }
    if(constraint_no>=max_constraints-1){
        if(orig)printf("%s\n",orig);
        printf("ERROR: too many constraints (max %d)\n"
               "  use 'del <number>' to delete some constraints\n",max_constraints);
        return 1; /* abort */
    }
    constraint_table[constraint_no]=strdup(line);
    constraint_no++;
    return 0; /* OK */
}

/*************************************************************************
* read_number(char *frm, int *to)
*
*    read a sequence of digits from the given position and put the
*    value into *to.
*    Return the number of positions advanced, or zero if no digit was found
*/
static int read_number(const char *frm, int *to)
{int i,d;
    d=0; i=0;
    while('0'<=*frm && *frm<='9'){ d=d*10+(*frm)-'0'; frm++; i++; }
    if(i>0){*to=d;}
    return i;
}

/** LIST -- list all or some of the constraints **/
static int com_list(const char *arg, const char *orig)
{int i0,i1,pos; int not_printed;
    if(*arg=='?' || strcmp(arg,"help")==0){ // help
        if(constraint_no==0) printf(" There are no constraints to be listed.\n");
        else printf(" The number of constraints is %d\n",constraint_no);
        printf(" Use 'all' to show all; '1-10' to show the first 10 constraints.\n"
               " In general, 'list 3,4-6,8' shows constraints #3,#4 to #6, and #8\n");
        return 0;
    }
    if(constraint_no==0){
       if(!orig) printf(" There are no constraints to be listed.\n");
       return 0; 
    }
    if(*arg==0 || strcmp(arg,"all")==0){ // list first ten or all constraints
        printf("--- Constraints (total %d)\n",constraint_no);
        i1=constraint_no;
        if(10<i1 && *arg==0) i1=10;
        for(i0=0;i0<i1;i0++){
            printf("%3d: %s\n",i0+1,constraint_table[i0]);
        }
        return 0;
    }
    i0=-1; not_printed=1;
    while(*arg)switch(*arg){
  case ' ': case '\t': arg++; break;
  case ',': arg++; if(i0>0 && i0<=constraint_no){
               if(not_printed){
                   printf("--- Constraints (total %d)\n",constraint_no);
                   not_printed=0;
               }
               printf("%3d: %s\n",i0,constraint_table[i0-1]);
            }
            i0=-1;
            break;
  case '-': arg++; while(*arg==' '||*arg=='\t') arg++;
            pos=read_number(arg,&i1);
            if(!pos ||i0<=0||i0>i1){ printf("   Wrong syntax, use 'list 3,4-6,8'\n"); return 1; }
            arg += pos;
            if(not_printed){
                   printf("--- Constraints (total %d)\n",constraint_no);
                   not_printed=0;
            }
            if(i0==0)i0++;
            while(i0<=i1 && i0<=constraint_no){
               printf("%3d: %s\n",i0,constraint_table[i0-1]);
               i0++;
            }
            i0=-1;
            break;
  default:  pos=read_number(arg,&i1);
            if(!pos){ printf("   Wrong syntax, use 'list 3,4-6,12-14'\n"); return 1; }
            arg += pos;
            if(i0>0 && i0<=constraint_no){
               if(not_printed){
                   printf("--- Constraints (total %d)\n",constraint_no);
                   not_printed=0;
               }
               printf("%3d: %s\n",i0,constraint_table[i0-1]);
            }
            i0=i1;
            break;
    }
    if(i0>0 && i0<=constraint_no){
        if(not_printed){
            printf("--- Constraints (total %d)\n",constraint_no);
            not_printed=0;
        }
        printf("%3d: %s\n",i0,constraint_table[i0-1]);
    }
    return 0; /* OK */
}

/** DEL -- delete all or a single constraint **/
static int com_del(const char *arg, const char *line)
{int pos,no;
    if(constraint_no==0){
        if(!line) printf(" There are no constraints to delete.\n");
        return 0; /* OK */
    }
    if(*arg==0 || *arg == '?' || strcmp(arg,"help")==0){
        if(!line)
        printf("Specify the constraint to be deleted from 1 to %d, or say 'all'.\n",
             constraint_no);
        return 1; /* abort */
    }
    if(strcmp(arg,"all")==0){
        int c='y';
        if(!line){
           printf("All constraints (%d) will be deleted. Proceed (y/n)? ",constraint_no);
           c=yesno();
        }
        if(c=='y'||c=='Y'){
           for(no=0;no<constraint_no;no++) free(constraint_table[no]);
           constraint_no=0;
        }
        return 0; /* OK */
    }
    pos=read_number(arg,&no);
    arg+=pos; if(pos==0 || *arg || no<=0 || no>constraint_no){
        if(!line)
           printf("specify exactly one constraint index from 1 to %d, or say 'all'\n",constraint_no);
        return 1;
    }
    if(!line)
      printf(" This constraint has been deleted:\n   %s\n",constraint_table[no-1]);
    free(constraint_table[no-1]);
    while(no<constraint_no){
       constraint_table[no-1]=constraint_table[no]; no++;
    }
    constraint_no--;
    return 0; /* OK */
}

/** STYLE -- change or report syntax style **/
static int com_style(const char *argv, const char *line)
{   if(strcasecmp(argv,"full")==0){
        if(minitip_style==syntax_full){
            if(!line)
                printf("Expression style is FULL, not changed.\n");
            return 0; /* no change, OK */
        }
        if(constraint_no>0){
            if(line){
                printf("Cannot change style to FULL when there are constraints.\n");
                return 1; /* abort */
            }
            printf("Changing style will delete all constraints. Proceed (y/n)? ");
            int no, c=yesno(); if(c!='y' && c!='Y') return 1;
            for(no=0;no<constraint_no;no++) free(constraint_table[no]);
            constraint_no=0;
        }
        minitip_style=syntax_full; minitip_sepchar=';';
        set_syntax_style(minitip_style,minitip_sepchar);
        if(!line)
            printf("Expression style is set to FULL.\n");
        return 0; /* OK */
    }
    if(strncasecmp(argv,"simple",6)==0){
        char sepchar=',';
        argv+=6; while(*argv==' ' || *argv=='\t') argv++;
        if(*argv) sepchar=*argv;
        if(!strchr(minitip_SEPARATOR_CHARS,sepchar)){
            printf("Wrong separator character for 'simple' style. Possibilities are %s\n",minitip_SEPARATOR_CHARS);
            return 1; /* abort */
        }
        if(minitip_style==syntax_short && sepchar == minitip_sepchar){
            if(!line)
                printf("The present style is SIMPLE using '%c' as separator, not changed.\n",minitip_sepchar);
            return 0;
        }
        if(constraint_no>0){
            if(line){
                printf("Cannot change style to SIMPLE when there are constraints.\n");
                return 1; /* abort */
            }
            printf("Changing style will delete all constraints. Proceed (y/n)? ");
            int no, c=yesno(); if(c!='y' && c!='Y') return 1;
            for(no=0;no<constraint_no;no++) free(constraint_table[no]);
            constraint_no=0;
        }
        minitip_style=syntax_short; minitip_sepchar=sepchar;
        set_syntax_style(minitip_style,minitip_sepchar);
        if(!line)
           printf("Expression style is set to SIMPLE using '%c' as separator.\n",
                   minitip_sepchar);
        return 0; /* OK */
    }
    if(line && strcasecmp(argv,"help")!=0) return 1; /* abort */
    if(minitip_style==syntax_short) printf(
"The present style is SIMPLE using '%c' as separator.\n",minitip_sepchar);
    else printf(
"The present style is FULL (standard).\n");
    printf(
"Enter 'style full' to set the style to FULL (standard), or 'style simple'\n"
"followed by the character to be used as separator (default: ',') to set\n"
"the style to SIMPLE. Enter 'syntax style' for a detailed description.\n");
    return 0; /* OK */
}

/** MACRO -- manipulate macros **/

/*---------------------------------------------------------------------
*  int standard_macros
*      user-defined macros starts from this index
*  void setup_standard_macros()
*      set up the standard macros H() and I() and D() as Delta
*  int com_macro_add()
*      add a new macro
*  int com_macro_list()
*      list macros defined for letters
*  int com_macro_del()
*      delete the macro with the given prototype
*/
static int standard_macros=0;
static void setup_standard_macros(void)
{   set_syntax_style(syntax_short,',');
    if(parse_macro_definition("H(a)=a")) error_message(NULL);
    if(parse_macro_definition("H(a|b)=ab-b")) error_message(NULL);
    if(parse_macro_definition("I(a,b)=a+b-ab")) error_message(NULL);
    if(parse_macro_definition("I(a,b|c)=ac+bc-c-abc")) error_message(NULL);
    standard_macros = macro_total; 
    if(parse_macro_definition("D(a,b,c)=(a,b|c)+(b,c|a)+(c,a|b)")) error_message(NULL);
}

/** MACRO add -- define a new macro **/
static int com_macro_add(const char *arg, const char* line)
{
    if(!*arg || strcmp(arg,"help")==0){
         if(!line){ printf(
"add a new macro to be used in entropy formulas.\n"
"Example:  ");
             if(minitip_style==syntax_short) printf(
     "X(a%1$cb|c%1$cd) = (a%1$cb)+(b|c)+(a|c)+(a|d)-(c%1$cd)\n",minitip_sepchar);
             else printf(
     "X(A;B|C;D) = I(A;B)+H(B|C)+H(A|C)+H(A|D)+I(C;D)\n");
             printf(
"The same macro name can be used repeatedly as different argument separators\n"
"define different macros.\n");
         }
         return 0;
    }
    if(parse_macro_definition(arg)){ /* some error */
        error_message(line); return 1; /* abort */
    }
    return 0; /* OK, stored */
}
/** MACRO list -- list macros associated with a letter **/
static int com_macro_list(const char *arg, const char* line)
{int printit[30]; int name,lastname; int i,error;
    if(macro_total<=standard_macros){
        if(!line) printf(" No macros are defined,\n");
        return 0; /* OK */
    }
    if(!(*arg)){ /* no argument */
        if(!line){
          printf(" Total number of macros defined: %d\n",
              macro_total-standard_macros);
        }
        for(name='a'; name<='z';name++) printit[name-'a']=1;
    } else {
        for(name='a'; name<='z';name++) printit[name-'a']=0 ;
        lastname='\0'; error=0;
        while(*arg) switch(*arg){
      case ' ': case '\t': case ',': arg++; break;
      case '-':  arg++; while(*arg==' '||*arg=='\t') arg++;
            name=(*arg)|0x20; arg++; 
            if(error || !lastname || !('a'<=name && name<='z') || name<lastname){
                 error=1; break;
            }
            while(lastname<=name){printit[lastname-'a']=1; lastname++;}
            lastname='\0'; break;
      default: name=(*arg)|0x20; arg++;
            if(error) break;
            if(lastname)printit[lastname-'a']=1;
            if('a'<=name && name<='z') lastname=name; else error=1;
            break;
        }
        if(lastname)printit[lastname-'a']=1;
        if(error){
           printf(" Wrong syntax, use 'macro list A, D-G'\n");
           return 1; /* abort */
        }
    }
    i=0;
    for(name='A';name<='Z';name++)if(printit[name-'A']){
        i+=print_macros_with_name((char)name,standard_macros);
    }
    if(i==0 && !line){ printf(" No macros match the condition\n"); }
    return 0; /* OK */
}
/** MACRO del -- delete macro with given prototype **/
static int com_macro_del(const char *arg, const char* line)
{int macro_id;
    if(!*arg || strcmp(arg,"help")==0){
        if(!line){ printf(
" to delete a macro specify its header only, such as '");
           if(minitip_style==syntax_short) 
              printf("X(a%1$cb|c)'\n",minitip_sepchar);
           else printf("X(A;B|C)'\n");
        }
        return 0; /* OK */
    }
    macro_id=parse_delete_macro(arg);
    if(macro_id<0){ /* some error */
        error_message(line); return 1; /* abort */
    }
    if(macro_id<standard_macros){
        if(line)printf("%s\n",line);
        printf("ERROR: no match was found. Use 'macro list' to list all macros\n");
        return 1; /* abort */
    }
    printf(" This macro has been deleted:\n   ");
    print_macro_with_idx(macro_id);
    delete_macro_with_idx(macro_id);
    return 0; /* OK */
}

/** MACRO -- find the subcommand **/
static struct {
   char       *name;
   com_func_t *func;
} macro_commands[] = {
     {"add",   com_macro_add  },
     {"list",  com_macro_list },
     {"delete",com_macro_del  },
     {NULL, NULL }
};
static int com_macro(const char *arg, const char* line)
{int i,j,cmd;
    if(!*arg){
       if(!line) printf(" add, list, delete macros\n");
       return 0;
    }
    cmd=-1;
    for(j=0;cmd<0 && macro_commands[j].name;j++){
        int len=strlen(macro_commands[j].name);
        if(strncmp(arg,macro_commands[j].name,len)==0 &&
          (arg[len]==0 || arg[len]==' ' || arg[len]=='\t') ) cmd=j;
    }
    if(cmd>=0){
        for(i=strlen(macro_commands[cmd].name);arg[i]==' '||arg[i]=='\t';i++);
        cmdarg_position += i; arg += i;
    } else { /* pretend it is an 'add' instruction */
        cmd=0;
    }
    return macro_commands[cmd].func(arg,line);
}

/** BATCH -- execute commands from a file **/
static int com_batch(const char *arg,const char *line)
{FILE *batch_file; char *filename; const char *errmsg;
    if(!*arg){
        if(!line) printf(" execute minitip commands from a file\n");
        return 0; /* OK */
    }
    filename=NULL; errmsg=NULL; batch_file=NULL;
    if(batch_depth>=minitip_MAX_BATCH_DEPTH){
        errmsg="maximal inclusion depth for run files reached";
    } else if(!(filename=prepare_filename(arg))){
        errmsg="wrong filename syntax";
    } else if(not_regular_file(filename)){
        errmsg="run file not found";
    } else if(!(batch_file=fopen(filename,"r"))){
        errmsg="cannot open run file";
    }
    if(filename) free(filename);
    if(!errmsg){
        batch_depth++;
        for(done=0;!done;) switch(read_batch_line(batch_file)){
          case  1: /* long line */
               errmsg="too long line, perhaps not a minitip file";
               done=1; break;
          case -1: /* EOF */
               done=1; break;
          default: /* regular line */
               if(execute_cmd(batch_line,batch_line)){ /* error */
                  errmsg="error in run file, aborting execution";
                  done=1;
               }
        }
        done=0; batch_depth--; fclose(batch_file);
    }
    if(errmsg){
        if(line)printf("%s\n",line);
        printf(" ERROR: %s\n",errmsg);
        return 1; /* abort */
    }
    return 0; /* OK */
}

/** CHECK -- check entropy relation with all constraints **/
static int com_check(const char *line, const char *orig)
{int i,keep;
    if(!*line || strcmp(line,"help")==0){
        if(!orig)printf(" Check the validity of an entropy relation with all constraints.\n"
                        " Enter 'syntax relation' for more help.\n");
        return 0; /* empty line, OK */
    }
    keep=0; // add all constraints
    for(i=0;i<constraint_no;i++){
        parse_constraint(constraint_table[i],keep); keep=1;
    }
    if(parse_entropy(line,keep)){/* some error */
        error_message(orig);
        return 1;
    }
    if(orig) printf("%s\n",orig);
    check_expression(line,1);
    return 0; /* OK */
}

/** NOCON -- check entropy relation without constraints **/
static int com_nocon(const char *line, const char *orig)
{   if(!*line || strcmp(line,"help")==0){
        if(!orig)printf(" Crosscheck an entropy relation without any constraints.\n"
                        " Enter 'syntax relation' for more help.\n");
        return 0;
    }
    if(parse_entropy(line,0)){ /* some error */
        error_message(orig);
        return 1;
    }
    if(orig) printf("%s\n",orig);
    if(constraint_no>0){
        printf("Checking without constraints ...\n");
    }
    check_expression(line,0);
    return 0;
}

/** DIFF -- print difference of two expressions **/
static int com_diff(const char *line, const char *orig)
{   if(!*line || strcmp(line,"help")==0){
        if(!orig)
            printf(" Show the difference of two formulas separated by '=='\n"
                   " Use 'syntax zap' for more help.\n");
        return 0;
    }
    if(parse_diff(line)){ /* some error */
        error_message(orig);
        return 1;
    }
    if(orig)printf("%s\n",orig);
    printf(" ==> "); print_expression(); printf("\n");
    return 0; /* OK */
}

/***********************************************************************
* Interface to the LP solver
*
*    The argument of call_lp(func) is called as ret=func(idx); it 
*    should parse the expression to be evaluated when idx<0, otherwise
*    the constraint with index idx. The ret value is 0 if a constraint
*    with index idx exists, otherwise 1. (There could be no error at
*    this stage.)
*  char *expr_to_check
*  int use_constraints
*    used to pass info to compute_expression(idx) which realizes the 
*    function argument to call_lp().
*  int compute_expression(int idx)
*    the argument to call_lp()
*  void check_expression(char *src, int with_constraints)
*    calls the lp solver and prints out the result.
*  int check_offline_expression(char *src, int quiet)
*    calls the lp solver; prints out the result if quiet is not set, 
*    and determines the exit value of minitip.
*/

#include "mklp.h"
static const char *expr_to_check;
static int use_constraints;
/* parses expr_to_check if i<0; otherwise parses the i'th constraint.
   return 1 if no such constrain exists */
static int compute_expression(int i)
{
    if(i<0){ parse_entropy(expr_to_check,1); return 0; }
    if(!use_constraints || i>=constraint_no) return 1;
    parse_constraint(constraint_table[i],1);
    return 0;
}
static void check_expression(const char *src, int with_constraints)
{char *ret; char *constr;
    expr_to_check=src; use_constraints=with_constraints;
    constr= with_constraints && constraint_no>0
         ? " with the constraints" : "";
    ret=call_lp(compute_expression);
    if(ret==EXPR_TRUE){
        printf("      ==> TRUE%s\n",constr);
    } else if(ret==EXPR_FALSE){
        printf("      ==> FALSE\n");
    } else if(ret==EQ_GE_ONLY){
        printf("      ==> FALSE, only >= is true%s\n",constr);
    } else if(ret==EQ_LE_ONLY){
        printf("      ==> FALSE, only <= is true%s\n",constr);
    } else {
        printf("ERROR in solving the LP: %s\n",ret);
    }
}
static int check_offline_expression(const char *src, int quiet)
{char *ret; char *constr,*outstr;
   expr_to_check=src; use_constraints=1;
    ret=call_lp(compute_expression);
    if(!quiet){
      constr= constraint_no>0? " with the constraints" : "";
      if(ret==EXPR_TRUE){       outstr="     ==> TRUE"; }
      else if(ret==EXPR_FALSE){ outstr="     ==> FALSE"; constr=""; }
      else if(ret==EQ_GE_ONLY){ outstr="     ==> FALSE, only >= is true"; }
      else if(ret==EQ_LE_ONLY){ outstr="     ==> FALSE, only <= is true"; }
      else {
        printf("ERROR in solving the LP: %s\n",ret);
        return EXIT_ERROR;
      }
      printf("%s\n%s%s\n",src,outstr,constr);
    }
    return ret==EXPR_TRUE ? EXIT_TRUE :
       ret==EXPR_FALSE || ret==EQ_GE_ONLY || ret== EQ_LE_ONLY ? EXIT_FALSE :
       EXIT_ERROR ;
}

/***********************************************************************
*  int check_offline(int argno. char *argv[], int quiet)
*
*    checks the first expression with the other arguments as
*    constraints. Print out the messages if quiet is not set, and
*    determines the program's exit value.
*/
static int check_offline(int argc, char *argv[], int quiet)
{int i,j,keep;
    set_syntax_style(minitip_style,minitip_sepchar);
    keep=0;
    for(i=1;i<argc;i++){
        for(j=0;j<constraint_no;j++) if(strcmp(argv[i],constraint_table[j])==0){
            if(!quiet) printf("ERROR: constraint #%d is the same as constraint %d:\n%s\n",
                     i,j+1,argv[i]);
            return EXIT_ERROR; // other error
        }
        if(parse_constraint(argv[i],keep)){
            if(!quiet) error_message(argv[i]);
            return EXIT_SYNTAX; // syntax error
        }
        if(constraint_no>=max_constraints-1){
            if(!quiet) printf("ERROR: too many constraints (max %d)\n",max_constraints);
            return EXIT_ERROR; // other error
        }
        constraint_table[constraint_no]=argv[i];
        constraint_no++;
        keep=1;
    }
    if(parse_entropy(argv[0],keep)){
        if(!quiet) error_message(argv[0]);
        return EXIT_SYNTAX; // syntax error
    }
    return check_offline_expression(argv[0],quiet);
}

/***********************************************************************
* Initialize random and extract some randomness
*
*    The LP solver is called with a random permutation of the constraint
*    rows and columns. It helps to overcome numerical instability.
*    Randomness used is instantiated by time and process ID, moreover
*    further randomness is extracted from the commands typed in.
*
*  void initialize_random()
*    initialize the standard random() function
*  void extract_randomness(char *line)
*    hash the input string to a number between 0 and 1000, and advance
*    random() by that number.
*/
#include <time.h>
#include <unistd.h>
static void initialize_random(void)
{    srandom(time(NULL) ^ getpid()); }
static void extract_randomness(const char *from)
{int i=0x1234;
    for(i=0x1234;*from;from++){
        i=((i*7)+(unsigned char)(*from))&0xffff;
    }
    i%=1003;
    while(i){random();i--; }
}

/***********************************************************************
* Main routines
*
* void print_version(char verbose)
*    prints out the version number and copyright notice
* void short_help(void)
*    prints out the short help for the flag '-h'
* int main(int argc, char *argv[])
*    the main routine, process flags and do as instructed.
*/

inline static void print_version(char verbose)
{   if(verbose) com_about(NULL,NULL); 
    else printf("This is 'minitip' Version " VERSION_STRING "\n"
    COPYRIGHT "\n");
}

inline static void short_help(void){printf(
"This is minitip, a MINimal Information Theoretic Inequality Prover.\n"
"Usage:\n"
"    minitip [flags]\n"
"for interactive usage, or\n"
"    minitip [flags] <expression> [condition1] [condition2] ... \n"
"Flags:\n"
"   -h        -- this help\n"
"   -s        -- start using minimal syntax style (default, same as -s,)\n"
"   -s<chr>   -- minimal style, use <chr> as the separator character\n"
"   -S        -- start using full syntax style\n"
"   -q        -- quiet, just check, don't print anything\n"
"   -e        -- last flag, use when the expression starts with '-'\n"
"   -f <file> -- use <file> as the history file (default: '" DEFAULT_HISTORY_FILE "')\n"
"   -v        -- version and copyright\n"
"Exit value when checking validity of the argument:\n"
"    " mkstringof(EXIT_TRUE)  "  -- the expression (with the given constrains) checked TRUE\n"
"    " mkstringof(EXIT_FALSE) "  -- the expression (with the given constrains) checked FALSE\n"
"    " mkstringof(EXIT_SYNTAX)"  -- syntax error in the expression or in some of the constraints\n"
"    " mkstringof(EXIT_ERROR) "  -- some error (problem too large, LP failure, etc)\n"
"For more information, type 'help' from within minitip\n\n");
}

int main(int argc, char *argv[])
{char *line; int i; int quietflag, endargs;;
    /* argument handling */
    quietflag=0; endargs=0;
    for(i=1; i<argc && endargs==0 && argv[i][0]=='-';i++){
        switch(argv[i][1]){
      case 'h': short_help(); return EXIT_INFO;
      case 'v': print_version(argv[i][2]); return EXIT_INFO;
      case 's': minitip_style=syntax_short; minitip_sepchar=','; /* default */
                if(argv[i][2]){
                    minitip_sepchar=argv[i][2];
                    if(!strchr(minitip_SEPARATOR_CHARS,minitip_sepchar)){
                        printf("Illegal separator character in flag -s\n"
                               "Accepted searators are: %s\n",minitip_SEPARATOR_CHARS);
                        return EXIT_ERROR;
                    }
                }
                break;
      case 'S': minitip_style=syntax_full; minitip_sepchar=';'; break;
      case 'f': line=&(argv[i][2]);
                if(*line==0){ i++; if(i<argc){ line=argv[i]; } }
                if(!line || !*line){
                   printf("Flag '-f' requires the history file name\n");
                   return EXIT_ERROR;
                }
                HISTORY_FILE=strdup(line);
                // replace all ctrl chars by underscore
                for(line=HISTORY_FILE; *line; line++){
                    if((unsigned char)(*line)<=0x20 || (unsigned char)(*line)==0x7f || (unsigned char)(*line)==0xff ) *line='_';
                }
                break;
      case 'q': quietflag=1; break;
      case 'e': endargs=1; break;
      default:  printf("unknown flag '%s', use '-help' for help\n",argv[i]); return EXIT_ERROR;
        }
    }
    if(i<argc){ /* further arguments; do as instructed */
        return check_offline(argc-i,argv+i,quietflag);
    }
    if(quietflag || endargs) return EXIT_ERROR; /* -q or -c flag and no argument */
    initialize_readline();
    initialize_random();
    setup_standard_macros();
    set_syntax_style(minitip_style,minitip_sepchar);
    for(done=0;!done;){
        line=readline(minitip_PROMPT);
        if(!line) break;
        extract_randomness(line);
        execute_cmd(line,NULL);
        store_if_not_new(line);
        free(line);
    }
    return 0;
}

/* EOF */


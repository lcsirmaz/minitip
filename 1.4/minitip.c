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
#define VERSION_MINOR	4
#define VERSION_SUB	3
#define VERSION_STRING	mkstringof(VERSION_MAJOR.VERSION_MINOR.VERSION_SUB)

#define COPYRIGHT	\
"Copyright (C) 2016-2018 Laszlo Csirmaz, Central European University, Budapest"

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
static int get_param(const char *str);
static void set_param(const char *str,int value);
#define UNUSED __attribute__((unused)) 

/***********************************************************************
* Auxiliary variables
*
*  char *HISTORY_FILE
*     read from and save history to this file.
*  int done
*     has value zero until the "quit" command is encountered, which set
*     this variable to 1
*  int in_minitiprc
*     set 1 while executing minitip.RC
*  syntax_style_t minitip_style
*     the actual style to parse expressions
*  char minitip_sepchar
*     the actual separator character
*  int cmdarg_position
*     store the position where the parsed string starts. When the parser
*     returns an error, the position is relative to the start of the string,
*     this variable is used to offset the starting position.
*/
static char *HISTORY_FILE=NULL;
static int done=0;
static int in_minitiprc=0;
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
*  int resize_constraint_table(newsize)
*     resize the constraint table to the given one. Returns the new
*     size which is the max of occupied slots and newsize
*/
static int max_constraints=0;
static int constraint_no=0;
char **constraint_table=NULL;

static int resize_constraint_table(int newsize)
{char **newtable;
    if(newsize< constraint_no) newsize=constraint_no;
    newtable=realloc(constraint_table,newsize*sizeof(char*));
    if(newtable==NULL){ // no change
       return max_constraints; 
    }
    max_constraints=newsize; constraint_table=newtable;
    return max_constraints;
}

/***********************************************************************
*  int cmp_s(char *s2, char *s1)
*   return 0 when s2 starts with s1 followed by a space or a tab; or
*     when s2 and s1 are equal; otherwise return 1.
*
* int strstart(char *str, char *pattern)
*   if str starts with the same characters as pattern until space, tab,
*   zero, then return the number of matched chars; otherwise return
*   -1.
*/
static int cmp_s(const char*s2,const char*s1)
{   while(*s1 && *s1==*s2){s1++;s2++;}
    return (*s2==0 || *s2==' '|| *s2=='\t') ? 0 : 1;
}
static int strstart(const char *str, const char *pattern)
{int i=0;
    if(*str != *pattern) return -1;
    while(*str==*pattern){ i++; str++; pattern++; }
    if(*str==0 || *str==' ' || *str == '\t') return i;
    return -1;
}
/***********************************************************************
*  char *prepare_filename(char *rawstring)
*    create another string from the argument:
*    - replace leading ~/ by the value of the HOME variable
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
        if(home){ /* home directory found */
            strncpy(buff,home,MAX_PATH_LENGTH);buff[MAX_PATH_LENGTH]=0;
            i=strlen(buff); raw++;
        }
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
*   structure containing available commands. An entry has the fields:
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
  com_add,	/* add constraint */
  com_list,	/* list constraints */
  com_del,	/* delete constraint */
  com_style,	/* style: change / help*/
  com_syntax,	/* formula syntax help */
  com_nocon,	/* check without constraints */
  com_diff,	/* difference of two expressions */
  com_about,	/* print license info */
  com_args,	/* command line arguments */
  com_macro,	/* handle macros: del, list, define */
  com_batch,	/* execute commands from a file */
  com_save,	/* save history */
  com_dump,	/* dump constraints and macro definitions */
  com_set;	/* list / set parameters */

/* forward declarations: parameter completion */
static rl_compentry_func_t
  cmd_name,	/* help */
  pm_syntax,	/* syntax */
  pm_style,	/* style */
  pm_list,	/* list */
  pm_help,	/* offer 'help' */
  pm_macro,	/* macro */
  pm_set;	/* set */
static rl_completion_func_t
  am_set;	/* set parameter value */

/* the COMMAND structure */
typedef struct {
    char *name;         /* command string */
    com_func_t *func;   /* function which executes the command */
    int pfile;          /* allow filename completion in argument position */
    rl_compentry_func_t
        *pcomp;         /* argument completion function */
    rl_completion_func_t
        *acomp;		/* completion function for first argument value */
    char *doc;          /* short document */
} COMMAND;

static COMMAND commands[] = {
/* name     func   pfile pcomp      acomp       doc              */
{"quit",   com_quit,  0, NULL,      NULL,	"quit minitip" },
{"help",   com_help,  0, cmd_name,  NULL,	"display this text" },
{"?",      com_help,  0, cmd_name,  NULL,	"synonym for 'help'" },
{"check",  com_check, 0, pm_help,   NULL,	"check entropy relation with constraints" },
{"test",   com_check, 0, pm_help,   NULL,	"synonym for 'check'" },
{"xcheck", com_nocon, 0, pm_help,   NULL,	"check entropy relation without constraints" },
{"add",	   com_add,   0, pm_help,   NULL,	"add new constraint" },
{"list",   com_list,  0, pm_list,   NULL,	"list all or specified constraints: 3,5-7"},
{"del",	   com_del,   0, pm_help,   NULL,	"delete numbered constraint"},
{"zap",	   com_diff,  0, pm_help,   NULL,	"print missing entropy terms on RHS"},
{"macro",  com_macro, 0, pm_macro,  NULL,	"add, list, delete macros"},
{"run",    com_batch, 1, NULL,      NULL,	"execute commands from a file"},
{"style",  com_style, 0, pm_style,  NULL,	"show / change formula style"},
{"syntax", com_syntax,0, pm_syntax, NULL,	"describe how to enter entropy formulas"},
{"set",    com_set,   0, pm_set,    am_set,	"list / set runtime parameters"},
{"dump",   com_dump,  1, NULL,      NULL,	"dump constraints and macro definitions to a file"},
{"save",   com_save,  1, NULL,      NULL,	"save command history to a file"},
{"about",  com_about, 0, NULL,      NULL,	"history, license, author, etc"},
{"args",   com_args,  0, NULL,      NULL,       "accepted command line arguments"},
{NULL,     NULL,      0, NULL,      NULL,	NULL }
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
*  int which_command(char *text, int *over)
*     return the index of the command at the front, and the position
*     of the next non-space character.
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
  "style","variable","entropy","expression","relation","constraint","macro","zap",NULL };
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
/* find command */
static int which_command(const char*text, int *over)
{int cmd,j,len;
    for(cmd=-1,j=0;cmd<0 && commands[j].name;j++){
        len=strlen(commands[j].name);
        if(strncmp(text,commands[j].name,len)==0 &&
           (text[len]==0 || text[len]==' '||text[len]=='\t') ) cmd=j;
    }
    if(cmd<0 && get_param("abbrev")==1){ // abbreviation allowed
        len=0;
        for(len=0,j=0;commands[j].name;j++){
           if(strstart(text,commands[j].name)>0) len++;
        }
        if(len==1){
           for(j=0;cmd<0 && commands[j].name;j++){
              len=strstart(text,commands[j].name);
              if(len>0) cmd=j;
           }
        }
    }
    if(cmd>=0){ // skip space and tab
        for(;text[len]==' '||text[len]=='\t'; len++);
        *over=len;
    }
    return cmd;
}
/* the readline completion procedure */
static char **cmd_completion(const char*text, int start, UNUSED int end)
{char **matches=NULL;
  rl_attempted_completion_over=1; /* that's all folks */
  if(start==0){
      matches=rl_completion_matches(text,cmd_name);
  } else { // check that this is the first argument after the command
      int j,cmd;
      cmd=which_command(rl_line_buffer,&j);
      if(cmd>=0){
        if(j==start){ // first argument  
            if(commands[cmd].pfile) rl_attempted_completion_over=0;
            if(commands[cmd].pcomp)
               matches=rl_completion_matches(text,commands[cmd].pcomp);
        } else if(commands[cmd].acomp) {
             matches=commands[cmd].acomp(text,start,j);
        }
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
*   return value: 
*       0  -- no error; batch file can continue
*       1  -- continue batch file when loose
*       2  -- fatal error, cannot continue
*/
static int execute_cmd(char *text,const char *line)
{int i,j,len,cmd;
    xassert(text!=NULL);
    for(i=0;text[i]==' '|| text[i]=='\t';i++);
    if(text[i]==0 || (line && text[i]=='#')){ // empty or comment line
        if(line && text[i]=='#' && get_param("comment")==1 && !in_minitiprc ){
           printf("%s\n",line);
        }
        return 0;
    }
    if(text[i]=='#') return 0; 
    // remove white spaces from the end
    for(j=strlen(text)-1;j>=0 && (text[j]==' '|| text[j]=='\t');j--) text[j]=0;
    cmd=which_command(text+i,&len);
    if(cmd>=0){ // execute the command which have been found
      i+=len;
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
*   com_args (char *arg, char *line)    command line arguments
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
*   com_dump (char *arg, char *line)    dump constraints and macro definitions
*   com_save (char *arg, char *line)    save history
*   com_set  (char *arg, char *line)    list/set parameters
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
"There is ABSOLUTELY NO WARRANTY, use at your own risk.\n\n"
COPYRIGHT
"\n");
    return 0; /* OK */
}

/** ARGS -- list of command line arguments **/
static int com_args(UNUSED const char *arg, const char *line)
{   if(!line)
    printf(
"the following command line flags are accepted when used interactively:\n"
"   -s         -- start using minimal syntax style (default, same as '-s,')\n"
"   -s<chr>    -- minimal style using <chr> as the separator character\n"
"   -S         -- start using full (standard) syntax style\n"
"   -f <file>  -- use <file> as the command history file (default: " DEFAULT_HISTORY_FILE ")\n"
"   -c <file>  -- use <file> as the config file (default: " DEFAULT_RC_FILE ")\n"
"   -c-        -- don't read the default config file\n"
"   -m <macro> -- add this macro definition\n"
"\n"
"the following flags imply non-iteractive usage:\n"
"   -q         -- quiet, just check, don't print anything\n"
"   -v         -- version and copyright information\n"
"   -e         -- last flag, followed by the expression to be checked\n"
"   <expr> <constr1> <constr2> ...\n"
"              -- <expr> is checked using the given constraints\n"
"\n");
    return 0; /* OK */
}

/** HELP -- give a comprehensive help on commands **/
static int com_help(const char *arg, const char *line)
{int i, printed=0;
    for(i=0;commands[i].name;i++){
        if(!*arg || cmp_s(arg,commands[i].name)==0){
            printf(" %s\t\t%s\n",commands[i].name,commands[i].doc);
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
        if(get_param("save")==1){ c='y'; }
        else if(get_param("save")==2){ c='n'; }
        else { // ask
          printf("Save commands to the history file %s (y/n)? ",HISTORY_FILE);
          c=yesno(); while(c!='y' && c!='n' && c!='Y' && c!='N'){
            printf("             please hit 'y' or 'n' (y/n)? ");
            c=yesno(); }
        }
       done=1;
       if(c=='y'||c=='Y') write_history(HISTORY_FILE);
    }
    else printf(" No arguments are accepted.\n");
    return 0; /* OK */
}

/** SYNTAX  -- give help on syntax of different constructs **/
static int com_syntax(const char *argv, const char *line)
/** style / variable / entropy / expression / relation / constraint / macro / zap **/
{
/** STYLE **/
    if(strncasecmp(argv,"style",3)==0){ // on style
        printf(
"Minitip can work in 'full' (standard) or in 'simple' style, which\n"
"  determines random =>variables and =>entropy terms are parsed.\n"
"FULL (or standard) style\n"
"  Random variables are identifiers such as X12 or Winter. The entropy\n"
"  and mutual information follows the standard: H(X1,Winter) is the\n"
"  joint entropy of X1 and Winter; I(A,Winter;X1,Y|Z) is the\n"
"  conditional mutual information of A,Winter and X1,Y given Z.\n"
"SIMPLE (or lazy) style\n"
"  Random variables are single lower case letters (additional primes are\n"
"  allowed) such as x or x'. Put variables next to each other for their\n"
"  joint distribution. Any list such as abc denotes its own entropy.\n"
"  In terms letter H for entropy and I for mutual information can be\n"
"  omitted. Variable lists are separated by comma (default) or by some\n"
"  other specified character. With , as separator I(ab,xy|z) or (ab,xy|z)\n"
"  is the conditional mutual information of a,b and x,y given z. The same\n"
"  entropy term with separator : is written as (ab:xy|z).\n"
"Enter 'style full' or 'style simple <separator-char>' to set the style.\n"
"Warning: changing the style deletes all stored =>constraints.\n"
"The present style is ");
        if(minitip_style==syntax_short) printf(
"SIMPLE (lazy) using \"%c\" as separator.\n",minitip_sepchar);
        else printf("FULL (standard).\n");
        return 0;
    }
/** VARIABLES **/
    if(strncasecmp(argv,"var",3)==0){ // give syntax of vars and var list
        printf(
"The form of random variables and variable lists depends on the =>style.\n");
        printf( minitip_style==syntax_short ?
// simple style
"In simple style random variables are lower case letters from a to z,\n"
"optionally followed by a sequence of primes such as a, a', a'', etc.\n"
"Put variables next to each other for their joint distribution, such\n"
"as ac'rs' or abrstu.\n" :
// full style
"In full style random variables are identifiers such as X, Snow_fall,\n"
"Winter; primes can be appended so you can use A' or Winter' as well.\n"
"Variables are case sensitive thus x and X denote different variables.\n"
"Variable list is a sequence separated by commas, such as X',y,Y or\n"
"Snow_fall,Winter.\n");
        return 0;
    }
    if(strncasecmp(argv,"entrop",3)==0){ // entropy syntax
/** ENTROPY **/
        printf(
"Basic entropy terms such as conditional entropy or mutual information\n"
"are built from a list of random =>variables. The notation depends on\n"
"the =>style. ");
        if(minitip_style==syntax_short) printf(
// simple style
"In simple (lazy) style with separating character %1$c these\n"
"terms can be abbreviated to minimal:\n"
"a) the joint entropy of the list of random variables a,c,r,s is entered as\n"
"     H(acrs)     or  acrs\n"
"b) the conditional entropy H(a,b|r,s) can be written as\n"
"     H(ab|rs)    or  (ab|rs)\n"
"c) the mutual information I(a,b;r,s) is entered as\n"
"     I(ab%1$crs)    or  (ab%1$crs)\n"
"d) the conditional mutual information I(a,b;r,s|c,d) is\n"
"     I(ab%1$crs|cd) or  (ab%1$crs|cd)\n"
"e) the Ingleton expression  -(a%1$cb)+(a%1$cb|c)+(a%1$cb|d)+(c%1$cd) is\n"
"     [a%1$cb%1$cc%1$cd]\n"
"f) an invocation of the three argument =>macro X(%1$c|) is entered as\n"
"     X(ab%1$cc|a)\n",minitip_sepchar);
        else printf(
// full style
"In full (standard) style it follows the standard notation:\n"
" a) H(W,S)       entropy of the joint distribution of W and S\n"
" b) H(W,S|F,T)   conditional entropy\n"
" c) I(W,F;S)     mutual information of W,F and S\n"
" d) I(W,F;S|day) conditional mutual information\n"
" e) [A;B;C;D]    shorthand for the Ingleton expression\n"
"                     -I(A;B)+I(A;B|C)+I(A;B|D)+I(C;D)\n"
" f) X(A,B;C|A)   invocation of the three argument =>macro X(;|).\n");
        return 0;
    }
/** MACRO **/
    if(strncasecmp(argv,"macro",4)==0){ // macro
        printf(
"A MACRO is a shorthand for an entropy =>expression. A macro definition\n"
"starts with a capital letter from A-Z followed by the argument list which\n"
"is enclosed in parentheses. Arguments are separated by either %c or |.\n"
"The same macro name can identify several different expressions depending\n"
"on the number of arguments and the separator characters. The following\n"
"lines define two different macros with four arguments each:\n",minitip_sepchar);
        if(minitip_style==syntax_short) printf(
//simple style
"    macro add T(x%1$cy|t%1$cz) = 3(tx%1$cy|z)+2(x%1$cty|z)+(t%1$cz|xy)\n"
"    macro add T(a|b|c%1$cd) = -(a|bc)+2(a%1$cc|bd)-7*[a%1$cb%1$cc%1$cd]\n",minitip_sepchar);
        else printf(
//full style
"    macro add T(X;Y|Z1;Z2) = 3I(Z1,X;Y|Z2)+2I(X;Y,Z2|Z1)+H(X,Y|Z1,Z2)\n"
"    macro add T(a|b|c;d) = -H(a|b,c) + 2I(a;c|b,d)-7*[a;b;c;d]\n");
        printf(
"Only variables in the argument list can be used in the right hand side\n"
"expression. Macros in the expression are expanded, so should be defined\n"
"earlier. When invoking a macro, each argument can be either a =>variable\n"
"or a variable list; the separators must match those in the definition. Thus\n");
       if(minitip_style==syntax_short) printf(
"      3*T(ac%1$cad|bc%1$cbd) -4T(xu|yu|t%1$cu)\n",minitip_sepchar);
       else printf(
"      3*T(A,C;A,D|B,C;B,D) - 4T(X1,Z2|X2,Z2|Y1;Y2,Z2)\n");
       printf(
"expands the first and the second definition, respectively. Macros are\n"
"stored and printed out in raw format using entropies only.\n"
"\n"
"Use 'macro add', 'macro list', 'macro delete' to add, list, or delete\n"
"macros. The 'add' keyword can be omitted.\n");
       return 0;
    }
/** EXPRESSION **/
    if(strncasecmp(argv,"expr",3)==0){ // linear combination
        printf(
"An EXPRESSION is a linear combination of =>entropy terms and =>macro\n"
"invocations, such as\n");
        if(minitip_style==syntax_short) printf(
"      -1.234*(x|y) - 12.345(a%1$cb|h) + 3X(x%1$cb|ay)\nwhere X(%1$c|)",minitip_sepchar);
        else printf(
"       -1.234*H(X|Y) - 12.345I(a;b|H) + 3X(X;b|a,Y)\nwhere X(;|)");
        printf(
" is a macro. The * sign between the constant and the\n"
"entropy term is optional and can be omitted.\n");
        return 0;
    }
/** RELATION **/
    if(strncasecmp(argv,"relation",3)==0){ //formula description
        printf(
"A RELATION compares two entropy =>expressions using = (equal),\n"
"<= (less or equal) or >= (greater or equal) as in\n");
        if(minitip_style==syntax_short) printf(
// simple style
"       I(x%1$cy) +3H(a)-1.234* I(x%1$ca|z) >= 0\n"
"       -1.234*(x|y) - 12.234*(a%1$cb|h) <= -2bxy\n"
"       (b%1$cd|a'c)+(b%1$cc|a') = (b%1$ccd|a')\n",minitip_sepchar);
        else printf(
// full style
"       I(X;Y)+3 H(A) -1.234 I(X;A|Z) >= 0\n"
"       +1.234*H(X|Y) - 12.234*I(A;B|H) <= -2H(B,X,Y)\n"
"       I(X;Y1|Z,Y2)+I(X;Y2|Z) = I(X;Y1,Y2|Z)\n");
        printf(
"Only these three comparison operators can be used. Any side (but not\n"
"both) can be zero as in the first example.\n"
"An entropy relation can be checked for validity either with or without\n"
"constraints; and can be added as a =>constraint. Use 'check' or 'test'\n"
"for checking with constraints, 'xcheck' for checking without constraints;\n"
"and 'add' to add it as a constraint. Keywords 'check' and 'test' can be\n"
"omitted if the first character of the relation is not a letter.\n");
        return 0;
    }
/** CONSTRAINT **/
    if(strncasecmp(argv,"const",3)==0){ // constraint
        printf(
"When checking the validity of an entropy =>relation, it is done relative\n"
"to a set of CONSTRAINTS. A constraint is one of the following:\n"
"*  a =>relation, that is two entropy =>expressions compared by one of\n"
"     =, <= or >=0\n"
"*  functional dependency: the first =>variable list is determined by the\n"
"   second one:\n"
"         varlist1 : varlist2\n"
"*  independence: the =>variable lists are totally independent:\n"
"         varlist1 .  varlist2 .  varlist3 .  ...\n"
"     or  varlist1 || varlist2 || varlist3 || ...\n"
"*  Markov chain: the =>variable lists form a Markov chain:\n"
"         varlist1 /  varlist2 /  varlist3 /  ...\n"
"     or  varlist1 -> varlist2 -> varlist3 -> ...\n"
"Use the command 'add' to add a constraint; 'list' to list them; and\n"
"'del' to remove some or all of the constraints.\n");
        return 0;
    }
/** ZAP **/
    if(strncmp(argv,"zap",3)==0){ // zap
        printf(
"Calculate the missing terms on the right hand side of two =>expressions\n"
"connected by '=='. Leave the right hand side empty to print the formula as\n"
"a composition of entropies. Example:\n");
        if(minitip_style==syntax_short) printf(
// simple style
"        zap (a%1$cb|c)+(b%1$cc|a)+(c%1$ca|b) ==\n"
"Result:\n"
"         ==> -a-b-c+2ab+2ac+2bc-3abc\n",minitip_sepchar);
        else printf(
// full style
"        zap I(A;B|C)+I(B;C|A)+I(C;A|B) =='\n"
"Result:\n"
"         ==> -H(A)-H(B)-H(C)+2H(A,B)+2H(A,C)+2H(B,C)-3H(A,B,C)\n");
        printf(
"Similarly to the 'check' and 'test' keywords, 'zap' can be omitted if\n"
"the first character of the expression is not a letter.\n");
        return 0;
    }
    if(!line) /* only if online */
    printf(
"Please enter one of the following topics:\n"
"  style      -- choose between \"simple\" and \"full\" style\n"
"  variables  -- random variables and sequences of random variables\n"
"  entropy    -- entropy term syntax and shorthand\n"
"  macro      -- macros, what they are\n"
"  expression -- linear combination of entropy terms and macros\n"
"  relation   -- compare two expressions by =, <= or >=\n"
"  constraint -- syntax of constraints\n"
"  zap        -- calculate the missing terms on the right hand side\n");
    return 0; /* OK */
}
/** ADD  -- add a constraint **/
static int com_add(const char* line,const char *orig)
{int i;
    if(*line==0 || *line=='?' || strncmp(line,"help",4)==0){ // empty line, help
        if(!orig) printf(" Add a new constraint, which can be\n"
        " *  an equality or inequality comparing two entropy expressions; or\n"
        " *  functional dependency; total independence; or Markov chain.\n"
        " Enter 'syntax constraint' for more help.\n" );
        return 0;
    }
    // check if it is there ...
    for(i=0;i<constraint_no;i++) if(strcmp(line,constraint_table[i])==0){
        if(batch_depth>0) return 0;
        printf(" This constraint is #%d, no need to add again\n",i+1);
        return 1; /* abort */
    }
    if(parse_constraint(line,0)!=PARSE_OK){ // some error
        error_message(orig);
        return 2; /* abort */
    }
    if(constraint_no>=max_constraints-1){
        if(orig)printf("%s\n",orig);
        printf("ERROR: too many constraints (max %d)\n"
               "  use 'del <number>' to delete some constraints\n",max_constraints);
        return 2; /* fatal; abort */
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
    *to=d;
    return i;
}
static void list_header(int reset)
{static int not_printed=1;
    if(reset){ not_printed=1; }
    else if(not_printed){
       not_printed=0;
       printf(" Constraints (total %d)\n",constraint_no);
    }
}

/** LIST -- list all or some of the constraints **/
static int com_list(const char *arg, const char *orig)
{int i0,i1,pos;
    if(*arg=='?' || strcmp(arg,"help")==0){ // help
        if(constraint_no==0) printf(" There are no constraints to be listed.\n");
        else printf(" The number of constraints is %d\n",constraint_no);
        printf(" Use 'list all' to show all; 'list -10' to show the first 10 constraints.\n"
               " In general, 'list 3,4-6,8' shows constraints #3,#4 to #6, and #8. Enter\n"
               " 'syntax constraint' for help on constraints.\n");
        return 0;
    }
    if(constraint_no==0){
       if(!orig) printf(" There are no constraints to be listed.\n");
       return 0; 
    }
    list_header(1);
    if(*arg==0 || strcmp(arg,"all")==0){ // list first ten or all constraints
        list_header(0);
        i1=constraint_no;
        if(10<i1 && *arg==0) i1=10;
        for(i0=0;i0<i1;i0++){
            printf("%3d: %s\n",i0+1,constraint_table[i0]);
        }
        if(i1<constraint_no && *arg==0){
            printf("...\n");
        }
        return 0;
    }
    i0=-1;
    while(*arg)switch(*arg){
  case ' ': case '\t': arg++; break;
  case ',': arg++; if(i0>0 && i0<=constraint_no){
               list_header(0);
               printf("%3d: %s\n",i0,constraint_table[i0-1]);
            }
            i0=-1;
            break;
  case '-': arg++; while(*arg==' '||*arg=='\t') arg++;
            if(i0<0) i0=1; // handle 'list -10'
            pos=read_number(arg,&i1);
            if(!pos || i0==0 || i0>i1){ printf("   Wrong syntax, use 'list 3,4-6,8'\n"); return 1; }
            arg += pos;
            list_header(0);
            for(; i0<=i1 && i0<=constraint_no; i0++){
               printf("%3d: %s\n",i0,constraint_table[i0-1]);
            }
            i0=0;
            break;
  default:  pos=read_number(arg,&i1);
            if(!pos || i1==0 ){ printf("   Wrong syntax, use 'list 3,4-6,12-14'\n"); return 1; }
            arg += pos;
            if(i0>0 && i0<=constraint_no){
               list_header(0);
               printf("%3d: %s\n",i0,constraint_table[i0-1]);
            }
            i0=i1;
            break;
    }
    if(i0>0 && i0<=constraint_no){
        list_header(0);
        printf("%3d: %s\n",i0,constraint_table[i0-1]);
    }
    return 0; /* OK */
}
/** DEL -- delete all or a single constraint **/
static int com_del(const char *arg, const char *line)
{int pos,no;
    if(constraint_no==0){
        if(!line) printf(
" There are no constraints to delete.\n"
" Enter 'syntax constraint' for help on constraints.\n");
        return 0; /* OK */
    }
    if(*arg==0 || *arg == '?' || strcmp(arg,"help")==0){
        if(!line)
        printf(
" Specify the constraint to be deleted from 1 to %d, or say 'del all'.\n"
" Enter 'syntax constraint' for help on constraints.\n",  constraint_no);
        return 0; /* OK */
    }
    if(strcmp(arg,"all")==0){
        int c='y';
        if(!line){
           printf(" All constraints (%d) will be deleted. Proceed (y/n)? ",constraint_no);
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
           printf(" Specify exactly one constraint index from 1 to %d, or say 'all'\n",constraint_no);
        return 2;
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
                printf(" Expression style is FULL, not changed.\n");
            return 0; /* no change, OK */
        }
        if(constraint_no>0){
            if(line){
                printf(" Cannot change style to FULL when there are constraints.\n");
                return 2; /* abort */
            }
            printf(" Changing style will delete all constraints (%d). Proceed (y/n)? ",
              constraint_no);
            int c=yesno(); if(c!='y' && c!='Y') return 1;
            while(constraint_no>0){
                constraint_no--; free(constraint_table[constraint_no]);
            }
        }
        minitip_style=syntax_full; minitip_sepchar=';';
        set_syntax_style(minitip_style,minitip_sepchar,get_param("simplevar"));
        if(!line)
            printf(" Expression style is set to FULL.\n");
        return 0; /* OK */
    }
    if(strncasecmp(argv,"simple",6)==0){
        char sepchar=',';
        argv+=6; while(*argv==' ' || *argv=='\t') argv++;
        if(*argv) sepchar=*argv;
        if(!strchr(minitip_SEPARATOR_CHARS,sepchar)){
            printf(" Wrong separator character for 'simple' style. Possibilities are %s\n",minitip_SEPARATOR_CHARS);
            return 2; /* abort */
        }
        if(minitip_style==syntax_short && sepchar == minitip_sepchar){
            if(!line)
                printf(" The present style is SIMPLE using '%c' as separator, not changed.\n",minitip_sepchar);
            return 0;
        }
        if(constraint_no>0){
            if(line){
                printf(" Cannot change style to SIMPLE when there are constraints.\n");
                return 2; /* abort */
            }
            printf(" Changing style will delete all constraints (%d). Proceed (y/n)? ",
              constraint_no);
            int c=yesno(); if(c!='y' && c!='Y') return 1;
            while(constraint_no>0){
                constraint_no--; free(constraint_table[constraint_no]);
            }
        }
        minitip_style=syntax_short; minitip_sepchar=sepchar;
        set_syntax_style(minitip_style,minitip_sepchar,get_param("simplevar"));
        if(!line)
           printf(" Expression style is set to SIMPLE using '%c' as separator.\n",
                   minitip_sepchar);
        return 0; /* OK */
    }
    if(line && strcasecmp(argv,"help")!=0) return 1; /* abort */
    if(minitip_style==syntax_short) printf(
" The present style is SIMPLE using '%c' as separator.\n",minitip_sepchar);
    else printf(
" The present style is FULL (standard).\n");
    printf(
" Enter 'style full' to set the style to FULL (standard), or 'style simple'\n"
" followed by the character to be used as separator (default: ',') to set\n"
" the style to SIMPLE (lazy). Enter 'syntax style' for more help.\n");
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
{   set_syntax_style(syntax_short,',',1);
    if(parse_macro_definition("H(a)=a")!=PARSE_OK) error_message(NULL);
    if(parse_macro_definition("H(a|b)=ab-b")!=PARSE_OK) error_message(NULL);
    if(parse_macro_definition("I(a,b)=a+b-ab")!=PARSE_OK) error_message(NULL);
    if(parse_macro_definition("I(a,b|c)=ac+bc-c-abc")!=PARSE_OK) error_message(NULL);
    standard_macros = macro_total; 
//  if(parse_macro_definition("D(a,b,c)=(a,b|c)+(b,c|a)+(c,a|b)")) error_message(NULL);
}

/** MACRO add -- define a new macro **/
static int com_macro_add(const char *arg, const char* line)
{
    if(!*arg || *arg=='?' || strcmp(arg,"help")==0){
         if(!line){ printf(
" add a new macro to be used in entropy expressions. Example:\n"
"       macro add ");
             if(minitip_style==syntax_short) printf(
     "X(a%1$cb|c%1$cd) = (a%1$cb)+(b|c)+(a|c)+(a|d)-(c%1$cd)\n",minitip_sepchar);
             else printf(
     "X(A;B|C;D) = I(A;B)+H(B|C)+H(A|C)+H(A|D)-I(C;D)\n");
             printf(
" The same macro name can be used repeatedly with different set of argument\n"
" separators; they define different macros. The keyword 'add' can be omitted\n"
" after 'macro'. Enter 'syntax macro' for more help on macros.\n");
         }
         return 0;
    }
    /* 0: error, 1: keep old, 2: replace */
    if(parse_macro_definition(arg)!=PARSE_OK){ /* some error */
        error_message(line); return 2; /* abort */
    }
    return 0; /* OK, stored */
}
/** MACRO list -- list macros associated with a letter **/
static int com_macro_list(const char *arg, const char* line)
{int printit[30]; int name,lastname; int i,error;
    if(*arg == '?' || strcmp(arg,"help")==0){
        if(!line) printf(
" To list all macros, use 'macro list' without arguments. To list macros\n"
" A, F, G, H only, use 'macro list A, F-H'. For more help on macros,\n"
" enter 'syntax macro'.\n");
        return 0;
    }
    if(macro_total<=standard_macros){
        if(!line) printf(" No macros are defined.\n");
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
           printf(" Wrong syntax, use 'macro list A, F-H'\n");
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
    if(!*arg || *arg=='?' || strcmp(arg,"help")==0){
        if(!line){ printf(
" To delete a macro specify its header only, such as '");
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
        printf(" ERROR: no match was found. Enter 'macro list' to list all macros.\n");
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
       if(macro_total<=standard_macros){
           if(!line) printf(
" add, list, or delete macros. For more help, type 'syntax macro'.\n");
           return 0;
       } else {
           return com_macro_list(arg,line);
       }
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

/** SET -- list / set parameters **/
typedef struct {
    char *name;		/* parameter name */
    char *type;		/* NULL: int, "": string, "a/b/c": choice */
    int value;		/* value */
    int ll,ul;		/* lower limit, upper limit */
    char *doc;		/* short documentation */
} PARAMETERS;

static PARAMETERS parameters[] = {
/* name		type	 value	lower	upper		doc                */
{"iterlimit",	NULL,	80000,	100,	100000000,	"LP iteration limit"},
{"timelimit",	NULL,	10,	1,	10000,		"LP time limit in seconds"},
{"constrlimit", NULL,	50,	10,	100000,		"maximal number of constraints"},
{"macrolimit",	NULL,	50,	10,	100000,		"maximal number of macros"},
{"run",		"strict/loose",	1,1,2,			"strict/loose - how to handle errors in run file"},
{"comment",	"yes/no",	2,1,2,			"yes/no - show comments from run file"},
{"abbrev",	"yes/no",	2,1,2,			"yes/no - allow abbreviated commands"},
{"save",	"yes/no/ask",	3,1,3,			"yes/no/ask - save command history at exit"},
{"simplevar",	"basic/extended",1,1,2,			"basic/extended - accept 'a123' as a variable"},
{"history",	"",		1,1,1,			"default command history file"},
{NULL,		NULL,	0,	0,	0,		NULL}
};

/* show parameter's value */
static char *show_choice(const char *str, int c)
{static char buf[50]; char *b;
   while(c>1){
      c--; while(*str && *str!='/') str++;
      if(*str=='/') str++;
   }
   b=&buf[0]; while(*str && *str!='/'){*b=*str; b++; str++; }
   *b=0;
   return buf;
}
static void show_parameter(char *buf,int idx)
{PARAMETERS *p=&parameters[idx];
char *str=p->type;
    if(str==NULL){ // integer parameter
      sprintf(buf,"%s=%d",p->name,p->value);
      return;
    }
    if(*str==0){ // default history file
      sprintf(buf,"%s=%s",p->name,HISTORY_FILE);
      return;
    }
    // choice
    sprintf(buf,"%s=%s",p->name,show_choice(str,p->value));
    return;
}
/* completing runtime parameters */
static char *pm_set(const char *txt, int state)
{static int list_index=0; const char *name; char buf[MAX_PATH_LENGTH+30];
    if(!state){ list_index=0; }
    while((name=list_index> 0 ? parameters[list_index-1].name : "help")!=NULL){
       list_index++;
       if(strncmp(name,txt,strlen(txt))==0){
           if(list_index==1) return strdup(name); // help
           int i,cnt;
           for(i=0,cnt=0;parameters[i].name;i++)
              if(strncmp(parameters[i].name,txt,strlen(txt))==0) cnt++;
           if(cnt>1) return strdup(name);
           show_parameter(buf,list_index-2);
           return strdup(buf);
       }
    }
    return (char*)NULL;
}
/* completing runtime parameter values */
static const char *pm_type=NULL;
static char *am_value(const char *txt, int state)
{static int list_index=1; char *value;
    if(!state){ list_index=1; }
    while(*(value=show_choice(pm_type,list_index))!=0){
       list_index++;
       if(strncmp(value,txt,strlen(txt))==0){
          return strdup(value);
       }
    }
    return (char*)NULL;
}
static char *am_equal(const char *txt, int state)
{static int list_index=0;
    if(!state){ list_index=0; }
    if(list_index==0){ list_index=1;
        if(strncmp("=",txt,strlen(txt))==0){
            return strdup("=");
        }
    }
    return (char*)NULL;
}
static char **am_set(const char *txt, int start, int pmstart)
{char **matches=NULL; int len; char *name; PARAMETERS *P,*cmd;
    cmd=NULL;
    for(P=&parameters[0],name=P->name; cmd==NULL && name; P++,name=P->name){
        len=strlen(name);
        if(strncmp(rl_line_buffer+pmstart,name,len)==0) cmd=P;
    }
    if(cmd==NULL) return matches;
    while(rl_line_buffer[pmstart+len]==' ') len++;
    if(start==pmstart+len){ matches=rl_completion_matches(txt,am_equal); }
    else if(rl_line_buffer[pmstart+len]=='='){
      len++; while(rl_line_buffer[pmstart+len]==' ') len++;
      if(start==pmstart+len && (pm_type=cmd->type)!=NULL){
        if(*pm_type==0) rl_attempted_completion_over=0;
        else matches=rl_completion_matches(txt,am_value);
      }
    }
    return matches;
}
/* list / set runtime parameters */
static int com_set(const char *arg, const char *line)
{PARAMETERS *P; int n; char buf[MAX_PATH_LENGTH+50];
    if(*arg==0){ // list of parameters
        for(n=0,P=&parameters[0];P->name;P++,n++){
            show_parameter(buf,n);
            printf(" %-20s (%s)\n",buf,P->doc);
        }
        return 0;
    }
    for(P=&parameters[0];P->name;P++){
        n=strlen(P->name);
        if(strncasecmp(arg,P->name,n)==0){
           while(arg[n]==' ') n++;
           if( arg[n]!='=') continue;
           arg+=n+1;
           if(P->type==NULL){ // numerical argument
                n=-1;
                if(1!=sscanf(arg,"%d",&n) || n<P->ll || n>P->ul){
                    printf(" Parameter value is out of limits %d .. %d\n",
                       P->ll, P->ul);
                    return 1;
                }
                if(strcmp(P->name,"constrlimit")==0){
                    n=resize_constraint_table(n);
                } else if(strcmp(P->name,"macrolimit")==0){
                    n=resize_macro_table(n);
                }                P->value=n;
                return 0;
           }
           if(*(P->type)==0){ // history file
                char *newfile=prepare_filename(arg);
                if(newfile==NULL){
                    printf(" Wrong filename syntax\n");
                    return 1;
                }
                free(HISTORY_FILE); HISTORY_FILE=newfile;
                return 0;
           }
           while(*arg==' '||*arg=='\t') arg++;
           for(n=P->ll; n<=P->ul; n++){
                if(strcmp(show_choice(P->type,n),arg)==0){
                    if(strcmp(P->name,"simplevar")==0)
                      set_syntax_style(minitip_style,minitip_sepchar,n);
                    P->value=n; return 0;
                }
           }
           printf(" Wrong or missing value\n");
           return 1;
        }
    }
    if(*arg=='?' || strncmp(arg,"help",4)==0){ // help
       if(!line) printf(
         " List / set runtime parameters. Enter 'set' without arguments to list\n"
         " parameters; enter 'set parameter=value' to change the value of a\n"
         " single parameter.\n");
       return 0;
    }
    if(line) printf("%s\n",line);
    printf(" Unknown runtime parameter. Enter 'set' without arguments to list\n"
           " all parameters.\n");
    return 1; /* OK */
}
/* get actual value of a parameter */
static int get_param(const char *str)
{PARAMETERS *P;
    for(P=&parameters[0];P->name;P++){
        if(strcmp(P->name,str)==0) return P->value;
    }
    return 0;
}
static void set_param(const char *str,int value)
{PARAMETERS *P;
    for(P=&parameters[0];P->name;P++){
        if(strcmp(P->name,str)==0) P->value=value;
    }
}
/** SAVE -- save history file **/
static int com_save(const char *arg, const char *line)
{char *filename;
    if(*arg=='?' || strcmp(arg,"help")==0){
       if(!line) printf(
        " Type 'save' or 'save <file>' to save command history to\n"
        " the default '%s', or to <file>. Load history using the\n"
        " command line argument '-f <file>'\n",HISTORY_FILE);
       return 0;
    }
    if(line) printf("%s\n",line);
    while(*arg==' '|| *arg=='\t') arg++;
    if(*arg==0){int c;
        filename = HISTORY_FILE;
        if(!line){
          printf("Save command history to %s (y/n)? ",filename);
          c=yesno(); while(c!='y' && c!='n' && c!='Y' && c!='N'){
            printf("              please hit 'y' or 'n' (y/n)? ");
            c=yesno(); }
          if(c=='n'||c=='N') return 0;
        }
    } else if(!(filename=prepare_filename(arg))){
       printf(" ERROR: wrong filename syntax\n");
       return 1; /* abort */
    }
    if(write_history(filename)!=0){
       printf(" ERROR: Cannot save command history to %s\n",filename);
    }
    free(filename);
    return 0;
}
/** DUMP -- dump constraints and macro definitions to be read by com_batch **/
static int com_dump(const char *arg, const char *line)
{FILE *dump_file; char *filename; const char *errmsg; int idx;
    if(!*arg || *arg=='?' || strcmp(arg,"help")==0){
       if(!line) printf(
       " type 'dump <file>' to save constraints and macro definitions\n"
       " These can be reloaded by the 'run <file>' command\n");
       return 0;
    }
    if(line) printf("%s\n",line);
    // check if there is anything to dump
    dump_file=NULL; errmsg=NULL;
    if(macro_total<=standard_macros && constraint_no==0){ // nothing to dump
        printf(" there is nothing to dump\n");
        return 0;
    }
    if(!(filename=prepare_filename(arg))){
        errmsg="wrong file name syntax";
    } else if(!(dump_file=fopen(filename,"a"))){
        errmsg="cannot open the file for appending data";
    }
    if(filename) free(filename);
    if(errmsg){
        printf(" ERROR: %s\n",errmsg);
        return 1; /* abort */
    }
    if(minitip_style==syntax_short)
        fprintf(dump_file,"\nstyle simple %c\n",minitip_sepchar);
    else
        fprintf(dump_file,"\nstyle full\n");
    // macros
    for(idx=standard_macros; idx<macro_total;idx++)
        dump_macro_with_idx(dump_file,idx);
    // constraints
    for(idx=0;idx<constraint_no;idx++){
        fprintf(dump_file,"add %s\n",constraint_table[idx]);
    }
    fclose(dump_file);
    return 0; /* OK */
}
static int execute_batch_file(FILE *batch_file)
{int level;
    if(!batch_file) return 0;
    batch_depth++;
    for(done=0;!done;) switch(read_batch_line(batch_file)){
       case 1:  /* long line */
          done=1; level=-1; break;
       case -1: /* EOF */
          done=1; level=0; break;
       default: /* regular line */
          level=execute_cmd(batch_line,batch_line);
          if(level>=2 || (level==1 && get_param("run")==1) )
             done=1;
    }
    done=0; batch_depth--;
    fclose(batch_file);
    return level;
}

/** BATCH -- execute commands from a file **/
static int com_batch(const char *arg,const char *line)
{FILE *batch_file; char *filename; const char *errmsg; int level;
    if(!*arg || *arg=='?' || strcmp(arg,"help")==0){
        if(!line) printf(" 'run <file>' executes minitip commands from <file>\n");
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
    if(errmsg) level=2;
    else {
        level=execute_batch_file(batch_file);
        if(level<0){
           errmsg="too long line, perhaps not a minitip file";
           level=1;
        } else if(level>0){
           errmsg="error in run file, aborting execution";
        }
    }
    if(errmsg){
        if(line)printf("%s\n",line);
        printf(" ERROR: %s\n",errmsg);
        return level; /* abort */
    }
    return 0; /* OK */
}

/** result texts of checking **/

#define res_TRUE	"    ==> TRUE"
#define res_TRUEEQ	"    ==> TRUE, simplifies to 0=0"
#define res_TRUEGE	"    ==> TRUE, simplifies to 0>=0"
#define res_FALSE	"    ==> FALSE"
#define res_ONLYGE	"    ==> FALSE, only >= is true"
#define res_ONLYLE	"    ==> FALSE, only <= is true"
#define res_CONSTR	" with the constraints"

/** CHECK -- check entropy relation with all constraints **/

static int com_check(const char *line, const char *orig)
{int i,keep,parse;;
    if(in_minitiprc) return 0;
    if(!*line || *line=='?' || strcmp(line,"help")==0){
        if(!orig)printf(" Check the validity of an entropy relation with all constraints.\n"
                        " Enter 'syntax relation' for more help.\n");
        return 0; /* empty line, OK */
    }
    keep=0; // add all constraints
    for(i=0;i<constraint_no;i++){
        parse_constraint(constraint_table[i],keep); keep=1;
    }
    parse=parse_entropy(line,keep);
    if(parse==PARSE_ERR){
          error_message(orig);
          return 1;
    }
    if(orig) printf("%s\n",orig);
    switch(parse){
      case PARSE_EQ:
          printf(res_TRUEEQ "\n"); break;
      case PARSE_GE:
          printf(res_TRUEGE "\n"); break;
      default:
          check_expression(line,1); break;
    }
    return 0; /* OK */
}

/** NOCON -- check entropy relation without constraints **/
static int com_nocon(const char *line, const char *orig)
{int parse;
    if(in_minitiprc) return 0;
    if(!*line || *line=='?' || strcmp(line,"help")==0){
        if(!orig)printf(" Crosscheck an entropy relation without any constraints.\n"
                        " Enter 'syntax relation' for more help.\n");
        return 0;
    }
    parse=parse_entropy(line,0);
    if(parse==PARSE_ERR){
        error_message(orig);
        return 1;
    }
    if(orig) printf("%s\n",orig);
    switch(parse){
      case PARSE_EQ:
         printf(res_TRUEEQ "\n"); break;
      case PARSE_GE:
         printf(res_TRUEGE "\n"); break;
      default:
         if(constraint_no>0){
           printf("Checking without constraints ...\n");
         }
         check_expression(line,0); break;
    }
    return 0;
}

/** DIFF -- print difference of two expressions **/
static int com_diff(const char *line, const char *orig)
{   if(in_minitiprc) return 0;
    if(!*line || *line=='?' || strcmp(line,"help")==0){
        if(!orig)
            printf(" Show the difference of two formulas separated by '=='\n"
                   " Enter 'syntax zap' for more help.\n");
        return 0;
    }
    if(parse_diff(line)!=PARSE_OK){ /* some error */
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
{char *ret; char *constr,*outstr;
    expr_to_check=src; use_constraints=with_constraints;
    ret=call_lp(compute_expression,get_param("iterlimit"),get_param("timelimit"));
    constr= with_constraints && constraint_no>0 ? res_CONSTR : "";
    if(ret==EXPR_TRUE){
        outstr=res_TRUE;
    } else if(ret==EXPR_FALSE){
        outstr=res_FALSE;
    } else if(ret==EQ_GE_ONLY){
        outstr=res_ONLYGE;
    } else if(ret==EQ_LE_ONLY){
        outstr=res_ONLYLE;
    } else {
        printf("ERROR in solving the LP: %s\n",ret);
        return;
    }
    printf("%s%s\n",outstr,constr);
}
static int check_offline_expression(const char *src, int quiet)
{char *ret; char *constr,*outstr;
    expr_to_check=src; use_constraints=1;
    ret=call_lp(compute_expression,get_param("iterlimit"),get_param("timelimit"));
    if(!quiet){
      constr= constraint_no>0 ? res_CONSTR : "";
      if(ret==EXPR_TRUE){       outstr=res_TRUE; }
      else if(ret==EXPR_FALSE){ outstr=res_FALSE; constr=""; }
      else if(ret==EQ_GE_ONLY){ outstr=res_ONLYGE; }
      else if(ret==EQ_LE_ONLY){ outstr=res_ONLYLE; }
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
*    determines the program's exit value. argno is positive.
*/
static int check_offline(int argc, char *argv[], int quiet)
{int i,j,keep,parse; char *src;
    src=argv[0]; while(*src && *src!='=') src++; 
    if(*src=='='){
       src++;
       while(*src==' '|| *src=='\t') src++;
       if(*src=='='){
          cmdarg_position=0;
          if(parse_diff(argv[0])!=PARSE_OK){
             if(!quiet) error_message(argv[0]);
             return EXIT_SYNTAX;
          }
          print_expression(); printf("\n");
          return EXIT_TRUE;
       }
    }
    keep=0; cmdarg_position=0;
    for(i=1;i<argc;i++){
        for(j=0;j<constraint_no;j++) if(strcmp(argv[i],constraint_table[j])==0){
            if(!quiet) printf("ERROR: constraint #%d is the same as constraint %d:\n%s\n",
                     i,j+1,argv[i]);
            return EXIT_ERROR; // other error
        }
        if(parse_constraint(argv[i],keep)!=PARSE_OK){
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
    parse=parse_entropy(argv[0],keep);
    if(parse==PARSE_ERR){
        if(!quiet) error_message(argv[0]);
        return EXIT_SYNTAX; // syntax error
    }
    switch(parse){
      case PARSE_EQ:
        if(!quiet) printf(res_TRUEEQ "\n");
        return EXIT_TRUE;
      case PARSE_GE:
        if(!quiet) printf(res_TRUEGE "\n");
        return EXIT_TRUE;
      default:
        break;
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
* Execute minitiprc
*
* int execute_minitiprc(int complain)
*    search for .minitiprc and execute the commands in it.
*    Return value: 0: OK, 1: error during execution,
*     2: complain is set and file not found
*/
static char *RC_FILE=DEFAULT_RC_FILE;


static int execute_minitiprc(int complain)
{FILE *rcfile; char buff[MAX_PATH_LENGTH+20];
    if(RC_FILE==NULL) return 0;
    rcfile=fopen(RC_FILE,"r");
    if(!rcfile){
       if(complain) return 2; // file not found
       char *home=getenv("HOME");
       if(home){
           strncpy(buff,home,MAX_PATH_LENGTH);buff[MAX_PATH_LENGTH]=0;
           strcat(buff,"/");
           strncat(buff,RC_FILE,MAX_PATH_LENGTH+15-strlen(home));
           buff[MAX_PATH_LENGTH+15]=0;
           rcfile=fopen(buff,"r");
       }
    }
    return execute_batch_file(rcfile)? 1 : 0;
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
"This is minitip V" VERSION_STRING ", a MINimal Information Theoretic Inequality Prover.\n"
"Usage:\n"
"    minitip [flags]\n"
"for interactive usage, or\n"
"    minitip [flags] <expression> [constraint1] [constraint2] ... \n"
"Flags:\n"
"   -h         -- this help\n"
"   -s         -- start using minimal syntax style (default, same as '-s,')\n"
"   -s<chr>    -- minimal style, use <chr> as the separator character\n"
"   -S         -- start using full syntax style\n"
"   -q         -- quiet, just check, don't print anything\n"
"   -e         -- last flag, use when the expression starts with '-'\n"
"   -f <file>  -- use <file> as the command history file (default: '" DEFAULT_HISTORY_FILE "')\n"
"   -c <file>  -- use <file> as the config file (default: '" DEFAULT_RC_FILE "')\n"
"   -c-        -- don't read the default config file\n"
"   -m <macro> -- add macro definition\n"
"   -v         -- version and copyright\n"
"Exit value when checking validity of <expression>:\n"
"    " mkstringof(EXIT_TRUE)  "  -- the expression (with the given constrains) checked TRUE\n"
"    " mkstringof(EXIT_FALSE) "  -- the expression (with the given constrains) checked FALSE\n"
"    " mkstringof(EXIT_SYNTAX)"  -- syntax error in the expression or in some of the constraints\n"
"    " mkstringof(EXIT_ERROR) "  -- some error (problem too large, LP failure, etc)\n"
"For more information, type 'help' from within minitip\n\n");
}

int main(int argc, char *argv[])
{char *line; int i; int quietflag, endargs, styleset, rcfile;
 char *histfile; syntax_style_t mi_style; char mi_sepchar;

    /* some default values */
    if(HISTORY_FILE==NULL) HISTORY_FILE=strdup(DEFAULT_HISTORY_FILE);
    resize_constraint_table(minitip_INITIAL_CONSTR);
    set_param("constrlimit",minitip_INITIAL_CONSTR);
    resize_macro_table(minitip_INITIAL_MACRONO);
    set_param("macrolimit",minitip_INITIAL_MACRONO);
    initialize_random();
    setup_standard_macros();
    minitip_style=minitip_INITIAL_STYLE;
    minitip_sepchar=minitip_INITIAL_SEPCHAR;
    set_syntax_style(minitip_INITIAL_STYLE,minitip_INITIAL_SEPCHAR,1);

    /* argument handling */
    quietflag=0; endargs=0; styleset=0; rcfile=0; histfile=NULL;
    for(i=1; i<argc && endargs==0 && argv[i][0]=='-';i++){
        switch(argv[i][1]){
      case 'h': short_help(); return EXIT_INFO;
      case 'v': print_version(argv[i][2]); return EXIT_INFO;
      case 's': mi_style=syntax_short; mi_sepchar=','; /* default */
                styleset=1;
                if(argv[i][2]){
                    mi_sepchar=argv[i][2];
                    if(!strchr(minitip_SEPARATOR_CHARS,mi_sepchar)){
                        printf("Illegal separator character in flag -s\n"
                               "Accepted separators are: %s\n",minitip_SEPARATOR_CHARS);
                        return EXIT_ERROR;
                    }
                }
                break;
      case 'S': mi_style=syntax_full; mi_sepchar=';'; 
                styleset=1; break;
      case 'f': line=&(argv[i][2]);
                if(*line==0){ i++; if(i<argc){ line=argv[i]; } }
                if(!line || !*line){
                   printf("Flag '-f' requires the command history file name\n");
                   return EXIT_ERROR;
                }
                if(strlen(line)>=MAX_PATH_LENGTH){
                   printf("History file name after flag '-f' is too long\n");
                   return EXIT_ERROR;
                }
                histfile=strdup(line);
                // replace all ctrl chars by underscore
                for(line=histfile; *line; line++){
                    if((unsigned char)(*line)< 0x20 || (unsigned char)(*line)==0x7f || (unsigned char)(*line)==0xff ) *line='_';
                }
                break;
      case 'c': if(argv[i][2]=='-'){
                    RC_FILE=NULL; break;
                }
                i++; line=i<argc?argv[i]:NULL;
                if(!line || !*line){
                   printf("Flag '-c' requires a file name\n");
                   return EXIT_ERROR;
                }
                if(strlen(line)>MAX_PATH_LENGTH){
                   printf("File name after flag '-c' is too long\n");
                   return EXIT_ERROR;
                }
                RC_FILE=line; rcfile=1;
                break;
      case 'm': line=&(argv[i][2]);
                if(*line==0){ i++; if(i<argc){ line=argv[i]; } }
                if(!line || !*line){
                   printf("Missing macro definition after the flag '-m'\n");
                   return EXIT_ERROR;
                }
                if(styleset){ minitip_style=mi_style; minitip_sepchar=mi_sepchar; }
                set_syntax_style(minitip_style,minitip_sepchar,1);
                if(parse_macro_definition(line)!=PARSE_OK){
                   printf("Macro definition after flag '-m'\n");
                   error_message(line);
                   return EXIT_ERROR;
                }
                break;
      case 'q': quietflag=1; break;
      case 'e': endargs=1; break;
      default:  printf("Unknown flag '%s', use '-h' for help\n",argv[i]); return EXIT_ERROR;
        }
    }
    /* reset default style */
    minitip_style=minitip_INITIAL_STYLE;
    minitip_sepchar=minitip_INITIAL_SEPCHAR;
    set_syntax_style(minitip_INITIAL_STYLE,minitip_INITIAL_SEPCHAR,1);
    in_minitiprc=1;
    /* execute the rc file */
    switch(execute_minitiprc(rcfile)){
       case 1: printf("Error in rc file '%s'\n",RC_FILE);
               return EXIT_ERROR;
       case 2: printf("File '%s' after -c flag not found\n",RC_FILE);
               return EXIT_ERROR;
       default: break;
    }
    in_minitiprc=0;
    if(styleset){ // command line override
        if(constraint_no>0 && 
          (minitip_style!=mi_style || minitip_sepchar!=mi_sepchar)){
            if(!quietflag){
              if(constraint_no==1)
                printf("Deleting the constraint as it was");
              else
                printf("Deleteting all (%d) constraints as they were",constraint_no);
              printf(" entered in different style\n");
            }
            while(constraint_no>0){
               constraint_no--; free(constraint_table[constraint_no]);
            }
        }
        minitip_style=mi_style; minitip_sepchar=mi_sepchar;
        set_syntax_style(minitip_style,minitip_sepchar,get_param("simplevar"));
    }
    if(i<argc){ /* further arguments; do as instructed */
        return check_offline(argc-i,argv+i,quietflag);
    }
    if(quietflag || endargs) return EXIT_ERROR; /* -q flag and no argument */
    if(histfile){ // command line override
        free(HISTORY_FILE); HISTORY_FILE=histfile;
    }
    initialize_readline();
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


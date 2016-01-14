/* minitip
   check entropy expression against Shannon inequalities
*/

#define VERSION_MAJOR	0
#define VERSION_MINOR	9
#define stringify(x)	#x
#define mkstringof(x)	stringify(x)
#define VERSION_STRING	mkstringof(VERSION_MAJOR.VERSION_MINOR)

#define COPYRIGHT	\
"This program is free, open-source software. You may redistribute it and/or\n"\
"modify under the terms of the GNU General Public License (GPL) as published\n"\
"by the Free Software Foundation http://www.gnu.org/licenses/gpl.html\n"\
"There is ABSOLUTELY NO WARRANTY, use at your own risk.\n\n"\
"Copyright (C) 2016 Laszlo Csirmaz, Central European University, Budapest\n"

/*----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "xassert.h"

/* forward declaration */
static void check_expression(char *src, int with_constraints);
extern int yesno(void);
/*===============================================================*/
#define PROMPT	"minitip: "
#define DEFAULT_HISTORY_FILE ".minitip"
char *HISTORY_FILE = DEFAULT_HISTORY_FILE;

#include <readline/readline.h>
#include <readline/history.h>

static int done=0; syntax_style_t minitip_style=syntax_short;
/* functions */
static int com_quit (char*);	/* quit */
static int com_help (char*);	/* help */
static int com_check(char*);	/* check */
static int com_add  (char*);	/* add restriction */
static int com_list (char*);	/* list restriction */
static int com_del  (char*);	/* delete restriction */
static int com_style(char*);	/* style */
static int com_syntax(char*);	/* formula syntax */
static int com_nocon(char*);	/* check without constraints */
static int com_about(char*);	/* print license info */

typedef struct {
    char *name;
    rl_icpfunc_t *func;
    char *doc;
} COMMAND;

static COMMAND commands[] = {
{"quit",	com_quit,	"quit minitip" },
{"help",	com_help,	"display this text" },
{"?",		com_help,	"synonym for 'help'" },
{"check",	com_check,	"check inequality with constraints" },
{"checkw",	com_nocon,	"check without constraints" },
{"add",		com_add,	"add new constraint" },
{"list",	com_list,	"list constraints: 3, 4-5"},
{"del",		com_del,	"delete numbered constraints"},
{"style",	com_style,	"show / change formula style"},
{"syntax",	com_syntax,	"entropy expression syntax"},
{"about",	com_about,	"history, license, author, etc"},
{NULL,		NULL,		NULL}
};

/***********************************************************************
 * readline completion  */

/* returns the next matching command */
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

static char **cmd_completion(const char*text, int start,  __attribute__((unused)) int end)
{char **matches=NULL;
  rl_attempted_completion_over=1; /* that's all folks */
  if(start==0){
      matches=rl_completion_matches(text,cmd_name);
  }
  return matches;
}

static void initialize_readline(void)
{
  rl_readline_name="minitip";
  read_history(HISTORY_FILE);
  using_history();
  rl_attempted_completion_function = cmd_completion;
}

/*-----------------------------------------------------------*/
/* readline functions */
static int cmdarg_position;
#define UNUSED __attribute__((unused)) 

static void execute_cmd(char *line)
{int i,j,cmd;
    xassert(line!=NULL);
    while(*line==' '|| *line=='\t') line++;
    cmd=-1;
    //it is going for the first match
    for(j=0;cmd<0 && commands[j].name; j++){
        i=strlen(commands[j].name);
        if(strncmp(line,commands[j].name,i)==0 && (line[i]==0 || line[i]==' ') ) cmd=j;
    }
    if(cmd>=0){
      for(i=strlen(commands[cmd].name);line[i]==' '||line[i]=='\t';i++);
      cmdarg_position=i;
      commands[cmd].func(line+i);
      return;
    }
    for(i=0;line[i] && (line[i]==' '||line[i]=='\t'); i++);
    if(line[i]==0){ return; } /* empty line */
    if(('a'<=line[i]&& line[i]<='z')||('A'<=line[i]&&line[i]<='Z')){
        printf("Unknown command; use 'help' to get a list of commands\n");
        return;
    }
    // if no = sign, do nothing ...
    for(;line[i] && line[i]!='='; i++);
    if(line[i]!='='){
        printf("unknown command; use 'help' to get a list of commands\n");
        return;
    }
    // otherwise assume that it is an expression to be checked
    cmdarg_position=0;
    com_check(line);
    return;
}

static int cmp_s(const char*s2,const char*s1)
{
/* check whether s1 is an initial segment of s2 */
    while(*s1 && *s1!=' ' && *s1==*s2){s1++;s2++;}
    return (*s2==0 || *s2==' '|| *s2=='\t') ? 0 : 1;
}

static int com_about(char *arg UNUSED)
{
    printf(
"\n"
"    minitip -- Minimal Information Theoretic Inequality Prover\n\n"
"The original ITIP software (runs under MATLAB) was developed by Raymond W.\n"
"Yeung and Ying-On Yan; it is available at http://user-www.ie.cuhk.edu.hk/~ITIP/.\n"
"The stand alone version Xitip at http://xitip.epfl.ch has graphical interface\n"
"and runs both in Windows and Linux.\n"
"This program was written in C, uses curses based command line input with\n"
"history expansion; has user friendly syntax checker; extended syntax; and glpk\n"
"(gnu linear programming kit) as the LP solver. Enter 'help' to see all\n"
"commands.\n\n"
COPYRIGHT
"\n");
    return 0;
}

static int com_help(char *arg)
{int i, printed=0;
    for(i=0;commands[i].name;i++){
        if(!*arg || cmp_s(arg,commands[i].name)==0){
            printf("%s\t\t%s\n",commands[i].name,commands[i].doc);
            printed++;
        }
    }
    if(!printed){
        printf("No command matches '%s'. Possibilities are:\n",arg);
        for(i=0;commands[i].name;i++){
            printf("%s%c",commands[i].name,printed%6==5?'\n':'\t');
            printed++;
        }
        printf("\n");
    }
    return 0;
}
static int com_quit(char *arg)
{
    if(*arg==0){int c;
        printf("Save the commands to the history file %s (y/n)? ",HISTORY_FILE);
       c=yesno(); while(c!='y' && c!='n' && c!='Y' && c!='N'){
        printf("             please hit 'y' or 'n' (y/n)? ");
        c=yesno();
       }
       done=1;
       if(c=='y'||c=='Y') write_history(HISTORY_FILE);
    }
    else printf("'quit' should not have any argument \n");
    return 0;
}
static int com_syntax(char *argv)
{
    if(strncasecmp(argv,"style",3)==0){ // on style
        printf(
"Entropy expressions can be entered in two styles: 'full' and 'simple'.\n\n"
"The 'full' style random variables are arbitrary identifiers, such as 'X12'\n"
"or 'Winter'. 'H(X1,Winter)' is the entropy, 'I(A,Winter;X,Y|Z)' is the\n"
"(conditional) mutual information.\n\n"
"The 'simple' style uses minimal syntax: variables are small letters from\n"
"'a' to 'z' only; letters 'I' and 'H' are omitted, e.g., '(ab,xy|z)' is\n"
"the conditional mutual information of 'a,b' and 'x,y' given 'z'.\n\n"
"The present style is '%s'. To change style, use 'style %s'.\n",
minitip_style==syntax_short ? "simple" : "full",
minitip_style==syntax_short ? "full" : "simple");
        return 0;
    }
    if(strncasecmp(argv,"var",3)==0){ // give syntax of vars and var list
        printf( minitip_style==syntax_short ?
"In simple style RANDOM VARIABLES are small letters from 'a' to 'z'.\n"
"Put variables next to each other for their joint distribution: 'acrs'\n"
"Spaces are allowed, 'a c  rs' denotes the same collection as 'acrs'.\n" :
// full style
"In full style RANDOM VARIABLES are identifiers such as 'X', 'Snow_fall',\n"
"'Winter', etc. Variables are case sensitive, 'x' and 'X' are different.\n"
"A variable sequence is a list of variables separated by commas (,) such\n"
" as 'X, y,Y' or 'Snow_fall, Winter'.'\n");
        return 0;
    }
    if(strncasecmp(argv,"entrop",3)==0){ // entropy syntax
        printf( minitip_style==syntax_short ?
"In simple style an ENTROPY EXPRESSION is abbreviated to minimal:\n"
"a) acrs         any variable sequence stands for its own entropy'\n"
"b) (ab|rs)      conditional entropy H(a,b| r,s)\n"
"c) (ab,rs)      mutual information: I(a,b;r,s)\n"
"d) (ab,rs|cd)   conditional mutual information I(a,b;r,s|c,d)\n"
"e) [a,b,c,d]    Ingleton expression: -(a,b)+(a,b|c)+(a,b|d)+(c,d)\n" :

"In full style an ENTROPY EXPRESSION follows the standard notation:\n"
"a) H(W,S)       entropy of the joint distribution of 'W' and 'S'\n"
"b) H(Wter|Fall) conditional entropy\n"
"c) I(W,F;S)     mutual information of 'W,F' and 'S'\n"
"d) I(W,F;S|day) conditional mutual information\n"
"e) [A;B;C;D]    shorthand for the Ingleton expression\n"
"                -I(A;B)+I(A;B|C)+I(A;B|D)+I(C;D)\n");
        return 0;
    }
    if(strncasecmp(argv,"relation",3)==0){ //formula description
        printf(
"A RELATION compares two linear compositions of entropy expressions:\n"
"%s\n"
"The '*' sign between the constant and the entropy expression is optional.\n"
"Only '=' (equal), '>=' (greater or equal) and '<=' (less or equal) can be\n"
"used. Any side (but not both) can be zero as in the above example.\n",
          minitip_style==syntax_short ?
"       (x,y)+3a-1.234*(x,a|z)>=0\n"
"       -1.234*(x|y) - 12.234*(a,b|h) <= -2bxy\n"
"       (b,d|ac)+(b,c|a)=(b,cd|a)":
"       I(X;Y)+3 H(A) -1.234 I(X;A|Z) >= 0\n"
"       +1.234*H(X|Y) - 12.234*I(A;B|H) <= -2H(B,X,Y)\n"
"       I(X;Y1|Z,Y2)+I(X;Y2|Z)=I(X;Y1,Y2|Z)");
        return 0;
    }
    if(strncasecmp(argv,"const",3)==0){ // constraint
        printf(
"A CONSTRAINT can be a RELATION (two entropy expressions compared by '=',\n"
"'<=' or '>='), or one of the following shorthands:\n"
"        {vars} : {vars}\n"
"expressing that the first list is determined by the second one;\n"
"        {vars} .  {vars} .  {vars} .   ...\n"
" or     {vars} || {vars} || {vars} ||  ...\n"
"(any number) for expressing that they are totally independent;\n"
"        {vars} /  {vars} /  {vars} /  ...\n"
" or     {vars} -> {vars} -> {vars} -> ...\n"
"(any number) that they form a Markov chain.\n");
        printf( minitip_style==syntax_short ?
"A variable list is a sequence of letters from 'a' to 'z'.\n" : 
"A variable list is a sequence of random variables (identifiers)\n"
"separated by commas, such that 'Winter, Fall, Summer'\n");
        return 0;        
    }
    printf(
"Please enter one of the following topics:\n"
"  style      -- choose between \"simple\" and \"full\" style\n"
"  variables  -- random variables and sequences of random variables\n"
"  entropy    -- entropy expression syntax and shorthands\n"
"  relation   -- compare two linear entropy expressions\n"
"  constraint -- how to enter a constraint\n");
    return 0;
}

static void error_message(void)
{int i, pos=strlen(PROMPT)+cmdarg_position;
 char *err=syntax_error.harderrstr;
    if(err){ pos += syntax_error.harderrpos; }
    else { err=syntax_error.softerrstr; pos+=syntax_error.softerrpos; }
    for(i=0;i<pos;i++)printf("-"); printf("^\nERROR: %s\n",err);
}

/*===========================================================*/
/* store, list, delete constraints */
#define max_constraints	1000
static int constraint_no=0;
char *constraint_table[max_constraints];

static int com_add(char* line)
{int i;
    // check if it is there ...
    for(i=0;i<constraint_no;i++) if(strcmp(line,constraint_table[i])==0){
        printf("This constraint is #%d, no need to add again\n",i+1);
        return 0;
    }
    if(parse_constraint(line,0)){ // some error
        error_message();
        return 0;
    }
    if(constraint_no>=max_constraints-1){
        printf("ERROR: too many constraints (max %d)\n"
               "  use 'del' to delete some constraints\n",max_constraints);
        return 0;
    }
    constraint_table[constraint_no]=strdup(line);
    constraint_no++;
    return 0;
}
/* reads digits, returns the number of positions advanced,
   zero if no digit was found */
static int read_number(char *frm, int *to)
{int i,d;
    d=0; i=0;
    while('0'<=*frm && *frm<='9'){ d=d*10+(*frm)-'0'; frm++; i++; }
    if(i>0){*to=d;}
    return i;
}
static int com_list(char *arg)
{int i0,i1,pos; int not_printed;
    if(constraint_no==0){ printf("  no constraints (yet)...\n"); return 0; }
    if(*arg=='?'){
        printf("--- Constraints (total: %d)\n",constraint_no);
        return 0;
    }
    if(*arg==0){ // no argument, print the first 20 lines
        printf("--- Constraints (total %d)\n",constraint_no);
        for(i0=0;i0<20 && i0<constraint_no;i0++){
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
            if(!pos ||i0<=0||i0>i1){ printf("   Wrong syntax, use '3,4-6,8'\n"); return 0; }
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
            i1=-1;
            break;
  default:  pos=read_number(arg,&i1);
            if(!pos){ printf("   Wrong syntax, use '3,4-6,12-14'\n"); return 0; }
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
    return 0;
}

static int com_del(char *arg)
{int pos,no;
    if(constraint_no==0){
        printf("There are no constraints to delete.\n"); return 0;
    }
    if(*arg==0 || *arg == '?'){
        printf("--- Specify a constraint to be deleted from 1 to %d, or say 'all'.\n",
             constraint_no);
        return 0;
    }
    if(strcmp(arg,"all")==0){
        printf("All constraints (%d) will be deleted. Proceed (y/n)? ",constraint_no);
        int c=yesno(); if(c=='y'||c=='Y'){
           for(no=0;no<constraint_no;no++) free(constraint_table[no]);
           constraint_no=0;
        }
        return 0;
    }
    pos=read_number(arg,&no);
    arg+=pos; if(pos==0 || *arg || no<=0 || no>constraint_no){
        printf("specify exactly one constraint index from 1 to %d\n",constraint_no);
        return 0;
    }
    printf("This constraint has been deleted:\n   %s\n",constraint_table[no-1]);
    free(constraint_table[no-1]);
    while(no<constraint_no){
       constraint_table[no-1]=constraint_table[no]; no++;
    }
    constraint_no--;
    return 0;
}
/* change style, do it only if there are no saved constraints */
static int com_style(char *argv)
{   if(strcasecmp(argv,"full")==0){
        if(minitip_style==syntax_full) return 0; /* no change */
        if(constraint_no>0){
            printf("Changing style will delete all constraints. Proceed (y/n)? ");
            int no; int c=yesno(); if(c!='y' && c!='Y') return 0;
            for(no=0;no<constraint_no;no++) free(constraint_table[no]);
            constraint_no=0;
        }
        minitip_style=syntax_full;
        set_syntax_style(minitip_style); return 0; 
    }
    else if(strcasecmp(argv,"simple")==0){
        if(minitip_style==syntax_short) return 0;
        if(constraint_no>0){
            printf("Changing style will delete all constraints. Proceed (y/n)? ");
            int no; int c=yesno(); if(c!='y' && c!='Y') return 0;
            for(no=0;no<constraint_no;no++) free(constraint_table[no]);
            constraint_no=0;
        }
        minitip_style=syntax_short;
        set_syntax_style(minitip_style); return 0; 
    }
    printf("Expression style is %s. To change it, type \"style %s\".\n"
       "To get a description, type \"syntax style\".\n"
       "Warning: changing the style deletes all saved constraints.\n",
          minitip_style==syntax_short ? "SIMPLE" : "FULL",
          minitip_style==syntax_short ? "full"  : "simple");
    return 1;
}
/* check with all valid constraints */
static int com_check(char *line)
{int i,keep;
    if(!*line){
        printf("check what?\n"); return 0;
    }
    keep=0; // add all constraints
    for(i=0;i<constraint_no;i++){
        parse_constraint(constraint_table[i],keep); keep=1;
    }
    if(parse_entropy(line,keep)){/* some error */
        error_message();
        return 0;
    }
    check_expression(line,1);
    return 0; 
}
/* check without constraints */
static int com_nocon(char *line)
{   if(!*line){
        printf("check what?\n"); return 0;
    }
    if(parse_entropy(line,0)){ /* some error */
        error_message();
        return 0;
    }
    if(constraint_no>0){
        printf("Checking without constraints ...\n");
    }
    check_expression(line,0);
    return 0;
}
/*---------------------------------------------------------*/
/* offline return codes */
#define EXIT_TRUE	0
#define EXIT_INFO	0
#define EXIT_FALSE	1
#define EXIT_SYNTAX	2
#define EXIT_ERROR	3
/*---------------------------------------------------------*/
#include "mklp.h"
static char *expr_to_check;
static int use_constraints;
/* computes expr_to_check if i<0;
   otherwise computes the i'th constraint;
   return 1 if no such constrain exists */
static int compute_expression(int i)
{
    if(i<0){ parse_entropy(expr_to_check,1); return 0; }
    if(!use_constraints || i>=constraint_no) return 1;
    parse_constraint(constraint_table[i],1);
    return 0;
}
static void check_expression(char *src, int with_constraints)
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
static int check_offline_expression(char *src, int quiet)
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
        printf("ERROR is solving the LP: %s\n",ret);
        return EXIT_ERROR;
      }
      printf("%s\n%s%s\n",src,outstr,constr);
    }
    return ret==EXPR_TRUE ? EXIT_TRUE :
       ret==EXPR_FALSE || ret==EQ_GE_ONLY || ret== EQ_LE_ONLY ? EXIT_FALSE :
       EXIT_ERROR ;
}
/*-----------------------------------------------------------*/
/* check the expression with the given constraints as given in the list */
static void offline_error(char *line,int quiet)
{   if(quiet) return;
    cmdarg_position-=strlen(PROMPT);
    printf("%s\n",line);
    error_message();
}

static int check_offline(int argc, char *argv[], int quiet)
{int i,j,keep;
    set_syntax_style(minitip_style);
    keep=0;
    for(i=1;i<argc;i++){
        for(j=0;j<constraint_no;j++) if(strcmp(argv[i],constraint_table[j])==0){
            if(!quiet) printf("ERROR: constraint #%d is the same as constraint %d:\n%s\n",
                     i,j+1,argv[i]);
            return EXIT_ERROR; // other error
        }
        if(parse_constraint(argv[i],keep)){
            offline_error(argv[i],quiet);
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
        offline_error(argv[0],quiet);
        return EXIT_SYNTAX; // syntax error
    }
    return check_offline_expression(argv[0],quiet);
}

/*===========================================================*/

/* store only if the line is not among the last 10 stored */
static void store_if_not_new(char *line)
{int i,total; HIST_ENTRY **hl;
    if(strlen(line)<4) return;
    /* do not store 'quit' */
    if(strncasecmp(line,"quit",4)==0) return;
    hl=history_list();
    if(hl){
      for(total=0;hl[total]!=NULL; total++);
      for(i=0; total>0 && i<10; i++){
        total--;
        if(hl[total] && strcmp(line,hl[total]->line)==0) return;
      }
    }
    add_history(line);
}

inline static void print_version(void){printf(
"minitip v" VERSION_STRING 
" is a MINimal Information Theoretic Inequality Prover.\n\n"
COPYRIGHT
"\n");}

inline static void short_help(void){printf(
"MINimal Information Theoretic Inequality Prover.  Usage:\n\n"
"    minitip [flags]\n"
"for interactive usage, or\n"
"    minitip [flags] <expression> <condition1> <condition2> ... \n"
"\n"
"Flags:\n"
"   -h        -- this help\n"
"   -s        -- use minimal style (default)\n"
"   -S        -- use full style\n"
"   -q        -- quiet, just check, don't print anything\n"
"   -f <file> -- use <file> as the history file (default: '" DEFAULT_HISTORY_FILE "')\n"
"   -v        -- version and copyright\n"
"\n"
"Return value when checking validity of the argument:\n"
"   0  - the expression (with the given constrains) checked TRUE\n"
"   1  - the expression (with the given constrains) checked FALSE\n"
"   2  - syntax error in the expression or in some of the constraints\n"
"   3  - some error (problem too large, LP failure, etc)\n"
"\n"
"For more information, type 'help' from within minitip\n\n");
}

int main(int argc, char *argv[])
{char *line; int i; int quietflag;
    /* argument handling */
    quietflag=0;
    for(i=1; i<argc && argv[i][0]=='-';i++){
        switch(argv[i][1]){
      case 'h': short_help(); return EXIT_INFO;
      case 'v': print_version(); return EXIT_INFO;
      case 's': minitip_style=syntax_short; break;
      case 'S': minitip_style=syntax_full; break;
      case 'f': line=&(argv[i][2]);
                if(*line==0){ i++; if(i<argc){ line=argv[i]; } }
                if(!line || !*line){
                   printf("Parameter '-f' requires the history file name\n");
                   return EXIT_ERROR;
                }
                HISTORY_FILE=strdup(line);
                // replace all ctrl chars by _
                for(line=HISTORY_FILE; *line; line++){
                    if((unsigned char)(*line)<=0x20 || (unsigned char)(*line)==0x7f || (unsigned char)(*line)==0xff ) *line='_';
                }
                break;
      case 'q': quietflag=1; break;
      default:  printf("unknown flag '%s', use '-help' for help\n",argv[i]); return EXIT_ERROR;
        }
    }
    if(i<argc){ /* further arguments; do as instructed */
        return check_offline(argc-i,argv+i,quietflag);
    }
    if(quietflag) return EXIT_ERROR; /* -q flag and no argument */
    initialize_readline();
    set_syntax_style(minitip_style);
    for(done=0;!done;){
        line=readline(PROMPT);
        if(!line) break;
        execute_cmd(line);
        store_if_not_new(line);
        free(line);
    }
    return 0;
}



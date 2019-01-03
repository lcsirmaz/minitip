## minitip - a MINimal Information Theoretic Inequality Prover

The original ITIP software (which runs under MATLAB) was developed by
Raymond W. Yeung and Ying-On Yan; it is available at
http://user-www.ie.cuhk.edu.hk/~ITIP/
The stand-alone version Xitip at http://xitip.epfl.ch has graphical
interface, and runs both in Windows and Linux.

This program was written in C, uses the readline library to read entropy
expressions; has a user friendly syntax checker; extended syntax; macro
facility; online help; glpk (gnu linear programming kit) as the LP solver.

#### USAGE

When the command line contains an entropy expression, it is checked for
validity (see the section [METHOD](#method) below). Additional constraints
can be specified as additional arguments. Example:

    PROMPT> minitip '[a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0'
    [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0
         ==> FALSE
    
    PROMPT> minitip '[a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0' '(e,ad|bc)=0'
    [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0
         ==> TRUE with the constraints

The single quotes around the arguments prevent the shell from interpreting 
special symbols, such as  =, \<, \|, (, and ).
Entropy expressions were entered using the **simple** (or lazy) style:
[a,b,c,d] is the Ingleton expression
*-I(a;b)+I(a;b|c)+I(a;b|d)+I(c;d)*; letters *H* and *I* indicating entropy
and mutual information can be omitted; and comma (,) is used as a separator
instead of semicolon (;). For **full style** use the flag **-S** as follows:

    PROMPT> minitip -S '[a;b;c;d]+I(e;b|c)+I(e;c|b)+I(b;c|e)>=-3*I(e;a,d|b,c)'
    [a;b;c;d]+I(e;b|c)+I(e;c|b)+I(b;c|e)>=-3*I(e;a,d|b,c)
         ==> TRUE

In interactive usage entropy expressions and constraints are entered
at the terminal. Here is an example session.

    PROMPT> minitip
    minitip: help
     quit        quit minitip
     help        display this text
     ?           synonym for 'help'
     check       check entropy relation with constraints
     test        synonym for 'check'
     xcheck      check entropy relation without constraints
     add         add new constraint
     list        list all or specified constraints: 3,5-7
     del         delete numbered constraint
     unroll      print missing entropy terms on RHS
     ext         convert to extended information measures
     nat         convert to natural coordinates
     macro       add, list, delete macros
     run         execute commands from a file
     style       show / change formula style
     syntax      describe how to enter entropy formulas
     set         list / set runtime parameters
     dump        dump constraints and macro definitions to a file
     save        save command history to a file
     about       history, license, author, etc
     args        accepted command line arguments
    minitip: add (e,ad|bc)=0
    minitip: list all
    Constraints (total 1)
      1: (e,ad|bc)=0
    minitip: check [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|)>=0
    ----------------------------------------------^
    ERROR: variable list is expected here
    minitip: check [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0
          ==> TRUE with the constraints
    minitip: xcheck [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0
    Checking without constraints ...
          ==> FALSE
    minitip: quit
    Save the commands to the command history file .minitip (y/n)? n
    PROMPT> 

Accepted flags and exit values are:

| Flag  | meaning  |
|:------|:-----| 
| -h    | print a short help and quit |
| -s    | use **minimal** style (default, same as -s,) |
| -s\<chr\> | **minimal** style, use \<chr\> as separator character |
| -S    | use **full** style |
| -q    | quiet, just check, don't print anything. Must have an argument to check |
| -e    | last flag, use when the expression starts with a minus sign |
| -f \<file\> | use \<file\> as the command history file (default: **.minitip**) |
| -c \<file\> | use \<file\> as the config file (default: **.minitiprc**) |
| -c-   | don't read the default config file |
| -m \<macro\> | add macro definition |
| -v    | print version, copyright, and quit |

| Exit values | (when checking validity of the first argument) |
| :-------- | :-------------|
| 0         | the expression (with the given constraints) checked TRUE |
| 1         | the expression (with the given constraints) checked FALSE |
| 2         | syntax error in the expression or in some of the constraints |
| 3         | other error (problem too large, LP failure, etc) |

#### UTILITIES

Minitip has several convenience features, such as

    run <file>

reads the lines from \<file\> and executes them as if they were entered from
the keyboard. The file can be created by any text editor.

    dump <file>

appends all constraints and macros to the given \<file\>. The file can be
edited and then executed to recover the dumped constraints and macros.

    save <command-history-file>

saves the commands entered during the session to the given file. The history
file is loaded using the command line argument

    -f <history-file>

Some features of minitip can be fine-tuned by the 'set' command, such as 

* the time and iteration limit on the LP solver; 
* how many macros and constraints are accepted; 
* whether executing command from a file should stop at all errors, or only at fatal ones;
* whether comment lines (lines starting with a # mark) are printed or not
* command can be abbreviated to any unique prefix
* when finishing, command line history should be automatically saved or not
* in simple style variables ending with digits are accepted or not

When started, minitip looks for the default config file .minitiprc
first in the working directory, then in the home directory. If found, 
the commands are executed before any other action. Alternative
config file can be specified by the command line argument

    -c \<file\>'

The default config file can be inhibited by using the command line
argument '-c-'. A sample config file can be

    # sample .minitiprc
    # comment lines in run files are printed
    set comment=yes
    # forget history when quitting minitip
    set save=no
    # allow abbreviated commands
    set abbrev=yes
    # don't stop run files for non-fatal errors
    set run=loose
    # define a macro
    macro D(a,b,c)=(a,b|c)+(a,c|b)+(b,c|a)
    # print all commands
    help
    # that's all ...


#### METHOD

After collecting all random variables in the expression (and constraints), 
the program creates all Shannon entropy inequalities for those variables.
Then checks whether the expression is a consequence of the constraints and
the Shannon inequalities. It is done by creating a Linear Program instance,
and calling an LP solver. Thus the result printed out by the program is
the answer to the following question:

> Does this inequality follow from the (basic) Shannon inequalities, and
> from the given constraints?

and **not** whether the given inequality is a valid entropy inequality (that is,
holds for arbitrary collection of random variables).

The size of the LP problem can be very big. Using 10 random variables there
will be 1023 entropies (one for all nonempty subsets), and over 11500 Shannon
inequalities. The Shannon inequalities form a numerically ill-conditioned 
(highly degenerate) LP instance, thus, from time to time, any LP solver will
give erroneous answer. Please watch for those cases.

#### HISTORY

The original ITIP software was created and written by Raymond W. Yeung and
Ying-On Yan; it is available at http://user-www.ie.cuhk.edu.hk/~ITIP. It
runs under MATLAB.
The stand-alone version Xitip at http://xitip.epfl.ch has graphical
interface and runs both in Windows and Linux.
This version is written in C from scratch, and has extended syntax, more
user friendly syntax checker, and uses the glpk LP solver.

#### COMPILATION

The program uses the 'readline' library and include files, and the
'glpk' library and include files. The following line should compile it
on linux without any problem:

     gcc -O3 *.c -lglpk -lreadline -o minitip

#### AUTHOR

Laszlo Csirmaz, <csirmaz@ceu.edu>



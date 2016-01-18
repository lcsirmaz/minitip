## minitip - a MINimal Information Theoretic Inequality Prover

The original ITIP software (which runs under MATLAB) was developed by
Raymond W. Yeung and Ying-On Yan; it is available at
http://user-www.ie.cuhk.edu.hk/~ITIP/

The stand-alone version Xitip at http://xitip.epfl.ch has graphical
interface, and runs both in Windows and Linux.

This program was written in C, uses the readline library to read entropy
expressions; has a user friendly syntax checker; extended syntax; and
glpk (gnu linear programming kit) as the LP solver.

#### USAGE

When the command line contains an entropy expression, it is checked for
validity. Additional constraints can be specified as additional arguments.
Example:

    PROMPT> minitip '[a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0'
    [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0
         ==> FALSE
    
    PROMPT> minitip '[a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0' '(e,ad|bc)=0'
    [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0
         ==> TRUE with the constraints

The single quotes around the arguments prevent the shell from interpreting 
the special symbols =, \<, \|, (, and ) in the formula.

The entropy expressions were entered using the **simple** style:
[a,b,c,d] is the Ingleton expression
*-I(a;b)+I(a;b|c)+I(a;b|d)+I(c;d)*; letters *H* and *I* indicating entropy
and mutual information are omitted; and comma (,) is used as a separator
instead of semicolon (;). For **full style** use the flag **-S** as follows:

    PROMPT> minitip -S '[a;b;c;d]+I(e;b|c)+I(e;c|b)+((b;c|e)>=-3*I(e;a,d|b,c)'
    [a;b;c;d]+I(e;b|c)+I(e;c|b)+I(b;c|e)>=-3*I(e;a,d|b,c)
         ==> TRUE

In interactive usage entropy expressions and constraints are entered
at the terminal. Here is an example session.

    PROMPT> minitip
    minitip: help
     quit            quit minitip
     help            display this text
     ?               synonym for 'help'
     check           check inequality with constraints
     checkw          check without constraints
     add             add new constraint
     list            list constraints: 3, 4-5
     del             delete numbered constraints
     style           show / change formula style
     syntax          entropy expression syntax
     about           history, license, author, etc
    minitip: add (e,ad|bc)=0
    minitip: list
    --- Constraints (total 1)
      1: (e,ad|bc)=0
    minitip: check [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0
          ==> TRUE with the constraints
    minitip: checkw [a,b,c,d]+(e,b|c)+(e,c|b)+(b,c|e)>=0
     Checking without constraints ...
          ==> FALSE
    minitip: quit
    Save the commands to the history file .minitip (y/n)? n
    PROMPT> 

Accepted flags and return values are:

| Flag  | meaning  |
|:------|:-----| 
| -h    | print short help and quit |
| -s    | use **minimal** style (default) |
| -S    | use **full** style |
| -q    | quiet, just check, don't print anything |
| -f \<file\> | use \<file\> as the history file (default: **.minitip**) |
| -v    | version and copyright |

| Return values | (when checking validity of the first argument) |
| :-------- | :-------------|
| 0         | the expression (with the given constraints) checked TRUE |
| 1         | the expression (with the given constraints) checked FALSE |
| 2         | syntax error in the expression or in some of the constraints |
| 3         | some error (problem too large, LP failure, etc) |


#### METHOD

After collecting all random variables in the expression (and constraints), 
the program creates all Shannon entropy inequalities for those variables.
Then checks whether the expression is a consequence of the constraints and
the Shannon inequalities. It is done by creating a Linear Program instance,
and calling and LP solver. Thus the result printed out by the program is
the answer to the following question:

> Does this inequality follow from the (basic) Shannon inequalities, and
> from the given constraints?

and **not** whether the given inequality is a valid entropy inequality (that is,
it holds for arbitrary collection of random variables).

The size of the LP problem can be very big. Using 10 random variables there
will be 1023 entropies (one for all nonempty subsets), and over 11500 Shannon
inequalities. The Shannon inequalities form a numerically ill-conditioned 
(highly degenerate) LP instance, thus, from time to time, any LP solver will
give erroneous answer.

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

     gcc -O3 *.c -lglp -lreadline -o minitip

#### AUTHOR

Laszlo Csirmaz, <csirmaz@ceu.edu>

#### DATE

15-Jan-2016


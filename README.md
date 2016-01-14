# minitip
minitip 
=======

a MINimal Information Theoretic Inequality Prover

    The original ITIP software (which runs under MATLAB) was developed by
    Raymond W. Yeung and Ying-On Yan; it is available at
    http://user-www.ie.cuhk.edu.hk/~ITIP/

    The stand-alone version Xitip at http://xitip.epfl.ch has graphical
    interface, and runs both in Windows and Linux.

    This program was written in C, uses the readline library to read entropy
    expressions; has a user friendly syntax checker; extended syntax; and
    glpk (gnu linear programming kit) as the LP solver.

USAGE

    When the command line contains an entropy expression, it is checked for
    validity. Additional constraints can be specified as additional arguments.
    
    In interactive usage, entropy expressions and constraints are entered
    at the terminal.

METHOD

    Collecting all random variables in the expression (and constraints), first
    the program creates all Shannon entropy inequalities for those variables.
    Then checks whether the expression is a consequence of the constraints and
    the Shannon inequalities. It is done by creating a Linear Program instance,
    and calling and LP solver.

HISTORY

    The original ITIP software was created and written by Raymond W. Yeung and
    Ying-On Yan; it is available at http://user-www.ie.cuhk.edu.hk/~ITIP. It
    runs under MATLAB.

    The stand-alone version Xitip at http://xitip.epfl.ch has graphical
    interface and runs both in Windows and Linux.

    This version is written in C from scratch, and has extended syntax, more
    user friendly syntax checker, and uses the glpk LP solver.

COMPILATION

    The program uses the 'readline' library and include files, and the
    'glpk' library and include files. It should compile without any problem.

AUTHOR

    Laszlo Csirmaz, <csirmaz@ceu.edu>

DATE

    15-Jan-2016


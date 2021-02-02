#### $Id: solaris.base.mk,v 1.1 2004/04/21 22:20:36 te Exp $ ####
# Tamer Embaby <tsemba@menanet.net> 
#

# C Compiler. XXX
CC		= gcc

# The directory that contains master make file (current directory.)
BASEDIR		= $$HOME/mk

# The C source tree root
C_ROOT		= $$HOME/c

# External files that should be distributed with every release of program,
# this is a list of iles without leading directory names e.g.,
#	somthing.inc mk.exe.inc
# These files must originally reside in $(BASEDIR).
BASE_EXT	=
DISTRIB_EXTERNAL_FILES += $(BASE_EXT)

# Debug flag.  To include debugging information or to optimize code.
DEBUG		= -g
#DEBUG		= -O3

# Header file(s) that will make whole project rebuild.
HDRDEP		= 

# Default C compile flags.
CFL		= -Wall $(DEBUG) -c -I.

# Extra compile flag(s) defined by the user.
CFL		+= $(UCFLAGS)

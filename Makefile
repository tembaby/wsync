# $Id: Makefile,v 1.4 2002/11/19 03:05:22 te Exp $
#

OBJS		= main.o strlib.o fsops.o edit.o html_dir.o
TARGET		= wsync
DEBUG 		=
UCFLAGS 	= -DUNIX -DUSE_MMAP -DDEBUG -Wmissing-prototypes
INSTALLDIR	= /home/te/bin
TARBALL		= wsync.tar.gz
HDRDEP		= wsync.h
XLIBS		= -L. -lutil -lftp

.include <unix.prog.c.mk>

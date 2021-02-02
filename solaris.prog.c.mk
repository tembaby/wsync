#### $Id: solaris.prog.c.mk,v 1.1 2004/04/21 22:20:36 te Exp $ ####
# Tamer Embaby <tsemba@menanet.net> 
#
# Generic makefile for C projects.
#

include solaris.base.mk

all		: compile

compile		: $(TARGET)

.c.o		: $(HDRDEP)
	$(CC) $(CFL) $< -o $@

$(TARGET)	: $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(XLIBS)

force		: clean all

clean		:
	@rm -rf *.o [Ee]rrs out output $(TARGET) $(TARGET).core $(CLEAN_EXTRA)


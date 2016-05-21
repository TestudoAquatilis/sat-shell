SOURCES=main.c sat_shell.c sat_problem.c sat_base_cnf.c sat_formula.c pty_run.c
PARSERSOURCES=sat_formula_parser.y
LEXSOURCES=sat_formula_lexer.l
EXECUTABLE=sat-shell

LIBS=glib-2.0 tcl tclln zlib

MAKEFILE=Makefile
VERSION=1.1

CC=gcc
LEXERG=flex
PARSERG=bison -d

#OPTFLAGS=-ggdb
OPTFLAGS=-O2
CFLAGS=-c -Wall -std=gnu99 $(OPTFLAGS) -D VERSION_STRING=\"$(VERSION)\"
LDFLAGS=$(OPTFLAGS)

OBJDIR=obj
PARSERDIR=parser
LEXERDIR=lexer

CFLAGS+=$(shell pkg-config --cflags $(LIBS)) -I $(PARSERDIR)/ -I $(LEXERDIR)/ -I./
LDFLAGS+=$(shell pkg-config --libs $(LIBS)) -lutil

LEXCSOURCES=$(LEXSOURCES:%.l=$(LEXERDIR)/%.c)
PARSERCSOURCES=$(PARSERSOURCES:%.y=$(PARSERDIR)/%.c)
SOURCES+=$(PARSERCSOURCES) $(LEXCSOURCES)

OBJECTS=$(SOURCES:%.c=$(OBJDIR)/%.o)
DEPS=$(SOURCES:%.c=$(OBJDIR)/%.d)

.PHONY: all memcheck slicecheck
all: $(PARSERSOURCES) $(LEXSOURCES) $(SOURCES) $(EXECUTABLE)

-include $(OBJECTS:.o=.d)

$(EXECUTABLE): $(OBJECTS) $(MAKEFILE)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

$(OBJDIR)/%.o: %.c $(MAKEFILE) | $(OBJDIR)
	$(CC) -MM $(CFLAGS) $*.c > $(OBJDIR)/$*.d
	sed -i -e "s/\\(.*\\.o:\\)/$(OBJDIR)\\/\\1/" $(OBJDIR)/$*.d
	$(CC) $(CFLAGS) $*.c -o $(OBJDIR)/$*.o

$(PARSERDIR)/%.c $(PARSERDIR)/%.h: %.y $(MAKEFILE) | $(PARSERDIR)
	$(PARSERG) $*.y

$(LEXERDIR)/%.c $(LEXERDIR)/%.h: %.l $(MAKEFILE) | $(LEXERDIR)
	$(LEXERG) $*.l

$(LEXERDIR):
	mkdir -p $(LEXERDIR)

$(PARSERDIR):
	mkdir -p $(PARSERDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR) $(OBJDIR)/$(PARSERDIR) $(OBJDIR)/$(LEXERDIR)

clean:
	rm -f $(EXECUTABLE) $(OBJECTS) $(DEPS)
	rm -rf $(OBJDIR) $(PARSERDIR) $(LEXERDIR)

memcheck: all
	valgrind --leak-check=full --suppressions=memcheck/suppress_libtcl.supp ./$(EXECUTABLE)
slicecheck: all
	G_SLICE=debug-blocks ./$(EXECUTABLE)

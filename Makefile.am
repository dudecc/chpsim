## Process this file with automake to produce Makefile.in

# Include clib library
CLIBDIR = clib
SUBDIRS = $(CLIBDIR)
AM_CFLAGS = -I$(CLIBDIR)
LDADD = $(CLIBDIR)/libalways.a

# Include readline, GMP and math
LDADD += -lreadline -lgmp -lm

bin_PROGRAMS = chpsim chpconv
chpsim_SOURCES = chpsim.c builtin_io.c chp.c exec.c expr.c interact.c ifrchk.c \
                 lex.c modules.c parse.c parse_obj.c print.c routines.c \
                 sem_analysis.c statement.c types.c value.c variables.c \
                 properties.c properties.h \
                 lex.h parse_obj.h print.h value.h ifrchk.h builtin_io.h chp.h \
                 exec.h expr.h modules.h parse.h routines.h interact.h \
                 sem_analysis.h statement.h types.h variables.h \
                 path.def symbol.def
chpconv_SOURCES = chpconv.c builtin_io.c chp.c exec.c expr.c interact.c \
                  ifrchk.c lex.c modules.c parse.c parse_obj.c print.c \
                  routines.c sem_analysis.c statement.c types.c value.c \
                  properties.c properties.h \
                  variables.c lex.h parse_obj.h print.h value.h ifrchk.h \
                  builtin_io.h chp.h exec.h expr.h modules.h parse.h routines.h \
                  interact.h sem_analysis.h statement.h types.h variables.h \
                  path.def symbol.def

# CHP library files
dist_pkgdata_DATA = builtin.chp stdio.chp channel.chp
AM_CFLAGS += -DPKGDATADIR='"$(pkgdatadir)"'

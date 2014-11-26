#-------------------------------------------------------------------------
#
# Makefile for src/bin/pg_config
#
# Copyright (c) 1998-2014, PostgreSQL Global Development Group
#
# src/bin/pg_config/Makefile
#
#-------------------------------------------------------------------------

PGFILEDESC = "pg_config - report configuration information"
PGAPPICON=win32

subdir = src/bin/pg_config
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS=   pg_config.o $(WIN32RES)

# don't include subdirectory-path-dependent -I and -L switches
STD_CPPFLAGS := $(filter-out -I$(top_srcdir)/src/include -I$(top_builddir)/src/include,$(CPPFLAGS))
STD_LDFLAGS := $(filter-out -L$(top_builddir)/src/port,$(LDFLAGS))

empty:= 
space:= $(empty) $(empty)
quote:=\"

define dval
  override CPPFLAGS += -DVAL_$1=$(subst $(space),\\040,$(quote)$($1)$(quote))
endef

CONFIGURE := $(configure_args)

$(eval $(call dval,CONFIGURE))
$(eval $(call dval,CC))
$(eval $(call dval,STD_CPPFLAGS))
$(eval $(call dval,CFLAGS))
$(eval $(call dval,CFLAGS_SL))
$(eval $(call dval,STD_LDFLAGS))
$(eval $(call dval,LDFLAGS_EX))
$(eval $(call dval,LDFLAGS_SL))
$(eval $(call dval,LIBS))

all: pg_config

pg_config: $(OBJS) | submake-libpgport
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@$(X)

install: all installdirs
	$(INSTALL_SCRIPT) pg_config$(X) '$(DESTDIR)$(bindir)/pg_config$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/pg_config$(X)'

clean distclean maintainer-clean:
	rm -f pg_config$(X) $(OBJS)
	rm -rf tmp_check

check: all
	$(prove_check)

installcheck:
	$(prove_installcheck)

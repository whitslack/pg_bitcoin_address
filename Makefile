PG_CONFIG = pg_config
PKG_CONFIG = pkg-config

MODULE_big = pg_base58check
EXTENSION = pg_base58check
DATA = pg_base58check--1.0.sql
OBJS = pg_base58check.o
PG_CFLAGS = -Wextra $(addprefix -Werror=,implicit-function-declaration incompatible-pointer-types int-conversion) -Wcast-qual -Wconversion -Wno-declaration-after-statement -Wdisabled-optimization -Wdouble-promotion -Wno-implicit-fallthrough -Wmissing-declarations -Wno-missing-field-initializers -Wpacked -Wno-parentheses -Wno-sign-conversion -Wstrict-aliasing $(addprefix -Wsuggest-attribute=,pure const noreturn malloc) -fstrict-aliasing -fvisibility=hidden
SHLIB_LINK =

LIBBASE58CHECK_CPPFLAGS := $(shell $(PKG_CONFIG) --cflags-only-I libbase58check)
LIBBASE58CHECK_CFLAGS := $(shell $(PKG_CONFIG) --cflags-only-other libbase58check)
LIBBASE58CHECK_LDFLAGS := $(shell $(PKG_CONFIG) --libs-only-L libbase58check)
LIBBASE58CHECK_LDLIBS := $(shell $(PKG_CONFIG) --libs-only-other libbase58check) $(shell $(PKG_CONFIG) --libs-only-l libbase58check)

PG_CPPFLAGS += $(LIBBASE58CHECK_CPPFLAGS)
PG_CFLAGS += $(LIBBASE58CHECK_CFLAGS)
PG_LDFLAGS += $(LIBBASE58CHECK_LDFLAGS)
SHLIB_LINK += $(LIBBASE58CHECK_LDLIBS)

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
override CPPFLAGS := $(subst $() -I, -isystem ,$(CPPFLAGS))

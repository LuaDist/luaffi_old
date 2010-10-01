
AR		= ar
RANLIB		= ranlib
CC		= gcc
CXX		= g++

objsuf		= .o

MAKEDEPEND	= gcc -M -MP

ifeq ($(WIN32),1)
WIN32CFLAG	= -DWIN32
WIN32FLAG	= -mno-cygwin
endif

LUAINCS		= -I/usr/include/lua5.1

CFLAGS		= -I. $(LUAINCS) $(WIN32FLAG) $(WIN32CFLAGS)
CXXFLAGS	= -I. $(LUAINCS) $(WIN32FLAG) $(WIN32CFLAGS)

LDFLAGS		= -lffi -llua5.1 -lm $(WIN32FLAG)

RCFLAGS		= $(CFLAGS)
RCXXFLAGS	= $(CXXFLAGS)

ifeq ($(OSX),1)
RCXXFLAGS	+= -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch i386 -arch ppc
RCFLAGS		+= -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch i386 -arch ppc
LDFLAGS		+= -Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch i386 -arch ppc
SHARED		= -bundle -undefined dynamic_lookup
#-dynamiclib
CXX=export MACOSX_DEPLOYMENT_TARGET="10.3"; g++
else
SHARED		= -shared
endif

GFLAGS		= -O2 -g
CFLAGS		+= $(GFLAGS)
CXXFLAGS	+= $(GFLAGS)
LDFLAGS		+= $(GFLAGS)

objs		= $(csrc:.c=$(objsuf)) $(cxxsrc:.cxx=$(objsuf))
deps		= $(objs:$(objsuf)=.dep)

all: $(deps) $(lib)

$(lib): $(objs)

clean:
	@echo [cleaning]
	rm -f $(lib) $(objs)

.SUFFIXES: .c .cxx .o .a .so .dep

%.so: $(objs)
	@echo [linking module]
	$(CXX) $(SHARED) -o $@ $^ $(LDFLAGS)

%.a: $(objs)
	@echo [building library $@]
	rm -f $@
	$(AR) crs $@ $(objs)
	$(RANLIB) $@

.c$(objsuf):
	@echo [compiling $<]
	$(CC) $(RCFLAGS) -c $< -o $@

.cxx$(objsuf):
	@echo [compiling $<]
	$(CXX) $(RCXXFLAGS) -c $< -o $@

.cxx.dep:
	@echo [building dependencies for $<]
	($(MAKEDEPEND) $(CXXFLAGS) $< 2> /dev/null) | sed s/\\\.o:/\\$(objsuf):/ > $@

.c.dep:
	@echo [building dependencies for $<]
	($(MAKEDEPEND) $(CXXFLAGS) $< 2> /dev/null) | sed s/\\\.o:/\\$(objsuf):/ > $@

-include $(deps)

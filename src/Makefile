
SHELL = /bin/sh

SRCDIR = .
XPDFSRCDIR = ../xpdf
XPDFLIBDIR = ../xpdf
GOOSRCDIR = ../goo
GOOLIBDIR = ../goo
FOFISRCDIR = ../fofi
FOFILIBDIR = ../fofi
SPLASHSRCDIR = ../splash
SPLASHLIBDIR = ../splash

CXXFLAGS = -I/usr/local/include -g -O2 -DHAVE_CONFIG_H -DHAVE_DIRENT_H=1  -I.. -DHAVE_REWINDDIR=1 -DHAVE_POPEN=1 -I.. -I$(GOOSRCDIR) -I$(XPDFSRCDIR) -I$(FOFISRCDIR) -I$(SPLASHSRCDIR) -I$(srcdir)           -I/usr/X11R6/include

LDFLAGS = 
FTLIBS = 

OTHERLIBS = 

CXX ?= c++

LIBPREFIX = lib
EXE = 

#------------------------------------------------------------------------

.SUFFIXES: .cc

.cc.o:
	$(CXX) $(CXXFLAGS) -c $<

#------------------------------------------------------------------------

CXX_SRC = \
	$(SRCDIR)/pdf3json.cc \
	$(SRCDIR)/ImgOutputDev.cc \
	$(SRCDIR)/XmlFonts.cc \
	$(SRCDIR)/XmlLinks.cc 

#------------------------------------------------------------------------

all: pdf3json$(EXE)

#-------------------------------------------------------------------------

pdf3json_OBJS = ImgOutputDev.o XmlFonts.o XmlLinks.o \
    pdf3json.o
pdf3json_LIBS = -L$(GOOLIBDIR) -L$(FOFILIBDIR) -L$(SPLASHLIBDIR) $(FTLIBS) -L$(XPDFLIBDIR) $(OTHERLIBS) -lXpdf -lGoo -lfofi -lsplash -lm

pdf3json$(EXE): $(pdf3json_OBJS) $(GOOLIBDIR)/$(LIBPREFIX)Goo.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o pdf3json$(EXE) $(pdf3json_OBJS) \
		$(pdf3json_LIBS)

#-------------------------------------------------------------------------
pdf3json_WINOBJS = pdf3json.exe ImgOutPutDev.obj  pdf3json.obj  XmlFonts.obj  XmlLinks.obj

clean:
	rm -f $(pdf3json_OBJS) pdf3json$(EXE)
	rm -f $(pdf3json_WINOBJS)

#------------------------------------------------------------------------

distdepend:
	cp Makefile.in Makefile.in.bak
	sed '/^#----- dependences -----/q' Makefile.in.bak >Makefile.in
	$(CXX) $(CXXFLAGS) -MM $(CXX_SRC) >>Makefile.in


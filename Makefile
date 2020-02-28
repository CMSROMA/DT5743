########################################################################
#
##              --- CAEN SpA - Computing Division ---
#
##   CAENDigitizer Software Project
#
##   Created  :  October    2009      (Rel. 1.0)
#
##   Auth: A. Lucchesi
#
#########################################################################
ARCH	=	`uname -m`

OUTDIR  =    	./bin/$(ARCH)/Release/
OUTNAME =    	DT5743DAQ
OUTCONVNAME =    	DT5743Conv

OUT     =    	$(OUTDIR)/$(OUTNAME)
OUTCONV     =    	$(OUTDIR)/$(OUTCONVNAME)

CC	=	gcc
CXX	=	g++

COPTS	=	-fPIC -DLINUX -O2

#FLAGS	=	-soname -s
#FLAGS	=       -Wall,-soname -s
#FLAGS	=	-Wall,-soname -nostartfiles -s
#FLAGS	=	-Wall,-soname

DEPLIBS	=	-lCAENDigitizer 
DEPLIBSCXX	=	`root-config --glibs`

LIBS	=	-L..

INCLUDEDIR =	-I./include 
INCLUDEDIRCXX =	-I./include 
INCLUDEDIRCXX +=`root-config --cflags`	

OBJS	=	src/ReadoutTest_Digitizer.o src/keyb.o 
OBJSCONV	=	src/Event.o src/dt5743convert.o

INCLUDES =	./include/*

#########################################################################

all	:	$(OUT) $(OUTCONV)

clean	:
		/bin/rm -f $(OBJS) $(OUT) $(OUTCONV)

$(OUT)	:	$(OBJS)
		/bin/rm -f $(OUT)
		if [ ! -d $(OUTDIR) ]; then mkdir -p $(OUTDIR); fi
		$(CC) $(FLAGS) -o $(OUT) $(OBJS) $(DEPLIBS)

$(OUTCONV):	$(OBJSCONV)
		/bin/rm -f $(OUTCONV)
		if [ ! -d $(OUTDIR) ]; then mkdir -p $(OUTDIR); fi
		$(CXX) $(FLAGS) -o $(OUTCONV) $(OBJSCONV) $(DEPLIBSCXX)

$(OBJS)	:	$(INCLUDES) Makefile

$(OBJSCONV)	:	$(INCLUDES) Makefile

%.o	:	%.c
		$(CC) $(COPTS) $(INCLUDEDIR) -c -o $@ $<

%.o	:	%.cxx
		$(CXX) $(COPTS) $(INCLUDEDIRCXX) -c -o $@ $<


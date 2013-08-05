# Makefile for yorick-z package
# $Id: Makefile,v 1.1 2005-09-18 22:07:11 dhmunro Exp $

# run configure script to create Makeyorz and fill in following macros

Y_MAKEDIR=
Y_EXE=
Y_EXE_PKGS=
Y_EXE_HOME=
Y_EXE_SITE=
Y_HOME_PKG=

# ----------------------------------------------------- optimization flags

COPT=$(COPT_DEFAULT)
TGT=$(DEFAULT_TGT)

# ------------------------------------------------ macros for this package

PKG_NAME=yorz
# change to give TGT=exe executable a name other than yorick
PKG_EXENAME=yorick

# Makeyorz defines PKG_I, OBJS, PKG_DEPLIBS, PKG_I_START, and *_INC macros
include ./Makeyorz

# list of additional package names you want in PKG_EXENAME
# (typically Y_EXE_PKGS should be first here)
EXTRA_PKGS=$(Y_EXE_PKGS)

# list of additional files for clean
PKG_CLEAN=test*.mpg test*.jpg test*.png cfg* yorz.doc

# set compiler or loader flags specific to this package
PKG_CFLAGS=
PKG_LDFLAGS=

# executable install directory
Y_BINDIR=$(Y_HOME)/bin

# -------------------------------- standard targets and rules (in Makepkg)

# set macros Makepkg uses in target and dependency names
# DLL_TARGETS, LIB_TARGETS, EXE_TARGETS
# are any additional targets (defined below) prerequisite to
# the plugin library, archive library, and executable, respectively
PKG_I_DEPS=$(PKG_I)
Y_DISTMAKE=distmake
ALL_TARGETS=yorz.doc

include $(Y_MAKEDIR)/Make.cfg
include $(Y_MAKEDIR)/Makepkg
include $(Y_MAKEDIR)/Make$(TGT)

# override macros Makepkg sets for rules and other macros
# Y_HOME and Y_SITE in Make.cfg may not be correct (e.g.- relocatable)
Y_HOME=$(Y_EXE_HOME)
Y_SITE=$(Y_EXE_SITE)

# reduce chance of yorick-1.5 corrupting this Makefile
MAKE_TEMPLATE = /usr/lib/yorick/1.5/protect-against-1.5

# ------------------------------------- targets and rules for this package

ypng.o: spng.h

spng.o: spng.c spng.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PNG_INC) -c spng.c

yzlib.o: yzlib.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ZLIB_INC) -c yzlib.c

yjpeg.o: yjpeg.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(JPEG_INC) -c yjpeg.c

ympeg.o: ympeg.c $(YAVCODEC_H)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AVCODEC_INC) -c ympeg.c

yorz.doc: $(PKG_I)
	$(Y_EXE) -batch ./maked.i $(PKG_I)

install:: yorz.doc
	$(YNSTALL) yorz.doc "$(DEST_Y_SITE)/doc"

uninstall::
	rm -f "$(DEST_Y_SITE)/doc/yorz.doc"

distclean::
	rm -f Makeyorz yorz.i

# -------------------------------------------------------- end of Makefile

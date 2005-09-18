/*
 * $Id: maked.i,v 1.1 2005-09-18 22:07:11 dhmunro Exp $
 * make .doc files for yorz package
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#include "mkdoc.i"

list = get_argv();
if (numberof(list) < 2) {
  write, "WARNING no .i documentation files specfied on command line";
} else {
  mkdoc, list(2:0), "yorz.doc";
  write, "DOCUMENT comments written to yorz.doc";
}

quit;

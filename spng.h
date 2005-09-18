/*
 * $Id: spng.h,v 1.1 2005-09-18 22:07:10 dhmunro Exp $
 * simplified png interface, directed to scientific data storage
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#ifndef SPNG_H
#define SPNG_H 1

typedef struct sp_memops sp_memops;
typedef struct sp_info sp_info;

struct sp_info {
  int depth, nchan, width, height;
  /* depth = 1<=depth<=16 is number of bits per channel per pixel
   * nchan = 1 for gray or pseudocolor
   *         2 for gray + alpha
   *         3 for RGB
   *         4 for RGBA
   *   npal>0 and palette!=0 distinguishes gray from pseudocolor
   *   depth<=8 for pseudocolor (npal<=256)
   * alpha is opacity, so 0 means transparent
   * image order is RGBA, if all channels are present
   * palette also legal for nchan == 3, 4 as suggested palette
   * 
   */
  unsigned char *cimage;   /* use this if depth<=8, 1 byte per pixel */
  unsigned short *simage;  /* use this if depth==16 */

  int npal;                         /* palette[npal][3], alpha[npal] */
  unsigned char *palette, *alpha;   /* PLTE, tRNS */
  int colors;                       /* 0 or SP_TRNS, SP_BKGD bits */
  unsigned short trns[3], bkgd[3];  /* tRNS, bKGD */

  int ntxt;                         /* tEXt, zTXt */
  char **keytxt;                    /* keytxt[ntxt][2] is [key,text] */
  short itime[6];                   /* tIME, yy/mm/dd hh:mm:ss */

  /* pHYs is size as printed or scanned (n_xpix, n_ypix), (0,0) means none
   * pixels/meter if per_meter!=0, else just aspect ratio */
  int n_xpix, n_ypix, per_meter;    /* pHYs size as printed or scanned */

  /* sCAL is size for depicted object
   * sunit == 0 for unknown, or SP_METERS or SP_RADIANS
   * (xpix_sz, ypix_sz) == (0,0) for no sCAL */
  int sunit;                        /* 0 or SP_METERS or SP_RADIANS */
  double xpix_sz, ypix_sz;

  /* pCAL is pixel value mapping */
  char *purpose, *punit;
  int eqtype, x0, x1, mx;
  double p[4];

  int nerrs, nwarn;                 /* error, warning counts */
  char msg[96];                     /* error or first warning message */
};

/* colors values */
#define SP_TRNS 1
#define SP_BKGD 2

/* sunit values */
#define SP_UNKNOWN 0
#define SP_METERS 1
#define SP_RADIANS 2

/* eqtype values */
#define SP_LINEAR 0
#define SP_EXP 1
#define SP_POW 2
#define SP_SINH 3

struct sp_memops {
  /* for simage or cimage */
  void *(*imalloc)(int depth, int nchan, int width, int height);
  void (*ifree)(void *image);
  /* for palette, alpha */
  void *(*cmalloc)(int nchan, int npal);
  void (*cfree)(void *palette);
  /* for keytxt array of text pointers */
  void *(*pmalloc)(int ntxt);
  void (*pfree)(void *keytxt);
  /* for text strings keytxt[i], purpose, punit */
  void *(*tmalloc)(int nbytes);
  void (*tfree)(void *text);
  /* for internal scratch space */
  void *(*smalloc)(unsigned long nbytes);
  void (*sfree)(void *scratch);
};

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

extern int sp_read(const char *filename, sp_memops *memops, sp_info *info);
extern int sp_write(const char *filename, sp_memops *memops, sp_info *info);
extern void sp_free(sp_info *info, sp_memops *memops);
extern sp_info *sp_init(sp_info *info);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif

/*
Not all depths are available for all values of nchan:
             nchan  palette  depths
gray           1      no     1, 2, 4, 8, 16
pseudocolor    1      yes    1, 2, 4, 8
gray+alpha     2      -      8, 16
rgb            3      -      8, 16
rgba           4      -      8, 16

Note that if you do call sp_setpal, then the image depth is limited to
8 bits, so the maximum permissible value of npal is 256; png pseudocolor
palettes are limited to 256 colors.  This restriction to 8 bit depth
means that many or even most scientific data sets should be stored as
grayscale (or rgb) images.

The png format also permits a palette for rgb or rgba images.  In
that case, the palette is merely a suggested set of colors if the
rendering program is restricted to a pseudocolor display.
Transparency is not allowed for this use of a palette, so alpha!=0 is
an error if nchan!=1.  (The spng interface always writes a PLTE
chunk; spng does not support the sPLT chunk for defining more
elaborate suggested palettes.)

If there is no alpha channel, you may specify a single transparent color.

When nchan=1, trns[0] is transparent gray.
When nchan=3, trns[0,1,2] are rgb for transparent color.
When nchan=2 or nchan=4, trns is illegal.

The png format also allows you to suggest a background color against
which your image will look good.  (This would be particularly important
if your image has transparency, either via an alpha channel or
sp_settrans.)

When nchan = 1 or 2, bkgd[0] is background gray.
When nchan = 3 or 4, trns[0,1,2] are rgb for background color.

The png standard mentions the following keywords: "Title", "Author",
"Description", "Copyright", "Creation Time", "Software" (program that
created the image), "Disclaimer", "Warning", "Source" (device used to
create the image), and "Comment".

The spng interface supports three additional png chunks:
   pHYs = pixel dimensions and/or aspect ratio as scanned or printed
   sCAL = physical scale of subject of image
   pCAL = mapping from pixel values to physical values

Parameter n_xpix or n_ypix is the number of pixels per unit in the
horizontal (vertical) direction, and per_meter!=0 indicates that one
unit is one meter.  If per_meter=0, the unit is unknown, and only the
ratio of the vertical to horizontal numbers of pixels is significant.

Parameter xpix_sz or ypix_sz is the number of units per pixel and
sunit is either SP_METERS or SP_RADIANS.

Parameter purpose is a name or description of the calibration, and punit
is the units of the physical pixel value for a pCAL mapping.

The pCAL defines first a mapping from floating point data values to
integer values, and second a mapping from integer values into the
specified bit depth of the image to be stored.  The integer mapping is
as follows:
   original = original integer (digitized) data
   x0 <= original <= x1
   stored = scaled integer data actually stored in png
   0 <= stored <= max = 2^depth-1
   stored = (unsigned)(max*((double)original-x0)/(x1-x0) + 0.50000001)
The floating point mapping is:
     the png standard defines four types of floating point maps:
   original = (int)((physical-p0)*(x1-x0)/p1 + 0.0000001)
     if type=0 (linear)
   original = (int)(log((physical-p0)/p1)*(x1-x0)/p2 + 0.0000001)
     if type=1 (exponential)
   original = (int)(log((physical-p0)/p1)*(x1-x0)/log(p2) + 0.0000001)
     if type=2 (exponential alternate form)
   original = (int)(asinh((physical-p0)/p1)*(x1-x0)/p2 + p3 + 0.0000001)
     if type=3 (hyperbolic)

The spng interface does not support images of less than 8 bits (one char)
in memory.  You may create png files with depth less than 8 bits, but
you must write such files using an image array with at least one byte
per pixel.

*/

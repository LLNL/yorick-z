/*
 * $Id: ypng.c,v 1.1 2005-09-18 22:07:08 dhmunro Exp $
 * png interface for yorick
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#include "spng.h"

#include "pstdlib.h"
#include "ydata.h"
#include "yio.h"


extern void Y__png_read(int nArgs);
extern void Y__png_write(int nArgs);

/* for simage or cimage */
static void *ypng_imalloc(int depth, int nchan, int width, int height);
static void ypng_ifree(void *image);
/* for palette, alpha */
static void *ypng_cmalloc(int nchan, int npal);
static void ypng_cfree(void *palette);
/* for keytxt array of text pointers */
static void *ypng_pmalloc(int ntxt);
static void ypng_pfree(void *keytxt);
/* for text strings keytxt[i], purpose, punit */
static void *ypng_tmalloc(int nbytes);
static void ypng_tfree(void *text);
/* for internal scratch space */
static void *ypng_smalloc(unsigned long nbytes);
static void ypng_sfree(void *scratch);

static sp_memops ypng_memops = {
  ypng_imalloc, ypng_ifree, ypng_cmalloc, ypng_cfree,
  ypng_pmalloc, ypng_pfree, ypng_tmalloc, ypng_tfree,
  ypng_smalloc, ypng_sfree };

/* infopp is pointer info[9]:
 *   info[0] = info.palette (char)
 *   info[1] = info.alpha (char) or <replaced by info.trns[3] (long)>
 *   info[2] = <replaced by info.bkgd[3] (long)>
 *   info[3] = <replaced by info.x0, x1, mx, eqtype, p[4] (double)>
 *   info[4] = <replaced by info.purpose, punit (string)>
 *   info[5] = <replaced by info.xpix_sz, ypix_sz, sunit (double)>
 *   info[6] = <replaced by info.n_xpix, n_ypix, per_meter (long)>
 *   info[7] = <replaced by info.keytxt (string)>
 *   info[8] = <replaced by info.itime[6] (long)>
 * dnwh[8]:
 *   dnwh[0] = info.depth;
 *   dnwh[1] = info.nchan;
 *   dnwh[2] = info.width;
 *   dnwh[3] = info.height;
 *   dnwh[4] = info.npal;
 *   dnwh[5] = info.ntxt;
 *   dnwh[6] = info.alpha!=0
 *   dnwh[7] = info.nwarn;
 */

void
Y__png_read(int nArgs)
{
  /* do not bother to check arguments here -- done in interpreted code */
  char *filename = YGetString(sp-4);
  long *dnwh = YGet_L(sp-3,0,0);
  void **infop = YGet_P(sp-2,0,0);
  void **imagep = YGet_P(sp-1,0,0);
  char **emsg = YGet_Q(sp-0,0,0);

  sp_info info;
  char *file = filename? p_native(filename) : 0;
  int rslt = file? sp_read(file, &ypng_memops, &info) : 0;
  if (file) p_free(file);

  dnwh[6] = info.nwarn;
  if (rslt || info.nwarn)
    emsg[0] = p_strcpy(info.msg);
  if (rslt) {
    PushIntValue(rslt);

  } else {
    int i;
    imagep[0] = (info.depth>8)? (void *)info.simage : (void *)info.cimage;
    dnwh[0] = info.depth;
    dnwh[1] = info.nchan;
    dnwh[2] = info.width;
    dnwh[3] = info.height;
    dnwh[4] = info.npal;
    dnwh[5] = info.ntxt;
    dnwh[6] = (info.alpha != 0);
    dnwh[7] = info.nwarn;
    infop[0] = info.palette;
    infop[1] = info.alpha;
    if ((info.colors & SP_TRNS) && !info.alpha) {
      Array *a = NewArray(&longStruct, ynew_dim((info.nchan>2)? 3L : 1L, 0));
      long *trns = a->value.l;
      infop[1] = trns;
      trns[0] = info.trns[0];
      if (info.nchan > 2) {
        trns[1] = info.trns[1];
        trns[2] = info.trns[2];
      }
    }
    if (info.colors & SP_BKGD) {
      Array *a = NewArray(&longStruct, ynew_dim((info.nchan>2)? 3L : 1L, 0));
      long *bkgd = a->value.l;
      infop[2] = bkgd;
      bkgd[0] = info.bkgd[0];
      if (info.nchan > 2) {
        bkgd[1] = info.bkgd[1];
        bkgd[2] = info.bkgd[2];
      }
    }
    if (info.x0 != info.x1) {
      Array *a = NewArray(&doubleStruct, ynew_dim(8L, 0));
      double *pcal = a->value.d;
      infop[3] = pcal;
      pcal[0] = (double)info.x0;
      pcal[1] = (double)info.x1;
      pcal[2] = (double)info.mx;
      pcal[3] = (double)info.eqtype;
      pcal[4] = info.p[0];
      pcal[5] = info.p[1];
      pcal[6] = info.p[2];
      pcal[7] = info.p[3];
      if (info.purpose || info.punit) {
        Array *a = NewArray(&stringStruct, ynew_dim(2L, 0));
        char **pcal2 = a->value.q;
        infop[4] = pcal2;
        pcal2[0] = info.purpose;
        pcal2[1] = info.punit;
      }
    }
    if (info.xpix_sz && info.ypix_sz) {
      Array *a = NewArray(&doubleStruct, ynew_dim(3L, 0));
      double *scal = a->value.d;
      infop[5] = scal;
      scal[0] = info.xpix_sz;
      scal[1] = info.ypix_sz;
      scal[2] = (double)info.sunit;
    }
    if (info.n_xpix && info.n_ypix) {
      Array *a = NewArray(&longStruct, ynew_dim(3L, 0));
      long *phys = a->value.l;
      infop[6] = phys;
      phys[0] = info.n_xpix;
      phys[1] = info.n_ypix;
      phys[2] = info.per_meter;
    }
    infop[7] = info.keytxt;
    if (info.itime[2]) {
      Array *a = NewArray(&longStruct, ynew_dim(6L, 0));
      long *time = a->value.l;
      infop[8] = time;
      for (i=0 ; i<6 ; i++) time[i] = info.itime[i];
    }

    PushIntValue(0);
  }
}

void
Y__png_write(int nArgs)
{
  /* do not bother to check arguments here -- done in interpreted code */
  char *filename = YGetString(sp-4);
  long *dnwh = YGet_L(sp-3,0,0);
  void **infop = YGet_P(sp-2,0,0);
  void *image = *YGet_P(sp-1,0,0);
  char **emsg = YGet_Q(sp-0,0,0);
  char *file;
  int rslt;

  unsigned char *palette = (dnwh[4]>0 && dnwh[4]<=256)? infop[0] : 0;
  unsigned char *alpha = (palette && dnwh[6])? infop[1] : 0;
  unsigned long *trns = dnwh[6]? 0 : infop[1];
  unsigned long *bkgd = infop[2];
  double *pcal = infop[3];
  char **pcal2 = infop[4];
  double *scal = infop[5];
  long *phys = infop[6];
  char **keytxt = infop[7];
  long *time = infop[8];

  int depth = dnwh[0];
  int nchan = dnwh[1];
  int width = dnwh[2];
  int height = dnwh[3];
  int npal = palette? dnwh[4] : 0;
  int ntxt = dnwh[5];

  sp_info info;
  sp_init(&info);

  if (depth > 8) info.simage = image;
  else info.cimage = image;

  info.depth = depth;
  info.nchan = nchan;
  info.width = width;
  info.height = height;
  info.npal = npal;
  info.palette = palette;
  info.alpha = alpha;
  if (trns) {
    info.colors |= SP_TRNS;
    info.trns[0] = trns[0];
    if (nchan > 2) info.trns[1] = trns[1], info.trns[2] = trns[2];
    else info.trns[1] = info.trns[2] = trns[0];
  }
  if (bkgd) {
    info.colors |= SP_BKGD;
    info.bkgd[0] = bkgd[0];
    if (nchan > 2) info.bkgd[1] = bkgd[1], info.bkgd[2] = bkgd[2];
    else info.bkgd[1] = info.bkgd[2] = bkgd[0];
  }
  if (pcal && pcal[0]!=pcal[1]) {
    info.x0 = (int)pcal[0];
    info.x1 = (int)pcal[1];
    info.mx = (int)pcal[2];       /* unused? */
    info.eqtype = (int)pcal[3];
    info.p[0] = pcal[4];
    info.p[1] = pcal[5];
    info.p[2] = pcal[6];
    info.p[3] = pcal[7];
    if (pcal2) {
      info.purpose = pcal2[0];
      info.punit = pcal2[1];
    }
  }
  if (scal) {
    info.xpix_sz = scal[0];
    info.ypix_sz = scal[1];
    info.sunit = (int)scal[2];
  }
  if (phys) {
    info.n_xpix = (int)phys[0];
    info.n_ypix = (int)phys[1];
    info.per_meter = (phys[2]!=0);
  }
  if (keytxt && ntxt) {
    info.ntxt = ntxt;
    info.keytxt = keytxt;
  }
  if (time) {
    int i;
    for (i=0 ; i<6 ; i++) info.itime[i] = (short)time[i];
  }

  file = p_native(filename);
  rslt = sp_write(filename, &ypng_memops, &info);
  p_free(file);
  dnwh[7] = info.nwarn;
  if (rslt || info.nwarn)
    emsg[0] = p_strcpy(info.msg);

  PushIntValue(rslt);
}

/* for simage or cimage */
static void *
ypng_imalloc(int depth, int nchan, int width, int height)
{
  Array *a;
  Dimension *d =
    ynew_dim((long)height, NewDimension((long)width, 1L, (nchan==1)? 0 :
                                        NewDimension((long)nchan, 1L, 0)));
  if (depth <= 8) {
    a = NewArray(&charStruct, d);
    return a->value.c;
  } else {
    a = NewArray(&shortStruct, d);
    return a->value.s;
  }
}

static void
ypng_ifree(void *image)
{
  Array *a = Pointee(image);
  Unref(a);
}

/* for palette, alpha */
static void *
ypng_cmalloc(int nchan, int npal)
{
  Array *a;
  if (nchan==1)
    a = NewArray(&charStruct, ynew_dim((long)npal, 0));
  else
    a = NewArray(&charStruct, ynew_dim((long)npal, NewDimension(3L, 1L, 0)));
  return a->value.c;
}

static void
ypng_cfree(void *palette)
{
  Array *a = Pointee(palette);
  Unref(a);
}

/* for keytxt array of text pointers */
static void *
ypng_pmalloc(int ntxt)
{
  Array *a = NewArray(&stringStruct, ynew_dim((long)(ntxt+1)/2,
                                              NewDimension(2L, 1L, 0)));
  return a->value.q;
}

static void
ypng_pfree(void *keytxt)
{
  Array *a = Pointee(keytxt);
  Unref(a);
}

/* for text strings keytxt[i], purpose, punit */
static void *
ypng_tmalloc(int nbytes)
{
  return p_malloc(nbytes);
}

static void
ypng_tfree(void *text)
{
  p_free(text);
}

/* for internal scratch space */
static void *
ypng_smalloc(unsigned long nbytes)
{
  return p_malloc(nbytes);
}

static void
ypng_sfree(void *scratch)
{
  p_free(scratch);
}

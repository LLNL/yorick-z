/*
 * $Id: spng.c,v 1.1 2005-09-18 22:07:10 dhmunro Exp $
 * simplified png interface, directed to scientific data storage
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "png.h"
#include "spng.h"

/*------------------------------------------------------------------------*/

sp_info *
sp_init(sp_info *info)
{
  info->depth = info->nchan = info->width = info->height = 0;
  info->cimage = 0;
  info->simage = 0;
  info->npal = info->colors = 0;
  info->palette = info->alpha = 0;
  info->trns[0] = info->trns[1] = info->trns[2] = 0;
  info->bkgd[0] = info->bkgd[1] = info->bkgd[2] = 0;
  info->n_xpix = info->n_ypix = info->sunit = info->per_meter = 0;
  info->xpix_sz = info->ypix_sz = 0.0;
  info->purpose = info->punit = 0;
  info->eqtype = info->x0 = info->x1 = info->mx = 0;
  info->p[0] = info->p[1] = info->p[2] = info->p[3] = 0.0;
  info->ntxt = 0;
  info->keytxt = 0;
  info->itime[0] = info->itime[1] = info->itime[2] =
    info->itime[3] = info->itime[4] = info->itime[5] = 0;
  info->nerrs = info->nwarn = 0;
  info->msg[0] = '\0';
  return info;
}

void
sp_free(sp_info *info, sp_memops *memops)
{
  if (info->cimage) {
    if (memops && memops->ifree) memops->ifree(info->cimage);
    else free(info->cimage);
    info->cimage = 0;
  }
  if (info->simage) {
    if (memops && memops->ifree) memops->ifree(info->simage);
    else free(info->simage);
    info->simage = 0;
  }
  if (info->palette) {
    if (memops && memops->cfree) memops->cfree(info->palette);
    else free(info->palette);
    info->palette = 0;
  }
  if (info->alpha) {
    if (memops && memops->cfree) memops->cfree(info->alpha);
    else free(info->alpha);
    info->alpha = 0;
  }
  if (info->purpose) {
    if (memops && memops->tfree) memops->tfree(info->purpose);
    else free(info->purpose);
    info->purpose = 0;
  }
  if (info->punit) {
    if (memops && memops->tfree) memops->tfree(info->punit);
    else free(info->punit);
    info->punit = 0;
  }
  if (info->keytxt) {
    int i, n = 2*info->ntxt;
    for (i=0 ; i<n ; i++) {
      if (memops && memops->tfree) memops->tfree(info->keytxt[i]);
      else free(info->keytxt[i]);
      info->keytxt[i] = 0;
    }
    if (memops && memops->pfree) memops->pfree(info->keytxt);
    else free(info->keytxt);
    info->keytxt = 0;
  }
}

/*------------------------------------------------------------------------*/

typedef struct spng_id spng_id;
struct spng_id {
  spng_id *id;
  png_structp p;
  png_infop pi;
  sp_memops *memops;
  sp_info *info;
};

static void spng_error(png_structp p, png_const_charp msg);
static void spng_warning(png_structp p, png_const_charp msg);

static png_voidp spng_malloc(png_structp p, png_size_t nbytes);
static void spng_free(png_structp p, png_voidp ptr);

/*------------------------------------------------------------------------*/

int
sp_read(const char *filename, sp_memops *memops, sp_info *info)
{
  spng_id id;
  FILE *f = 0;
  png_structp p = 0;
  png_infop pi = 0;
  png_voidp (*spx_malloc)(png_structp p, png_size_t nbytes) = 0;
  void (*spx_free)(png_structp p, png_voidp ptr) = 0;
  png_bytep *rows = 0;
  png_color_8p sbit = 0;

  if (memops && memops->smalloc && memops->sfree) {
    spx_malloc = spng_malloc;
    spx_free = spng_free;
  }

  id.id = &id;
  id.p = 0;
  id.pi = 0;
  id.memops = memops;
  id.info = info;
  sp_init(info);

  f = fopen(filename, "rb");
  if (!f) return 1;

  id.p = p = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
                                      &id, spng_error, spng_warning,
                                      &id, spx_malloc, spx_free);
  if (!p) { fclose(f); return 2; }

  if (setjmp(png_jmpbuf(p))) {
    if (rows) {
      if (!memops || !memops->sfree) free(rows);
      else memops->sfree(rows);
    }
    png_destroy_read_struct(&p, &pi, 0);
    fclose(f);
    sp_free(info, memops);
    return 3;
  }

  id.pi = pi = png_create_info_struct(p);
  if (!pi) spng_error(p, "png_create_info_struct failed");

  png_init_io(p, f);
  png_read_info(p, pi);

  {
    png_uint_32 w, h;
    int d, ctype;
    png_get_IHDR(p, pi, &w, &h, &d, &ctype, 0,0,0);
    info->width = w;
    info->height = h;
    info->depth = d;
    if (ctype == PNG_COLOR_TYPE_PALETTE) info->nchan = 0;
    else if (ctype == PNG_COLOR_TYPE_GRAY) info->nchan = 1;
    else if (ctype == PNG_COLOR_TYPE_GRAY_ALPHA) info->nchan = 2;
    else if (ctype == PNG_COLOR_TYPE_RGB) info->nchan = 3;
    else if (ctype == PNG_COLOR_TYPE_RGB_ALPHA) info->nchan = 4;
    else spng_error(p, "unknown PNG_COLOR_TYPE in png file");
  }

  if (!info->nchan || info->nchan>=3) {
    int npal, i;
    png_colorp palette;
    if (png_get_PLTE(p, pi, &palette, &npal)) {
      info->npal = npal;
      if (npal > 0) {
        int ntrans;
        png_bytep trans;
        png_color_16p tvals;
        if (!memops || !memops->cmalloc) info->palette = malloc(3*npal);
        else info->palette = memops->cmalloc(3, npal);
        if (!info->palette) spng_error(p, "spng failed to malloc palette");
        for (i=0 ; i<npal ; i++) {
          info->palette[3*i  ] = palette[i].red;
          info->palette[3*i+1] = palette[i].green;
          info->palette[3*i+2] = palette[i].blue;
        }
        if (!info->nchan &&
            png_get_tRNS(p, pi, &trans, &ntrans, &tvals)) {
          if (ntrans > npal) ntrans = npal;
          if (!memops || !memops->cmalloc) info->alpha = malloc(npal);
          else info->alpha = memops->cmalloc(1, npal);
          if (!info->alpha) spng_error(p, "spng failed to malloc alpha");
          for (i=0 ; i<ntrans ; i++) info->alpha[i] = trans[i];
          for ( ; i<npal ; i++) info->alpha[i] = 255;
        }
      }
    }
  }

  if (info->nchan==1 || info->nchan==3) {
    int ntrans;
    png_bytep trans;
    png_color_16p tvals;
    if (png_get_tRNS(p, pi, &trans, &ntrans, &tvals)) {
      if (info->nchan == 1) {
        info->trns[0] = info->trns[1] = info->trns[2] = tvals->gray;
      } else {
        info->trns[0] = tvals->red;
        info->trns[1] = tvals->green;
        info->trns[2] = tvals->blue;
      }
      info->colors |= SP_TRNS;
    }
  }

  if (info->nchan==1) {
    if (png_get_sBIT(p, pi, &sbit) && sbit->gray<info->depth)
      info->depth = sbit->gray;
    else
      sbit = 0;
  }
  if (!info->nchan) info->nchan = 1;

  {
    png_color_16p bg;
    if (png_get_bKGD(p, pi, &bg)) {
      if (info->nchan > 2) {
        info->bkgd[0] = bg->red;
        info->bkgd[1] = bg->green;
        info->bkgd[2] = bg->blue;
      } else if (info->palette) {
        info->bkgd[0] = bg->index;
        info->bkgd[1] = info->bkgd[2] = 0;
      } else {
        info->bkgd[0] = info->bkgd[1] = info->bkgd[2] = bg->gray;
      }
      info->colors |= SP_BKGD;
    }
  }

  {
    png_uint_32 xpx, ypx;
    int unit;
    if (png_get_pHYs(p, pi, &xpx, &ypx, &unit)) {
      info->n_xpix = xpx;
      info->n_ypix = ypx;
      info->per_meter = (unit==PNG_RESOLUTION_METER);
    }
  }

  {
    int sunit;
    if (png_get_sCAL(p, pi, &sunit, &info->xpix_sz, &info->ypix_sz)) {
      if (sunit == PNG_SCALE_METER) info->sunit = SP_METERS;
      else if (sunit == PNG_SCALE_RADIAN) info->sunit = SP_RADIANS;
      else info->sunit = SP_UNKNOWN;
    }
  }

  {
    png_charp key, unit;
    png_charpp params;
    png_int_32 x0, x1;
    int type, nparams;
    if (png_get_pCAL(p, pi, &key, &x0, &x1,
                     &type, &nparams, &unit, &params)) {
      long lenk, lenu;
      char *pend;
      if (nparams<2 || x0==x1) spng_error(p, "illegal pCAL values in png");
      if (type == PNG_EQUATION_LINEAR) info->eqtype = SP_LINEAR;
      else if (type == PNG_EQUATION_BASE_E) info->eqtype = SP_EXP;
      else if (type == PNG_EQUATION_ARBITRARY) info->eqtype = SP_POW;
      else if (type == PNG_EQUATION_HYPERBOLIC) info->eqtype = SP_SINH;
      else spng_error(p, "unknown pCAL equation type in png");
      info->x0 = x0;
      info->x1 = x1;
      info->mx = (1<<info->depth) - 1;
      info->p[0] = strtod(params[0], &pend);
      info->p[1] = strtod(params[1], &pend);
      info->p[2] = (nparams>2)? strtod(params[2], &pend) : 0.0;
      info->p[3] = (nparams>3)? strtod(params[3], &pend) : 0.0;
      lenk = key? strlen(key) : 0;
      lenu = unit? strlen(unit) : 0;
      if (memops->tmalloc) {
        if (lenk) info->purpose = memops->tmalloc(lenk+1);
        if (lenu) info->punit = memops->tmalloc(lenu+1);
      } else {
        if (lenk) info->purpose = malloc(lenk+1);
        if (lenu) info->punit = malloc(lenu+1);
      }
      if (key && info->purpose) strcpy(info->purpose, key);
      if (unit && info->punit) strcpy(info->punit, unit);
    }
  }

  {
    long i, rowbytes = info->nchan*info->width, nrows = info->height;
    unsigned char *image;
    if (info->depth < 8) {
      png_set_packing(p);  /* one pixel per byte */
    } else if (info->depth > 8) {
      short endian = 1;
      char *little_endian = (char *)&endian;
      if (little_endian[0]) png_set_swap(p);
      rowbytes += rowbytes;
    }
    if (sbit) png_set_shift(p, sbit);
    png_read_update_info(p, pi);
    if (png_get_rowbytes(p, pi) != rowbytes)
      spng_error(p, "unexpected number of bytes in image rows");
    if (!memops || !memops->imalloc) image = malloc(rowbytes*nrows);
    else image = memops->imalloc(info->depth>8? 16 : 8, info->nchan,
                                 info->width, info->height);
    if (!image) spng_error(p, "spng failed to malloc image");
    if (info->depth>8) info->simage = (unsigned short *)image;
    else info->cimage = image;
    if (!memops || !memops->smalloc) rows = malloc(sizeof(png_bytep)*nrows);
    else rows = memops->smalloc(sizeof(png_bytep)*nrows);
    if (!rows) spng_error(p, "spng failed to malloc rows");
    for (i=0 ; i<nrows ; i++, image+=rowbytes) rows[i] = (png_bytep)image;
    png_read_image(p, rows);
    if (!memops || !memops->sfree) free(rows);
    else memops->sfree(rows);
    rows = 0;
  }

  png_read_end(p, pi);

  {
    png_timep time;
    if (png_get_tIME(p, pi, &time)) {
      info->itime[0] = time->year;
      info->itime[1] = time->month;
      info->itime[2] = time->day;
      info->itime[3] = time->hour;
      info->itime[4] = time->minute;
      info->itime[5] = time->second;
    }
  }

  {
    png_textp ptext;
    int i, ntxt;
    long len;
    info->ntxt = png_get_text(p, pi, &ptext, &ntxt);
    if (ntxt > 0) {
      if (!memops->pmalloc) info->keytxt = malloc(sizeof(char*)*(ntxt+ntxt));
      else info->keytxt = memops->pmalloc(ntxt+ntxt);
      if (!info->keytxt) spng_error(p, "spng failed to malloc comments");
      for (i=0 ; i<ntxt ; i++)
        info->keytxt[i+i] = info->keytxt[i+i+1] = 0;
      for (i=0 ; i<ntxt ; i++) {
        len = ptext[i].key? ptext[i].text_length : 0;
        if (len > 0) {
          info->keytxt[i+i] =
            memops->tmalloc? memops->tmalloc(len+1) : malloc(len+1);
          info->keytxt[i+i][0] = '\0';
          if (info->keytxt[i+i])
            strncat(info->keytxt[i+i], ptext[i].key, len);
        }
        len = ptext[i].text? ptext[i].text_length : 0;
        if (len > 0) {
          info->keytxt[i+i+1] =
            memops->tmalloc? memops->tmalloc(len+1) : malloc(len+1);
          info->keytxt[i+i+1][0] = '\0';
          if (info->keytxt[i+i+1])
            strncat(info->keytxt[i+i+1], ptext[i].text, len);
        }
      }
    }
  }

  png_destroy_read_struct(&p, &pi, 0);
  fclose(f);
  return 0;
}

/*------------------------------------------------------------------------*/

int
sp_write(const char *filename, sp_memops *memops, sp_info *info)
{
  spng_id id;
  FILE *f = 0;
  png_structp p = 0;
  png_infop pi = 0;
  int ctype, nchan = info->nchan, depth = info->depth;
  int width = info->width, height = info->height;
  int npal = info->palette? info->npal : 0;
  png_voidp (*spx_malloc)(png_structp p, png_size_t nbytes) = 0;
  void (*spx_free)(png_structp p, png_voidp ptr) = 0;
  png_bytep *rows = 0;
  png_color_8 sbit;
  int do_sbit = 0;

  if (nchan==1) ctype = PNG_COLOR_TYPE_GRAY;
  else if (nchan==2) ctype = PNG_COLOR_TYPE_GRAY_ALPHA;
  else if (nchan==3) ctype = PNG_COLOR_TYPE_RGB;
  else if (nchan==4) ctype = PNG_COLOR_TYPE_RGB_ALPHA;
  else ctype = 0;
  if (nchan==1 && npal>0) {
    ctype = PNG_COLOR_TYPE_PALETTE;
    if (depth > 8) npal = -1;
  } else if (npal && nchan==2) {
    npal = -1;
  }
  /* cannot store images with alpha channel at depth 1, 2, or 4 */
  if ((nchan==2 || nchan>3) && depth<8) depth = 8;
  if ((depth>8)? (info->simage==0) : (info->cimage==0)) depth = 0;

  if (memops && memops->smalloc && memops->sfree) {
    spx_malloc = spng_malloc;
    spx_free = spng_free;
  }
  info->nerrs = info->nwarn = 0;
  info->msg[0] = '\0';

  id.id = &id;
  id.p = 0;
  id.pi = 0;
  id.memops = memops;
  id.info = info;

  f = fopen(filename, "wb");
  if (!f || nchan<1 || nchan>4 || width<1 || height<1 ||
      depth<1 || depth>16 || (nchan!=1 && ((depth-1)&depth)) ||
      npal<0 || npal>PNG_MAX_PALETTE_LENGTH) return 1;

  id.p = p = png_create_write_struct_2(PNG_LIBPNG_VER_STRING,
                                       &id, spng_error, spng_warning,
                                       &id, spx_malloc, spx_free);
  if (!p) { fclose(f); return 2; }

  if (setjmp(png_jmpbuf(p))) {
    if (rows) {
      if (!memops || !memops->sfree) free(rows);
      else memops->sfree(rows);
    }
    png_destroy_read_struct(&p, &pi, 0);
    fclose(f);
    return 3;
  }

  id.pi = pi = png_create_info_struct(p);
  if (!pi) spng_error(p, "png_create_info_struct failed");

  png_init_io(p, f);

  if ((depth-1) & depth) {
    do_sbit = !npal;
    sbit.gray = depth;
    if (sbit.gray > 8) depth = 16;
    else if (sbit.gray > 4) depth = 8;
    else depth = 4;
  }
  png_set_IHDR(p, pi, (png_uint_32)width, (png_uint_32)height, depth, ctype,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  if (npal > 0) {
    png_color ppal[PNG_MAX_PALETTE_LENGTH];
    int i;
    for (i=0 ; i<npal ; i++) {
      ppal[i].red =   info->palette[3*i  ];
      ppal[i].green = info->palette[3*i+1];
      ppal[i].blue =  info->palette[3*i+2];
    }
    png_set_PLTE(p, pi, ppal, npal);
    if (nchan == 1) {
      if (info->alpha) {
        png_byte alpha[PNG_MAX_PALETTE_LENGTH];
        for (i=0 ; i<npal ; i++) alpha[i] = info->alpha[i];
        png_set_tRNS(p, pi, alpha, npal, 0);
      }
      nchan = 0;
    }
  }

  /* trns is info->alpha for pseudocolor, illegal if aplha in pixel */
  if ((info->colors & SP_TRNS) && (nchan==1 || nchan==3)) {
    /* transparent color values have same range as image pixel values */
    png_color_16 tval;
    if (nchan == 3) {
      tval.red = info->trns[0];
      tval.green = info->trns[1];
      tval.blue = info->trns[2];
      tval.gray =
        ((long)info->trns[0]+(long)info->trns[1]+(long)info->trns[2])/3;
    } else {
      tval.gray = tval.red = tval.green = tval.blue = info->trns[0];
    }
    png_set_tRNS(p, pi, 0, 0, &tval);
  }
  if (!nchan) nchan = 1;

  if (do_sbit) png_set_sBIT(p, pi, &sbit);

  if (info->colors & SP_BKGD) {
    /* background color values have same range as image pixel values */
    png_color_16 bg;
    if (nchan > 2) {
      bg.red = info->bkgd[0];
      bg.green = info->bkgd[1];
      bg.blue = info->bkgd[2];
      bg.gray =
        ((long)info->bkgd[0]+(long)info->bkgd[1]+(long)info->bkgd[2])/3;
    } else if (npal > 0) {
      bg.index = info->bkgd[0];
      bg.gray = bg.red = bg.green = bg.blue = 0;
    } else {
      bg.gray = bg.red = bg.green = bg.blue = info->bkgd[0];
      bg.index = 0;
    }
    png_set_bKGD(p, pi, &bg);
  }

  if (info->n_xpix>0 && info->n_ypix>0) {
    png_set_pHYs(p, pi, (png_uint_32)info->n_xpix, (png_uint_32)info->n_ypix,
                 (info->per_meter!=0)?
                   PNG_RESOLUTION_METER:PNG_RESOLUTION_UNKNOWN);
  }

  if (info->xpix_sz>0.0 && info->ypix_sz>0.0) {
    int unit = PNG_SCALE_UNKNOWN;
    if (info->sunit == SP_METERS) unit = PNG_SCALE_METER;
    else if (info->sunit == SP_RADIANS) unit = PNG_SCALE_RADIAN;
    png_set_sCAL(p, pi, unit, info->xpix_sz, info->ypix_sz);
  }

  if ((info->x0 != info->x1) && info->p[1] &&
      (info->eqtype==SP_LINEAR || info->eqtype==SP_EXP ||
       info->eqtype==SP_POW || info->eqtype==SP_SINH) &&
      (info->eqtype==SP_LINEAR || info->p[2])) {
    char p0[20], p1[20], p2[20], p3[20];
    int type = PNG_EQUATION_LINEAR, np = 2;
    char *params[4];
    params[0]=p0; params[1]=p1; params[2]=p2; params[3]=p3;
    if (info->eqtype == SP_EXP) type = PNG_EQUATION_BASE_E, np = 3;
    else if (info->eqtype == SP_POW) type = PNG_EQUATION_ARBITRARY, np = 3;
    else if (info->eqtype == SP_SINH) type = PNG_EQUATION_HYPERBOLIC, np = 4;
    sprintf(params[0], "%.10e", info->p[0]);
    sprintf(params[1], "%.10e", info->p[1]);
    if (np>2) sprintf(params[2], "%.10e", info->p[2]);
    if (np>3) sprintf(params[3], "%.10e", info->p[3]);
    png_set_pCAL(p, pi, info->purpose, (png_int_32)info->x0,
                 (png_int_32)info->x1, type, np, info->punit,
                 (png_charpp)params);
  }

  if (info->itime[1]) {
    static int mxday[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year = info->itime[0];
    int month = info->itime[1];
    int day = info->itime[2];
    int hour = info->itime[3];
    int minute = info->itime[4];
    int second = info->itime[5];
    if (year>=0 && year<=9999 && month>=1 && month<=12 &&
        day>=1 && day<=mxday[month-1] &&
        hour>=0 && hour<=23 && minute>=0 && minute<=59 &&
        second>=0 && second<=61 && (day!=29 || month!=2 || !(year%4))) {
      png_time tpng;
      if (year<60) year += 2000;
      else if (year<100) year += 1900;
      tpng.year = year;
      tpng.month = month;
      tpng.day = day;
      tpng.hour = hour;
      tpng.minute = minute;
      tpng.second = second;
      png_set_tIME(p, pi, &tpng);
    }
  }

  if (info->ntxt && info->keytxt) {
    png_size_t len;
    png_text ptext;
    char k[PNG_KEYWORD_MAX_LENGTH+1];
    int i;
    for (i=0 ; i<info->ntxt ; i++) {
      /* silently enforce strlen(key)<=PNG_KEYWORD_MAX_LENGTH */
      k[0] = '\0';
      if (info->keytxt[i+i])
        strncat(k, info->keytxt[i+i], PNG_KEYWORD_MAX_LENGTH);
      len = info->keytxt[i+i+1]? strlen(info->keytxt[i+i+1]) : 0;
      ptext.compression =
        (len>1024)? PNG_TEXT_COMPRESSION_zTXt : PNG_TEXT_COMPRESSION_NONE;
      ptext.key = k;
      ptext.text = info->keytxt[i+i+1];
      ptext.text_length = len;
#ifdef PNG_iTXt_SUPPORTED
      ptext.itxt_length = 0;
      ptext.lang = 0;
      ptext.lang_key = 0;
#endif
      png_set_text(p, pi, &ptext, 1);
    }
  }

  png_write_info(p, pi);

  {
    long i, rowbytes = nchan*width, nrows = height;
    unsigned char *image = info->cimage;
    if (depth < 8) png_set_packing(p);  /* one pixel per byte */
    if (depth > 8) {
      short endian = 1;
      char *little_endian = (char *)&endian;
      if (little_endian[0]) png_set_swap(p);
      rowbytes += rowbytes;
      image = (unsigned char *)info->simage;
    }
    if (do_sbit) png_set_shift(p, &sbit);
    if (!memops || !memops->smalloc) rows = malloc(sizeof(png_bytep)*nrows);
    else rows = memops->smalloc(sizeof(png_bytep)*nrows);
    if (!rows) spng_error(p, "spng failed to malloc rows");
    for (i=0 ; i<nrows ; i++, image+=rowbytes) rows[i] = (png_bytep)image;
    png_write_image(p, rows);
    if (!memops || !memops->sfree) free(rows);
    else memops->sfree(rows);
    rows = 0;
  }

  png_write_end(p, pi);
  png_destroy_write_struct(&p, &pi);
  fclose(f);
  return 0;
}

/*------------------------------------------------------------------------*/

static png_voidp
spng_malloc(png_structp p, png_size_t nbytes)
{
  spng_id *id = png_get_mem_ptr(p);
  if (id && id->id==id && id->memops->smalloc) {
    return id->memops->smalloc((unsigned long)nbytes);
  } else {
    return png_malloc_default(p, nbytes);
  }
}

static void
spng_free(png_structp p, png_voidp ptr)
{
  spng_id *id = png_get_mem_ptr(p);
  if (id && id->id==id && id->memops->sfree) {
    id->memops->sfree((void *)ptr);
  } else {
    png_free_default(p, ptr);
  }
}

static void
spng_error(png_structp p, png_const_charp msg)
{
  spng_id *id = png_get_error_ptr(p);
  if (id && id->id==id && id->info) {
    id->info->nerrs++;
    if (!id->info->msg[0]) strncat(id->info->msg, msg, 95);
  }
  longjmp(png_jmpbuf(p), 1);
}

static void
spng_warning(png_structp p, png_const_charp msg)
{
  spng_id *id = png_get_error_ptr(p);
  if (id && id->id==id && id->info) {
    if (!id->info->nerrs && !id->info->msg[0])
      strncat(id->info->msg, msg, 95);
    id->info->nwarn++;
  }
}

/*------------------------------------------------------------------------*/

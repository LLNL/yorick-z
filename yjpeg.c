/*
 * $Id: yjpeg.c,v 1.1 2005-09-18 22:07:09 dhmunro Exp $
 * jpeg interface for yorick
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#include "ydata.h"
#include "yio.h"
#include "pstdlib.h"

#include <string.h>
#include <stdio.h>
#include "jpeglib.h"

#define YJ_DEFAULT_QUALITY 75

extern BuiltIn Y_jpeg_read, Y_jpeg_write;

typedef struct yj_error_mgr yj_error_mgr;
struct yj_error_mgr {
  struct jpeg_error_mgr base;
  FILE *file;
};

static void yj_output_message(j_common_ptr jpeg);
static void yj_error_exit(j_common_ptr jpeg);

void
Y_jpeg_read(int nArgs)
{
  long icom = (nArgs>=2)? YGet_Ref(sp-nArgs+2) : -1;
  Dimension *dims = 0;
  long *lims = (nArgs>=3)? YGet_L(sp-nArgs+3, 1, &dims) : 0;
  char *filename = (nArgs>=1)? p_native(YGetString(sp-nArgs+1)) : 0;
  FILE *file = (filename && filename[0])? fopen(filename, "rb") : 0;
  struct jpeg_decompress_struct jpeg;
  struct yj_error_mgr jerr;
  JSAMPROW image, *row_pointer;
  int i, j, row_stride;

  p_free(filename);
  if (nArgs<1 || nArgs>3) YError("jpeg_read takes 1, 2, or 3 arguments");
  if (lims && TotalNumber(dims)!=4)
    YError("jpeg_read third argument must be [xmin,xmax,ymin,ymax]");
  if (!file) YError("jpeg_read cannot open specified file");

  jpeg.err = jpeg_std_error(&jerr.base);
  jerr.base.error_exit = yj_error_exit;
  jerr.base.output_message = yj_output_message;
  jerr.file = file;

  jpeg_create_decompress(&jpeg);
  jpeg_stdio_src(&jpeg, file);

  if (icom >= 0) jpeg_save_markers(&jpeg, JPEG_COM, 0xffff);
  jpeg_read_header(&jpeg, TRUE);
  if (icom >= 0) {
    jpeg_saved_marker_ptr mk = jpeg.marker_list;
    long ncom = 0;
    for (mk=jpeg.marker_list ; mk ; mk=mk->next)
      if (mk->marker==JPEG_COM && mk->data_length>0) ncom++;
    if (ncom) {
      Array *a = (Array *)PushDataBlock(NewArray(&stringStruct,
                                                 ynew_dim(ncom, 0)));
      char **com = a->value.q;
      long i = 0;
      for (mk=jpeg.marker_list ; mk ; mk=mk->next) {
        if (mk->marker==JPEG_COM && mk->data_length>0)
          com[i++] = p_strncat(0, (char *)mk->data, mk->data_length & 0xffff);
      }
    } else {
      PushDataBlock(RefNC(&nilDB));
    }
    YPut_Result(sp, icom);  /* was sp-nArgs+2 before push */
    Drop(1);
  }
  jpeg_calc_output_dimensions(&jpeg);

  if (lims &&
      (lims[0]<1 || lims[2]<1 || lims[0]>lims[1] || lims[2]>lims[3] ||
       lims[1]>jpeg.output_width || lims[3]>jpeg.output_height)) {
    /* just return dimensions */
    Dimension *d = ynew_dim(3L, 0);
    Array *a = (Array *)PushDataBlock(NewArray(&longStruct, d));
    a->value.l[0] = jpeg.output_components;
    a->value.l[1] = jpeg.output_width;
    a->value.l[2] = jpeg.output_height;

  } else {
    int nchan = jpeg.output_components;
    Dimension *d;
    Array *a;
    long x0 = lims? lims[0] : 1;
    long x1 = lims? lims[1] : jpeg.output_width;
    long y0 = lims? lims[2] : 1;
    long y1 = lims? lims[3] : jpeg.output_height;

    row_stride = jpeg.output_width * jpeg.output_components;
    row_pointer = jpeg.mem->alloc_sarray((j_common_ptr)&jpeg, JPOOL_IMAGE,
                                         row_stride, 1);

    jpeg_start_decompress(&jpeg);

    d = ynew_dim(y1-y0+1, NewDimension(x1-x0+1, 1L, (nchan==1)? 0 :
                                       NewDimension((long)nchan, 1L, 0)));
#if BITS_IN_JSAMPLE == 8
    a = (Array *)PushDataBlock(NewArray(&charStruct, d));
    image = (JSAMPLE *)a->value.c;
#elif BITS_IN_JSAMPLE == 12
    a = (Array *)PushDataBlock(NewArray(&shortStruct, d));
    image = (JSAMPLE *)a->value.s;
#else
#error unsupported BITS_IN_JSAMPLE
#endif

    x0 = (x0-1)*jpeg.output_components;
    x1 *= jpeg.output_components;
    for (i=0 ; jpeg.output_scanline<y1 ; i+=x1-x0) {
      jpeg_read_scanlines(&jpeg, row_pointer, 1);
      if (jpeg.output_scanline < y0) continue;
      for (j=x0 ; j<x1 ; j++) image[i+j-x0] = row_pointer[0][j];
    }

    jpeg_finish_decompress(&jpeg);
  }

  jpeg_destroy_decompress(&jpeg);
  fclose(file);
}

void
Y_jpeg_write(int nArgs)
{
  Dimension *dims = 0;
  long idims[3];
  char **com = (nArgs>=3)? YGet_Q(sp-nArgs+3, 1, &dims) : 0;
  long ncom = com? TotalNumber(dims) : 0;
  int quality = (nArgs==4)? YGetInteger(sp-nArgs+4) : -1;
#if BITS_IN_JSAMPLE == 8
  JSAMPROW image = (JSAMPROW)((nArgs>=2)? YGet_C(sp-nArgs+2, 0, &dims) : 0);
#elif BITS_IN_JSAMPLE == 12
  JSAMPROW image = (JSAMPROW)((nArgs>=2)? YGet_S(sp-nArgs+2, 0, &dims) : 0);
#else
#error unsupported BITS_IN_JSAMPLE
#endif
  int ndims = YGet_dims(dims, idims, 3);
  JSAMPROW row_pointer[1];
  char *filename = (nArgs>=2)? p_native(YGetString(sp-nArgs+1)) : 0;
  FILE *file = (filename && filename[0])? fopen(filename, "wb") : 0;
  struct jpeg_compress_struct jpeg;
  struct yj_error_mgr jerr;
  int i, row_stride;

  p_free(filename);

  if (nArgs<2 || nArgs>4) YError("jpeg_write takes 2, 3, or 4 arguments");
  if (!file) YError("jpeg_write cannot open specified file");
  if (ndims==2) idims[2]=idims[1], idims[1]=idims[0], idims[0]=1;
  if (ndims<2 || ndims>3 || (idims[0]!=1 && idims[0]!=3))
    YError("jpeg_write needs 2D gray or rgb image");

  jpeg.err = jpeg_std_error(&jerr.base);
  jerr.base.error_exit = yj_error_exit;
  jerr.base.output_message = yj_output_message;
  jerr.file = file;

  jpeg_create_compress(&jpeg);
  jpeg_stdio_dest(&jpeg, file);

  jpeg.image_width = idims[1];
  jpeg.image_height = idims[2];
  jpeg.input_components = idims[0];
  jpeg.in_color_space = (idims[0]==3)? JCS_RGB : JCS_GRAYSCALE;
  jpeg_set_defaults(&jpeg);

  if (quality <= 0) quality = YJ_DEFAULT_QUALITY;
  else if (quality > 100) quality = 100;
  jpeg_set_quality(&jpeg, quality, TRUE);

  jpeg_start_compress(&jpeg, TRUE);
  for (i=0 ; i<ncom ; i++) if (com[i])
    jpeg_write_marker(&jpeg, JPEG_COM, (JOCTET *)com[i], strlen(com[i])+1);

  row_stride = idims[0]*idims[1];
  for (i=0 ; jpeg.next_scanline<jpeg.image_height ;
       i+=row_stride) {
    row_pointer[0] = &image[i];
    jpeg_write_scanlines(&jpeg, row_pointer, 1);
  }

  jpeg_finish_compress(&jpeg);
  fclose(file);
  jpeg_destroy_compress(&jpeg);
}

static void
yj_error_exit(j_common_ptr jpeg)
{
  yj_error_mgr *yjpeg = (yj_error_mgr *)jpeg;
  char msg[16+JMSG_LENGTH_MAX];
  if (jpeg->is_decompressor) {
    strcpy(msg, "jpeg_read: ");
    jpeg->err->format_message(jpeg, msg+11);
    jpeg_destroy_decompress((j_decompress_ptr)jpeg);
  } else {
    strcpy(msg, "jpeg_write: ");
    jpeg->err->format_message(jpeg, msg+12);
    jpeg_destroy_compress((j_compress_ptr)jpeg);
  }
  if (yjpeg->file) fclose(yjpeg->file);
  yjpeg->file = 0;
  YError(msg);
}

static void
yj_output_message(j_common_ptr jpeg)
{
  /* silently ignore warning messages -- it's not our job to track
   * down irregularities in jpeg files
   */
}

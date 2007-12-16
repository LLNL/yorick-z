/*
 * $Id: jpeg.i,v 1.3 2007-12-16 00:04:54 dhmunro Exp $
 * yorick interface to jpeg image compression
 * ftp://ftp.uu.net/graphics/jpeg/      http://www.faqs.org/faqs/jpeg-faq/
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

if (!is_void(plug_in)) plug_in, "yorz";

func jpeg2(name)
/* DOCUMENT jpeg2, name
     writes the picture in the current graphics window to the jpeg file
     NAME, or to NAME+".jpg" is NAME does not end in ".jpg".
   SEE ALSO: jpeg, png, pdf, eps, hcps
 */
{
  if (strpart(name,-3:0)!=".jpg") name+= ".jpg";
  jpeg_write, name, rgb_read();
  return name;
}

extern jpeg_read;
/* DOCUMENT image = jpeg_read(filename)
 *       or image = jpeg_read(filename, comments)
 *       or shape = jpeg_read(filename, comments, [0,0,0,0])
 *       or image = jpeg_read(filename, comments, subset)
 *
 * Read jpeg file FILENAME.  The returned IMAGE is 3-by-width-by-height
 * for rgb images (the usual case) or just width-by-height for grayscale
 * images.  Note that the scanline order is top-to-bottom.
 * If COMMENTS is present, it must be a simple variable reference.
 * That variable will be set to either nil or a string array containing
 * all the descriptive comments in the file.
 *
 * In the third form, the return value is [nchan,width,height] instead
 * of the image, where nchan=1 or nchan=3.
 * In the fourth form, SUBSET is [i0,i1,j0,j1] and the returned image is
 * the subset full_image(..,i0:i1,j0:j1) of the full image.  (This is
 * inefficient, but, for example, some Mars Rover pictures released by
 * NASA are inconveniently large.)
 *
 * SEE ALSO: jpeg_write
 */

extern jpeg_write;
/* DOCUMENT jpeg_write, filename, image
 *       or jpeg_write, filename, image, comments, quality
 *
 * Write jpeg file FILENAME containing IMAGE at the specified QUALITY.
 * The default QUALITY is 75; the range is from 0 to 100.  The IMAGE
 * can be either 3-by-width-by-height for rgb or width-by-height for
 * grayscale.  Note that scanline order is top-to-bottom.
 * If COMMENTS is non-nil, it is a string or an array of strings that
 * will be written as descriptive comments in the jpeg file.
 *
 * SEE ALSO: jpeg_read
 */

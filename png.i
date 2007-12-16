/*
 * $Id: png.i,v 1.3 2007-12-16 00:04:54 dhmunro Exp $
 * yorick interface to libpng image compression (www.libpng.org)
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

if (!is_void(plug_in)) plug_in, "yorz";

func png2(name)
/* DOCUMENT png2, name
     writes the picture in the current graphics window to the png file
     NAME, or to NAME+".png" is NAME does not end in ".png".
   SEE ALSO: png, jpeg, pdf, eps, hcps
 */
{
  if (strpart(name,-3:0)!=".png") name+= ".png";
  png_write, name, rgb_read();
  return name;
}

func png_read(filename, &depth, &nfo, type=, quiet=)
/* DOCUMENT image = png_read(filename)
 *       or image = png_read(filename, depth, nfo)
 *
 * Read png file FILENAME.  The returned IMAGE is either an array
 * of char or short, unless type= is specified (see below).
 * The IMAGE may have a leading dimension of 2 if it is gray+alpha,
 * 3 if it is rgb, or 4 if it is rgba.
 * In the second form, DEPTH and NFO must be simple variable references.
 * NFO is set to a pointer array to descriptive information by png_read:
 * *nfo(1) = PLTE 3-by-N char array of palette rgb values
 * *nfo(2) = tRNS char array of alpha (opacity) values corresponding
 *                 to PLTE or single gray or rgb short value (transparent)
 * *nfo(3) = bKGD single gray or rgb short value
 *              note that bKGD and the single value tRNS have the same
 *              range and meaning as a pixel value, in particular,
 *              for a pseudocolor image, they represent a palette index
 * *nfo(4) = pCAL [x0,x1,max,eqtype,p0,p1,p2,p3,...] physical pixel value
 *                 relation between pixel value and physical value
 *                 array of double values (see below for meaning)
 * *nfo(5) = pCAL [calibration_name, unit_name] string pair
 * *nfo(6) = sCAL [wide,high,sunit] physical scale of pixels as scanned
 *                 or printed, sunit 1.0 for meters or 2.0 for radians
 * *nfo(7) = pHYs long [n_xpix,n_ypix,per_meter] values
 *                 n_xpix,n_ypix are pixels per unit,
 *                 per_meter is 0 for aspect ratio only, 1 for meters
 * *nfo(8) = tEXt (or zTXt or iTXt) 2-by-N string array of (key,text)
 * *nfo(9) = tIME string value image modification time
 * any or all of these NFO values may be nil.  The four character
 * designators (e.g. PLTE) are the png chunk names for the corresponding
 * information.
 *
 * pCAL array of doubles has following meaning:
 *    max = 2^depth-1
 *    original = long( floor( (image(i)*(x1-x0)+long(max)/2) / max ) ) + x0
 *    image(i) = long( floor( ((original-x0)*max+long(x1-x0)/2) / (x1-x0) ) )
 *    eqtype = 0   physical = p0 + p1*original/(x1-x0)
 *    eqtype = 1   physical = p0 + p1*exp(p2*original/(x1-x0))
 *    eqtype = 2   physical = p0 + p1*p2^(original/(x1-x0))
 *    eqtype = 3   physical = p0 + p1*sinh(p2*(original-p3)/(x1-x0))
 *
 * If the type= keyword is non-nil and non-zero, the returned value
 * is as if png_scale(image, nfo, type=type), which scales the raw image
 * according to the information in pCAL, or is a no-op if pCAL does
 * not exist.
 *
 * SEE ALSO: png_write, png_scale
 */
{
  dnwh = array(long, 8);
  nfo = array(pointer, 9);
  image = &[];
  emsg = string(0);
  rslt = _png_read(filename, dnwh, nfo, image, emsg);
  if (rslt) {
    if (rslt == 1) error, "unable to open "+filename;
    else if (rslt == 2) error, "PNG ERROR: png_create_read_struct_2 failed";
    error, "PNG ERROR: "+emsg;
  }
  if (dnwh(8) && !quiet) {
    write, format="PNG %ld warnings: %s\n", dnwh(8), emsg;
  }
  depth = dnwh(1);
  if (is_void(type)) image = *image;
  else image = png_scale(*image, nfo, type=type);
  return image;
}

func png_write(filename, image, depth, nfo, palette=, alpha=, bkgd=,
               pcal=, pcals=, scal=, phys=, text=, time=, trns=, quiet=)
/* DOCUMENT png_write, filename, image
 *       or png_write, filename, image, depth, nfo
 *
 * Write png file FILENAME containing IMAGE at the specified DEPTH.
 * The default DEPTH is 8 bits.  For grayscale IMAGE, 1<=DEPTH<=16,
 * otherwise depth is 8 or 16.  If NFO is specified, it is an
 * array of pointers as described in the help for png_read.  You can
 * optionally specify the same information as keywords:
 *   palette=[[r0,g0,b0],[r1,g1,b1],...]
 *   alpha=[a0,a1,...] if image is simple 2D and palette specified
 *   trns=value       if image is gray (no palette)
 *        [r,g,b]     if image is color
 *        illegal if image has alpha channel
 *   bkgd=value or [r,g,b] suggested background color
 *     note that bkgd and trns have the same range and meaning as a
 *     pixel value, in particular, for a pseudocolor, a palette index
 *   pcal=[x0,x1,max,eqtype,p0,p1,p2,p3,...]
 *   pcals=[calibration_name, unit_name]  as for pCAL (see png_read)
 *   scal=[wide,high,sunit]  as for sCAL (see png_read)
 *   phys=[n_xpix,n_ypix,per_meter]  as for pHYs (see png_read)
 *   text=[[key1,text1],[key1,text1],...]
 *     recognized keys are: Title, Author, Description, Copyright,
 *     Creation Time, Software, Disclaimer, Warning, Source (a device),
 *     and Comment
 *   time=string  modification time (timestamp() is default)
 * When both NFO and keywords are supplied, the keywords override any
 * corresponding value in nfo.
 *
 * If IMAGE has a data type other than short or char, a default pCAL
 * will be supplied if it is a simple grayscale (2D) image.  If DEPTH
 * is not supplied, it defaults to 8 if IMAGE is type char and/or if
 * a palette is supplied, or to 16 otherwise.
 *
 * SEE ALSO: png_read, png_map
 */
{
  /* eventually, do conversion and even select pcal automatically */
  if (structof(image(1)+0) != long)
    error, "image must be converted to integer data type";

  dnwh = array(long, 8);
  dims = dimsof(image);
  nchan = (dims(1)==2);
  if (dims(1)==3) nchan = dims(2);
  if (nchan<1 || nchan>4)
    error, "image must have 1-4 channels (gray, gray+A, RGB, or RGBA)";
  dnwh(2) = nchan;
  dnwh(3:4) = dims(-1:0);

  if (structof(nfo)==pointer && numberof(nfo)==9) {
    if (is_void(palette)) eq_nocopy, palette, *nfo(1);
    if (nfo(2)) {
      if (nchan==1 && palette) {
        if (is_void(alpha)) eq_nocopy, alpha, *nfo(2);
      } else {
        if (is_void(trns)) eq_nocopy, trns, *nfo(2);
      }
    }
    if (is_void(bkgd)) eq_nocopy, bkgd, *nfo(3);
    if (is_void(pcal)) eq_nocopy, pcal, *nfo(4);
    if (is_void(pcals)) eq_nocopy, pcals, *nfo(5);
    if (is_void(scal)) eq_nocopy, scal, *nfo(6);
    if (is_void(phys)) eq_nocopy, phys, *nfo(7);
    if (is_void(text)) eq_nocopy, text, *nfo(8);
    if (is_void(time)) eq_nocopy, time, *nfo(9);
  } else if (!is_void(nfo)) {
    error, "illegal nfo format";
  }
  nfo = array(pointer, 9);

  pseudo = (nchan==1 && !is_void(palette));

  dep = 0;
  if (!is_void(pcal)) {
    mx = long(pcal(3));
    if (mx>65535 || mx<1) error, "pcal(3) mx parameter too big or too small";
    else if (mx > 255) dep = 16;
    else if (mx > 15)  dep = 8;
    else if (mx > 3)   dep = 4;
    else if (mx > 1)   dep = 2;
    else               dep = 1;
    x0 = long(pcal(1));
    x1 = long(pcal(2));
    eqtype = long(pcal(4));
    p = double(pcal(5:8));
  }

  istruct = structof(image);
  if (!depth) {
    if (dep) depth = (istruct==char)? min(dep,8) : dep;
    else if (istruct==char || pseudo) depth = 8;
    else depth = 16;
  }
  dnwh(1) = depth;

  /* eventually, this should pay attention to pcal */
  if (depth > 8) {
    if (istruct != short) image = short(image);
  } else {
    if (istruct != char) image = char(image);
  }

  npal = 0;
  if (!is_void(palette)) {
    if (nchan==2) error, "palette illegal for gray+alpha (2 channel) image";
    dims = dimsof(palette);
    if (!numberof(dims) || anyof(dims(1:2)!=[2,3]) || dims(3)>256 ||
        max(palette)>255 || min(palette)<0 ||
        structof(palette+0)!=long)
      error, "illegal palette format or length";
    if (structof(palette)!=char) palette = char(palette);
    npal = dims(3);
    nfo(1) = &palette;
  }
  if (!is_void(alpha)) {
    if (nchan!=1 || !npal)
      error, "alpha illegal for non-pseudocolor image";
    dims = dimsof(alpha);
    if (!numberof(dims) || dims(1)!=1 || max(alpha)>255 || min(alpha)<0 ||
        structof(alpha+0)!=long)
      error, "illegal alpha format or length";
    if (structof(alpha)!=char) alpha = char(alpha);
    dnwh(7) = 1;
    nfo(2) = &alpha;
  }
  if (!is_void(trns)) {
    if (nchan==2 || nchan==4)
      error, "trns illegal when image pixels have alpha channel";
    else if (nchan==1 && npal)
      error, "trns illegal for pseudocolor image, use alpha=";
    else if (nchan==1 && numberof(trns)==1)
      trns = trns(1)(1,-:1:3);
    dims = dimsof(trns);
    if (!numberof(dims) || dims(1)!=1 || dims(2)!=3 || min(trns)<0 ||
        structof(trns+0)!=long)
      error, "illegal trns format or length";
    trns = long(trns);
    nfo(2) = &trns;
  }
  if (!is_void(bkgd)) {
    if (nchan < 3) bkgd = bkgd(1)(1,-:1:3);
    dims = dimsof(bkgd);
    if (!numberof(dims) || dims(1)!=1 || dims(2)!=3 || min(bkgd)<0 ||
        structof(bkgd+0)!=long)
      error, "illegal bkgd format or length";
    bkgd = long(bkgd);
    nfo(3) = &bkgd;
  }
  if (!is_void(pcal)) {
    pcal = double(pcal);
    dims = dimsof(pcal);
    if (!numberof(dims) || dims(1)!=1 || dims(2)!=8 ||
        anyof(pcal(1:4)!=long(pcal(1:4))))
      error, "illegal pcal format or length";
    if (pcal(4)<1 || pcal(4)>4) error, "unknown eqtype in pcal";
    nfo(4) = &pcal;
  }
  if (!is_void(pcals)) {
    dims = dimsof(pcals);
    if (!numberof(dims) || dims(1)!=1 || dims(2)!=2 ||
        structof(pcals)!=string)
      error, "illegal pcals format or length";
    nfo(5) = &pcals;
  }
  if (!is_void(scal)) {
    scal = double(scal);
    dims = dimsof(scal);
    if (!numberof(dims) || dims(1)!=1 || dims(2)!=3 ||
        scal(3)!=long(scal(3)))
      error, "illegal scal format or length";
    nfo(6) = &scal;
  }
  if (!is_void(phys)) {
    phys = phys+0;
    dims = dimsof(phys);
    if (!numberof(dims) || dims(1)!=1 || dims(2)!=3 ||
        structof(phys)!=long || anyof(phys(1:2)<=0))
      error, "illegal phys format or length";
    nfo(7) = &phys;
  }
  ntxt = 0;
  if (!is_void(text)) {
    dims = dimsof(text);
    if (!numberof(dims) || dims(1)!=2 || dims(2)!=2 ||
        structof(text)!=string)
      error, "illegal text or nfo(8) format or length";
    ntxt = dims(3);
    nfo(8) = &text;
  }
  if (!is_void(time)) {
    time = time+0;
    dims = dimsof(time);
    if (!numberof(dims) || dims(1)!=1 || dims(2)!=6 ||
        structof(time)!=long || anyof(time<0))
      error, "illegal time format or length";
    nfo(9) = &time;
  }

  dnwh(5) = npal;
  dnwh(6) = ntxt;

  emsg = string(0);
  rslt = _png_write(filename, dnwh, nfo, &image, emsg);
  if (rslt) {
    if (rslt == 1) error, "bad inputs or unable to create "+filename;
    else if (rslt == 2) error, "PNG ERROR: png_create_write_struct_2 failed";
    error, "PNG ERROR: "+emsg;
  }
  if (dnwh(8) && !quiet) {
    write, format="PNG %ld warnings: %s\n", dnwh(8), emsg;
  }
}

func png_scale(image, nfo, type=)
/* DOCUMENT image = png_scale(raw_image, nfo, type=type)
 *   scales RAW_IMAGE to type TYPE (char, short, int, long, float, or
 *   double, according to the pCAL information in NFO.  The NFO
 *   parameter may be either the array of pointers returned by
 *   png_read, or an array of reals as for *nfo(4) (see png_read).
 *
 * SEE ALSO: png_map, png_read, png_write
 */
{
  if (structof(nfo) == pointer) nfo = *nfo(4);
  if (is_void(nfo)) return image;
  dx = nfo(2) - nfo(1);
  x0 = long(nfo(1));
  x1 = long(nfo(2));
  mx = long(nfo(3));
  eq = long(nfo(4));
  p = double(nfo(5:0));

  if (eq<0 || eq>3) error, "unknown equation type";

  sample = image(1);
  image = long(image);
  if (sizeof(sample) == 1) image &= ~0xff;
  else if (sizeof(sample) == 2) image &= ~0xffff;
  else if (anyof((image>mx) | (image<0))) error, "image out of scaling range";

  if (mx<1 || mx>65535 || ((mx+1)&mx))
    error, "max=2^depth-1 has impossible value";

  if (x1 > x0) {
    x1 -= x0;
  } else if (x1 < x0) {
    /* adjust so that x1 > 0 in all cases */
    x1 = x0 - x1;
    x0 -= x1;  /* i.e.- original x1 */
    image = mx - image;
  } else {
    error, "x0 == x1 is impossible scaling";
  }

  /* here is formula from PNG Extensions document (x1>0):
   * d = image*(x1%mx);
   * image = image*(x1/mx) + d/mx + (d%mx + mx/2)/mx + x0;
   */
  q = x1 / mx;
  r = x1 & mx;  /* mx = 2^n - 1 */
  if (r) {
    r *= image;
    r = r/mx + ((r&mx)+(mx>>1))/mx;
  }
  if (q) image *= q;
  image += r + x0;

  if (is_void(type)) {
    if (!eq && !p(1) && p(2)==dx) {
      if (x0>=0 && x1<256) type = char;
      else if (x0>-32768 && x1+x0<32768) type = short;
      else type = long;
    } else {
      type = double;
    }
  }
  sample = type(0);
  if (structof(sample+0) != long) {
    dx = 1./dx;
    if (!eq) image *= dx;
    else if (eq == 1) image = exp(p(3)*dx * image);
    else if (eq == 2) image = p(3) ^ (dx * image);
    else image = sinh(p(3)*dx * (image-p(4)));
    image = p(1) + p(2)*image;
  }

  return type(image);
}

func png_map(image, nfo)
/* DOCUMENT image = png_map(full_image, nfo)
 *   maps FULL_IMAGE to png-stored values, according to the
 *   pCAL information in NFO.
 *   The NFO parameter may be either the array of pointers as returned by
 *   png_read, or an array of reals as for *nfo(4) (see png_read).
 *   You can use png_pcal to compute an NFO mapping tailored to IMAGE.
 *
 * SEE ALSO: png_pcal, png_scale, png_read, png_write
 */
{
  if (!is_void(cmax)) image = min(image,cmax);
  if (!is_void(cmin)) image = max(image,cmin);

  if (structof(nfo) == pointer) nfo = *nfo(4);

  dx = nfo(2) - nfo(1);
  x0 = long(nfo(1));
  x1 = long(nfo(2));
  mx = long(nfo(3));
  eq = long(nfo(4));
  p = double(nfo(5:0));

  if (eq<0 || eq>3) error, "unknown equation type";

  if (x0 == x1) error, "x0 == x1 is impossible scaling";

  if (mx<1 || mx>65535 || ((mx+1)&mx))
    error, "max=2^depth-1 has impossible value";

  if (eq || p(1) || p(2)!=dx) {
    /* pre-clip image to map inside interval [x0,x1] */
    x = nfo(1:2);
    if (!eq) x /= dx;
    else if (eq == 1) x = exp(p(3)/dx * x);
    else if (eq == 2) x = p(3) ^ (x/dx);
    else image = sinh(p(3)/dx * (x-p(4)));
    x = p(1) + p(2)*x;
    image = min(max(x), max(min(x),image));

    /* then convert image to integer values in interval [x0,x1] */
    image = (image - p(1)) * (1./p(2));
    if (!eq) image *= dx;
    else if (eq == 1) image = (dx/p(3)) * _png_log(image);
    else if (eq == 2) image = (dx/_png_log(p(3))) * _png_log(image);
    else image = (dx/p(3)) * asinh(image) + p(4);
    image = long(image);
  }

  if (x1 > x0) {
    x1 -= x0;
  } else {
    /* adjust so that x1 > 0 in all cases */
    x1 = x0 - x1;
    x0 -= x1;  /* i.e.- original x1 */
  }
  image = min(x1, max(0,image-x0));
  if (x1 <= 0xffff) image = (image*mx + (x1>>1)) / x1;
  else image = long((double(image)*mx + (x1>>1)) / x1);

  return (mx <= 255)? char(image) : short(image);
}

func png_pcal(image, depth, cmin=, cmax=, res=, log=)
/* DOCUMENT pcal = png_pcal(image)
 *       or pcal = png_pcal(image, depth)
 *
 * KEYWORDS: cmin=, cmax=, res=, log=
 *   cmin, cmax   clip image to these minimum and maximum values
 *   res          image "resolution", or minimum step size
 *   log          non-zero forces log map if image all positive
 *                or all negative
 *
 *   returns 8-element pCAL png mapping for IMAGE, appropriate for
 *   use as pcal= keyword in png_write.  The png_map function applies
 *   pcal to produce the as-stored char or short array; the png_scale
 *   function applies pcal to recreate the original IMAGE from the
 *   as-stored array.
 *
 *   There are three kinds of pCAL mappings: linear, log, and asinh.
 *   Linear and log scales are familiar; the asinh scale is a modified
 *   log scale that can be used for arrays that change sign:
 *
 *   linear:  image = a*stored + b
 *   log:     image = b * exp(a*stored)
 *   asinh:   image = b * sinh(a*(stored - mx/2))
 *
 *   You can specify a bit DEPTH for the stored array, which can be
 *   between 2 and 16 inclusive.  For bit depth 1, just threshhold
 *   the image (image>const_thresh).  By default, for integer IMAGE,
 *   DEPTH is the smallest depth that covers the range of IMAGE values,
 *   but never more than 16.  For float or double IMAGE, the default
 *   DEPTH is always 16.
 *
 *   If IMAGE has any integer data type, the pCAL mapping will always
 *   be linear; use IMAGE+0.0 if you want a log or asinh map.
 *
 *   The png pCAL definition allows b<0 in the log scale, so it can
 *   be used for image values that are either all positive or all
 *   negative.  In either case, the integer stored values take equal
 *   ratio steps from the minimum to the maximum image values (or
 *   cmin and cmax).  For the linear scale, of course, each step in
 *   the stored integer represents an constant increment in the image
 *   value.  The asinh scale is a logarithmic ratio scale for stored
 *   values near 0 or mx (the maximum stored integer value), reverting
 *   to a linear scale near the middle of its range where the image
 *   value passes through zero.
 *
 *   To get the asinh scale, you must specify the res= keyword:
 *   You must specify the smallest step size for the asinh scale by
 *   setting the res= keyword.  For a log scale, the res= value will
 *   replace the actual minimum array value or cmin value (or cmax if
 *   image is all negative values), clipping any smaller absolute values.
 *   If mx is large enough to cover the whole scale with the given res=
 *   value in linear steps, a linear scale will be used.
 *
 *   You can specify log=1 to force log scaling if image is all
 *   positive or all negative.
 *
 * SEE ALSO: png_scale, png_write, png_read
 */
{
  sample = image(1);
  in = min(image);
  ix = max(image);
  if (!is_void(cmax)) { in = min(in, cmax);  ix = min(ix, cmax); }
  if (!is_void(cmin)) { in = max(in, cmin);  ix = max(ix, cmin); }

  if (depth && (depth<2 || depth>16))
    error, "depth must be 2<=depth<=16 (for depth=1 just threshhold)";

  if (structof(sample+0) == long) {
    x0 = long(in);
    x1 = long(ix);
    if (!depth) for (mx=2,depth=1 ; mx>x1-x0 ; mx<<1,depth++);
    mx = (1<<depth) - 1;
    eq = 0;
    p = [0., dx, 0., 0.];

  } else {
    if (!depth) depth = 16;
    mx = (1<<depth) - 1;
    x0 = 0;
    x1 = mx;

    if (res && mx*abs(res)>=(ix-in)) {
      eq = 0;
    } else if (in*ix > 0.) {
      if (res && abs(res)<min(abs(in),abs(ix))) {
        if (in > 0.) in = abs(res);
        else ix = -abs(res);
      }
      lrat = _png_log(ix/in);
      if (in!=ix && (log || abs(lrat)>_png_log(32.))) {
        eq = 1;
        p = [0., in, lrat/mx, 0.];
      } else {
        eq = 0;
      }
    } else {
      if (res) {
        eq = 3;
        /* sinh(0.5*p3)/sinh(p3/mx) = max(abs(x))/res
         * roughly (0.5-1/mx)*p3 = log(axmax/res)
         * know axmax/abs(res) > mx
         */
        p4 = 0.5*mx;
        p = _png_eq3(p4, max(abs(in),abs(ix))/abs(res));
        p2 = abs(res) / sinh(p);
        p3 = mx * p;
        p = [0., p2, p3, p4];
      } else {
        eq = 0;
      }
    }
    if (!eq) p = [double(in), ((in==ix)? mx : double(ix-in)/mx), 0., 0.];
  }

  return grow(double([x0, x1, mx, eq]), p);
}

func _png_eq3(a, b)
{
  /* solve sinh(a*x)/sinh(x) = b for x assuming b>a>1 */
  x = _png_log(b)/(a-1.);   /* overestimate of root */
  for (i=1,err=1. ; i<50 && err>1e-9 ; i++) {
    s = sinh(x);
    bs = b*s;
    y = a*x - asinh(bs);
    yp = a - b*sqrt((1.+s*s)/(1.+bs*bs));
    if (yp <= 0.) error, "eq=3 root solve lost";
    dx = y/yp;
    x -= dx;
    err = abs(dx)/x;
  }
  return x;
}

if (is_void(_png_log)) _png_log = log;

extern _png_read;
extern _png_write;

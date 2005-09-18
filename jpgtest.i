/*
 * $Id: jpgtest.i,v 1.1 2005-09-18 22:07:08 dhmunro Exp $
 * test yorick jpeg interface
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */
require, "jpeg.i";

func jpgtest(&gray, &rgb, keep=)
{
  f = open(Y_SITE+"g/earth.gp");
  lines = rdline(f, 2000);
  lines = lines(where(lines));
  tok = strtok(lines, " \t=")(1,);
  list = where((tok>="0") & (tok<="999999"));
  lines = lines(list);
  npal = numberof(lines);
  pal = array(char, 3, npal);
  if (sread(lines, pal) != 3*npal)
    error, "problem reading palette earth.gp";

  x = span(-3,4,400)(,-:1:380);
  y = span(-4,3,380)(-:1:400,);
  z = (16.-abs(x,y)^2)*sin(1.5*x-y+.5*x*y);
  zb = bytscl(z, top=npal-1);
  rgb = pal(,1+zb);
  gray = bytscl(z, top=255);

  jpeg_write, "test-gray.jpg", gray;
  jpeg_write, "test-rgb.jpg", rgb;

  im = jpeg_read("test-gray.jpg");
  dev = (0.+gray-im)(*)(rms);
  write, format="dflt quality gray rms=%g (expect 0.73)\n", dev;
  if (!keep) remove, "test-gray.jpg";
  im = jpeg_read("test-rgb.jpg");
  dev = (0.+rgb-im)(*)(rms);
  write, format="dflt quality rgb rms=%g (expect 1.67)\n", dev;
  if (!keep) remove, "test-rgb.jpg";

  jpeg_write, "test-gray-hi.jpg", gray, , 100;
  jpeg_write, "test-rgb-hi.jpg", rgb, "rgb-hi Test Image", 100;

  com = "test comments";
  im = jpeg_read("test-gray-hi.jpg", com);
  dev = (0.+gray-im)(*)(rms);
  write, format="100 quality gray rms=%g (expect 0.22)\n", dev;
  if (!is_void(com)) write, "unexpected comments in test-gray-hi";
  if (!keep) remove, "test-gray-hi.jpg";
  im = jpeg_read("test-rgb-hi.jpg", com);
  dev = (0.+rgb-im)(*)(rms);
  write, format="100 quality rgb rms=%g (expect 0.84)\n", dev;
  if (numberof(com) != 1)
    write, format="unexpected comment count = %ld\n", numberof(com);
  if (com(1) != "rgb-hi Test Image")
    write, format="unexpected comment = %s\n", com(1);
  if (!keep) remove, "test-rgb-hi.jpg";

  jpeg_write, "test-gray-lo.jpg", gray, , 1;
  jpeg_write, "test-rgb-lo.jpg", rgb, , 1;

  im = jpeg_read("test-gray-lo.jpg");
  dev = (0.+gray-im)(*)(rms);
  write, format="1 quality gray rms=%g (expect 10.)\n", dev;
  if (!keep) remove, "test-gray-lo.jpg";
  im = jpeg_read("test-rgb-lo.jpg");
  dev = (0.+rgb-im)(*)(rms);
  write, format="1 quality rgb rms=%g (expect 16.)\n", dev;
  if (!keep) remove, "test-rgb-lo.jpg";
}

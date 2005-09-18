/*
 * $Id: pngtest.i,v 1.1 2005-09-18 22:07:10 dhmunro Exp $
 * test yorick png interface
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */
require, "png.i";

func pngtest(keep=)
{
  pal = get_palette("earth.gp");
  npal = numberof(pal)/3;
  depth = 0;
  nfo = [];

  x = span(-3,4,400)(,-:1:380);
  y = span(-4,3,380)(-:1:400,);
  z = (16.-abs(x,y)^2)*sin(1.5*x-y+.5*x*y);
  zb = bytscl(z, top=npal-1);

  png_write, "test-gray.png", zb;
  png_write, "test-pal.png", zb, palette=pal;
  png_write, "test-rgb.png", pal(,1+zb);

  im = png_read("test-gray.png");
  if (x1 = anyof(im!=zb)) write, "FAILURE: test-gray.png (image)";
  else write, "OK: test-gray.png";
  if (!x1 && !keep) remove, "test-gray.png";
  im = png_read("test-pal.png", depth, nfo);
  if (x1 = anyof(im!=zb)) write, "FAILURE: test-pal.png (image)";
  if (x2 = anyof(pal!=*nfo(1))) write, "FAILURE: test-pal.png (palette)";
  if (!x1 && !x2) write, "OK: test-pal.png";
  if (!x1 && !x2 && !keep) remove, "test-pal.png";
  im = png_read("test-rgb.png");
  if (x1 = anyof(im!=pal(,1+zb))) write, "FAILURE: test-rgb.png (image)";
  else write, "OK: test-rgb.png";
  if (!x1 && !keep) remove, "test-rgb.png";

  superlong = "Super long string (above 1024 characters) will become zTXt.\n";
  superlong += superlong;  /* 120 chars */
  superlong += superlong;  /* 240 chars */
  superlong += superlong;  /* 480 chars */
  superlong += superlong;  /* 960 chars */
  superlong += superlong;  /* 1920 chars */
  ts = timestamp();
  kt = [["Title", "funky scythes"],
        ["Description", "test of yorick png interface"],
        ["Creation Time", ts],
        ["Comment", "This is a very long comment containing embedded\n"+
                    "newline characters.  However, it does not begin\n"+
                    "to approach the 1024 character zTXt cutoff."],
        ["Test-ztxt", superlong]];

  tr1 = '\60';
  tr3 = pal(,49);
  alpha = array('\377', npal);
  alpha(1:49) = 0;
  bk1 = 100;
  bk3 = [255,0,0];
  pcal = [0, 1023, 255, 3, 0., 1.3, 2.5, 512.];
  pcals = ["test calibration", "bogomips"];
  scal = [400., 380., 1];
  phys = [5000, 5000, 1];
  time = [2004, 1, 20, 20, 0, 0];

  png_write, "test-all.png", zb, palette=pal, alpha=alpha, bkgd=bk1,
    pcal=pcal, pcals=pcals, scal=scal, phys=phys, time=time, text=kt;

  im = png_read("test-all.png", depth, nfo);
  x = [anyof(im!=zb), anyof(pal!=*nfo(1)), anyof(alpha!=*nfo(2)),
       anyof(bk1!=*nfo(3)), anyof(pcal!=*nfo(4)), anyof(pcals!=*nfo(5)),
       anyof(scal!=*nfo(6)), anyof(phys!=*nfo(7)), anyof(kt!=*nfo(8)),
       anyof(time!=*nfo(9))];
  if (noneof(x)) write, "OK: test-all.png";
  else { write, "FAILURE: test-all.png:"; x; }
  if (noneof(x) && !keep) remove, "test-all.png";

  zb3 = pal(,1+max(zb,48));
  png_write, "test-rgbx.png", zb3, trns=tr3, bkgd=bk3,
    pcal=pcal, pcals=pcals, scal=scal, phys=phys, time=time, text=kt;

  im = png_read("test-rgbx.png", depth, nfo);
  x = [anyof(im!=zb3), !is_void(*nfo(1)), anyof(tr3!=*nfo(2)),
       anyof(bk3!=*nfo(3)), anyof(pcal!=*nfo(4)), anyof(pcals!=*nfo(5)),
       anyof(scal!=*nfo(6)), anyof(phys!=*nfo(7)), anyof(kt!=*nfo(8)),
       anyof(time!=*nfo(9))];
  if (noneof(x)) write, "OK: test-rgbx.png";
  else { write, "FAILURE: test-rgbx.png:"; x; }
  if (noneof(x) && !keep) remove, "test-rgbx.png";

  zb = short(long(65535.999*(z-min(z))/(max(z)-min(z))));
  png_write, "test-gray16.png", zb;

  im = png_read("test-gray16.png");
  if (x1 = (anyof(im!=zb) || structof(im)!=short))
    write, "FAILURE: test-gray16.png (image)";
  else write, "OK: test-gray16.png";
  if (!x1 && !keep) remove, "test-gray16.png";

  zb = short(1023.999*(z-min(z))/(max(z)-min(z)));
  png_write, "test-gray10.png", zb, 10;

  im = png_read("test-gray10.png");
  if (x1 = (anyof(im!=zb) || structof(im)!=short))
    write, "FAILURE: test-gray10.png (image)";
  else write, "OK: test-gray10.png";
  if (!x1 && !keep) remove, "test-gray10.png";
}

func get_palette(name)
{
  if (strmatch(name,"/") || strmatch(name,"\\")) f = open(name);
  else f = open(Y_SITE+"g/"+name);
  lines = rdline(f, 2000);
  lines = lines(where(lines));
  tok = strtok(lines, " \t=")(1,);
  list = where((tok>="0") & (tok<="999999"));
  lines = lines(list);
  npal = numberof(lines);
  palette = array(char, 3, npal);
  if (sread(lines, palette) != 3*npal)
    error, "problem reading palette "+name;
  return palette;
}

/* ------------ independent png file partial decoder ------------- */

func pngdec(name, &plte)
{
  f = open(name, "rb");
  xdr_primitives, f;
  sig = array(char, 8);
  if (_read(f,0,sig)!=8 || anyof(sig!=[137,80,78,71,13,10,26,10])) {
    write, "bad PNG signature -- "+name+" not a PNG";
    return;
  }
  addr = 8;
  type = crc = [];
  data = pngchunk(f, addr, type, crc);
  if (structof(type)!=string) return;
  if (type != "IHDR" || numberof(data)!=13) {
    write, "expecting IHDR chunk, got "+type;
    return;
  }
  ldat = long(data(1:8));
  width = (ldat(1)<<24) | (ldat(2)<<16) | (ldat(3)<<8) | ldat(4);
  height = (ldat(5)<<24) | (ldat(6)<<16) | (ldat(7)<<8) | ldat(8);
  depth = long(data(9));
  ctype = long(data(10));
  cmpr = long(data(11));
  filt = long(data(12));
  intl = long(data(13));
  if (ctype == 0) ctype = "gray";
  else if (ctype == 2) ctype = "y+a";
  else if (ctype == 3) ctype = "plte";
  else if (ctype == 4) ctype = "rgb";
  else if (ctype == 6) ctype = "rgba";
  else ctype = "???"
  write, width, height, depth, ctype, cmpr, filt, intl,
    format="shape= %ldx%ld  depth= %ld  ctype= %s  c f i = %ld %ld %ld\n";
  write, format="crc = 0x%08lx   check = 0x%08lx\n", crc, pngcrc(type,data);
  for (;;) {
    write, format="%ld ", addr;
    data = pngchunk(f, addr, type, crc);
    if (structof(type)!=string || type=="IEND") break;
    write, format="%s  crc = 0x%08lx   check = 0x%08lx\n", type,
      crc, pngcrc(type,data);
    write, type;
    if (type=="IDAT") continue;
    if (type=="PLTE") {
      npal = numberof(data)/3;
      plte = array(char, 3, npal);
      plte(*) = data;
    }
  }
  if (structof(type)!=string) {
    write, "error reading file before IEND";
    return;
  }
  write, type;
}

func pngchunk(f, &addr, &type, &crc)
{
  len = crc = 0;
  type = data = [];
  if (_read(f, addr, len)!=1) goto oops;
  addr += 4;
  type = array(char, 4);
  if (_read(f, addr, type)!=4) goto oops;
  addr += 4;
  type = string(&type);
  if (len > 0) {
    data = array(char, len);
    if (_read(f, addr, data)!=len) goto oops;
    addr += len;
  }
  if (_read(f, addr, crc)!=1) goto oops;
  addr += 4;
  return data;

 oops:
  write, "bad PNG chunk";
  type = [];
  return [];
}

pngtable = [];

func pngcrc(type, data)
{
  if (is_void(pngtable)) {
    pngtable = array(long, 256);
    for (n=0 ; n<256 ; n++) {
      c = n;
      for (k=0 ; k<8 ; k++) {
        if (c & 1)
          c = 0xedb88320 ~ ((c >> 1) & 0x7fffffff);
        else
          c = (c >> 1) & 0x7fffffff;
      }
      pngtable(n+1) = c;
    }
  }

  type = (*pointer(type))(1:4);
  grow, type, data;
  c = 0xffffffff;
  for (n=1 ; n<=numberof(type) ; n++) {
    c = pngtable(1+((c ~ type(n)) & 0xff)) ~ ((c >> 8) & 0xffffff);
  }
  c = c ~ 0xffffffff;

  return c;
}

/*
 * $Id: ztest.i,v 1.1 2005-09-18 22:07:11 dhmunro Exp $
 * test yorick zlib interface
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */
require, "zlib.i";

func ztest(void)
{
  x = span(-1.5,2.5,100)(,-:1:101);
  y = span(-2.2,1.8,101)(-:1:100,);
  dat = bytscl(exp(-abs(x,y)^2), top=255);
  grow, dat, dat, dat, dat, dat, dat, dat, dat;

  b = z_deflate();
  navail = z_deflate(b, dat);
  zdat = z_flush(b,-);

  b = z_inflate();
  flag = z_inflate(b, zdat);
  if (flag) {
    write, format="FAILURE: simple inflate, flag = %ld\n", flag;
  } else {
    udat = z_flush(b);
    if (sizeof(udat)!=sizeof(dat) || anyof(udat!=dat(*)))
      write, format="%s\n", "FAILURE: simple inflate, udat != dat";
  }

  zdat1 = z_flush(z_deflate(), dat);
  if (sizeof(zdat1)!=sizeof(zdat) || anyof(zdat1!=zdat))
      write, format="%s\n", "FAILURE: single step deflate on flush";

  udat = dat;
  udat(..) = 0;
  flag = z_inflate(z_inflate(), zdat, udat);
  if (flag)
    write, format="FAILURE: inflate to given array, flag=%ld\n", flag;
  if (anyof(udat!=dat))
    write, format="%s\n", "FAILURE: single inflate, udat != dat";

  /* noise makes it ten times harder to compress */
  datx = dat + char(2*random(dimsof(dat)));
  zdat1 = z_flush(z_deflate(), datx);

  b = z_deflate();
  zdat2 = [];
  for (i=1 ; i<707 ; i+=101) {
    if (z_deflate(b, datx(,i:i+100)) > 10) {
      grow, zdat2, z_flush(b);
      write, format="OK: partial deflate flush, len=%ld\n", sizeof(zdat2);
    }
  }
  grow, zdat2, z_flush(b, datx(,i:i+100));
  if (sizeof(zdat2)!=sizeof(zdat1) || anyof(zdat2!=zdat1))
      write, format="%s\n", "FAILURE: multiple deflate";

  b = z_inflate();
  ntot = numberof(zdat1);
  nchnk = ntot/10;
  udat2 = [];
  quiet = 0;
  for (i=0 ; i<ntot ; i+=nchnk) {
    if (i+nchnk > ntot) nchnk = ntot - i;
    flag = z_inflate(b, zdat1(i+1:i+nchnk));
    if (!flag) {
      grow, udat2, z_flush(b);
      break;
    } else if (flag == 2) {
      grow, udat2, z_flush(b);
      if (!quiet++)
        write, format="OK: partial inflate flush, len=%ld\n", sizeof(udat2);
    } else if (flag != 1) {
      write, format="FAILURE: multiple inflate, flag = %ld\n", flag;
    } else if (i+nchnk == ntot) {
      write, format="%s\n", "FAILURE: multiple inflate to complete";
      break;
    }
  }
  if (sizeof(udat2)!=sizeof(datx) || anyof(udat2!=datx(*)))
    write, format="%s\n", "FAILURE: multiple inflate, udat != dat";

  /* check compress with level */
  zdat2 = z_flush(z_deflate(9), datx);
  write, format="OK: level 9 size=%ld (default size=%ld)\n",
    sizeof(zdat2), sizeof(zdat1);

  udat2 = datx;
  udat2(..) = 0;
  flag = z_inflate(z_inflate(), zdat2, udat2);
  if (flag)
    write, format="FAILURE: level 9 inflate, flag=%ld\n", flag;
  if (anyof(udat!=dat))
    write, format="%s\n", "FAILURE: level 9 inflate, udat2 != datx";

  /* try different data type */
  datl = long(datx(*));
  zdat2 = z_flush(z_deflate(), datl);
  write, format="OK: long type size=%ld (char type size=%ld)\n",
    sizeof(zdat2), sizeof(zdat1);

  b = z_inflate();
  ntot = numberof(zdat2);
  nchnk = ntot/10;
  udat2 = [];
  quiet = 0;
  for (i=0 ; i<ntot ; i+=nchnk) {
    if (i+nchnk > ntot) nchnk = ntot - i;
    flag = z_inflate(b, zdat2(i+1:i+nchnk));
    if (!flag) {
      grow, udat2, z_flush(b, long);
      break;
    } else if (flag == 2) {
      grow, udat2, z_flush(b, long);
      if (!quiet++)
        write, format="OK: long inflate flush, len=%ld\n", sizeof(udat2);
    } else if (flag != 1) {
      write, format="FAILURE: long inflate, flag = %ld\n", flag;
    } else if (i+nchnk == ntot) {
      write, format="%s\n", "FAILURE: long inflate to complete";
      break;
    }
  }
  if (structof(udat2) != long)
    write, format="%s\n", "FAILURE: long inflate, wrong z_flush type";
  if (sizeof(udat2)!=sizeof(datl) || anyof(udat2!=datl))
    write, format="%s\n", "FAILURE: long inflate, udat2 != datl";

  dict = dat(6000:8000);
  b = z_deflate(,dict);
  adl = z_setdict(b);
  zdatd = z_flush(b, dat);
  write, format="OK: dict size=%ld (default size=%ld)\n",
    sizeof(zdatd), sizeof(zdat);
  if (adl != z_crc32(,dict,1))
    write, format="%s\n", "FAILURE: dict adler32 checksum doesn't match";

  b = z_inflate();
  flag = z_inflate(b, zdatd);
  if (flag == 3) {
    if (adl != z_setdict(b))
    write, format="%s\n", "FAILURE: unexpected dict adler32 checksum";
    if (z_setdict(b, dict)) {
      flag = z_inflate(b, zdatd);
      if (flag) {
        write, format="FAILURE: dict inflate, flag = %ld\n", flag;
      } else {
        udat = z_flush(b);
        if (sizeof(udat)!=sizeof(dat) || anyof(udat!=dat(*)))
          write, format="%s\n", "FAILURE: dict inflate, udat != dat";
      }
    } else {
      write, format="%s\n", "FAILURE: z_setdict rejected dict";
    }
  } else {
    write, format="FAILURE: flag=%ld unexpected on dict inflate\n", flag;
  }

  
  b = z_inflate();
  flag = z_inflate(b, grow(zdat, char(indgen(5))));
  if (flag && flag!=-1) {
    write, format="FAILURE: extra bytes inflate, flag = %ld\n", flag;
  } else {
    if (flag != -1)
      write, format="%s\n", "FAILURE: did not detect extra bytes on inflate";
    udat = z_flush(b);
    if (sizeof(udat)!=sizeof(dat) || anyof(udat!=dat(*)))
      write, format="%s\n", "FAILURE: extra bytes inflate, udat != dat";
  }

  zdatbad = zdat;
  zdatbad(1000) = ~zdatbad(1000);
  b = z_inflate();
  flag = z_inflate(b, zdatbad);
  if (flag != -2)
    write, format="%s\n", "FAILURE: did not detect corrupt zdata on inflate";
}

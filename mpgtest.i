/*
 * $Id: mpgtest.i,v 1.1 2005-09-18 22:07:10 dhmunro Exp $
 * test yorick mpeg encoder
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */
require, "mpeg.i";

require, "movie.i";
if (is_void(_orig_movie)) _orig_movie = movie;
require, "demo2.i";

func mpgtest(void)
{
  _mpgtest_name = "test.mpg";
  movie = _mpgtest_movie;
  demo2, 3;
}

func _mpgtest_movie(__f, __a, __b, __c)
{
  movie = _orig_movie;
  return mpeg_movie(_mpgtest_name, __f, __a, __b, __c);
}

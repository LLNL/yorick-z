/* $Id: check.i,v 1.1 2005-09-18 22:07:11 dhmunro Exp $
 */
plug_dir, ["."];
write, "\n\n---------------------------------------checking zlib.i";
require, "ztest.i";
if (typeof(z_deflate) == "builtin") ztest; else
  write, "************** no zlib support in this yorick-z";
write, "\n\n---------------------------------------checking png.i";
require, "pngtest.i";
if (typeof(_png_read) == "builtin") pngtest; else
  write, "************** no png support in this yorick-z";
write, "\n\n---------------------------------------checking jpeg.i";
require, "jpgtest.i";
if (typeof(jpeg_read) == "builtin") jpgtest; else
  write, "************** no jpeg support in this yorick-z";
write, "\n\n---------------------------------------checking mpeg.i";
require, "mpgtest.i";
func do_mpgtest
{
  if (typeof(mpeg_create) != "builtin") {
    write, "************** no mpeg support in this yorick-z";
    return;
  }
  if (catch(-1)) {
    write, "caught interpreted error while checking mpeg.i";
    write, "... libavcodec (ffmpeg) may not be installed";
    return;
  }
  mpgtest;
  write, "************** created test.mpg, check it using mpeg viewer";
}
do_mpgtest;
write, "\n\n---------------------------------------yorick-z tests complete";
quit;

/*
 * $Id: mpeg.i,v 1.1 2005-09-18 22:07:08 dhmunro Exp $
 * yorick interface to mpeg movie encoding
 * 
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

if (!is_void(plug_in)) plug_in, "yorz";

extern mpeg_create;
/* DOCUMENT mpeg = mpeg_create(filename)
 *       or mpeg = mpeg_create(filename, params)
 *
 * Create an mpeg-1 movie file FILENAME.  Write frames with mpeg_write,
 * close with mpeg_close.  The return value is an mpeg encoder object.
 *
 * If given, PARAMS is [bit_rate, frame_rate, gop_size, max_b_frames]
 *    which default to [ 400000,      25,       10,         1       ]
 * The rates are per second, the gop_size is the number of frames
 * before an I-frame is emitted, and max_b_frames is the largest
 * number of consecutive B-frames.  (The third kind of frame is the
 * P-frame; generally, the encoder emits B-frames until it is forced
 * to emit a P-frame by max_b_frames, or an I-frame by gop_size.  The
 * smaller these numbers, the higher quality the movie, but the lower
 * the compression.)  Any of the four PARAMS values may be zero to
 * get the default value, except for max_b_frames, which should be <0
 * to get the default value.
 *
 * SEE ALSO: mpeg_write, mpeg_close, mpeg_movie
 */

extern mpeg_write;
/* DOCUMENT mpeg_write, mpeg, rgb
 *
 * Write a frame RGB into the mpeg file corresponding to the MPEG
 * encoder returned by mpeg_create. RGB is a 3-by-width-by-height
 * array of char.  Every frame must have the same width and height.
 * To finish the movie and close the file, call mpeg_close.
 *
 * Note that you may have trouble rendering the resulting mpeg
 * file if the image width and height are note multiples of 8.
 *
 * SEE ALSO: mpeg_create, mpeg_close, mpeg_movie
 */

func mpeg_close(&mpeg)
/* DOCUMENT mpeg_close, mpeg
 *
 * Close the mpeg file corresponding to the MPEG encoder.  Actually,
 * this merely destroys the reference to the encoder; the file will
 * remain open until the final reference is destroyed.
 *
 * SEE ALSO: mpeg_create, mpeg_write, mpeg_movie
 */
{
  mpeg = [];
}

func mpeg_movie(filename, draw_frame,time_limit,min_interframe,bracket_time)
/* DOCUMENT mpeg_movie, filename, draw_frame
 *       or mpeg_movie, filename, draw_frame, time_limit
 *       or mpeg_movie, filename, draw_frame, time_limit, min_interframe
 *
 * An extension of the movie function (#include "movie.i") that generates
 * an mpeg file FILENAME.  The other arguments are the same as the movie
 * function (which see).  The draw_frame function is:
 *
 *   func draw_frame(i)
 *   {
 *     // Input argument i is the frame number.
 *     // draw_frame should return non-zero if there are more
 *     // frames in this movie.  A zero return will stop the
 *     // movie.
 *     // draw_frame must NOT include any fma command if the
 *     // making_movie variable is set (movie sets this variable
 *     // before calling draw_frame)
 *   }
 *
 * SEE ALSO: movie, mpeg_create, mpeg_write, mpeg_close
 */
{
  require, "movie.i";
  _mpeg_movie_mpeg = mpeg_create(filename, [0, 0, 16, 2]);
  fma = _mpeg_movie_fma;
  _mpeg_movie_count = 0;
  return movie(draw_frame, time_limit, min_interframe, bracket_time);
}

if (is_void(_mpeg_movie_fma0)) _mpeg_movie_fma0 = fma;
func _mpeg_movie_fma
{
  /* movie function does one fma before first draw_frame, skip it */
  if (_mpeg_movie_count++) {
    rgb = rgb_read();
    /* trim image until divisible by 8 */
    n = dimsof(rgb)(3:4) & 7;
    if (anyof(n))
      rgb = rgb(,n(1)/2+1:-(n(1)+1)/2,n(2)/2+1:-(n(2)+1)/2);
    mpeg_write, _mpeg_movie_mpeg, rgb;
  }
  _mpeg_movie_fma0;
}

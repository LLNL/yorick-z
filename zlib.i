/*
 * $Id: zlib.i,v 1.1 2005-09-18 22:07:08 dhmunro Exp $
 * yorick interface to zlib data compression library
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

if (!is_void(plug_in)) plug_in, "yorz";

/* NOTE:
 *  the gz stream functions are not wrapped -- for text files, you can
 *  get that functionality with the popen function and gzip program
 */

extern z_deflate;
/* DOCUMENT buffer = z_deflate()
 *       or buffer = z_deflate(level)
 *       or buffer = z_deflate(level, dictionary)
 *     then navail = z_deflate(buffer, data)
 *       or zdata = z_flush(buffer)
 *  finally zdata = z_flush(buffer, data)
 *       or zdata = z_flush(buffer, -)
 *
 * In any of the first three forms, initiate a zlib deflation
 * BUFFER.  The second two forms allow you to specify a compression
 * LEVEL (0-9 in increasing compression and decreasing speed), and/or
 * a special compression DICTIONARY (which you will need to supply
 * again in order to decompress the data later).
 *
 * After the BUFFER has been created, use z_deflate to compress DATA,
 * adding it to the compressed stream in the BUFFER.  After one or
 * several calls to z_deflate, you can call z_flush in the first form
 * to extract the current BUFFER contents as ZDATA, the portion of
 * the compressed data stream stored in BUFFER.  You can alternate
 * calls to z_deflate and z_flush as many times as you like in order
 * to compress an arbitrary amount of DATA into ZDATA without filling
 * memory.  The NAVAIL returned by z_deflate is a lower limit on the
 * number of bytes of compressed data a subsequent z_flush will return.
 *
 * The final block of DATA must be compressed by a call to z_flush,
 * in the final form.  This flushes all remaining data into the
 * resulting ZDATA and closes the BUFFER.  You can call z_flush
 * in this form immediately after creating the buffer, so that
 * the tersest way to compress a single block of data is:
 *        zdata = z_flush(z_deflate(), data)
 * Use - for DATA to indicate you have no more DATA, but want to
 * finish the compression.
 *
 * SEE ALSO: z_inflate, z_flush, z_crc32
 */

extern z_inflate;
/* DOCUMENT buffer = z_inflate()
 *     then flag = z_inflate(buffer, zdata)
 *       or flag = z_inflate(buffer, zdata, data)
 *       or data = z_flush(buffer)
 *       or data = z_flush(buffer, type)
 *
 * In the first form, initiate a zlib inflation BUFFER.  You use
 * that BUFFER in subsequent calls to z_inflate if you do not know
 * in advance how large the uncompressed DATA will be, or if you
 * want to do the decompression in chunks to conserve memory.
 *
 * Use the second or third forms to actually decompress ZDATA.
 * After one or more calls to z_inflate, you can call z_flush in
 * order to extract whatever uncompressed DATA has so far been
 * produced.  You can optionally specify a TYPE array for
 * z_flush, otherwise the DATA will be a 1D array of char.
 *
 * Alternatively, you can supply a DATA array as the third parameter
 * to z_inflate, in which case z_inflate will uncompress to your DATA
 * array instead of to an internal array in BUFFER.  You can use this
 * form if you already know the size and data type the data will
 * decompress to.  If the returned flag is 3, you can call z_setdict
 * and repeat the call.  Otherwise, a return value other than 0
 * probably represents an error.  Note that z_flush will not return
 * bytes that have been written to a DATA array supplied to z_inflate.
 *
 * The FLAG returned by z_inflate is
 *   0  if the ZDATA stream is complete, in which case no
 *      further calls to z_inflate are legal with that BUFFER
 *      - the next call to z_flush will return all remaining
 *        bytes of the uncompressed data
 *   1  if the ZDATA stream is incomplete, but no additional
 *      uncompressed data is yet available in BUFFER
 *   2  if the ZDATA stream is incomplete, and uncompressed data
 *      can be retrieved from BUFFER by calling z_flush
 *   3  if a DICTIONARY is required to continue decompression
 *      - use z_setdict to set a dictionary and call z_inflate
 *        a second time with the same DATA
 *  -1  if the ZDATA stream completed, but contained additional
 *      bytes after the end
 *  -2  if the ZDATA stream is corrupted
 *
 * SEE ALSO: z_deflate, z_flush, z_setdict, z_crc32
 */

extern z_flush;
/* DOCUMENT zdata_or_data = z_flush(buffer)
 *       or zdata = z_flush(buffer, data)
 *       or zdata = z_flush(buffer, -)
 *       or data = z_flush(buffer, type)
 *
 * Flushes all available ZDATA (if STATE is a z_deflate state) or
 * all available DATA (if STATE is a z_inflate state).  For z_deflate
 * states, a second argument to z_flush is the final DATA block to
 * complete the ZDATA stream.  For z_inflate states, you may specify
 * an array data TYPE so that the return DATA value will have that
 * data type instead of char.
 *
 * SEE ALSO: z_deflate, z_inflate, z_setdict
 */

extern z_setdict;
/* DOCUMENT adler32 = z_setdict(buffer)
 *       or flag = z_setdict(buffer, dictionary)
 *
 * In the first form, returns the adler32 checksum of the dictionary
 * required to continue decompressing a stream after z_inflate
 * returns 3, or [] (nil) if BUFFER does not need a dictionary.
 * You can also use this form to retrieve the adler32 checksum of
 * a dictionary you supplied in the call to z_deflate that
 * returned BUFFER.
 *
 * In the second form, sets the DICTIONARY for BUFFER so that
 * succeeding calls to z_inflate can continue decompressing.  The
 * return value FLAG is 1 on success, or 0 on failure.
 *
 * You can compute the adler32 checksum using the z_crc32 function.
 *
 * SEE ALSO: z_inflate, z_crc32
 */

extern z_crc32;
/* DOCUMENT crc32 = z_crc32(crc32, data)
 *       or adler32 = z_crc32(adler32, data, 1)
 *
 * Compute the crc32 or adler32 checksum of DATA.  The first
 * argument can be [] (nil) if this is the first chunk of DATA;
 * to checksum a long stream of data you can call z_crc32 on
 * a series of chunks, feeding the result of each call as input
 * to the following call.
 *
 * SEE ALSO: z_setdict
 */

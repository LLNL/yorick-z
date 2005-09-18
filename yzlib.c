/*
 * $Id: yzlib.c,v 1.1 2005-09-18 22:07:09 dhmunro Exp $
 * yorick interface to zlib deflate and inflate functions
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#include "ydata.h"
#include "yio.h"
#include "defmem.h"
#include "pstdlib.h"
#include "play.h"
#include <string.h>

#include "zlib.h"

extern BuiltIn Y_z_deflate, Y_z_inflate, Y_z_flush, Y_z_setdict, Y_z_crc32;

/*--------------------------------------------------------------------------*/

typedef struct yz_stream yz_stream;
typedef struct yz_block yz_block;

/* implement zlib state as a foreign yorick data type */
struct yz_stream {
  int references;      /* reference counter */
  Operations *ops;     /* virtual function table */
  int status;          /* 1 deflate, 2 inflate, 3 inflate/done, 0 ended */
  yz_block *chunk;     /* output chunks */
  Bytef *dict;         /* inflate dictionary */
  uInt ldict;
  int need_dict;       /* adler valid */
  uLong adler;         /* checksum for required dictionary */
  z_stream zs;         /* zlib stream state, see zlib.h */
};

struct yz_block {
  yz_block *prev;
  unsigned long used, avail;
  Byte data[sizeof(long)];
};

extern yz_stream *yz_create(int inflate, int level);
extern void yz_free(void *yzs);  /* ******* Use Unref(yzs) ******* */
extern Operations yz_ops;

extern PromoteOp PromXX;
extern UnaryOp ToAnyX, NegateX, ComplementX, NotX, TrueX;
extern BinaryOp AddX, SubtractX, MultiplyX, DivideX, ModuloX, PowerX;
extern BinaryOp EqualX, NotEqualX, GreaterX, GreaterEQX;
extern BinaryOp ShiftLX, ShiftRX, OrX, AndX, XorX;
extern BinaryOp AssignX, MatMultX;
extern UnaryOp EvalX, SetupX, PrintX;
extern MemberOp GetMemberX;

static UnaryOp yz_print;

Operations yz_ops = {
  &yz_free, T_OPAQUE, 0, T_STRING, "zlib_stream",
  {&PromXX, &PromXX, &PromXX, &PromXX, &PromXX, &PromXX, &PromXX, &PromXX},
  &ToAnyX, &ToAnyX, &ToAnyX, &ToAnyX, &ToAnyX, &ToAnyX, &ToAnyX,
  &NegateX, &ComplementX, &NotX, &TrueX,
  &AddX, &SubtractX, &MultiplyX, &DivideX, &ModuloX, &PowerX,
  &EqualX, &NotEqualX, &GreaterX, &GreaterEQX,
  &ShiftLX, &ShiftRX, &OrX, &AndX, &XorX,
  &AssignX, &EvalX, &SetupX, &GetMemberX, &MatMultX, &yz_print
};

/*--------------------------------------------------------------------------*/

/* Set up a block allocator which grabs space for 32 yz_stream objects
 * at a time.  Since yz_stream contains an ops pointer, the alignment
 * of a yz_stream must be at least as strict as a void*.  */
static MemryBlock yz_mblock = {0, 0, sizeof(yz_stream), 32*sizeof(yz_stream)};

yz_stream *
yz_create(int inflate, int level)
{
  int status;
  yz_stream *yzs = NextUnit(&yz_mblock);
  yzs->references = 0;
  yzs->ops = &yz_ops;
  /* voidpf (*zalloc)(voidpf opaque, uInt items, uInt size)
   * void (*zfree)(voidpf opaque, voidpf address)
   *   uInt is unsigned int, voidpf is void FAR *, declared in zlib.h
   */
  yzs->zs.zalloc = Z_NULL;
  yzs->zs.zfree = Z_NULL;
  yzs->zs.opaque = Z_NULL;
  yzs->zs.data_type = Z_UNKNOWN;

  yzs->chunk = 0;
  yzs->dict = 0;
  yzs->ldict = 0;
  yzs->need_dict = 0;
  yzs->adler = 0;
  yzs->status = 0;
  status = inflate? inflateInit(&yzs->zs) : deflateInit(&yzs->zs, level);

  if (status != Z_OK) {
    FreeUnit(&yz_mblock, yzs);
    if (status == Z_STREAM_ERROR)
      YError("zlib (deflate): invalid compression level");
    else if (status == Z_VERSION_ERROR)
      YError("zlib (deflate/inflate): libz version mismatch");
    else if (status == Z_MEM_ERROR)
      YError("zlib (deflate/inflate): memory error on init");
    else
      YError("zlib (deflate/inflate): unknown error on init");
    return 0;
  }

  yzs->status = inflate? 2 : 1;
  return yzs;
}

static void yz_free_chunk(yz_block *yzbp);

void
yz_free(void *yzsv)  /* ******* Use Unref(yzs) ******* */
{
  yz_stream *yzs = yzsv;
  int status;
  if (!yzs) return;
  yz_free_chunk(yzs->chunk);
  yzs->chunk = 0;
  if (yzs->dict) {
    p_free(yzs->dict);
    yzs->dict = 0;
  }
  status = yzs->status;
  yzs->status = 0;
  if (status == 1) deflateEnd(&yzs->zs);
  else if (status==2 && status==3) inflateEnd(&yzs->zs);
  FreeUnit(&yz_mblock, yzs);
}

static void
yz_free_chunk(yz_block *yzb)
{
  if (yzb) {
    if (yzb->prev) yz_free_chunk(yzb->prev);
    yzb->prev = 0;
    p_free(yzb);
  }
}

static void
yz_print(Operand *op)
{
  yz_stream *yzs = op->value;
  ForceNewline();
  if (yzs->status==1) PrintFunc("zlib deflate buffer object");
  else if (yzs->status==2) PrintFunc("zlib inflate buffer object");
  else if (yzs->status==3) PrintFunc("zlib finished inflate buffer object");
  else PrintFunc("zlib buffer object, (de)compression finished");
  ForceNewline();
}

static Bytef *yz_getspace(yz_stream *yzs, unsigned long nbytes);
static void yz_do_deflate(yz_stream *yzs,
                          Bytef *data, unsigned long ldata, int flush);

void
Y_z_deflate(int nArgs)
{
  int level = Z_DEFAULT_COMPRESSION;
  yz_stream *yzs = 0;
  Bytef *data = 0;
  unsigned long ldata = 0;
  if (nArgs > 0) {
    Operand op;
    Symbol *stack = sp-nArgs+1;
    if (nArgs > 2) YError("z_deflate takes at most 2 arguments");
    if (!stack->ops) YError("z_deflate takes no keywords");
    stack->ops->FormOperand(stack, &op);
    if (op.ops == &yz_ops) {
      yzs = op.value;
      if (yzs->status == 2)
        YError("z_deflate: cannot use inflate state for deflate call");
      else if (yzs->status != 1)
        YError("z_deflate: deflate buffer closed, compression finished");
    } else if (op.value != &nilDB) {
      level = (int)YGetInteger(stack);
    }
    if (nArgs > 1) {
      stack++;
      stack->ops->FormOperand(stack, &op);
      if (op.value != &nilDB) {
        if (!op.ops->isArray)
          YError("z_deflate data or dictionary must be an array data type");
        if (op.ops==&stringOps || op.ops==&pointerOps)
          YError("z_deflate cannot handle string or pointer data types");
        ldata = op.type.number * op.type.base->size;
        data = op.value;
      }
    }
  }

  if (!yzs) {
    /* create a new zlib state buffer */
    yzs = (yz_stream *)PushDataBlock(yz_create(0, level));
    if (data) {
      int flag = deflateSetDictionary(&yzs->zs, data, (uInt)ldata);
      if (flag != Z_OK) {
        yzs->status = 0;
        deflateEnd(&yzs->zs);
        YError("z_deflate: zlib error setting dictionary");
      } else {
	yzs->adler = yzs->zs.adler;
	yzs->need_dict = 1;
      }
    }

  } else {
    /* compress a chunk of data */
    long ntotal = 0;
    yz_block *chunk;
    yz_do_deflate(yzs, data, ldata, Z_NO_FLUSH);
    for (chunk=yzs->chunk ; chunk ; chunk=chunk->prev) ntotal += chunk->used;
    PushLongValue(ntotal>1023? ntotal : 0L);
  }
}

void
Y_z_flush(int nArgs)
{
  yz_stream *yzs = 0;
  Bytef *data = 0;
  unsigned long ldata = 0;
  StructDef *type = &charStruct;
  Operand op;
  long i=0, lbuf, nbytes=0, nitems=0, nextra=0;
  yz_block *k, *n, *t;
  Array *array;
  unsigned char *c;
  char junk[4];
  Symbol *stack = sp-nArgs+1;
  if (nArgs<1 || nArgs>2) YError("z_flush takes 1 or 2 arguments");
  if (!stack->ops) YError("z_flush takes no keywords");
  stack->ops->FormOperand(stack, &op);
  if (op.ops == &yz_ops) {
    yzs = op.value;
    if (yzs->status!=1 && yzs->status!=2 && yzs->status!=3)
      YError("z_flush: zlib buffer closed, stream finished");
  } else {
    YError("z_flush first parameter must be a zlib buffer");
  }
  if (nArgs > 1) {
    stack++;
    stack->ops->FormOperand(stack, &op);
    if (yzs->status == 1) {
      if (op.ops == &rangeOps) {
        Range *r = op.value;
        if (r->nilFlags!=11 || r->inc!=1)
          YError("z_flush deflate data must be an array data type or -");
	data = (Bytef *)junk;
      } else if (op.value != &nilDB) {
        if (!op.ops->isArray)
          YError("z_flush deflate data must be an array data type or -");
        if (op.ops==&stringOps || op.ops==&pointerOps)
          YError("z_flush cannot handle string or pointer data types");
        ldata = op.type.number * op.type.base->size;
        data = op.value;
      }
    } else if (op.ops == &structDefOps) {
      type = op.value;
      if (!type->dataOps->isArray)
        YError("z_flush inflate type must be an array data type");
      if (type->dataOps==&stringOps || type->dataOps==&pointerOps)
        YError("z_flush string or pointer type illegal as inflate type");
    } else {
      YError("z_flush illegal inflate type argument");
    }
  }

  if (yzs->status == 1) {
    /* deflate final chunk if given */
    if (data) yz_do_deflate(yzs, data, ldata, Z_FINISH);
  }

  /* reverse list of chunks, count total bytes */
  for (n=0,k=yzs->chunk ; k ; n=k,k=t) {
    nbytes += k->used;
    t = k->prev;
    k->prev = n;
  }
  yzs->chunk = n;
  /* extract compressed data to zdata return value */
  if (type == &charStruct) {
    nitems = nbytes;
  } else if (yzs->status==3) {
    nitems = (nbytes+type->size-1) / type->size;
  } else {
    nitems = nbytes / type->size;
    nextra = nbytes - type->size*nitems;
  }
  array = (Array *)PushDataBlock(NewArray(type, ynew_dim(nitems, (void*)0)));
  c = (unsigned char *)array->value.c;
  for (k=t=yzs->chunk ; k ; t=k,k=k->prev) {
    lbuf = k->prev? k->used : k->used-nextra;
    for (i=0 ; i<lbuf ; i++) c[i] = k->data[i];
    c += lbuf;
  }
  for (lbuf=0 ; lbuf<nextra ; lbuf++) yzs->chunk->data[lbuf] = t->data[i++];
  /* zap all but first chunk */
  k = yzs->chunk;
  k->avail += k->used - nextra;
  k->used = nextra;
  for (;;) {
    t = k->prev;
    if (!t) break;
    k->prev = t->prev;
    p_free(t);
  }
}

void
Y_z_inflate(int nArgs)
{
  yz_stream *yzs = 0;
  Bytef *zdata = 0;
  unsigned long lzdata = 0;
  Bytef *data = 0;
  unsigned long ldata = 0;
  if (nArgs > 0) {
    Symbol *stack = sp-nArgs+1;
    Operand op;
    if (nArgs > 3) YError("z_inflate takes at most 3 arguments");
    if (!stack->ops) YError("z_inflate takes no keywords");
    stack->ops->FormOperand(stack, &op);
    if (op.ops == &yz_ops) {
      yzs = op.value;
      if (yzs->status == 1)
        YError("z_inflate: cannot use deflate state for inflate call");
      else if (yzs->status != 2)
        YError("z_inflate: inflate buffer closed, decompression finished");
      if (nArgs > 1) {
        stack++;
        if (!stack->ops) YError("z_inflate takes no keywords");
        stack->ops->FormOperand(stack, &op);
        if (op.value != &nilDB) {
          if (op.ops != &charOps)
            YError("z_inflate zdata must have char data type");
          lzdata = op.type.number * op.type.base->size;
          zdata = op.value;
        }
        if (nArgs > 2) {
          stack++;
          stack->ops->FormOperand(stack, &op);
          if (!op.ops->isArray)
            YError("z_inflate output data must be an array data type");
          if (op.ops==&stringOps || op.ops==&pointerOps)
            YError("z_inflate cannot handle string or pointer output data");
          ldata = op.type.number * op.type.base->size;
          data = op.value;
        }
      }
    } else if (op.value!=&nilDB || nArgs>1) {
      YError("z_inflate arguments are garbled, see help,z_inflate");
    }
  }

  if (!yzs) {
    /* create a new zlib state buffer */
    yzs = (yz_stream *)PushDataBlock(yz_create(1, 0));

  } else {
    /* uncompress a chunk of zdata, assuming factor of 2 expansion */
    int flag, dict_set=0;
    unsigned long lguess = (lzdata<<1);
    char junk[4];

    yzs->zs.next_in = zdata? zdata : (Bytef *)junk;
    yzs->zs.avail_in = lzdata;
    for (;;) {
      if (!data) {
        yzs->zs.next_out = yz_getspace(yzs, lguess);
        yzs->zs.avail_out = yzs->chunk->avail;
      } else {
        yzs->zs.next_out = data;
        yzs->zs.avail_out = ldata;
      }
      flag = inflate(&yzs->zs, Z_NO_FLUSH);
      if (!data) {
	yzs->chunk->used += yzs->chunk->avail - yzs->zs.avail_out;
	yzs->chunk->avail = yzs->zs.avail_out;
      }
      if (flag==Z_NEED_DICT && !dict_set) {
	dict_set = 1;
	if (yzs->dict) {
	  flag = inflateSetDictionary(&yzs->zs, yzs->dict, yzs->ldict);
	  dict_set = 2;
	  p_free(yzs->dict);
	  yzs->dict = 0;
	  if (flag == Z_OK) continue;
	  dict_set = (flag == Z_DATA_ERROR)? 3 : 4;
	} else {
	  yzs->adler = yzs->zs.adler;
	  if (inflateReset(&yzs->zs) != Z_OK)
	    dict_set = 5;
	  else
	    yzs->need_dict = 1;
	}
	flag = Z_NEED_DICT;
	/* if dict_set==1 here, yzs->dict==0 and inflateReset succeeded */
      }
      if (flag != Z_OK) {
	if (flag!=Z_NEED_DICT || dict_set!=1) {
	  yzs->status = 0;
	  inflateEnd(&yzs->zs);
	  if (flag!=Z_STREAM_END && flag!=Z_DATA_ERROR) {
	    if (flag != Z_NEED_DICT)
	      YError("z_inflate: STREAM, MEM, or BUF Z_?_ERROR in inflate");
	    else if (dict_set == 2)
	      YError("z_inflate: multiple dictionary requests by inflate");
	    else if (dict_set == 3)
	      YError("z_inflate: wrong dictionary, bad adler32 checksum");
	    else if (dict_set == 4)
	      YError("z_inflate: zlib error setting dictionary");
	    else
	      YError("z_inflate: inflateReset failed after Z_NEED_DICT");
	    flag = Z_MEM_ERROR;  /* not reached */
	  }
	}
        break;
      }
      if (yzs->zs.avail_out) break;

      if (data) data = 0;
      /* difficult to recompute lguess, because we don't know how much
       * is being help in internal zlib state
       * - so stick with initial estimate?
       * - could be very bad if huge compression factor
       */
    }

    if (flag == Z_STREAM_END) {
      flag = yzs->zs.avail_in? -1 : 0;
      yzs->status = 3;
    } else if (flag == Z_OK) {
      yz_block *k;
      for (k=yzs->chunk ; k && !k->used ; k=k->prev);
      flag = k? 2 : 1;
    } else if (flag == Z_NEED_DICT) {
      flag = 3;
    } else /* if (flag == Z_DATA_ERROR) */ {
      flag = -2;
    }
    PushLongValue((long)flag);
  }
}

void
Y_z_setdict(int nArgs)
{
  yz_stream *yzs = 0;
  Operand op;
  Symbol *stack = sp-nArgs+1;
  if (nArgs<1 || nArgs>2) YError("z_setdict takes 1 or 2 arguments");
  if (!stack->ops) YError("z_setdict takes no keywords");
  stack->ops->FormOperand(stack, &op);
  if (op.ops == &yz_ops) {
    yzs = op.value;
    if (yzs->status!=1 && yzs->status!=2)
      YError("z_setdict: zlib buffer closed, stream finished");
  } else {
    YError("z_setdict first parameter must be a zlib buffer");
  }
  if (nArgs == 1) {
    if (yzs->need_dict)
      PushLongValue(yzs->adler);
    else
      PushDataBlock(RefNC(&nilDB));

  } else if (yzs->need_dict && yzs->status==2) {
    Bytef *dict = 0;
    long i, ldict = 0;
    stack++;
    stack->ops->FormOperand(stack, &op);
    if (!op.ops->isArray)
      YError("z_setdict input data must be an array data type");
    if (op.ops==&stringOps || op.ops==&pointerOps)
      YError("z_setdict cannot handle string or pointer input data");
    ldict = op.type.number * op.type.base->size;
    dict = op.value;
    yzs->dict = p_malloc(ldict);
    yzs->ldict = ldict;
    for (i=0 ; i<ldict ; i++) yzs->dict[i] = dict[i];
    PushIntValue(1);

  } else {
    PushIntValue(0);
  }
}

void
Y_z_crc32(int nArgs)
{
  Operand op;
  Symbol *stack = sp-nArgs+1;
  int use_adler32;
  uLong cksum, ldata;
  if (nArgs<2 || nArgs>3) YError("z_crc32 takes 2 or 3 arguments");
  if (!stack->ops || !stack[1].ops) YError("z_crc32 takes no keywords");
  use_adler32 = (nArgs==3 && YGetInteger(stack+2)!=0);
  if (YNotNil(stack))
    cksum = YGetInteger(stack);
  else
    cksum = use_adler32? adler32(0L, Z_NULL, 0) : crc32(0L, Z_NULL, 0);
  stack++;
  stack->ops->FormOperand(stack, &op);
  if (!op.ops->isArray)
    YError("z_crc32 input data must be an array data type");
  if (op.ops==&stringOps || op.ops==&pointerOps)
    YError("z_crc32 cannot handle string or pointer input data");
  ldata = op.type.number * op.type.base->size;
  PushLongValue(use_adler32? adler32(cksum, op.value, ldata) :
		crc32(cksum, op.value, ldata));
}

static void
yz_do_deflate(yz_stream *yzs, Bytef *data, unsigned long ldata, int flush)
{
  /* compress final chunk of data, guessing factor of 4 compression */
  int flag;
  unsigned long lguess = (ldata >> 2) + 1;

  yzs->zs.next_in = data;
  yzs->zs.avail_in = ldata;
  for (;;) {
    yzs->zs.next_out = yz_getspace(yzs, lguess);
    yzs->zs.avail_out = yzs->chunk->avail;
    flag = deflate(&yzs->zs, flush);
    yzs->chunk->used += yzs->chunk->avail - yzs->zs.avail_out;
    yzs->chunk->avail = yzs->zs.avail_out;
    if (flag != Z_OK) {
      yzs->status = 0;
      deflateEnd(&yzs->zs);
      if (flag != Z_STREAM_END) {
        if (flush != Z_FINISH)
          YError("z_deflate: zlib error during deflate");
        else
          YError("z_flush: zlib error during final deflate");
      }
      break;
    }
    if (yzs->zs.avail_out) break;

    /* difficult to recompute lguess here, because we don't know
     * how much is held in internal zlib state
     */
  }
}

static Bytef *
yz_getspace(yz_stream *yzs, unsigned long nbytes)
{
  yz_block *chunk;
  long len;
  if (yzs->chunk && yzs->chunk->avail>=1024)
    return (Bytef *)yzs->chunk->data + yzs->chunk->used;
  len = (( ((nbytes-1)>>12) + 1 ) << 12);
  chunk = p_malloc(sizeof(yz_block) + len);
  if (!chunk) return 0;
  chunk->prev = yzs->chunk;
  chunk->avail = len + sizeof(long);
  chunk->used = 0;
  yzs->chunk = chunk;
  return (Bytef *)chunk->data;
}

/*--------------------------------------------------------------------------*/

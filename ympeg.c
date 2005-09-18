/*
 * $Id: ympeg.c,v 1.1 2005-09-18 22:07:11 dhmunro Exp $
 * mpeg encoding interface for yorick
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
#include <stdio.h>

/* default parameter values */
#define YMPG_BIT_RATE 400000
#define YMPG_FRAME_RATE 24
#define YMPG_GOP_SIZE 10
#define YMPG_MAX_B_FRAMES 1

#ifdef YAVC_SHARED
   /* -DYAVC_SHARED to dynamically link libavcodec using dlopen */

#  include "yio.h"
#  include "yavcodec.h"
#  define YAVC_(f) 0
#else
   /* -UYAVC_SHARED to statically link libavcodec */
#  include "avcodec.h"
#  define YAVC_(f) f
#endif

static AVCodec *yavc_encoder = YAVC_(&mpeg1video_encoder);

static unsigned int (*yavc_version)(void)
     = YAVC_(avcodec_version);
static void (*yavc_init)(void)
     = YAVC_(avcodec_init);
static void (*yavc_register)(AVCodec *format)
     = YAVC_(register_avcodec);
static AVCodec *(*yavc_find_encoder)(enum CodecID id)
     = YAVC_(avcodec_find_encoder);
static AVCodecContext *(*yavc_alloc_context)(void)
     = YAVC_(avcodec_alloc_context);
static AVFrame *(*yavc_alloc_frame)(void)
     = YAVC_(avcodec_alloc_frame);
static int (*yavc_open)(AVCodecContext *avctx, AVCodec *codec)
     = YAVC_(avcodec_open);
static int (*yavc_encode_video)(AVCodecContext *avctx, uint8_t *buf,
                                int buf_size, const AVFrame *pict)
     = YAVC_(avcodec_encode_video);
static int (*yavc_close)(AVCodecContext *avctx)
     = YAVC_(avcodec_close);
static void *(*yavc_malloc)(unsigned int size)
     = YAVC_(av_malloc);
static void (*yavc_free)(void *ptr)
     = YAVC_(av_free);
static int (*yavc_fill)(AVPicture *picture, uint8_t *ptr,
                        int pix_fmt, int width, int height)
     = YAVC_(avpicture_fill);
static int (*yavc_get_size)(int pix_fmt, int width, int height)
     = YAVC_(avpicture_get_size);
static int (*yavc_convert)(AVPicture *dst,int dst_pix_fmt,const AVPicture *src,
                           int pix_fmt, int width, int height)
     = YAVC_(img_convert);

/* static int yavc_bld_version = LIBAVCODEC_VERSION_INT; */
static int yavc_lib_version = -1;

/*--------------------------------------------------------------------------*/

extern BuiltIn Y_mpeg_create, Y_mpeg_write;

typedef struct ympg_stream ympg_stream;
typedef struct ympg_block ympg_block;

/* implement zlib state as a foreign yorick data type */
struct ympg_stream {
  int references;      /* reference counter */
  Operations *ops;     /* virtual function table */
  FILE *f;
  AVCodecContext *c;
  AVCodec *codec;
  uint8_t *in, *out;
  AVFrame *frame;
  long nout, nframes;
  int width, height, outsize;
};

extern ympg_stream *ympg_create(char *filename, long *params);
extern void ympg_free(void *ympg);  /* ******* Use Unref(ympg) ******* */
extern Operations ympg_ops;

extern PromoteOp PromXX;
extern UnaryOp ToAnyX, NegateX, ComplementX, NotX, TrueX;
extern BinaryOp AddX, SubtractX, MultiplyX, DivideX, ModuloX, PowerX;
extern BinaryOp EqualX, NotEqualX, GreaterX, GreaterEQX;
extern BinaryOp ShiftLX, ShiftRX, OrX, AndX, XorX;
extern BinaryOp AssignX, MatMultX;
extern UnaryOp EvalX, SetupX, PrintX;
extern MemberOp GetMemberX;

static UnaryOp ympg_print;

Operations ympg_ops = {
  &ympg_free, T_OPAQUE, 0, T_STRING, "zlib_stream",
  {&PromXX, &PromXX, &PromXX, &PromXX, &PromXX, &PromXX, &PromXX, &PromXX},
  &ToAnyX, &ToAnyX, &ToAnyX, &ToAnyX, &ToAnyX, &ToAnyX, &ToAnyX,
  &NegateX, &ComplementX, &NotX, &TrueX,
  &AddX, &SubtractX, &MultiplyX, &DivideX, &ModuloX, &PowerX,
  &EqualX, &NotEqualX, &GreaterX, &GreaterEQX,
  &ShiftLX, &ShiftRX, &OrX, &AndX, &XorX,
  &AssignX, &EvalX, &SetupX, &GetMemberX, &MatMultX, &ympg_print
};

/*--------------------------------------------------------------------------*/

/* Set up a block allocator which grabs space for 32 ympg_stream objects
 * at a time.  Since ympg_stream contains an ops pointer, the alignment
 * of a ympg_stream must be at least as strict as a void*.  */
static MemryBlock ympg_mblock = {0, 0, sizeof(ympg_stream),
				 32*sizeof(ympg_stream)};

static int ympg_initialized = 0;
static void ympg_link(void);

ympg_stream *
ympg_create(char *filename, long *params)
{
  char *name = p_native(filename);
  FILE *f = (name && name[0])? fopen(name, "w") : 0;
  ympg_stream *ympg = 0;

  p_free(name);
  if (f) {
    AVCodec *codec;
    if (params && (params[0]<0 || params[1]<0 || params[2]<0))
      YError("mpeg_create: bad parameter list dimensions or values");
    if (ympg_initialized != 1) {
      if (!yavc_convert) ympg_link();
      yavc_lib_version = yavc_version();
      yavc_init();
      yavc_register(yavc_encoder);
      ympg_initialized = 1;
    }
    codec = yavc_find_encoder(CODEC_ID_MPEG1VIDEO);
    if (codec) {
      ympg = NextUnit(&ympg_mblock);
      ympg->references = 0;
      ympg->ops = &ympg_ops;
      ympg->f = f;
      ympg->c = yavc_alloc_context();
      /* ffmpeg 0.4.8 bit_rate was first item in AVCodecContext */
      if (yavc_lib_version < 0x000409)
        /* ffmpeg 0.4.8 bit_rate was first item in AVCodecContext */
        ympg->c = (void *)&ympg->c->bit_rate;
      ympg->codec = codec;
      ympg->frame = yavc_alloc_frame();
      ympg->in = ympg->out = 0;
      ympg->width = ympg->height = ympg->outsize = 0;
      ympg->nout = ympg->nframes = 0;
      if (!ympg->c || !ympg->frame) {
	if (ympg->c) yavc_free(ympg->c);
	if (ympg->frame) yavc_free(ympg->frame);
	FreeUnit(&ympg_mblock, ympg);
	ympg = 0;
	YError("mpeg_create: yavc_alloc_context or alloc_frame failed");
      } else {
        ympg->c->bit_rate =
          (params && params[0])? params[0] : YMPG_BIT_RATE;
        ympg->c->frame_rate =
          (params && params[1])? params[1] : YMPG_FRAME_RATE;  
        /* note c->frame_rate_base=1 by default, unnecessary for mpeg1? */
        ympg->c->gop_size =
          (params && params[2])? params[2] : YMPG_GOP_SIZE;
        ympg->c->max_b_frames =
          (params && params[3]>=0)? params[3] : YMPG_MAX_B_FRAMES;
      }
    } else {
      YError("mpeg_create: failed to find MPEG1VIDEO encoder");
    }
  } else {
    YError("mpeg_create: fopen failed to create mpeg output file");
  }

  return ympg;
}

void
ympg_free(void *ympgv)  /* ******* Use Unref(ympg) ******* */
{
  ympg_stream *ympg = ympgv;

  /* get the delayed frames */
  if (ympg->f && ympg->nframes) {
    if (ympg->nout) for (;;) {
      ympg->nout = yavc_encode_video(ympg->c, ympg->out, ympg->outsize, 0);
      if (!ympg->nout) break;
      fwrite(ympg->out, 1, ympg->nout, ympg->f);
    }

    /* add sequence end code to mpeg file */
    ympg->out[0] = 0x00;
    ympg->out[1] = 0x00;
    ympg->out[2] = 0x01;
    ympg->out[3] = 0xb7;
    fwrite(ympg->out, 1, 4, ympg->f);
  }

  if (ympg->f) fclose(ympg->f);
  ympg->f = 0;
  if (ympg->c) {
    if (!ympg->codec) yavc_close(ympg->c);
    yavc_free(ympg->c);
  }
  ympg->c = 0;

  if (ympg->out) yavc_free(ympg->out);
  ympg->out = 0;
  if (ympg->in) yavc_free(ympg->in);
  ympg->in = 0;
  if (ympg->frame) yavc_free(ympg->frame);
  ympg->frame = 0;

  FreeUnit(&ympg_mblock, ympg);
}

/* ARGSUSED */
static void
ympg_print(Operand *op)
{
  /* ympg_stream *yzs = op->value; */
  ForceNewline();
  PrintFunc("mpeg encoder object");
  ForceNewline();
}

/*--------------------------------------------------------------------------*/

void
Y_mpeg_create(int nArgs)
{
  char *filename = (nArgs>=1 && nArgs<=2)? YGetString(sp-nArgs+1) : 0;
  long bad_params[4] = { -1, -1, -1, -1 };
  long *params = 0;
  if (nArgs == 2) {
    Dimension *dims = 0;
    params = YGet_L(sp-nArgs+2, 1, &dims);
    if (!dims || dims->next || dims->number!=4) params = bad_params;
  }
  PushDataBlock(ympg_create(filename, params));
}

void
Y_mpeg_write(int nArgs)
{
  Operand op;
  Symbol *stack = sp-nArgs+1;
  int ndims;
  Dimension *dims;
  long idims[3];
  uint8_t *image;
  AVPicture img;
  int width, height, wyuv, hyuv;
  ympg_stream *ympg;

  if (nArgs != 2) YError("mpeg_write takes at exactly 2 arguments");
  if (!stack->ops) YError("mpeg_write takes no keywords");
  stack->ops->FormOperand(stack, &op);
  if (op.ops != &ympg_ops)
    YError("mpeg_write: first argument must be an mpeg encoder object");
  ympg = op.value;
  image = (uint8_t *)YGet_C(stack+1, 0, &dims);
  ndims = YGet_dims(dims, idims, 3);
  width = idims[1];
  height = idims[2];
  if (ndims!=3 || idims[0]!=3 || width<8 || height<8)
    YError("mpeg_write: image not rgb or too small");
  wyuv = (width+7) & ~7;
  hyuv = (height+7) & ~7;

  if (ympg->codec) {
    int size = yavc_get_size(PIX_FMT_YUV420P, wyuv, hyuv);
    ympg->in = yavc_malloc(size);
    ympg->outsize = size>100512? size+512 : 100512;
    ympg->out = yavc_malloc(ympg->outsize);
    if (!ympg->in || !ympg->out)
      YError("mpeg_write: av_malloc memory manager failed");

    /* note: ffmpeg source routinely casts AVFrame* to AVPicture* */
    yavc_fill((AVPicture*)ympg->frame, ympg->in, PIX_FMT_YUV420P, wyuv, hyuv);

    /* set picture size */
    ympg->c->width = wyuv;  
    ympg->c->height = hyuv;

    if (yavc_open(ympg->c, ympg->codec) < 0)
      YError("mpeg_create: avcodec_open failed");
    ympg->codec = 0;

  } else if (wyuv!=ympg->c->width || hyuv!=ympg->c->height) {
    YError("mpeg_write: image dimensions differ from previous frame");
  }

  yavc_fill(&img, image, PIX_FMT_RGB24, width, height);
  /* note: ffmpeg source routinely casts AVFrame* to AVPicture* */
  if (yavc_convert((AVPicture*)ympg->frame, PIX_FMT_YUV420P, 
                   &img, PIX_FMT_RGB24, width, height) < 0)
    YError("mpeg_write: avcodec RGB24 --> YUV420P converter missing");

  ympg->nout =
    yavc_encode_video(ympg->c, ympg->out, ympg->outsize, ympg->frame);
  while (ympg->nout==ympg->outsize) {
    fwrite(ympg->out, 1, ympg->nout, ympg->f);
    ympg->nout = yavc_encode_video(ympg->c, ympg->out, ympg->outsize, 0);
  }
  if (ympg->nout) fwrite(ympg->out, 1, ympg->nout, ympg->f);
  ympg->nframes++;
}

#ifdef YAVC_SHARED

#define NSYMS 15

static struct symadd_t {
  char *name;
  int is_data;
  void *paddr;
} ympg_symadd[NSYMS] = {{"mpeg1video_encoder",    1, &yavc_encoder},
                        {"avcodec_version",       0, &yavc_version},
                        {"avcodec_init",          0, &yavc_init},
                        {"register_avcodec",      0, &yavc_register},
                        {"avcodec_find_encoder",  0, &yavc_find_encoder},
                        {"avcodec_alloc_context", 0, &yavc_alloc_context},
                        {"avcodec_alloc_frame",   0, &yavc_alloc_frame},
                        {"avcodec_open",          0, &yavc_open},
                        {"avcodec_encode_video",  0, &yavc_encode_video},
                        {"avcodec_close",         0, &yavc_close},
                        {"av_malloc",             0, &yavc_malloc},
                        {"av_free",               0, &yavc_free},
                        {"avpicture_fill",        0, &yavc_fill},
                        {"avpicture_get_size",    0, &yavc_get_size},
                        /* img_convert must be final entry */
                        {"img_convert",           0, &yavc_convert}};

static void
ympg_link(void)
{
  if (!ympg_initialized) {
    char *yavc_path[] = { 0, 0, "libavcodec", "/lib/libavcodec",
                          "/usr/lib/libavcodec", "/usr/local/lib/libavcodec",
                          "/sw/lib/libavcodec", 0 };
    char **yavc_name = yavc_path;
    char *yavc_env = Ygetenv("Y_LIBAVCODEC");
    void *dll = 0;
    /* look for libavcodec first at name in Y_LIBAVCODEC environment
     * variable (not including .so or other extension), then Y_HOME/lib,
     * then as simply "libavcodec" (current working directory?),
     * then in system places /lib, /usr/lib, /usr/local/lib
     */
    if (yavc_env && yavc_env[0]) yavc_path[0] = yavc_env;
    else yavc_name++;
    if (yHomeDir && yHomeDir[0]) {
      char *yhscan = yHomeDir;
      while (yhscan[1]) yhscan++;
      yavc_path[1] = p_strncat(yHomeDir, (yhscan[0]=='/')? "lib/libavcodec" :
                               "/lib/libavcodec", 0);
    } else {
      yavc_name++;
    }
    for ( ; *yavc_name ; yavc_name++) {
      dll = p_dlopen(*yavc_name);
      if (dll) {
        int i, mask;
        for (i=0,mask=1 ; i<NSYMS ; i++,mask=2)
          if (p_dlsym(dll, ympg_symadd[i].name, ympg_symadd[i].is_data,
                      ympg_symadd[i].paddr) != 0) break;
        if (i < NSYMS)
          YError("mpeg_create: found libavcodec, but missing symbols");
        break;
      }
    }
    if (yavc_env) p_free(yavc_env);
    p_free(yavc_path[1]);
    yavc_path[0] = yavc_path[1] = yavc_env = 0;

    /* is this wrong? do we want to allow user to install it later? */
    if (!dll) ympg_initialized = 2;
  }
  if (ympg_initialized)
    YError("mpeg_create: unable to find or dynamically link to libavcodec");
}

#else

static void
ympg_link(void)
{
  YError("mpeg_create: (IMPOSSIBLE) bad static link to libavcodec");
}

#endif

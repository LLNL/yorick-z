/*
 * $Id: yavcodec.h,v 1.1 2005-09-18 22:07:11 dhmunro Exp $
 * subset of avcodec.h actually used by ympeg.c
 */
#ifndef AVCODEC_H
#define AVCODEC_H

#ifdef __cplusplus
extern "C" {
#endif

#define LIBAVCODEC_VERSION_INT 0x000409

enum PixelFormat {
  PIX_FMT_YUV420P,
  PIX_FMT_YUV422,    
  PIX_FMT_RGB24
  /* many more unused by ympeg.c */
};
enum CodecID {
  CODEC_ID_NONE, 
  CODEC_ID_MPEG1VIDEO
  /* many more unused by ympeg.c */
};

/* typedef unsigned char uint8_t; */
/* some systems define uint8_t, others do not, workaround is macro */
#ifndef uint8_t
#  define uint8_t unsigned char
#endif
typedef struct AVFrame AVFrame;
typedef struct AVPicture AVPicture;
typedef struct AVCodecContext AVCodecContext;
typedef struct AVCodec AVCodec;

extern AVCodec mpeg1video_encoder;

extern unsigned int avcodec_version(void);
extern void avcodec_init(void);
extern void register_avcodec(AVCodec *format);
extern AVCodec *avcodec_find_encoder(enum CodecID id);
extern AVCodecContext *avcodec_alloc_context(void);
extern AVFrame *avcodec_alloc_frame(void);
extern int avcodec_open(AVCodecContext *avctx, AVCodec *codec);
extern int avcodec_encode_video(AVCodecContext *avctx, uint8_t *buf,
                                int buf_size, const AVFrame *pict);
extern int avcodec_close(AVCodecContext *avctx);
extern void *av_malloc(unsigned int size);
extern void av_free(void *ptr);
extern int avpicture_get_size(int pix_fmt, int width, int height);
extern int avpicture_fill(AVPicture *picture, uint8_t *ptr,
                          int pix_fmt, int width, int height);
extern int img_convert(AVPicture *dst, int dst_pix_fmt,
                       AVPicture *src, int pix_fmt, int width, int height);

struct AVPicture {              /* members opaque, but size required */
    uint8_t *data[4];
    int linesize[4];
};

typedef struct AVClass AVClass;

struct AVCodecContext {         /* several members must be set */
  AVClass *av_class;            /* not present in ffmpeg-0.4.8 */
  int bit_rate;                 /* used */

  int bit_rate_tolerance;
  int flags;
  int sub_id;
  int me_method;
  void *extradata;
  int extradata_size;

  int frame_rate;               /* used */
  int width, height;            /* used */
  int gop_size;                 /* used */

  enum PixelFormat pix_fmt;
  int rate_emu;
  void (*draw_horiz_band)(void);
  int sample_rate;
  int channels;
  int sample_fmt;
  int frame_size;
  int frame_number;
  int real_pict_num;
  int delay;
  float qcompress;
  float qblur;
  int qmin;
  int qmax;
  int max_qdiff;

  int max_b_frames;             /* used */

  /* ... many more unused by ympeg.c
   * in particular frame_rate_base is way down in member list
   * but default value of 1 just means frame_rate is in frames/sec
   * mpeg12.c code apparently adjusts these to look like
   * frame_rate_base=1001 no matter what its actual value
   */
};

#ifdef __cplusplus
}
#endif

#endif /* AVCODEC_H */

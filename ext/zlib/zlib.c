/* 
 * MacRuby Zlib API.
 *
 * This file is covered by the Ruby license. See COPYING for more details.
 * 
 * Copyright (C) 2007-2010, Apple Inc. All rights reserved.
 * Copyright (C) UENO Katsuhiro 2000-2003
 */

#include "ruby/macruby.h"
#include "ruby/intern.h"
#include "ruby/node.h"
#include "ruby/io.h"
#include "objc.h"
#include "id.h"
#include "vm.h"
#include <zlib.h>
#include <time.h>

#define RUBY_ZLIB_VERSION  "0.6.0"

VALUE rb_io_puts(VALUE out, SEL sel, int argc, VALUE *argv);
VALUE rb_f_open(VALUE io, SEL sel, int argc, VALUE *argv);

#ifndef GZIP_SUPPORT
#define GZIP_SUPPORT  1
#endif

/* from zutil.h */
#ifndef DEF_MEM_LEVEL
#if MAX_MEM_LEVEL >= 8
#define DEF_MEM_LEVEL  8
#else
#define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif
#endif


/*--------- Prototypes --------*/

static NORETURN(void raise_zlib_error _((int, const char*)));
static VALUE rb_zlib_version _((VALUE, SEL));
static VALUE do_checksum _((int, VALUE*, uLong (*) _((uLong, const Bytef*, uInt))));
static VALUE rb_zlib_adler32 _((VALUE, SEL, int, VALUE*));
static VALUE rb_zlib_crc32 _((VALUE, SEL, int, VALUE*));
static VALUE rb_zlib_crc_table _((VALUE, SEL));

struct zstream;
struct zstream_funcs;
static void zstream_init _((struct zstream*, const struct zstream_funcs*));
static void zstream_expand_buffer _((struct zstream*));
static void zstream_expand_buffer_into _((struct zstream*, int));
static void zstream_append_buffer _((struct zstream*, const Bytef*, int));
static VALUE zstream_detach_buffer _((struct zstream*));
static VALUE zstream_shift_buffer _((struct zstream*, int));
static void zstream_buffer_ungetc _((struct zstream*, int));
static void zstream_append_input _((struct zstream*, const Bytef*, unsigned int));
static void zstream_discard_input _((struct zstream*, unsigned int));
static void zstream_reset_input _((struct zstream*));
static void zstream_passthrough_input _((struct zstream*));
static VALUE zstream_detach_input _((struct zstream*));
static void zstream_reset _((struct zstream*));
static VALUE zstream_end _((struct zstream*));
static void zstream_run _((struct zstream*, Bytef*, uInt, int));
static VALUE zstream_sync _((struct zstream*, Bytef*, uInt));
static VALUE zstream_new _((VALUE, const struct zstream_funcs*));
static struct zstream *get_zstream _((VALUE));

static VALUE rb_zstream_end _((VALUE, SEL));
static VALUE rb_zstream_reset _((VALUE, SEL));
static VALUE rb_zstream_finish _((VALUE, SEL));
static VALUE rb_zstream_flush_next_in _((VALUE, SEL));
static VALUE rb_zstream_flush_next_out _((VALUE, SEL));
static VALUE rb_zstream_avail_out _((VALUE, SEL));
static VALUE rb_zstream_set_avail_out _((VALUE, SEL, VALUE));
static VALUE rb_zstream_avail_in _((VALUE, SEL));
static VALUE rb_zstream_total_in _((VALUE, SEL));
static VALUE rb_zstream_total_out _((VALUE, SEL));
static VALUE rb_zstream_data_type _((VALUE, SEL));
static VALUE rb_zstream_adler _((VALUE, SEL));
static VALUE rb_zstream_finished_p _((VALUE, SEL));
static VALUE rb_zstream_closed_p _((VALUE, SEL));

static VALUE rb_deflate_s_allocate _((VALUE, SEL));
static VALUE rb_deflate_initialize _((VALUE, SEL, int, VALUE*));
static VALUE rb_deflate_init_copy _((VALUE, SEL, VALUE));
static VALUE deflate_run _((VALUE));
static VALUE rb_deflate_s_deflate _((VALUE, SEL, int, VALUE*));
static void do_deflate _((struct zstream*, VALUE, int));
static VALUE rb_deflate_deflate _((VALUE, SEL, int, VALUE*));
static VALUE rb_deflate_addstr _((VALUE, SEL, VALUE));
static VALUE rb_deflate_flush _((VALUE, SEL, int, VALUE*));
static VALUE rb_deflate_params _((VALUE, SEL, VALUE, VALUE));
static VALUE rb_deflate_set_dictionary _((VALUE, SEL, VALUE));

static VALUE inflate_run _((VALUE));
static VALUE rb_inflate_s_allocate _((VALUE, SEL));
static VALUE rb_inflate_initialize _((VALUE, SEL, int, VALUE*));
static VALUE rb_inflate_s_inflate _((VALUE, SEL, VALUE));
static void do_inflate _((struct zstream*, VALUE));
static VALUE rb_inflate_inflate _((VALUE, SEL, VALUE));
static VALUE rb_inflate_addstr _((VALUE, SEL, VALUE));
static VALUE rb_inflate_sync _((VALUE, SEL, VALUE));
static VALUE rb_inflate_sync_point_p _((VALUE, SEL));
static VALUE rb_inflate_set_dictionary _((VALUE, SEL, VALUE));

#if GZIP_SUPPORT
struct gzfile;
static VALUE gzfile_new _((VALUE, const struct zstream_funcs*, void (*) _((struct gzfile*))));
static void gzfile_reset _((struct gzfile*));
static void gzfile_close _((struct gzfile*, int));
static void gzfile_write_raw _((struct gzfile*));
static VALUE gzfile_read_raw_partial _((VALUE));
static VALUE gzfile_read_raw_rescue _((VALUE, VALUE));
static VALUE gzfile_read_raw _((struct gzfile*));
static int gzfile_read_raw_ensure _((struct gzfile*, int));
static char *gzfile_read_raw_until_zero _((struct gzfile*, long));
static unsigned int gzfile_get16 _((const unsigned char*));
static unsigned long gzfile_get32 _((const unsigned char*));
static void gzfile_set32 _((unsigned long n, unsigned char*));
static void gzfile_make_header _((struct gzfile*));
static void gzfile_make_footer _((struct gzfile*));
static void gzfile_read_header _((struct gzfile*));
static void gzfile_check_footer _((struct gzfile*));
static void gzfile_write _((struct gzfile*, Bytef*, uInt));
static long gzfile_read_more _((struct gzfile*));
static void gzfile_calc_crc _((struct gzfile*, VALUE));
static VALUE gzfile_read _((struct gzfile*, int));
static VALUE gzfile_read_all _((struct gzfile*));
static void gzfile_ungetc _((struct gzfile*, int));
static VALUE gzfile_writer_end_run _((VALUE));
static void gzfile_writer_end _((struct gzfile*));
static VALUE gzfile_reader_end_run _((VALUE));
static void gzfile_reader_end _((struct gzfile*));
static void gzfile_reader_rewind _((struct gzfile*));
static VALUE gzfile_reader_get_unused _((struct gzfile*));
static struct gzfile *get_gzfile _((VALUE));
static VALUE gzfile_ensure_close _((VALUE));
static VALUE rb_gzfile_s_wrap _((VALUE, SEL, int, VALUE*));
static VALUE gzfile_s_open _((int, VALUE*, VALUE, const char*));

static VALUE rb_gzfile_to_io _((VALUE, SEL));
static VALUE rb_gzfile_crc _((VALUE, SEL));
static VALUE rb_gzfile_mtime _((VALUE, SEL));
static VALUE rb_gzfile_level _((VALUE, SEL));
static VALUE rb_gzfile_os_code _((VALUE, SEL));
static VALUE rb_gzfile_orig_name _((VALUE, SEL));
static VALUE rb_gzfile_comment _((VALUE, SEL));
static VALUE rb_gzfile_lineno _((VALUE, SEL));
static VALUE rb_gzfile_set_lineno _((VALUE, SEL, VALUE));
static VALUE rb_gzfile_set_mtime _((VALUE, SEL, VALUE));
static VALUE rb_gzfile_set_orig_name _((VALUE, SEL, VALUE));
static VALUE rb_gzfile_set_comment _((VALUE, SEL, VALUE));
static VALUE rb_gzfile_close _((VALUE, SEL));
static VALUE rb_gzfile_finish _((VALUE, SEL));
static VALUE rb_gzfile_closed_p _((VALUE, SEL));
static VALUE rb_gzfile_eof_p _((VALUE, SEL));
static VALUE rb_gzfile_sync _((VALUE, SEL));
static VALUE rb_gzfile_set_sync _((VALUE, SEL, VALUE));
static VALUE rb_gzfile_total_in _((VALUE, SEL));
static VALUE rb_gzfile_total_out _((VALUE, SEL));

static VALUE rb_gzwriter_s_allocate _((VALUE, SEL));
static VALUE rb_gzwriter_s_open _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzwriter_initialize _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzwriter_flush _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzwriter_write _((VALUE, SEL, VALUE));
static VALUE rb_gzwriter_putc _((VALUE, SEL, VALUE));

static VALUE rb_gzreader_s_allocate _((VALUE, SEL));
static VALUE rb_gzreader_s_open _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzreader_initialize _((VALUE, SEL, VALUE));
static VALUE rb_gzreader_rewind _((VALUE, SEL));
static VALUE rb_gzreader_unused _((VALUE, SEL));
static VALUE rb_gzreader_read _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzreader_getc _((VALUE, SEL));
static VALUE rb_gzreader_readchar _((VALUE, SEL));
static VALUE rb_gzreader_each_byte _((VALUE, SEL));
static VALUE rb_gzreader_ungetc _((VALUE, SEL, VALUE));
static void gzreader_skip_linebreaks _((struct gzfile*));
static VALUE gzreader_gets _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzreader_gets _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzreader_readline _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzreader_each _((VALUE, SEL, int, VALUE*));
static VALUE rb_gzreader_readlines _((VALUE, SEL, int, VALUE*));
#endif /* GZIP_SUPPORT */

void Init_zlib _((void));

#define BSTRING_LEN(s) rb_bstr_length(s)
#define BSTRING_PTR(s) (rb_bstr_bytes(s))
#define BSTRING_PTR_BYTEF(s) ((Bytef*)rb_bstr_bytes(s))


/*--------- Exceptions --------*/

static VALUE cZError, cStreamEnd, cNeedDict;
static VALUE cStreamError, cDataError, cMemError, cBufError, cVersionError;

static void
raise_zlib_error(int err, const char *msg)
{
    VALUE exc;

    if (!msg) {
        msg = zError(err);
    }

    switch(err) {
        case Z_STREAM_END:
        exc = rb_exc_new2(cStreamEnd, msg);
        break;
        case Z_NEED_DICT:
        exc = rb_exc_new2(cNeedDict, msg);
        break;
        case Z_STREAM_ERROR:
        exc = rb_exc_new2(cStreamError, msg);
        break;
        case Z_DATA_ERROR:
        exc = rb_exc_new2(cDataError, msg);
        break;
        case Z_BUF_ERROR:
        exc = rb_exc_new2(cBufError, msg);
        break;
        case Z_VERSION_ERROR:
        exc = rb_exc_new2(cVersionError, msg);
        break;
        case Z_MEM_ERROR:
        exc = rb_exc_new2(cMemError, msg);
        break;
        case Z_ERRNO:
        rb_sys_fail(msg);
    /* no return */
        default:
        {
            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "unknown zlib error %d: %s", err, msg);
            exc = rb_exc_new2(cZError, buf);
        }
    }

    rb_exc_raise(exc);
}

/*-------- module Zlib --------*/

/*
 * Returns the string which represents the version of zlib library.
 */
static VALUE
rb_zlib_version(VALUE klass, SEL sel)
{
    VALUE str;

    str = rb_str_new2(zlibVersion());
    OBJ_TAINT(str);  /* for safe */
    return str;
}

static VALUE
do_checksum(argc, argv, func)
    int argc;
    VALUE *argv;
    uLong (*func) _((uLong, const Bytef*, uInt));
{
    VALUE str, vsum;
    unsigned long sum;

    rb_scan_args(argc, argv, "02", &str, &vsum);

    if (!NIL_P(vsum)) {
        sum = NUM2ULONG(vsum);
    } else if (NIL_P(str)) {
        sum = 0;
    } else {
        sum = func(0, Z_NULL, 0);
    }

    if (NIL_P(str)) {
        sum = func(sum, Z_NULL, 0);
    } else {
        StringValue(str);
        sum = func(sum, (const Bytef*)RSTRING_PTR(str), (uInt)RSTRING_LEN(str));
    }
    return rb_uint2inum(sum);
}

/*
 * call-seq: Zlib.adler32(string, adler)
 *
 * Calculates Alder-32 checksum for +string+, and returns updated value of
 * +adler+. If +string+ is omitted, it returns the Adler-32 initial value. If
 * +adler+ is omitted, it assumes that the initial value is given to +adler+.
 *
 * FIXME: expression.
 */
static VALUE
rb_zlib_adler32(VALUE klass, SEL sel, int argc, VALUE *argv)
{
    return do_checksum(argc, argv, adler32);
}

/*
 * call-seq: Zlib.crc32(string, adler)
 *
 * Calculates CRC checksum for +string+, and returns updated value of +crc+. If
 * +string+ is omitted, it returns the CRC initial value. If +crc+ is omitted, it
 * assumes that the initial value is given to +crc+.
 *
 * FIXME: expression.
 */
static VALUE
rb_zlib_crc32(VALUE klass, SEL sel, int argc, VALUE *argv)
{
    return do_checksum(argc, argv, crc32);
}

/*
 * Returns the table for calculating CRC checksum as an array.
 */
static VALUE
rb_zlib_crc_table(VALUE obj, SEL sel)
{
    const unsigned long *crctbl;
    VALUE dst;
    int i;

    crctbl = get_crc_table();
    dst = rb_ary_new2(256);

    for (i = 0; i < 256; i++) {
        rb_ary_push(dst, rb_uint2inum(crctbl[i]));
    }
    return dst;
}



/*-------- zstream - internal APIs --------*/

struct zstream {
    unsigned long flags;
    VALUE buf;
    long buf_filled;
    VALUE input;
    z_stream stream;
    const struct zstream_funcs {
    	int (*reset) _((z_streamp));
    	int (*end) _((z_streamp));
    	int (*run) _((z_streamp, int));
    } *func;
};

#define ZSTREAM_FLAG_READY      0x1
#define ZSTREAM_FLAG_IN_STREAM  0x2
#define ZSTREAM_FLAG_FINISHED   0x4
#define ZSTREAM_FLAG_CLOSING    0x8
#define ZSTREAM_FLAG_UNUSED     0x10

#define ZSTREAM_READY(z)       ((z)->flags |= ZSTREAM_FLAG_READY)
#define ZSTREAM_IS_READY(z)    ((z)->flags & ZSTREAM_FLAG_READY)
#define ZSTREAM_IS_FINISHED(z) ((z)->flags & ZSTREAM_FLAG_FINISHED)
#define ZSTREAM_IS_CLOSING(z)  ((z)->flags & ZSTREAM_FLAG_CLOSING)

/* I think that more better value should be found,
   but I gave up finding it. B) */
#define ZSTREAM_INITIAL_BUFSIZE       1024
#define ZSTREAM_AVAIL_OUT_STEP_MAX   16384
#define ZSTREAM_AVAIL_OUT_STEP_MIN    2048

static const struct zstream_funcs deflate_funcs = {
    deflateReset, deflateEnd, deflate,
};

static const struct zstream_funcs inflate_funcs = {
    inflateReset, inflateEnd, inflate,
};

static void
zstream_init(struct zstream *z, const struct zstream_funcs *func)
{
    z->flags = 0;
    z->buf = Qnil;
    z->buf_filled = 0;
    z->input = Qnil;
    z->stream.zalloc = Z_NULL;
    z->stream.zfree = Z_NULL;
    z->stream.opaque = Z_NULL;
    z->stream.msg = Z_NULL;
    z->stream.next_in = Z_NULL;
    z->stream.avail_in = 0;
    z->stream.next_out = Z_NULL;
    z->stream.avail_out = 0;
    z->func = func;
}

#define zstream_init_deflate(z)   zstream_init((z), &deflate_funcs)
#define zstream_init_inflate(z)   zstream_init((z), &inflate_funcs)

static void
zstream_expand_buffer(struct zstream *z)
{
    long inc;

    if (NIL_P(z->buf)) {
        GC_WB(&z->buf, rb_bstr_new());
        rb_bstr_resize(z->buf, ZSTREAM_INITIAL_BUFSIZE);
        z->buf_filled = 0;
        z->stream.next_out = BSTRING_PTR_BYTEF(z->buf);
        z->stream.avail_out = ZSTREAM_INITIAL_BUFSIZE;
        return;
    }

    if (BSTRING_LEN(z->buf) - z->buf_filled >= ZSTREAM_AVAIL_OUT_STEP_MAX) {
        /* keep other threads from freezing */
        z->stream.avail_out = ZSTREAM_AVAIL_OUT_STEP_MAX;
    } else {
        inc = z->buf_filled / 2;
        if (inc < ZSTREAM_AVAIL_OUT_STEP_MIN) {
            inc = ZSTREAM_AVAIL_OUT_STEP_MIN;
        }
        rb_bstr_resize(z->buf, z->buf_filled + inc);
        z->stream.avail_out = (inc < ZSTREAM_AVAIL_OUT_STEP_MAX) ?
            inc : ZSTREAM_AVAIL_OUT_STEP_MAX;
    }
    z->stream.next_out = BSTRING_PTR_BYTEF(z->buf) + z->buf_filled;
}

static void
zstream_expand_buffer_into(struct zstream *z, int size)
{
    if (NIL_P(z->buf)) {
        GC_WB(&z->buf, rb_bstr_new());
        z->buf_filled = 0;
        z->stream.next_out = BSTRING_PTR_BYTEF(z->buf);
        z->stream.avail_out = size;
    }
    else if (z->stream.avail_out != size) {
        rb_bstr_resize(z->buf, z->buf_filled + size);
        z->stream.next_out = BSTRING_PTR_BYTEF(z->buf) + z->buf_filled;
        z->stream.avail_out = size;
    }
}

static void
zstream_append_buffer(struct zstream *z, const Bytef *src, int len)
{
    if (NIL_P(z->buf)) {
	GC_WB(&z->buf, rb_bstr_new_with_data((UInt8*)src, len));
	z->buf_filled = len;
	z->stream.next_out = BSTRING_PTR_BYTEF(z->buf);
	z->stream.avail_out = 0;
	return;
    }

    if (BSTRING_LEN(z->buf) < (z->buf_filled + len)) {
	z->stream.avail_out = 0;
    }
    else {
	if (z->stream.avail_out >= len) {
	    z->stream.avail_out -= len;
	} else {
	    z->stream.avail_out = 0;
	}
    }

    rb_bstr_resize(z->buf, z->buf_filled);
    rb_bstr_concat(z->buf, (const UInt8 *)src, len);
    z->buf_filled += len;
    z->stream.next_out = BSTRING_PTR_BYTEF(z->buf) + z->buf_filled;
}

#define zstream_append_buffer2(z,v) \
    zstream_append_buffer((z),(Bytef*)rb_bstr_bytes(v),rb_bstr_length(v))

static VALUE
zstream_detach_buffer(struct zstream *z)
{
    VALUE dst;

    if (NIL_P(z->buf)) {
        dst = rb_bstr_new();
    }
    else {
        dst = z->buf;
        rb_bstr_resize(dst, z->buf_filled);
    }

    z->buf = Qnil;
    z->buf_filled = 0;
    z->stream.next_out = 0;
    z->stream.avail_out = 0;
    return dst;
}

static VALUE
zstream_shift_buffer(struct zstream *z, int len)
{
    VALUE dst;

    if (z->buf_filled <= len) {
        return zstream_detach_buffer(z);
    }

    dst = rb_str_bstr(rb_str_subseq(z->buf, 0, len));
    z->buf_filled -= len;
    UInt8 *buf = BSTRING_PTR(z->buf);
    memmove(buf, buf + len, z->buf_filled);
    z->stream.next_out = BSTRING_PTR_BYTEF(z->buf) + z->buf_filled;
    z->stream.avail_out = BSTRING_LEN(z->buf) - z->buf_filled;
    if (z->stream.avail_out > ZSTREAM_AVAIL_OUT_STEP_MAX) {
        z->stream.avail_out = ZSTREAM_AVAIL_OUT_STEP_MAX;
    }

    return dst;
}

static void
zstream_buffer_ungetc(struct zstream *z, int c)
{
    if (NIL_P(z->buf) || BSTRING_LEN(z->buf) - z->buf_filled == 0) {
        zstream_expand_buffer(z);
    }
    UInt8* buf = BSTRING_PTR(z->buf);
    memmove(buf+1, buf, z->buf_filled);
    buf[0] = (UInt8)c;
    z->buf_filled++;
    if (z->stream.avail_out > 0) {
        z->stream.next_out++;
        z->stream.avail_out--;
    }
}

static void
zstream_append_input(struct zstream *z, const Bytef *src, unsigned int len)
{
    if (len <= 0) return;

    if (NIL_P(z->input)) {
	GC_WB(&z->input, rb_bstr_new_with_data((UInt8*)src, len));
    } else {
	rb_bstr_concat(z->input, (const UInt8*)src, len);
    }
}

#define zstream_append_input2(z,v) \
    do { \
	v = rb_str_bstr(v); \
	zstream_append_input((z), BSTRING_PTR_BYTEF(v), BSTRING_LEN(v)); \
    } \
    while(0)

static void
zstream_discard_input(struct zstream *z, unsigned int len)
{
    if (NIL_P(z->input) || BSTRING_LEN(z->input) <= len) {
	z->input = Qnil;
    } else {
	UInt8 *buf = BSTRING_PTR(z->input);
	memmove(buf, buf+len, BSTRING_LEN(z->input) - len);
	rb_bstr_resize(z->input, BSTRING_LEN(z->input) - len);
    }
}

static void
zstream_reset_input(struct zstream *z)
{
    z->input = Qnil;
}

static void
zstream_passthrough_input(struct zstream *z)
{
    if (!NIL_P(z->input)) {
        zstream_append_buffer2(z, z->input);
        z->input = Qnil;
    }
}

static VALUE
zstream_detach_input(struct zstream *z)
{
    VALUE dst;

    if (NIL_P(z->input)) {
        dst = rb_bstr_new();
    } else {
	dst = z->input;
    }
    z->input = Qnil;
    return dst;
}

static void
zstream_reset(struct zstream *z)
{
    int err;

    err = z->func->reset(&z->stream);
    if (err != Z_OK) {
        raise_zlib_error(err, z->stream.msg);
    }
    z->flags = ZSTREAM_FLAG_READY;
    z->buf = Qnil;
    z->buf_filled = 0;
    z->stream.next_out = 0;
    z->stream.avail_out = 0;
    zstream_reset_input(z);
}

static VALUE
zstream_end(struct zstream *z)
{
    int err;

    if (!ZSTREAM_IS_READY(z)) {
        rb_warning("attempt to close uninitialized zstream; ignored.");
        return Qnil;
    }
    if (z->flags & ZSTREAM_FLAG_IN_STREAM) {
        rb_warning("attempt to close unfinished zstream; reset forced.");
        zstream_reset(z);
    }

    zstream_reset_input(z);
    err = z->func->end(&z->stream);
    if (err != Z_OK) {
        raise_zlib_error(err, z->stream.msg);
    }
    z->flags = 0;
    return Qnil;
}

static void
zstream_run(struct zstream *z, Bytef *src, uInt len, int flush)
{
    uInt n;
    int err;
    volatile VALUE guard;

    if (NIL_P(z->input) && len == 0) {
        z->stream.next_in = (Bytef*)"";
        z->stream.avail_in = 0;
    } else {
        zstream_append_input(z, src, len);
        z->stream.next_in = BSTRING_PTR_BYTEF(z->input);
        z->stream.avail_in = BSTRING_LEN(z->input);
    /* keep reference to `z->input' so as not to be garbage collected
        after zstream_reset_input() and prevent `z->stream.next_in'
            from dangling. */
            guard = z->input;
    }

    if (z->stream.avail_out == 0) {
        zstream_expand_buffer(z);
    }

    for (;;) {
        n = z->stream.avail_out;
        err = z->func->run(&z->stream, flush);
        z->buf_filled += n - z->stream.avail_out;
        rb_thread_schedule();

        if (err == Z_STREAM_END) {
            z->flags &= ~ZSTREAM_FLAG_IN_STREAM;
            z->flags |= ZSTREAM_FLAG_FINISHED;
            break;
        }
        if (err != Z_OK) {
            if (flush != Z_FINISH && err == Z_BUF_ERROR && z->stream.avail_out > 0) {
                z->flags |= ZSTREAM_FLAG_IN_STREAM;
                break;
            }
            zstream_reset_input(z);
            if (z->stream.avail_in > 0) {
                zstream_append_input(z, z->stream.next_in, z->stream.avail_in);
            }
            raise_zlib_error(err, z->stream.msg);
        }
        if (z->stream.avail_out > 0) {
            z->flags |= ZSTREAM_FLAG_IN_STREAM;
            break;
        }
        zstream_expand_buffer(z);
    }

    zstream_reset_input(z);
    if (z->stream.avail_in > 0) {
        zstream_append_input(z, z->stream.next_in, z->stream.avail_in);
        guard = Qnil; /* prevent tail call to make guard effective */
    }
}

static VALUE
zstream_sync(struct zstream *z, Bytef *src, uInt len)
{
    VALUE rest; // not sure what this is used for
    int err;

    if (!NIL_P(z->input)) {
        z->stream.next_in = BSTRING_PTR_BYTEF(z->input);
        z->stream.avail_in = BSTRING_LEN(z->input);
        err = inflateSync(&z->stream);
        if (err == Z_OK) {
            zstream_discard_input(z, BSTRING_LEN(z->input) - z->stream.avail_in);
            zstream_append_input(z, src, len);
            return Qtrue;
        }
        zstream_reset_input(z);
        if (err != Z_DATA_ERROR) {
            rest = rb_str_new((char*)z->stream.next_in, z->stream.avail_in);
            raise_zlib_error(err, z->stream.msg);
        }
    }

    if (len <= 0) return Qfalse;

    z->stream.next_in = src;
    z->stream.avail_in = len;
    err = inflateSync(&z->stream);
    if (err == Z_OK) {
        zstream_append_input(z, z->stream.next_in, z->stream.avail_in);
        return Qtrue;
    }
    if (err != Z_DATA_ERROR) {
        rest = rb_str_new((char*)z->stream.next_in, z->stream.avail_in);
        raise_zlib_error(err, z->stream.msg);
    }
    return Qfalse;
}

// XXX: Add finalizers!!

#if 0
static void
zstream_finalize(struct zstream *z)
{
    int err = z->func->end(&z->stream);
    if (err == Z_STREAM_ERROR) {
        rb_warn("the stream state was inconsistent.");
    }
    if (err == Z_DATA_ERROR)
	finalizer_warn("the stream was freed prematurely.");
}
#endif

static VALUE
zstream_new(VALUE klass, const struct zstream_funcs *funcs)
{
    struct zstream *z = ALLOC(struct zstream);
    zstream_init(z, funcs);
    return Data_Wrap_Struct(klass, NULL, NULL, z);
}

#define zstream_deflate_new(klass)  zstream_new((klass), &deflate_funcs)
#define zstream_inflate_new(klass)  zstream_new((klass), &inflate_funcs)

static struct zstream *
get_zstream(VALUE obj)
{
    struct zstream *z;

    Data_Get_Struct(obj, struct zstream, z);
    if (!ZSTREAM_IS_READY(z)) {
	rb_raise(cZError, "stream is not ready");
    }
    return z;
}


/* ------------------------------------------------------------------------- */

/*
 * Document-class: Zlib::ZStream
 *
 * Zlib::ZStream is the abstract class for the stream which handles the
 * compressed data. The operations are defined in the subclasses:
 * Zlib::Deflate for compression, and Zlib::Inflate for decompression.
 *
 * An instance of Zlib::ZStream has one stream (struct zstream in the source)
 * and two variable-length buffers which associated to the input (next_in) of
 * the stream and the output (next_out) of the stream. In this document,
 * "input buffer" means the buffer for input, and "output buffer" means the
 * buffer for output.
 *
 * Data input into an instance of Zlib::ZStream are temporally stored into
 * the end of input buffer, and then data in input buffer are processed from
 * the beginning of the buffer until no more output from the stream is
 * produced (i.e. until avail_out > 0 after processing).  During processing,
 * output buffer is allocated and expanded automatically to hold all output
 * data.
 *
 * Some particular instance methods consume the data in output buffer and
 * return them as a String.
 *
 * Here is an ascii art for describing above:
 *
 *    +================ an instance of Zlib::ZStream ================+
 *    ||                                                            ||
 *    ||     +--------+          +-------+          +--------+      ||
 *    ||  +--| output |<---------|zstream|<---------| input  |<--+  ||
 *    ||  |  | buffer |  next_out+-------+next_in   | buffer |   |  ||
 *    ||  |  +--------+                             +--------+   |  ||
 *    ||  |                                                      |  ||
 *    +===|======================================================|===+
 *        |                                                      |
 *        v                                                      |
 *    "output data"                                         "input data"
 *
 * If an error occurs during processing input buffer, an exception which is a
 * subclass of Zlib::Error is raised.  At that time, both input and output
 * buffer keep their conditions at the time when the error occurs.
 *
 * == Method Catalogue
 *
 * Many of the methods in this class are fairly low-level and unlikely to be
 * of interest to users.  In fact, users are unlikely to use this class
 * directly; rather they will be interested in Zlib::Inflate and
 * Zlib::Deflate.
 *
 * The higher level methods are listed below.
 *
 * - #total_in
 * - #total_out
 * - #data_type
 * - #adler
 * - #reset
 * - #finish
 * - #finished?
 * - #close
 * - #closed?
 */

/*
 * Closes the stream. All operations on the closed stream will raise an
 * exception.
 */
static VALUE
rb_zstream_end(VALUE obj, SEL sel)
{
    zstream_end(get_zstream(obj));
    return Qnil;
}

/*
 * Resets and initializes the stream. All data in both input and output buffer
 * are discarded.
 */
static VALUE
rb_zstream_reset(VALUE obj, SEL sel)
{
    zstream_reset(get_zstream(obj));
    return Qnil;
}

/*
 * Finishes the stream and flushes output buffer. See Zlib::Deflate#finish and
 * Zlib::Inflate#finish for details of this behavior.
 */
static VALUE
rb_zstream_finish(VALUE obj, SEL sel)
{
    struct zstream *z = get_zstream(obj);
    VALUE dst;

    zstream_run(z, (Bytef*)"", 0, Z_FINISH);
    dst = zstream_detach_buffer(z);

    OBJ_INFECT(dst, obj);
    return dst;
}

/*
 * Flushes input buffer and returns all data in that buffer.
 */
static VALUE
rb_zstream_flush_next_in(VALUE obj, SEL sel)
{
    struct zstream *z;
    VALUE dst;

    Data_Get_Struct(obj, struct zstream, z);
    dst = zstream_detach_input(z);
    OBJ_INFECT(dst, obj);
    return dst;
}

/*
 * Flushes output buffer and returns all data in that buffer.
 */
static VALUE
rb_zstream_flush_next_out(VALUE obj, SEL sel)
{
    struct zstream *z;
    VALUE dst;

    Data_Get_Struct(obj, struct zstream, z);
    dst = zstream_detach_buffer(z);
    OBJ_INFECT(dst, obj);
    return dst;
}

/*
 * Returns number of bytes of free spaces in output buffer.  Because the free
 * space is allocated automatically, this method returns 0 normally.
 */
static VALUE
rb_zstream_avail_out(VALUE obj, SEL sel)
{
    struct zstream *z;
    Data_Get_Struct(obj, struct zstream, z);
    return rb_uint2inum(z->stream.avail_out);
}

/*
 * Allocates +size+ bytes of free space in the output buffer. If there are more
 * than +size+ bytes already in the buffer, the buffer is truncated. Because 
 * free space is allocated automatically, you usually don't need to use this
 * method.
 */
static VALUE
rb_zstream_set_avail_out(VALUE obj, SEL sel, VALUE size)
{
    struct zstream *z = get_zstream(obj);

    Check_Type(size, T_FIXNUM);
    zstream_expand_buffer_into(z, FIX2INT(size));
    return size;
}

/*
 * Returns bytes of data in the input buffer. Normally, returns 0.
 */
static VALUE
rb_zstream_avail_in(VALUE obj, SEL sel)
{
    struct zstream *z;
    Data_Get_Struct(obj, struct zstream, z);
    return INT2FIX(NIL_P(z->input) ? 0 : (int)(BSTRING_LEN(z->input)));
}

/*
 * Returns the total bytes of the input data to the stream.  FIXME
 */
static VALUE
rb_zstream_total_in(VALUE obj, SEL sel)
{
    return rb_uint2inum(get_zstream(obj)->stream.total_in);
}

/*
 * Returns the total bytes of the output data from the stream.  FIXME
 */
static VALUE
rb_zstream_total_out(VALUE obj, SEL sel)
{
    return rb_uint2inum(get_zstream(obj)->stream.total_out);
}

/*
 * Guesses the type of the data which have been inputed into the stream. The
 * returned value is either <tt>Zlib::BINARY</tt>, <tt>Zlib::ASCII</tt>, or
 * <tt>Zlib::UNKNOWN</tt>.
 */
static VALUE
rb_zstream_data_type(VALUE obj, SEL sel)
{
    return INT2FIX(get_zstream(obj)->stream.data_type);
}

/*
 * Returns the adler-32 checksum.
 */
static VALUE
rb_zstream_adler(VALUE obj, SEL sel)
{
	return rb_uint2inum(get_zstream(obj)->stream.adler);
}

/*
 * Returns true if the stream is finished.
 */
static VALUE
rb_zstream_finished_p(VALUE obj, SEL sel)
{
    return ZSTREAM_IS_FINISHED(get_zstream(obj)) ? Qtrue : Qfalse;
}

/*
 * Returns true if the stream is closed.
 */
static VALUE
rb_zstream_closed_p(VALUE obj, SEL sel)
{
    struct zstream *z;
    Data_Get_Struct(obj, struct zstream, z);
    return ZSTREAM_IS_READY(z) ? Qfalse : Qtrue;
}


/* ------------------------------------------------------------------------- */

/*
 * Document-class: Zlib::Deflate
 *
 * Zlib::Deflate is the class for compressing data.  See Zlib::Stream for more
 * information.
 */

#define FIXNUMARG(val, ifnil) \
    (NIL_P((val)) ? (ifnil) \
    : ((void)Check_Type((val), T_FIXNUM), FIX2INT((val))))

#define ARG_LEVEL(val)     FIXNUMARG((val), Z_DEFAULT_COMPRESSION)
#define ARG_WBITS(val)     FIXNUMARG((val), MAX_WBITS)
#define ARG_MEMLEVEL(val)  FIXNUMARG((val), DEF_MEM_LEVEL)
#define ARG_STRATEGY(val)  FIXNUMARG((val), Z_DEFAULT_STRATEGY)
#define ARG_FLUSH(val)     FIXNUMARG((val), Z_NO_FLUSH)


static VALUE
rb_deflate_s_allocate(VALUE klass, SEL sel)
{
    return zstream_deflate_new(klass);
}

/*
 * call-seq: Zlib::Deflate.new(level=nil, windowBits=nil, memlevel=nil, strategy=nil)
 *
 * Creates a new deflate stream for compression. See zlib.h for details of
 * each argument. If an argument is nil, the default value of that argument is
 * used.
 *
 * TODO: document better!
 */
static VALUE
rb_deflate_initialize(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct zstream *z;
    VALUE level, wbits, memlevel, strategy;
    int err;

    rb_scan_args(argc, argv, "04", &level, &wbits, &memlevel, &strategy);
    Data_Get_Struct(obj, struct zstream, z);
    zstream_init_deflate(z);
    err = deflateInit2(&z->stream, ARG_LEVEL(level), Z_DEFLATED,
		       ARG_WBITS(wbits), ARG_MEMLEVEL(memlevel),
		       ARG_STRATEGY(strategy));
    if (err != Z_OK) {
	raise_zlib_error(err, z->stream.msg);
    }
    ZSTREAM_READY(z);

    return obj;
}

/*
 * Duplicates the deflate stream.
 */
static VALUE
rb_deflate_init_copy(VALUE self, SEL sel, VALUE orig)
{
    struct zstream *z1, *z2;
    int err;

    Data_Get_Struct(self, struct zstream, z1);
    z2 = get_zstream(orig);

    err = deflateCopy(&z1->stream, &z2->stream);
    if (err != Z_OK) {
	raise_zlib_error(err, 0);
    }
    z1->input = NIL_P(z2->input) ? Qnil : rb_str_dup(z2->input);
    z1->buf   = NIL_P(z2->buf)   ? Qnil : rb_str_dup(z2->buf);
    z1->buf_filled = z2->buf_filled;
    z1->flags = z2->flags;

    return self;
}

static VALUE
deflate_run(VALUE args)
{
    struct zstream *z = (struct zstream*)((VALUE*)args)[0];
    VALUE src = ((VALUE*)args)[1];

    zstream_run(z, BSTRING_PTR_BYTEF(src), BSTRING_LEN(src), Z_FINISH);
    return zstream_detach_buffer(z);
}

/*
 * call-seq: Zlib::Deflate.deflate(string[, level])
 *
 * Compresses the given +string+. Valid values of level are
 * <tt>Zlib::NO_COMPRESSION</tt>, <tt>Zlib::BEST_SPEED</tt>,
 * <tt>Zlib::BEST_COMPRESSION</tt>, <tt>Zlib::DEFAULT_COMPRESSION</tt>, and an
 * integer from 0 to 9. The default is Zlib::DEFAULT_COMPRESSION
 *
 * This method is almost equivalent to the following code:
 *
 *   def deflate(string, level)
 *     z = Zlib::Deflate.new(level)
 *     dst = z.deflate(string, Zlib::FINISH)
 *     z.close
 *     dst
 *   end
 *
 *
 */
static VALUE
rb_deflate_s_deflate(VALUE klass, SEL sel, int argc, VALUE *argv)
{
    struct zstream *z;
    VALUE src, level, dst, args[2];
    int err, lev;

    rb_scan_args(argc, argv, "11", &src, &level);

    z = (struct zstream *)xmalloc(sizeof(struct zstream));

    lev = ARG_LEVEL(level);
    StringValue(src);
    src = rb_str_bstr(src);
    zstream_init_deflate(z);
    err = deflateInit(&z->stream, lev);
    if (err != Z_OK) {
	raise_zlib_error(err, z->stream.msg);
    }
    ZSTREAM_READY(z);

    args[0] = (VALUE)z;
    args[1] = src;
    dst = rb_ensure(deflate_run, (VALUE)args, zstream_end, (VALUE)z);

    OBJ_INFECT(dst, src);
    return dst;
}

static void
do_deflate(struct zstream *z, VALUE src, int flush)
{
    if (NIL_P(src)) {
	zstream_run(z, (Bytef*)"", 0, Z_FINISH);
	return;
    }
    StringValue(src);
    src = rb_str_bstr(src);
    if (flush != Z_NO_FLUSH || BSTRING_LEN(src) > 0) { /* prevent BUF_ERROR */
	zstream_run(z, BSTRING_PTR_BYTEF(src), BSTRING_LEN(src), flush);
    }
}

/*
 * call-seq: deflate(string[, flush])
 *
 * Inputs +string+ into the deflate stream and returns the output from the
 * stream.  On calling this method, both the input and the output buffers of
 * the stream are flushed. If +string+ is nil, this method finishes the
 * stream, just like Zlib::ZStream#finish.
 *
 * The value of +flush+ should be either <tt>Zlib::NO_FLUSH</tt>,
 * <tt>Zlib::SYNC_FLUSH</tt>, <tt>Zlib::FULL_FLUSH</tt>, or
 * <tt>Zlib::FINISH</tt>. See zlib.h for details.
 *
 * TODO: document better!
 */
static VALUE
rb_deflate_deflate(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct zstream *z = get_zstream(obj);
    VALUE src, flush, dst;

    rb_scan_args(argc, argv, "11", &src, &flush);
    OBJ_INFECT(obj, src);
    do_deflate(z, src, ARG_FLUSH(flush));
    dst = zstream_detach_buffer(z);

    OBJ_INFECT(dst, obj);
    return dst;
}

/*
 * call-seq: << string
 *
 * Inputs +string+ into the deflate stream just like Zlib::Deflate#deflate, but
 * returns the Zlib::Deflate object itself.  The output from the stream is
 * preserved in output buffer.
 */
static VALUE
rb_deflate_addstr(VALUE obj, SEL sel, VALUE src)
{
    OBJ_INFECT(obj, src);
    do_deflate(get_zstream(obj), src, Z_NO_FLUSH);
    return obj;
}

/*
 * call-seq: flush(flush)
 *
 * This method is equivalent to <tt>deflate('', flush)</tt>.  If flush is omitted,
 * <tt>Zlib::SYNC_FLUSH</tt> is used as flush.  This method is just provided
 * to improve the readability of your Ruby program.
 *
 * TODO: document better!
 */
static VALUE
rb_deflate_flush(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct zstream *z = get_zstream(obj);
    VALUE v_flush, dst;
    int flush;

    rb_scan_args(argc, argv, "01", &v_flush);
    flush = FIXNUMARG(v_flush, Z_SYNC_FLUSH);
    if (flush != Z_NO_FLUSH) {  /* prevent Z_BUF_ERROR */
	zstream_run(z, (Bytef*)"", 0, flush);
    }
    dst = zstream_detach_buffer(z);

    OBJ_INFECT(dst, obj);
    return dst;
}

/*
 * call-seq: params(level, strategy)
 * 
 * Changes the parameters of the deflate stream. See zlib.h for details. The
 * output from the stream by changing the params is preserved in output
 * buffer.
 *
 * TODO: document better!
 */
static VALUE
rb_deflate_params(VALUE obj, SEL sel, VALUE v_level, VALUE v_strategy)
{
    struct zstream *z = get_zstream(obj);
    int level, strategy;
    int err;
    uInt n;

    level = ARG_LEVEL(v_level);
    strategy = ARG_STRATEGY(v_strategy);

    n = z->stream.avail_out;
    err = deflateParams(&z->stream, level, strategy);
    z->buf_filled += n - z->stream.avail_out;
    while (err == Z_BUF_ERROR) {
	rb_warning("deflateParams() returned Z_BUF_ERROR");
	zstream_expand_buffer(z);
	n = z->stream.avail_out;
	err = deflateParams(&z->stream, level, strategy);
	z->buf_filled += n - z->stream.avail_out;
    }
    if (err != Z_OK) {
	raise_zlib_error(err, z->stream.msg);
    }

    return Qnil;
}

/*
 * call-seq: set_dictionary(string)
 *
 * Sets the preset dictionary and returns +string+. This method is available
 * just only after Zlib::Deflate.new or Zlib::ZStream#reset method was called.
 * See zlib.h for details.
 *
 * TODO: document better!
 */
static VALUE
rb_deflate_set_dictionary(VALUE obj, SEL sel, VALUE dic)
{
    struct zstream *z = get_zstream(obj);
    VALUE src = dic;
    int err;

    OBJ_INFECT(obj, dic);
    StringValue(src);
    src = rb_str_bstr(src);
    err = deflateSetDictionary(&z->stream,
			       BSTRING_PTR(src), BSTRING_LEN(src));
    if (err != Z_OK) {
	raise_zlib_error(err, z->stream.msg);
    }

    return dic;
}

/* ------------------------------------------------------------------------- */

/*
 * Document-class: Zlib::Inflate
 *
 * Zlib:Inflate is the class for decompressing compressed data.  Unlike
 * Zlib::Deflate, an instance of this class is not able to duplicate (clone,
 * dup) itself.
 */



static VALUE
rb_inflate_s_allocate(VALUE klass, SEL sel)
{
    return zstream_inflate_new(klass);
}

/*
 * call-seq: Zlib::Inflate.new(window_bits)
 *
 * Creates a new inflate stream for decompression. See zlib.h for details
 * of the argument.  If +window_bits+ is +nil+, the default value is used.
 *
 * TODO: document better!
 */
static VALUE
rb_inflate_initialize(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct zstream *z;
    VALUE wbits;
    int err;

    rb_scan_args(argc, argv, "01", &wbits);
    Data_Get_Struct(obj, struct zstream, z);

    err = inflateInit2(&z->stream, ARG_WBITS(wbits));
    if (err != Z_OK) {
	raise_zlib_error(err, z->stream.msg);
    }
    ZSTREAM_READY(z);

    return obj;
}

static VALUE
inflate_run(VALUE args)
{
    struct zstream *z = (struct zstream*)((VALUE*)args)[0];
    VALUE src = ((VALUE*)args)[1];

    zstream_run(z, BSTRING_PTR_BYTEF(src), BSTRING_LEN(src), Z_SYNC_FLUSH);
    zstream_run(z, (Bytef*)"", 0, Z_FINISH);  /* for checking errors */
    return zstream_detach_buffer(z);
}

/*
 * call-seq: Zlib::Inflate.inflate(string)
 *
 * Decompresses +string+. Raises a Zlib::NeedDict exception if a preset
 * dictionary is needed for decompression.
 *
 * This method is almost equivalent to the following code:
 *
 *   def inflate(string)
 *     zstream = Zlib::Inflate.new
 *     buf = zstream.inflate(string)
 *     zstream.finish
 *     zstream.close
 *     buf
 *   end
 *
 */
static VALUE
rb_inflate_s_inflate(VALUE obj, SEL sel, VALUE src)
{
    struct zstream *z;
    VALUE dst, args[2];
    int err;

    z = (struct zstream *)xmalloc(sizeof(struct zstream));

    StringValue(src);
    zstream_init_inflate(z);
    err = inflateInit(&z->stream);
    if (err != Z_OK) {
	raise_zlib_error(err, z->stream.msg);
    }
    ZSTREAM_READY(z);

    args[0] = (VALUE)z;
    args[1] = src;
    dst = rb_ensure(inflate_run, (VALUE)args, zstream_end, (VALUE)z);

    OBJ_INFECT(dst, src);
    return dst;
}

static void
do_inflate(struct zstream *z, VALUE src)
{
    if (NIL_P(src)) {
	zstream_run(z, (Bytef*)"", 0, Z_FINISH);
	return;
    }
    StringValue(src);
    if (BSTRING_LEN(src) > 0) { /* prevent Z_BUF_ERROR */
	zstream_run(z, BSTRING_PTR_BYTEF(src), BSTRING_LEN(src), Z_SYNC_FLUSH);
    }
}

/*
 * call-seq: inflate(string)
 *
 * Inputs +string+ into the inflate stream and returns the output from the
 * stream.  Calling this method, both the input and the output buffer of the
 * stream are flushed.  If string is +nil+, this method finishes the stream,
 * just like Zlib::ZStream#finish.
 *
 * Raises a Zlib::NeedDict exception if a preset dictionary is needed to
 * decompress.  Set the dictionary by Zlib::Inflate#set_dictionary and then
 * call this method again with an empty string.  (<i>???</i>)
 *
 * TODO: document better!
 */
static VALUE
rb_inflate_inflate(VALUE obj, SEL sel, VALUE src)
{
    struct zstream *z = get_zstream(obj);
    VALUE dst;

    OBJ_INFECT(obj, src);

    if (ZSTREAM_IS_FINISHED(z)) {
	if (NIL_P(src)) {
	    dst = zstream_detach_buffer(z);
	}
	else {
	    StringValue(src);
	    src = rb_str_bstr(src);
	    zstream_append_buffer2(z, src);
	    dst = rb_bstr_new();
	}
    }
    else {
	do_inflate(z, src);
	dst = zstream_detach_buffer(z);
	if (ZSTREAM_IS_FINISHED(z)) {
	    zstream_passthrough_input(z);
	}
    }

    OBJ_INFECT(dst, obj);
    return dst;
}

/*
 * call-seq: << string
 *
 * Inputs +string+ into the inflate stream just like Zlib::Inflate#inflate, but
 * returns the Zlib::Inflate object itself.  The output from the stream is
 * preserved in output buffer.
 */
static VALUE
rb_inflate_addstr(VALUE obj, SEL sel, VALUE src)
{
    struct zstream *z = get_zstream(obj);

    OBJ_INFECT(obj, src);

    if (ZSTREAM_IS_FINISHED(z)) {
	if (!NIL_P(src)) {
	    StringValue(src);
	    zstream_append_buffer2(z, src);
	}
    }
    else {
	do_inflate(z, src);
	if (ZSTREAM_IS_FINISHED(z)) {
	    zstream_passthrough_input(z);
	}
    }

    return obj;
}

/*
 * call-seq: sync(string)
 *
 * Inputs +string+ into the end of input buffer and skips data until a full
 * flush point can be found.  If the point is found in the buffer, this method
 * flushes the buffer and returns false.  Otherwise it returns +true+ and the
 * following data of full flush point is preserved in the buffer.
 */
static VALUE
rb_inflate_sync(VALUE obj, SEL sel, VALUE src)
{
    struct zstream *z = get_zstream(obj);

    OBJ_INFECT(obj, src);
    StringValue(src);
    return zstream_sync(z, BSTRING_PTR_BYTEF(src), BSTRING_LEN(src));
}

/*
 * Quoted verbatim from original documentation:
 *
 *   What is this?
 *
 * <tt>:)</tt>
 */
static VALUE
rb_inflate_sync_point_p(VALUE obj, SEL sel)
{
    struct zstream *z = get_zstream(obj);
    int err;

    err = inflateSyncPoint(&z->stream);
    if (err == 1) {
	return Qtrue;
    }
    if (err != Z_OK) {
	raise_zlib_error(err, z->stream.msg);
    }
    return Qfalse;
}

/*
 * Sets the preset dictionary and returns +string+.  This method is available just
 * only after a Zlib::NeedDict exception was raised.  See zlib.h for details.
 *
 * TODO: document better!
 */
static VALUE
rb_inflate_set_dictionary(VALUE obj, SEL sel, VALUE dic)
{
    struct zstream *z = get_zstream(obj);
    VALUE src = dic;
    int err;

    OBJ_INFECT(obj, dic);
    StringValue(src);
    err = inflateSetDictionary(&z->stream,
			       (Bytef*)BSTRING_PTR(src), BSTRING_LEN(src));
    if (err != Z_OK) {
	raise_zlib_error(err, z->stream.msg);
    }

    return dic;
}


#if GZIP_SUPPORT

/* NOTE: Features for gzip files of Ruby/zlib are written from scratch
 *       and using undocumented feature of zlib, negative wbits.
 *       I don't think gzFile APIs of zlib are good for Ruby.
 */

/*------- .gz file header --------*/

#define GZ_MAGIC1             0x1f
#define GZ_MAGIC2             0x8b
#define GZ_METHOD_DEFLATE     8
#define GZ_FLAG_MULTIPART     0x2
#define GZ_FLAG_EXTRA         0x4
#define GZ_FLAG_ORIG_NAME     0x8
#define GZ_FLAG_COMMENT       0x10
#define GZ_FLAG_ENCRYPT       0x20
#define GZ_FLAG_UNKNOWN_MASK  0xc0

#define GZ_EXTRAFLAG_FAST     0x4
#define GZ_EXTRAFLAG_SLOW     0x2

/* from zutil.h */
#define OS_MSDOS    0x00
#define OS_AMIGA    0x01
#define OS_VMS      0x02
#define OS_UNIX     0x03
#define OS_ATARI    0x05
#define OS_OS2      0x06
#define OS_MACOS    0x07
#define OS_TOPS20   0x0a
#define OS_WIN32    0x0b

#define OS_VMCMS    0x04
#define OS_ZSYSTEM  0x08
#define OS_CPM      0x09
#define OS_QDOS     0x0c
#define OS_RISCOS   0x0d
#define OS_UNKNOWN  0xff

#ifndef OS_CODE
#define OS_CODE  OS_UNIX
#endif

static ID id_write, id_read, id_readpartial, id_flush, id_seek, id_close;
static VALUE cGzError, cNoFooter, cCRCError, cLengthError;



/*-------- gzfile internal APIs --------*/

struct gzfile {
    struct zstream z;
    VALUE io;
    int level;
    time_t mtime;       /* for header */
    int os_code;        /* for header */
    VALUE orig_name;    /* for header; must be a String */
    VALUE comment;      /* for header; must be a String */
    unsigned long crc;
    int lineno;
    int ungetc;
    void (*end)(struct gzfile *);
};

#define GZFILE_FLAG_SYNC             ZSTREAM_FLAG_UNUSED
#define GZFILE_FLAG_HEADER_FINISHED  (ZSTREAM_FLAG_UNUSED << 1)
#define GZFILE_FLAG_FOOTER_FINISHED  (ZSTREAM_FLAG_UNUSED << 2)

#define GZFILE_IS_FINISHED(gz) \
    (ZSTREAM_IS_FINISHED(&gz->z) && (gz)->z.buf_filled == 0)

#define GZFILE_READ_SIZE  2048

#if 0 // XXX: register the finalizer!
static void
gzfile_free(struct gzfile *gz)
{
    struct zstream *z = &gz->z;

    if (ZSTREAM_IS_READY(z)) {
	if (z->func == &deflate_funcs) {
	    finalizer_warn("Zlib::GzipWriter object must be closed explicitly.");
	}
	zstream_finalize(z);
    }
    free(gz);
}
#endif

static VALUE
gzfile_new(klass, funcs, endfunc)
    VALUE klass;
    const struct zstream_funcs *funcs;
    void (*endfunc) _((struct gzfile *));
{
    VALUE obj;
    struct gzfile *gz;

    obj = Data_Make_Struct(klass, struct gzfile, NULL, NULL, gz);
    zstream_init(&gz->z, funcs);
    gz->io = Qnil;
    gz->level = 0;
    gz->mtime = 0;
    gz->os_code = OS_CODE;
    gz->orig_name = Qnil;
    gz->comment = Qnil;
    gz->crc = crc32(0, Z_NULL, 0);
    gz->lineno = 0;
    gz->ungetc = 0;
    gz->end = endfunc;

    return obj;
}

#define gzfile_writer_new(gz) gzfile_new((gz),&deflate_funcs,gzfile_writer_end)
#define gzfile_reader_new(gz) gzfile_new((gz),&inflate_funcs,gzfile_reader_end)

static void
gzfile_reset(struct gzfile *gz)
{
    zstream_reset(&gz->z);
    gz->crc = crc32(0, Z_NULL, 0);
    gz->lineno = 0;
    gz->ungetc = 0;
}

static void
gzfile_close(struct gzfile *gz, int closeflag)
{
    VALUE io = gz->io;

    gz->end(gz);    
    gz->io = Qnil;
    gz->orig_name = Qnil;
    gz->comment = Qnil;
    if (closeflag && rb_respond_to(io, id_close)) {
	rb_funcall(io, id_close, 0);
    }
}

static void
gzfile_write_raw(struct gzfile *gz)
{
    VALUE str;

    if (gz->z.buf_filled > 0) {
	str = zstream_detach_buffer(&gz->z);
	OBJ_TAINT(str);  /* for safe */
	rb_funcall(gz->io, id_write, 1, str);
	if ((gz->z.flags & GZFILE_FLAG_SYNC)
	    && rb_respond_to(gz->io, id_flush))
	    rb_funcall(gz->io, id_flush, 0);
    }
}

static VALUE
gzfile_read_raw_partial(VALUE arg)
{
    struct gzfile *gz = (struct gzfile*)arg;
    VALUE str;

    str = rb_funcall(gz->io, id_readpartial, 1, INT2FIX(GZFILE_READ_SIZE));
    Check_Type(str, T_STRING);
    return str;
}

static VALUE
gzfile_read_raw_rescue(VALUE arg, VALUE exc)
{
    struct gzfile *gz = (struct gzfile*)arg;
    VALUE str = Qnil;
    if (rb_obj_is_kind_of(exc, rb_eNoMethodError)) {
        str = rb_funcall(gz->io, id_read, 1, INT2FIX(GZFILE_READ_SIZE));
        if (!NIL_P(str)) {
            Check_Type(str, T_STRING);
        }
    }
    return str; /* return nil when EOFError */
}

static VALUE
gzfile_read_raw(struct gzfile *gz)
{
    return rb_rescue2(gzfile_read_raw_partial, (VALUE)gz,
                      gzfile_read_raw_rescue, (VALUE)gz,
                      rb_eEOFError, rb_eNoMethodError, (VALUE)0);
}

static int
gzfile_read_raw_ensure(struct gzfile *gz, int size)
{
    VALUE str;

    while (NIL_P(gz->z.input) || BSTRING_LEN(gz->z.input) < size) {
	str = gzfile_read_raw(gz);
	if (NIL_P(str)) return Qfalse;
	zstream_append_input2(&gz->z, str);
    }
    return Qtrue;
}

static char *
gzfile_read_raw_until_zero(struct gzfile *gz, long offset)
{
    VALUE str;
    char *p;

    for (;;) {
	p = memchr(BSTRING_PTR(gz->z.input) + offset, '\0',
		   BSTRING_LEN(gz->z.input) - offset);
	if (p) break;
	str = gzfile_read_raw(gz);
	if (NIL_P(str)) {
	    rb_raise(cGzError, "unexpected end of file");
	}
	offset = BSTRING_LEN(gz->z.input);
	zstream_append_input2(&gz->z, str);
    }
    return p;
}

static unsigned int
gzfile_get16(const unsigned char *src)
{
    unsigned int n;
    n  = *(src++) & 0xff;
    n |= (*(src++) & 0xff) << 8;
    return n;
}

static unsigned long
gzfile_get32(const unsigned char *src)
{
    unsigned long n;
    n  = *(src++) & 0xff;
    n |= (*(src++) & 0xff) << 8;
    n |= (*(src++) & 0xff) << 16;
    n |= (*(src++) & 0xffU) << 24;
    return n;
}

static void
gzfile_set32(unsigned long n, unsigned char *dst)
{
    *(dst++) = n & 0xff;
    *(dst++) = (n >> 8) & 0xff;
    *(dst++) = (n >> 16) & 0xff;
    *dst     = (n >> 24) & 0xff;
}

static void
gzfile_make_header(struct gzfile *gz)
{
    Bytef buf[10];  /* the size of gzip header */
    unsigned char flags = 0, extraflags = 0;

    if (!NIL_P(gz->orig_name)) {
	flags |= GZ_FLAG_ORIG_NAME;
    }
    if (!NIL_P(gz->comment)) {
	flags |= GZ_FLAG_COMMENT;
    }
    if (gz->mtime == 0) {
	gz->mtime = time(0);
    }

    if (gz->level == Z_BEST_SPEED) {
	extraflags |= GZ_EXTRAFLAG_FAST;
    }
    else if (gz->level == Z_BEST_COMPRESSION) {
	extraflags |= GZ_EXTRAFLAG_SLOW;
    }

    buf[0] = GZ_MAGIC1;
    buf[1] = GZ_MAGIC2;
    buf[2] = GZ_METHOD_DEFLATE;
    buf[3] = flags;
    gzfile_set32(gz->mtime, &buf[4]);
    buf[8] = extraflags;
    buf[9] = gz->os_code;
    zstream_append_buffer(&gz->z, buf, sizeof(buf));

    if (!NIL_P(gz->orig_name)) {
	zstream_append_buffer2(&gz->z, gz->orig_name);
	zstream_append_buffer(&gz->z, (Bytef*)"\0", 1);
    }
    if (!NIL_P(gz->comment)) {
	zstream_append_buffer2(&gz->z, gz->comment);
	zstream_append_buffer(&gz->z, (Bytef*)"\0", 1);
    }

    gz->z.flags |= GZFILE_FLAG_HEADER_FINISHED;
}

static void
gzfile_make_footer(struct gzfile *gz)
{
    Bytef buf[8];  /* 8 is the size of gzip footer */

    gzfile_set32(gz->crc, buf);
    gzfile_set32(gz->z.stream.total_in, &buf[4]);
    zstream_append_buffer(&gz->z, buf, sizeof(buf));
    gz->z.flags |= GZFILE_FLAG_FOOTER_FINISHED;
}

static void
gzfile_read_header(struct gzfile *gz)
{
    const unsigned char *head;
    long len;
    char flags, *p;

    if (!gzfile_read_raw_ensure(gz, 10)) {  /* 10 is the size of gzip header */
	rb_raise(cGzError, "not in gzip format");
    }

    head = (unsigned char*)BSTRING_PTR(gz->z.input);

    if (head[0] != GZ_MAGIC1 || head[1] != GZ_MAGIC2) {
	rb_raise(cGzError, "not in gzip format");
    }
    if (head[2] != GZ_METHOD_DEFLATE) {
	rb_raise(cGzError, "unsupported compression method %d", head[2]);
    }

    flags = head[3];
    if (flags & GZ_FLAG_MULTIPART) {
	rb_raise(cGzError, "multi-part gzip file is not supported");
    }
    else if (flags & GZ_FLAG_ENCRYPT) {
	rb_raise(cGzError, "encrypted gzip file is not supported");
    }
    else if (flags & GZ_FLAG_UNKNOWN_MASK) {
	rb_raise(cGzError, "unknown flags 0x%02x", flags);
    }

    if (head[8] & GZ_EXTRAFLAG_FAST) {
	gz->level = Z_BEST_SPEED;
    }
    else if (head[8] & GZ_EXTRAFLAG_SLOW) {
	gz->level = Z_BEST_COMPRESSION;
    }
    else {
	gz->level = Z_DEFAULT_COMPRESSION;
    }

    gz->mtime = gzfile_get32(&head[4]);
    gz->os_code = head[9];
    zstream_discard_input(&gz->z, 10);

    if (flags & GZ_FLAG_EXTRA) {
	if (!gzfile_read_raw_ensure(gz, 2)) {
	    rb_raise(cGzError, "unexpected end of file");
	}
	len = gzfile_get16((BSTRING_PTR_BYTEF(gz->z.input)));
	if (!gzfile_read_raw_ensure(gz, 2 + len)) {
	    rb_raise(cGzError, "unexpected end of file");
	}
	zstream_discard_input(&gz->z, 2 + len);
    }
    if (flags & GZ_FLAG_ORIG_NAME) {
	p = gzfile_read_raw_until_zero(gz, 0);
	len = p - (char*)BSTRING_PTR(gz->z.input);
	gz->orig_name = rb_str_new((char*)BSTRING_PTR(gz->z.input), len);
	OBJ_TAINT(gz->orig_name);  /* for safe */
	zstream_discard_input(&gz->z, len + 1);
    }
    if (flags & GZ_FLAG_COMMENT) {
	p = gzfile_read_raw_until_zero(gz, 0);
	len = p - (char*)BSTRING_PTR(gz->z.input);
	gz->comment = rb_str_new((char*)BSTRING_PTR(gz->z.input), len);
	OBJ_TAINT(gz->comment);  /* for safe */
	zstream_discard_input(&gz->z, len + 1);
    }

    if (gz->z.input != Qnil && BSTRING_LEN(gz->z.input) > 0) {
	zstream_run(&gz->z, 0, 0, Z_SYNC_FLUSH);
    }
}

static void
gzfile_check_footer(struct gzfile *gz)
{
    unsigned long crc, length;

    gz->z.flags |= GZFILE_FLAG_FOOTER_FINISHED;

    if (!gzfile_read_raw_ensure(gz, 8)) { /* 8 is the size of gzip footer */
	rb_raise(cNoFooter, "footer is not found");
    }

    crc = gzfile_get32(BSTRING_PTR_BYTEF(gz->z.input));
    length = gzfile_get32(BSTRING_PTR_BYTEF(gz->z.input) + 4);

    gz->z.stream.total_in += 8;  /* to rewind correctly */
    zstream_discard_input(&gz->z, 8);

    if (gz->crc != crc) {
	rb_raise(cCRCError, "invalid compressed data -- crc error");
    }
    if (gz->z.stream.total_out != length) {
	rb_raise(cLengthError, "invalid compressed data -- length error");
    }
}

static void
gzfile_write(struct gzfile *gz, Bytef *str, uInt len)
{
    if (!(gz->z.flags & GZFILE_FLAG_HEADER_FINISHED)) {
	gzfile_make_header(gz);
    }

    if (len > 0 || (gz->z.flags & GZFILE_FLAG_SYNC)) {
	gz->crc = crc32(gz->crc, str, len);
	zstream_run(&gz->z, str, len, (gz->z.flags & GZFILE_FLAG_SYNC)
		    ? Z_SYNC_FLUSH : Z_NO_FLUSH);
    }
    gzfile_write_raw(gz);
}

static long
gzfile_read_more(struct gzfile *gz)
{
    volatile VALUE str;

    while (!ZSTREAM_IS_FINISHED(&gz->z)) {
	str = gzfile_read_raw(gz);
	if (NIL_P(str)) {
	    if (!ZSTREAM_IS_FINISHED(&gz->z)) {
		rb_raise(cGzError, "unexpected end of file");
	    }
	    break;
	}
	if (BSTRING_LEN(str) > 0) { /* prevent Z_BUF_ERROR */
	    zstream_run(&gz->z, BSTRING_PTR_BYTEF(str), BSTRING_LEN(str),
			Z_SYNC_FLUSH);
	}
	if (gz->z.buf_filled > 0) break;
    }
    return gz->z.buf_filled;
}

static void
gzfile_calc_crc(struct gzfile *gz, VALUE str)
{
    if (BSTRING_LEN(str) <= gz->ungetc) {
	gz->ungetc -= BSTRING_LEN(str);
    }
    else {
	gz->crc = crc32(gz->crc, BSTRING_PTR_BYTEF(str) + gz->ungetc,
			BSTRING_LEN(str) - gz->ungetc);
	gz->ungetc = 0;
    }
}

static VALUE
gzfile_read(struct gzfile *gz, int len)
{
    VALUE dst;

    if (len < 0)
        rb_raise(rb_eArgError, "negative length %d given", len);
    if (len == 0)
	return rb_bstr_new();
    while (!ZSTREAM_IS_FINISHED(&gz->z) && gz->z.buf_filled < len) {
	gzfile_read_more(gz);
    }
    if (GZFILE_IS_FINISHED(gz)) {
	if (!(gz->z.flags & GZFILE_FLAG_FOOTER_FINISHED)) {
	    gzfile_check_footer(gz);
	}
	return Qnil;
    }

    dst = zstream_shift_buffer(&gz->z, len);
    gzfile_calc_crc(gz, dst);

    OBJ_TAINT(dst);  /* for safe */
    return dst;
}

static VALUE
gzfile_readpartial(struct gzfile *gz, int len, VALUE outbuf)
{
    VALUE dst;

    if (len < 0)
        rb_raise(rb_eArgError, "negative length %d given", len);

    if (!NIL_P(outbuf))
            OBJ_TAINT(outbuf);

    if (len == 0) {
        if (NIL_P(outbuf))
            return rb_bstr_new();
        else {
            rb_bstr_resize(outbuf, 0);
            return outbuf;
        }
    }
    while (!ZSTREAM_IS_FINISHED(&gz->z) && gz->z.buf_filled == 0) {
	gzfile_read_more(gz);
    }
    if (GZFILE_IS_FINISHED(gz)) {
	if (!(gz->z.flags & GZFILE_FLAG_FOOTER_FINISHED)) {
	    gzfile_check_footer(gz);
	}
        if (!NIL_P(outbuf))
            rb_str_resize(outbuf, 0);
	rb_raise(rb_eEOFError, "end of file reached");
    }

    dst = zstream_shift_buffer(&gz->z, len);
    gzfile_calc_crc(gz, dst);

    if (NIL_P(outbuf)) {
        OBJ_TAINT(dst);  /* for safe */
        return dst;
    }
    else {
        rb_bstr_resize(outbuf, BSTRING_LEN(dst));
        UInt8 *buf = BSTRING_PTR(outbuf);
        memcpy(buf, BSTRING_PTR(dst), BSTRING_LEN(dst));
        return outbuf;
    }
}

static VALUE
gzfile_read_all(struct gzfile *gz)
{
    VALUE dst;

    while (!ZSTREAM_IS_FINISHED(&gz->z)) {
	gzfile_read_more(gz);
    }
    if (GZFILE_IS_FINISHED(gz)) {
	if (!(gz->z.flags & GZFILE_FLAG_FOOTER_FINISHED)) {
	    gzfile_check_footer(gz);
	}
	return rb_bstr_new();
    }

    dst = zstream_detach_buffer(&gz->z);
    gzfile_calc_crc(gz, dst);

    OBJ_TAINT(dst);  /* for safe */
    return dst;
}

static void
gzfile_ungetc(struct gzfile *gz, int c)
{
    zstream_buffer_ungetc(&gz->z, c);
    gz->ungetc++;
}

static VALUE
gzfile_writer_end_run(VALUE arg)
{
    struct gzfile *gz = (struct gzfile *)arg;

    if (!(gz->z.flags & GZFILE_FLAG_HEADER_FINISHED)) {
	gzfile_make_header(gz);
    }

    zstream_run(&gz->z, (Bytef*)"", 0, Z_FINISH);
    gzfile_make_footer(gz);
    gzfile_write_raw(gz);

    return Qnil;
}

static void
gzfile_writer_end(struct gzfile *gz)
{
    if (ZSTREAM_IS_CLOSING(&gz->z)) return;
    gz->z.flags |= ZSTREAM_FLAG_CLOSING;

    rb_ensure(gzfile_writer_end_run, (VALUE)gz, zstream_end, (VALUE)&gz->z);
}

static VALUE
gzfile_reader_end_run(VALUE arg)
{
    struct gzfile *gz = (struct gzfile *)arg;

    if (GZFILE_IS_FINISHED(gz)
	&& !(gz->z.flags & GZFILE_FLAG_FOOTER_FINISHED)) {
	gzfile_check_footer(gz);
    }

    return Qnil;
}

static void
gzfile_reader_end(struct gzfile *gz)
{
    if (ZSTREAM_IS_CLOSING(&gz->z)) return;
    gz->z.flags |= ZSTREAM_FLAG_CLOSING;

    rb_ensure(gzfile_reader_end_run, (VALUE)gz, zstream_end, (VALUE)&gz->z);
}

static void
gzfile_reader_rewind(struct gzfile *gz)
{
    long n;

    n = gz->z.stream.total_in;
    if (!NIL_P(gz->z.input)) {
	n += BSTRING_LEN(gz->z.input);
    }

    rb_funcall(gz->io, id_seek, 2, rb_int2inum(-n), INT2FIX(1));
    gzfile_reset(gz);
}

static VALUE
gzfile_reader_get_unused(struct gzfile *gz)
{
    VALUE str;

    if (!ZSTREAM_IS_READY(&gz->z)) return Qnil;
    if (!GZFILE_IS_FINISHED(gz)) return Qnil;
    if (!(gz->z.flags & GZFILE_FLAG_FOOTER_FINISHED)) {
	gzfile_check_footer(gz);
    }
    if (NIL_P(gz->z.input)) return Qnil;

    str = rb_str_dup(gz->z.input);
    OBJ_TAINT(str);  /* for safe */
    return str;
}

static struct gzfile *
get_gzfile(VALUE obj)
{
    struct gzfile *gz;

    Data_Get_Struct(obj, struct gzfile, gz);
    if (!ZSTREAM_IS_READY(&gz->z)) {
	rb_raise(cGzError, "closed gzip stream");
    }
    return gz;
}


/* ------------------------------------------------------------------------- */

/*
 * Document-class: Zlib::GzipFile
 *
 * Zlib::GzipFile is an abstract class for handling a gzip formatted
 * compressed file. The operations are defined in the subclasses,
 * Zlib::GzipReader for reading, and Zlib::GzipWriter for writing.
 *
 * GzipReader should be used by associating an IO, or IO-like, object.
 */


static VALUE
gzfile_ensure_close(VALUE obj)
{
    struct gzfile *gz;

    Data_Get_Struct(obj, struct gzfile, gz);
    if (ZSTREAM_IS_READY(&gz->z)) {
	gzfile_close(gz, 1);
    }
    return Qnil;
}

/*
 * See Zlib::GzipReader#wrap and Zlib::GzipWriter#wrap.
 */
static VALUE
rb_gzfile_s_wrap(VALUE klass, SEL sel, int argc, VALUE *argv)
{
    VALUE obj = rb_class_new_instance(argc, argv, klass);

    if (rb_block_given_p()) {
	return rb_ensure(rb_yield, obj, gzfile_ensure_close, obj);
    }
    else {
	return obj;
    }
}

/*
 * See Zlib::GzipReader#open and Zlib::GzipWriter#open.
 */
static VALUE
gzfile_s_open(int argc, VALUE *argv, VALUE klass, const char *mode)
{
    VALUE io, filename;

    if (argc < 1) {
	rb_raise(rb_eArgError, "wrong number of arguments (0 for 1)");
    }
    filename = argv[0];
    FilePathValue(filename);

    VALUE args[2];
    args[0] = filename;
    args[1] = rb_str_new2(mode);
    io = rb_class_new_instance(2, args, rb_cFile);

    argv[0] = io;
    return rb_gzfile_s_wrap(klass, 0, argc, argv);
}

/*
 * Same as IO.
 */
static VALUE
rb_gzfile_to_io(VALUE obj, SEL sel)
{
    return get_gzfile(obj)->io;
}

/*
 * Returns CRC value of the uncompressed data.
 */
static VALUE
rb_gzfile_crc(VALUE obj, SEL sel)
{
    return rb_uint2inum(get_gzfile(obj)->crc);
}

/*
 * Returns last modification time recorded in the gzip file header.
 */
static VALUE
rb_gzfile_mtime(VALUE obj, SEL sel)
{
    return rb_time_new(get_gzfile(obj)->mtime, (time_t)0);
}

/*
 * Returns compression level.
 */
static VALUE
rb_gzfile_level(VALUE obj, SEL sel)
{
    return INT2FIX(get_gzfile(obj)->level);
}

/*
 * Returns OS code number recorded in the gzip file header.
 */
static VALUE
rb_gzfile_os_code(VALUE obj, SEL sel)
{
    return INT2FIX(get_gzfile(obj)->os_code);
}

/*
 * Returns original filename recorded in the gzip file header, or +nil+ if
 * original filename is not present.
 */
static VALUE
rb_gzfile_orig_name(VALUE obj, SEL sel)
{
    VALUE str = get_gzfile(obj)->orig_name;
    if (!NIL_P(str)) {
	str = rb_str_dup(str);
    }
    OBJ_TAINT(str);  /* for safe */
    return str;
}

/*
 * Returns comments recorded in the gzip file header, or nil if the comments
 * is not present.
 */
static VALUE
rb_gzfile_comment(VALUE obj, SEL sel)
{
    VALUE str = get_gzfile(obj)->comment;
    if (!NIL_P(str)) {
	str = rb_str_dup(str);
    }
    OBJ_TAINT(str);  /* for safe */
    return str;
}

/*
 * ???
 */
static VALUE
rb_gzfile_lineno(VALUE obj, SEL sel)
{
    return INT2NUM(get_gzfile(obj)->lineno);
}

/*
 * ???
 */
static VALUE
rb_gzfile_set_lineno(VALUE obj, SEL sel, VALUE lineno)
{
    struct gzfile *gz = get_gzfile(obj);
    gz->lineno = NUM2INT(lineno);
    return lineno;
}

/*
 * ???
 */
static VALUE
rb_gzfile_set_mtime(VALUE obj, SEL sel, VALUE mtime)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE val;

    if (gz->z.flags & GZFILE_FLAG_HEADER_FINISHED) {
	rb_raise(cGzError, "header is already written");
    }

    if (FIXNUM_P(mtime)) {
	gz->mtime = FIX2INT(mtime);
    }
    else {
	val = rb_Integer(mtime);
	gz->mtime = FIXNUM_P(val) ? FIX2INT(val) : rb_big2ulong(val);
    }
    return mtime;
}

/*
 * ???
 */
static VALUE
rb_gzfile_set_orig_name(VALUE obj, SEL sel, VALUE str)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE s;
    char *p;

    if (gz->z.flags & GZFILE_FLAG_HEADER_FINISHED) {
	rb_raise(cGzError, "header is already written");
    }
    s = rb_str_dup(rb_str_to_str(str));
    p = memchr(RSTRING_PTR(s), '\0', RSTRING_LEN(s));
    if (p) {
	long beg = p - RSTRING_PTR(s);
	rb_str_delete(s, beg, RSTRING_LEN(s) - beg);
    }
    gz->orig_name = s;
    return str;
}

/*
 * ???
 */
static VALUE
rb_gzfile_set_comment(VALUE obj, SEL sel, VALUE str)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE s;
    char *p;

    if (gz->z.flags & GZFILE_FLAG_HEADER_FINISHED) {
	rb_raise(cGzError, "header is already written");
    }
    s = rb_str_dup(rb_str_to_str(str));
    p = memchr(RSTRING_PTR(s), '\0', RSTRING_LEN(s));
    if (p) {
	long beg = p - RSTRING_PTR(s);
	rb_str_delete(s, beg, RSTRING_LEN(s) - beg);
    }
    gz->comment = s;
    return str;
}

/*
 * Closes the GzipFile object. This method calls close method of the
 * associated IO object. Returns the associated IO object.
 */
static VALUE
rb_gzfile_close(VALUE obj, SEL sel)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE io;

    io = gz->io;
    gzfile_close(gz, 1);
    return io;
}

/*
 * Closes the GzipFile object. Unlike Zlib::GzipFile#close, this method never
 * calls the close method of the associated IO object. Returns the associated IO
 * object.
 */
static VALUE
rb_gzfile_finish(VALUE obj, SEL sel)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE io;

    io = gz->io;
    gzfile_close(gz, 0);
    return io;
}

/*
 * Same as IO.
 */
static VALUE
rb_gzfile_closed_p(VALUE obj, SEL sel)
{
    struct gzfile *gz;
    Data_Get_Struct(obj, struct gzfile, gz);
    return NIL_P(gz->io) ? Qtrue : Qfalse;
}

/*
 * ???
 */
static VALUE
rb_gzfile_eof_p(VALUE obj, SEL sel)
{
    struct gzfile *gz = get_gzfile(obj);
    return GZFILE_IS_FINISHED(gz) ? Qtrue : Qfalse;
}

/*
 * Same as IO.
 */
static VALUE
rb_gzfile_sync(VALUE obj, SEL sel)
{
    return (get_gzfile(obj)->z.flags & GZFILE_FLAG_SYNC) ? Qtrue : Qfalse;
}

/*
 * call-seq: sync = flag
 *
 * Same as IO.  If flag is +true+, the associated IO object must respond to the
 * +flush+ method.  While +sync+ mode is +true+, the compression ratio
 * decreases sharply.
 */
static VALUE
rb_gzfile_set_sync(VALUE obj, SEL sel, VALUE mode)
{
    struct gzfile *gz = get_gzfile(obj);

    if (RTEST(mode)) {
	gz->z.flags |= GZFILE_FLAG_SYNC;
    }
    else {
	gz->z.flags &= ~GZFILE_FLAG_SYNC;
    }
    return mode;
}

/*
 * ???
 */
static VALUE
rb_gzfile_total_in(VALUE obj, SEL sel)
{
    return rb_uint2inum(get_gzfile(obj)->z.stream.total_in);
}

/*
 * ???
 */
static VALUE
rb_gzfile_total_out(VALUE obj, SEL sel)
{
    struct gzfile *gz = get_gzfile(obj);
    return rb_uint2inum(gz->z.stream.total_out - gz->z.buf_filled);
}


/* ------------------------------------------------------------------------- */

/*
 * Document-class: Zlib::GzipWriter
 *
 * Zlib::GzipWriter is a class for writing gzipped files.  GzipWriter should
 * be used with an instance of IO, or IO-like, object. 
 *
 * For example:
 *
 *   Zlib::GzipWriter.open('hoge.gz') do |gz|
 *     gz.write 'jugemu jugemu gokou no surikire...'
 *   end
 *
 *   File.open('hoge.gz', 'w') do |f|
 *     gz = Zlib::GzipWriter.new(f)
 *     gz.write 'jugemu jugemu gokou no surikire...'
 *     gz.close
 *   end
 *
 *   # TODO: test these.  Are they equivalent?  Can GzipWriter.new take a
 *   # block?
 *
 * NOTE: Due to the limitation of Ruby's finalizer, you must explicitly close
 * GzipWriter objects by Zlib::GzipWriter#close etc.  Otherwise, GzipWriter
 * will be not able to write the gzip footer and will generate a broken gzip
 * file.
 */

static VALUE
rb_gzwriter_s_allocate(VALUE klass, SEL sel)
{
    return gzfile_writer_new(klass);
}

/*
 * call-seq: Zlib::GzipWriter.open(filename, level=nil, strategy=nil) { |gz| ... }
 *
 * Opens a file specified by +filename+ for writing gzip compressed data, and
 * returns a GzipWriter object associated with that file.  Further details of
 * this method are found in Zlib::GzipWriter.new and Zlib::GzipWriter#wrap.
 */
static VALUE
rb_gzwriter_s_open(VALUE klass, SEL sel, int argc, VALUE *argv)
{
    return gzfile_s_open(argc, argv, klass, "wb");
}

/*
 * call-seq: Zlib::GzipWriter.new(io, level, strategy)
 *
 * Creates a GzipWriter object associated with +io+. +level+ and +strategy+
 * should be the same as the arguments of Zlib::Deflate.new.  The GzipWriter
 * object writes gzipped data to +io+.  At least, +io+ must respond to the
 * +write+ method that behaves same as write method in IO class.
 */
static VALUE
rb_gzwriter_initialize(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct gzfile *gz;
    VALUE io, level, strategy;
    int err;

    rb_scan_args(argc, argv, "12", &io, &level, &strategy);
    Data_Get_Struct(obj, struct gzfile, gz);

    /* this is undocumented feature of zlib */
    gz->level = ARG_LEVEL(level);
    err = deflateInit2(&gz->z.stream, gz->level, Z_DEFLATED,
		       -MAX_WBITS, DEF_MEM_LEVEL, ARG_STRATEGY(strategy));
    if (err != Z_OK) {
	raise_zlib_error(err, gz->z.stream.msg);
    }
    gz->io = io;
    ZSTREAM_READY(&gz->z);

    return obj;
}

/*
 * call-seq: flush(flush=nil)
 *
 * Flushes all the internal buffers of the GzipWriter object.  The meaning of
 * +flush+ is same as in Zlib::Deflate#deflate.  <tt>Zlib::SYNC_FLUSH</tt> is used if
 * +flush+ is omitted.  It is no use giving flush <tt>Zlib::NO_FLUSH</tt>.
 */
static VALUE
rb_gzwriter_flush(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE v_flush;
    int flush;

    rb_scan_args(argc, argv, "01", &v_flush);

    flush = FIXNUMARG(v_flush, Z_SYNC_FLUSH);
    if (flush != Z_NO_FLUSH) {  /* prevent Z_BUF_ERROR */
	zstream_run(&gz->z, (Bytef*)"", 0, flush);
    }

    gzfile_write_raw(gz);
    if (rb_respond_to(gz->io, id_flush)) {
	rb_funcall(gz->io, id_flush, 0);
    }
    return obj;
}

/*
 * Same as IO.
 */
static VALUE
rb_gzwriter_write(VALUE obj, SEL sel, VALUE str)
{
    struct gzfile *gz = get_gzfile(obj);

    if (TYPE(str) != T_STRING) {
	str = rb_obj_as_string(str);
    }
    else {
	StringValue(str);
	str = rb_str_bstr(str);
    }
    gzfile_write(gz, BSTRING_PTR_BYTEF(str), BSTRING_LEN(str));
    return INT2FIX(BSTRING_LEN(str));
}

/*
 * Same as IO.
 */
static VALUE
rb_gzwriter_putc(VALUE obj, SEL sel, VALUE ch)
{
    struct gzfile *gz = get_gzfile(obj);
    char c = NUM2CHR(ch);

    gzfile_write(gz, (Bytef*)&c, 1);
    return ch;
}



/*
 * Document-method: <<
 * Same as IO.
 */
#define rb_gzwriter_addstr  rb_io_addstr
/*
 * Document-method: printf
 * Same as IO.
 */
#define rb_gzwriter_printf  rb_io_printf
/*
 * Document-method: print
 * Same as IO.
 */
#define rb_gzwriter_print  rb_io_print
/*
 * Document-method: puts
 * Same as IO.
 */
#define rb_gzwriter_puts  rb_io_puts


/* ------------------------------------------------------------------------- */

/*
 * Document-class: Zlib::GzipReader
 *
 * Zlib::GzipReader is the class for reading a gzipped file.  GzipReader should
 * be used an IO, or -IO-lie, object.
 *
 *   Zlib::GzipReader.open('hoge.gz') {|gz|
 *     print gz.read
 *   }
 *
 *   File.open('hoge.gz') do |f|
 *     gz = Zlib::GzipReader.new(f)
 *     print gz.read
 *     gz.close
 *   end
 *
 *   # TODO: test these.  Are they equivalent?  Can GzipReader.new take a
 *   # block?
 *
 * == Method Catalogue
 *
 * The following methods in Zlib::GzipReader are just like their counterparts
 * in IO, but they raise Zlib::Error or Zlib::GzipFile::Error exception if an
 * error was found in the gzip file.
 * - #each
 * - #each_line
 * - #each_byte
 * - #gets
 * - #getc
 * - #lineno
 * - #lineno=
 * - #read
 * - #readchar
 * - #readline
 * - #readlines
 * - #ungetc
 *
 * Be careful of the footer of the gzip file. A gzip file has the checksum of
 * pre-compressed data in its footer. GzipReader checks all uncompressed data
 * against that checksum at the following cases, and if it fails, raises
 * <tt>Zlib::GzipFile::NoFooter</tt>, <tt>Zlib::GzipFile::CRCError</tt>, or
 * <tt>Zlib::GzipFile::LengthError</tt> exception.
 *
 * - When an reading request is received beyond the end of file (the end of
 *   compressed data). That is, when Zlib::GzipReader#read,
 *   Zlib::GzipReader#gets, or some other methods for reading returns nil.
 * - When Zlib::GzipFile#close method is called after the object reaches the
 *   end of file.
 * - When Zlib::GzipReader#unused method is called after the object reaches
 *   the end of file.
 *
 * The rest of the methods are adequately described in their own
 * documentation.
 */

static VALUE
rb_gzreader_s_allocate(VALUE klass, SEL sel)
{
    return gzfile_reader_new(klass);
}

/*
 * call-seq: Zlib::GzipReader.open(filename) {|gz| ... }
 *
 * Opens a file specified by +filename+ as a gzipped file, and returns a
 * GzipReader object associated with that file.  Further details of this method
 * are in Zlib::GzipReader.new and ZLib::GzipReader.wrap.
 */
static VALUE
rb_gzreader_s_open(VALUE klass, SEL sel, int argc, VALUE *argv)
{
    return gzfile_s_open(argc, argv, klass, "rb");
}

/*
 * call-seq: Zlib::GzipReader.new(io)
 *
 * Creates a GzipReader object associated with +io+. The GzipReader object reads
 * gzipped data from +io+, and parses/decompresses them.  At least, +io+ must have
 * a +read+ method that behaves same as the +read+ method in IO class.
 *
 * If the gzip file header is incorrect, raises an Zlib::GzipFile::Error
 * exception.
 */
static VALUE
rb_gzreader_initialize(VALUE obj, SEL sel, VALUE io)
{
    struct gzfile *gz;
    int err;

    Data_Get_Struct(obj, struct gzfile, gz);

    /* this is undocumented feature of zlib */
    err = inflateInit2(&gz->z.stream, -MAX_WBITS);
    if (err != Z_OK) {
	raise_zlib_error(err, gz->z.stream.msg);
    }
    GC_WB(&gz->io, io);
    ZSTREAM_READY(&gz->z);
    gzfile_read_header(gz);

    return obj;
}

/*
 * Resets the position of the file pointer to the point created the GzipReader
 * object.  The associated IO object needs to respond to the +seek+ method.
 */
static VALUE
rb_gzreader_rewind(VALUE obj, SEL sel)
{
    struct gzfile *gz = get_gzfile(obj);
    gzfile_reader_rewind(gz);
    return INT2FIX(0);
}

/*
 * Returns the rest of the data which had read for parsing gzip format, or
 * +nil+ if the whole gzip file is not parsed yet.
 */
static VALUE
rb_gzreader_unused(VALUE obj, SEL sel)
{
    struct gzfile *gz;
    Data_Get_Struct(obj, struct gzfile, gz);
    return gzfile_reader_get_unused(gz);
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_read(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE vlen;
    int len;

    rb_scan_args(argc, argv, "01", &vlen);
    if (NIL_P(vlen)) {
	return gzfile_read_all(gz);
    }

    len = NUM2INT(vlen);
    if (len < 0) {
	rb_raise(rb_eArgError, "negative length %d given", len);
    }
    return gzfile_read(gz, len);
}

/*
 *  call-seq:
 *     gzipreader.readpartial(maxlen [, outbuf]) => string, outbuf
 *
 *  Reads at most <i>maxlen</i> bytes from the gziped stream but
 *  it blocks only if <em>gzipreader</em> has no data immediately available.
 *  If the optional <i>outbuf</i> argument is present,
 *  it must reference a String, which will receive the data.
 *  It raises <code>EOFError</code> on end of file.
 */
static VALUE
rb_gzreader_readpartial(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE vlen, outbuf;
    int len;

    rb_scan_args(argc, argv, "11", &vlen, &outbuf);

    len = NUM2INT(vlen);
    if (len < 0) {
	rb_raise(rb_eArgError, "negative length %d given", len);
    }
    if (!NIL_P(outbuf))
        Check_Type(outbuf, T_STRING);
    return gzfile_readpartial(gz, len, outbuf);
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_getc(VALUE obj, SEL sel)
{
    struct gzfile *gz = get_gzfile(obj);

    return gzfile_read(gz, 1);
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_readchar(VALUE obj, SEL sel)
{
    VALUE dst;
    dst = rb_gzreader_getc(obj, 0);
    if (NIL_P(dst)) {
	rb_raise(rb_eEOFError, "end of file reached");
    }
    return dst;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_getbyte(VALUE obj, SEL sel)
{
    struct gzfile *gz = get_gzfile(obj);
    VALUE dst;

    dst = gzfile_read(gz, 1);
    if (!NIL_P(dst)) {
	dst = INT2FIX((unsigned int)(BSTRING_PTR(dst)[0]) & 0xff);
    }
    return dst;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_readbyte(VALUE obj, SEL sel)
{
    VALUE dst;
    dst = rb_gzreader_getbyte(obj, 0);
    if (NIL_P(dst)) {
        rb_raise(rb_eEOFError, "end of file reached");
    }
    return dst;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_each_char(VALUE obj, SEL sel)
{
    VALUE c;

    RETURN_ENUMERATOR(obj, 0, 0);

    while (!NIL_P(c = rb_gzreader_getc(obj, 0))) {
        rb_yield(c);
    }
    return Qnil;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_each_byte(VALUE obj, SEL sel)
{
    VALUE c;

    RETURN_ENUMERATOR(obj, 0, 0);

    while (!NIL_P(c = rb_gzreader_getbyte(obj, 0))) {
	rb_yield(c);
    }
    return Qnil;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_ungetc(VALUE obj, SEL sel, VALUE ch)
{
    struct gzfile *gz = get_gzfile(obj);
    gzfile_ungetc(gz, NUM2CHR(ch));
    return Qnil;
}

static void
gzreader_skip_linebreaks(struct gzfile *gz)
{
    VALUE str;
    char *p;
    int n;

    while (gz->z.buf_filled == 0) {
	if (GZFILE_IS_FINISHED(gz)) return;
	gzfile_read_more(gz);
    }
    n = 0;
    p = (char*)BSTRING_PTR(gz->z.buf);

    while (n++, *(p++) == '\n') {
	if (n >= gz->z.buf_filled) {
	    str = zstream_detach_buffer(&gz->z);
	    gzfile_calc_crc(gz, str);
	    while (gz->z.buf_filled == 0) {
		if (GZFILE_IS_FINISHED(gz)) return;
		gzfile_read_more(gz);
	    }
	    n = 0;
	    p = (char*)BSTRING_PTR(gz->z.buf);
	}
    }

    str = zstream_shift_buffer(&gz->z, n - 1);
    gzfile_calc_crc(gz, str);
}

static void
rscheck(const char *rsptr, long rslen, VALUE rs)
{
    if ((const char*)RSTRING_PTR(rs) != rsptr && RSTRING_LEN(rs) != rslen)
	rb_raise(rb_eRuntimeError, "rs modified");
}

static VALUE
gzreader_gets(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    struct gzfile *gz = get_gzfile(obj);
    volatile VALUE rs;
    VALUE dst;
    const char *rsptr;
    char *p, *res;
    long rslen, n;
    int rspara;

    if (argc == 0) {
	rs = rb_rs;
    }
    else {
	rb_scan_args(argc, argv, "1", &rs);
	if (!NIL_P(rs)) {
	    Check_Type(rs, T_STRING);
	}
    }

    if (NIL_P(rs)) {
	dst = gzfile_read_all(gz);
	if (BSTRING_LEN(dst) != 0) gz->lineno++;
	else
	    return Qnil;
	return dst;
    }

    if (RSTRING_LEN(rs) == 0) {
	rsptr = "\n\n";
	rslen = 2;
	rspara = 1;
    } else {
	rsptr = (const char*)RSTRING_PTR(rs);
	rslen = RSTRING_LEN(rs);
	rspara = 0;
    }

    if (rspara) {
	gzreader_skip_linebreaks(gz);
    }

    while (gz->z.buf_filled < rslen) {
	if (ZSTREAM_IS_FINISHED(&gz->z)) {
	    if (gz->z.buf_filled > 0) gz->lineno++;
	    return gzfile_read(gz, rslen);
	}
	gzfile_read_more(gz);
    }

    p = (char*)BSTRING_PTR(gz->z.buf);
    n = rslen;
    for (;;) {
	if (n > gz->z.buf_filled) {
	    if (ZSTREAM_IS_FINISHED(&gz->z)) break;
	    gzfile_read_more(gz);
	    p = (char*)BSTRING_PTR(gz->z.buf) + n - rslen;
	}
	if (!rspara) rscheck(rsptr, rslen, rs);
	res = memchr(p, rsptr[0], (gz->z.buf_filled - n + 1));
	if (!res) {
	    n = gz->z.buf_filled + 1;
	} else {
	    n += (long)(res - p);
	    p = res;
	    if (rslen == 1 || memcmp(p, rsptr, rslen) == 0) break;
	    p++, n++;
	}
    }

    gz->lineno++;
    dst = gzfile_read(gz, n);
    if (rspara) {
	gzreader_skip_linebreaks(gz);
    }

    return dst;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_gets(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    VALUE dst;
    dst = gzreader_gets(obj, 0, argc, argv);
    if (!NIL_P(dst)) {
	rb_lastline_set(dst);
    }
    return dst;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_readline(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    VALUE dst;
    dst = rb_gzreader_gets(obj, 0, argc, argv);
    if (NIL_P(dst)) {
	rb_raise(rb_eEOFError, "end of file reached");
    }
    return dst;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_each(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    VALUE str;

    RETURN_ENUMERATOR(obj, 0, 0);

    while (!NIL_P(str = gzreader_gets(obj, 0, argc, argv))) {
	rb_yield(str);
    }
    return obj;
}

/*
 * See Zlib::GzipReader documentation for a description.
 */
static VALUE
rb_gzreader_readlines(VALUE obj, SEL sel, int argc, VALUE *argv)
{
    VALUE str, dst;
    dst = rb_ary_new();
    while (!NIL_P(str = gzreader_gets(obj, 0, argc, argv))) {
	rb_ary_push(dst, str);
    }
    return dst;
}

#endif /* GZIP_SUPPORT */


/*
 * The Zlib module contains several classes for compressing and decompressing
 * streams, and for working with "gzip" files.
 *
 * == Classes
 *
 * Following are the classes that are most likely to be of interest to the
 * user:
 * Zlib::Inflate
 * Zlib::Deflate
 * Zlib::GzipReader
 * Zlib::GzipWriter
 *
 * There are two important base classes for the classes above: Zlib::ZStream
 * and Zlib::GzipFile.  Everything else is an error class.
 *
 * == Constants
 *
 * Here's a list.
 *
 *   Zlib::VERSION
 *       The Ruby/zlib version string.
 *
 *   Zlib::ZLIB_VERSION
 *       The string which represents the version of zlib.h.
 *
 *   Zlib::BINARY
 *   Zlib::ASCII
 *   Zlib::UNKNOWN
 *       The integers representing data types which Zlib::ZStream#data_type
 *       method returns.
 *
 *   Zlib::NO_COMPRESSION
 *   Zlib::BEST_SPEED
 *   Zlib::BEST_COMPRESSION
 *   Zlib::DEFAULT_COMPRESSION
 *       The integers representing compression levels which are an argument
 *       for Zlib::Deflate.new, Zlib::Deflate#deflate, and so on.
 *
 *   Zlib::FILTERED
 *   Zlib::HUFFMAN_ONLY
 *   Zlib::DEFAULT_STRATEGY
 *       The integers representing compression methods which are an argument
 *       for Zlib::Deflate.new and Zlib::Deflate#params.
 *
 *   Zlib::DEF_MEM_LEVEL
 *   Zlib::MAX_MEM_LEVEL
 *       The integers representing memory levels which are an argument for
 *       Zlib::Deflate.new, Zlib::Deflate#params, and so on.
 *
 *   Zlib::MAX_WBITS
 *       The default value of windowBits which is an argument for
 *       Zlib::Deflate.new and Zlib::Inflate.new.
 *
 *   Zlib::NO_FLUSH
 *   Zlib::SYNC_FLUSH
 *   Zlib::FULL_FLUSH
 *   Zlib::FINISH
 *       The integers to control the output of the deflate stream, which are
 *       an argument for Zlib::Deflate#deflate and so on.
 *
 *   Zlib::OS_CODE
 *   Zlib::OS_MSDOS
 *   Zlib::OS_AMIGA
 *   Zlib::OS_VMS
 *   Zlib::OS_UNIX
 *   Zlib::OS_VMCMS
 *   Zlib::OS_ATARI
 *   Zlib::OS_OS2
 *   Zlib::OS_MACOS
 *   Zlib::OS_ZSYSTEM
 *   Zlib::OS_CPM
 *   Zlib::OS_TOPS20
 *   Zlib::OS_WIN32
 *   Zlib::OS_QDOS
 *   Zlib::OS_RISCOS
 *   Zlib::OS_UNKNOWN
 *       The return values of Zlib::GzipFile#os_code method.
 */
void Init_zlib()
{
    VALUE mZlib, cZStream, cDeflate, cInflate;
#if GZIP_SUPPORT
    VALUE cGzipFile, cGzipWriter, cGzipReader;
#endif

    mZlib = rb_define_module("Zlib");

    cZError = rb_define_class_under(mZlib, "Error", rb_eStandardError);
    cStreamEnd    = rb_define_class_under(mZlib, "StreamEnd", cZError);
    cNeedDict     = rb_define_class_under(mZlib, "NeedDict", cZError);
    cDataError    = rb_define_class_under(mZlib, "DataError", cZError);
    cStreamError  = rb_define_class_under(mZlib, "StreamError", cZError);
    cMemError     = rb_define_class_under(mZlib, "MemError", cZError);
    cBufError     = rb_define_class_under(mZlib, "BufError", cZError);
    cVersionError = rb_define_class_under(mZlib, "VersionError", cZError);
    
    rb_objc_define_method(*(VALUE *)mZlib, "zlib_version", rb_zlib_version, 0);
    rb_objc_define_method(*(VALUE *)mZlib, "adler32", rb_zlib_adler32, -1);
    rb_objc_define_method(*(VALUE *)mZlib, "crc32", rb_zlib_crc32, -1);
    rb_objc_define_method(*(VALUE *)mZlib, "crc_table", rb_zlib_crc_table, 0);

    rb_define_const(mZlib, "VERSION", rb_str_new2(RUBY_ZLIB_VERSION));
    rb_define_const(mZlib, "ZLIB_VERSION", rb_str_new2(ZLIB_VERSION));

    cZStream = rb_define_class_under(mZlib, "ZStream", rb_cObject);
    rb_undef_alloc_func(cZStream);
    rb_objc_define_method(cZStream, "avail_out", rb_zstream_avail_out, 0);
    rb_objc_define_method(cZStream, "avail_out=", rb_zstream_set_avail_out, 1);
    rb_objc_define_method(cZStream, "avail_in", rb_zstream_avail_in, 0);
    rb_objc_define_method(cZStream, "total_in", rb_zstream_total_in, 0);
    rb_objc_define_method(cZStream, "total_out", rb_zstream_total_out, 0);
    rb_objc_define_method(cZStream, "data_type", rb_zstream_data_type, 0);
    rb_objc_define_method(cZStream, "adler", rb_zstream_adler, 0);
    rb_objc_define_method(cZStream, "finished?", rb_zstream_finished_p, 0);
    rb_objc_define_method(cZStream, "stream_end?", rb_zstream_finished_p, 0);
    rb_objc_define_method(cZStream, "closed?", rb_zstream_closed_p, 0);
    rb_objc_define_method(cZStream, "ended?", rb_zstream_closed_p, 0);
    rb_objc_define_method(cZStream, "close", rb_zstream_end, 0);
    rb_objc_define_method(cZStream, "end", rb_zstream_end, 0);
    rb_objc_define_method(cZStream, "reset", rb_zstream_reset, 0);
    rb_objc_define_method(cZStream, "finish", rb_zstream_finish, 0);
    rb_objc_define_method(cZStream, "flush_next_in", rb_zstream_flush_next_in, 0);
    rb_objc_define_method(cZStream, "flush_next_out", rb_zstream_flush_next_out, 0);

    rb_define_const(mZlib, "BINARY", INT2FIX(Z_BINARY));
    rb_define_const(mZlib, "ASCII", INT2FIX(Z_ASCII));
    rb_define_const(mZlib, "UNKNOWN", INT2FIX(Z_UNKNOWN));
    cDeflate = rb_define_class_under(mZlib, "Deflate", cZStream);
    rb_objc_define_method(*(VALUE *)cDeflate, "deflate", rb_deflate_s_deflate, -1);
    rb_objc_define_method(*(VALUE *)cDeflate, "alloc", rb_deflate_s_allocate, 0);
    rb_objc_define_method(cDeflate, "initialize", rb_deflate_initialize, -1);
    rb_objc_define_method(cDeflate, "initialize_copy", rb_deflate_init_copy, 1);
    rb_objc_define_method(cDeflate, "deflate", rb_deflate_deflate, -1);
    rb_objc_define_method(cDeflate, "<<", rb_deflate_addstr, 1);
    rb_objc_define_method(cDeflate, "flush", rb_deflate_flush, -1);
    rb_objc_define_method(cDeflate, "params", rb_deflate_params, 2);
    rb_objc_define_method(cDeflate, "set_dictionary", rb_deflate_set_dictionary, 1);

    cInflate = rb_define_class_under(mZlib, "Inflate", cZStream);
    rb_objc_define_method(*(VALUE *)cInflate, "inflate", rb_inflate_s_inflate, 1);
    rb_objc_define_method(*(VALUE *)cInflate, "alloc", rb_inflate_s_allocate, 0);
    rb_objc_define_method(cInflate, "initialize", rb_inflate_initialize, -1);
    rb_objc_define_method(cInflate, "inflate", rb_inflate_inflate, 1);
    rb_objc_define_method(cInflate, "<<", rb_inflate_addstr, 1);
    rb_objc_define_method(cInflate, "sync", rb_inflate_sync, 1);
    rb_objc_define_method(cInflate, "sync_point?", rb_inflate_sync_point_p, 0);
    rb_objc_define_method(cInflate, "set_dictionary", rb_inflate_set_dictionary, 1);

    rb_define_const(mZlib, "NO_COMPRESSION", INT2FIX(Z_NO_COMPRESSION));
    rb_define_const(mZlib, "BEST_SPEED", INT2FIX(Z_BEST_SPEED));
    rb_define_const(mZlib, "BEST_COMPRESSION", INT2FIX(Z_BEST_COMPRESSION));
    rb_define_const(mZlib, "DEFAULT_COMPRESSION",
		    INT2FIX(Z_DEFAULT_COMPRESSION));

    rb_define_const(mZlib, "FILTERED", INT2FIX(Z_FILTERED));
    rb_define_const(mZlib, "HUFFMAN_ONLY", INT2FIX(Z_HUFFMAN_ONLY));
    rb_define_const(mZlib, "DEFAULT_STRATEGY", INT2FIX(Z_DEFAULT_STRATEGY));

    rb_define_const(mZlib, "MAX_WBITS", INT2FIX(MAX_WBITS));
    rb_define_const(mZlib, "DEF_MEM_LEVEL", INT2FIX(DEF_MEM_LEVEL));
    rb_define_const(mZlib, "MAX_MEM_LEVEL", INT2FIX(MAX_MEM_LEVEL));

    rb_define_const(mZlib, "NO_FLUSH", INT2FIX(Z_NO_FLUSH));
    rb_define_const(mZlib, "SYNC_FLUSH", INT2FIX(Z_SYNC_FLUSH));
    rb_define_const(mZlib, "FULL_FLUSH", INT2FIX(Z_FULL_FLUSH));
    rb_define_const(mZlib, "FINISH", INT2FIX(Z_FINISH));

#if GZIP_SUPPORT
    id_write = rb_intern("write");
    id_read = rb_intern("read");
    id_readpartial = rb_intern("readpartial");
    id_flush = rb_intern("flush");
    id_seek = rb_intern("seek");
    id_close = rb_intern("close");

    cGzipFile = rb_define_class_under(mZlib, "GzipFile", rb_cObject);
    cGzError = rb_define_class_under(cGzipFile, "Error", cZError);

    cNoFooter = rb_define_class_under(cGzipFile, "NoFooter", cGzError);
    cCRCError = rb_define_class_under(cGzipFile, "CRCError", cGzError);
    cLengthError = rb_define_class_under(cGzipFile,"LengthError",cGzError);

    cGzipWriter = rb_define_class_under(mZlib, "GzipWriter", cGzipFile);
    cGzipReader = rb_define_class_under(mZlib, "GzipReader", cGzipFile);
    rb_include_module(cGzipReader, rb_mEnumerable);

    rb_objc_define_method(*(VALUE *)cGzipFile, "wrap", rb_gzfile_s_wrap, -1);
    rb_undef_alloc_func(cGzipFile);
    rb_objc_define_method(cGzipFile, "to_io", rb_gzfile_to_io, 0);
    rb_objc_define_method(cGzipFile, "crc", rb_gzfile_crc, 0);
    rb_objc_define_method(cGzipFile, "mtime", rb_gzfile_mtime, 0);
    rb_objc_define_method(cGzipFile, "level", rb_gzfile_level, 0);
    rb_objc_define_method(cGzipFile, "os_code", rb_gzfile_os_code, 0);
    rb_objc_define_method(cGzipFile, "orig_name", rb_gzfile_orig_name, 0);
    rb_objc_define_method(cGzipFile, "comment", rb_gzfile_comment, 0);
    rb_objc_define_method(cGzipReader, "lineno", rb_gzfile_lineno, 0);
    rb_objc_define_method(cGzipReader, "lineno=", rb_gzfile_set_lineno, 1);
    rb_objc_define_method(cGzipWriter, "mtime=", rb_gzfile_set_mtime, 1);
    rb_objc_define_method(cGzipWriter, "orig_name=", rb_gzfile_set_orig_name,1);
    rb_objc_define_method(cGzipWriter, "comment=", rb_gzfile_set_comment, 1);
    rb_objc_define_method(cGzipFile, "close", rb_gzfile_close, 0);
    rb_objc_define_method(cGzipFile, "finish", rb_gzfile_finish, 0);
    rb_objc_define_method(cGzipFile, "closed?", rb_gzfile_closed_p, 0);
    rb_objc_define_method(cGzipReader, "eof", rb_gzfile_eof_p, 0);
    rb_objc_define_method(cGzipReader, "eof?", rb_gzfile_eof_p, 0);
    rb_objc_define_method(cGzipFile, "sync", rb_gzfile_sync, 0);
    rb_objc_define_method(cGzipFile, "sync=", rb_gzfile_set_sync, 1);
    rb_objc_define_method(cGzipReader, "pos", rb_gzfile_total_out, 0);
    rb_objc_define_method(cGzipWriter, "pos", rb_gzfile_total_in, 0);
    rb_objc_define_method(cGzipReader, "tell", rb_gzfile_total_out, 0);
    rb_objc_define_method(cGzipWriter, "tell", rb_gzfile_total_in, 0);

    rb_objc_define_method(*(VALUE *)cGzipWriter, "open", rb_gzwriter_s_open,-1);
    rb_objc_define_method(*(VALUE *)cGzipWriter, "alloc", rb_gzwriter_s_allocate, 0);
    rb_objc_define_method(cGzipWriter, "initialize", rb_gzwriter_initialize,-1);
    rb_objc_define_method(cGzipWriter, "flush", rb_gzwriter_flush, -1);
    rb_objc_define_method(cGzipWriter, "write", rb_gzwriter_write, 1);
    rb_objc_define_method(cGzipWriter, "putc", rb_gzwriter_putc, 1);
    rb_objc_define_method(cGzipWriter, "<<", rb_gzwriter_addstr, 1);
    rb_objc_define_method(cGzipWriter, "printf", rb_gzwriter_printf, -1);
    rb_objc_define_method(cGzipWriter, "print", rb_gzwriter_print, -1);
    rb_objc_define_method(cGzipWriter, "puts", rb_gzwriter_puts, -1);

    rb_objc_define_method(*(VALUE *)cGzipReader, "open", rb_gzreader_s_open,-1);
    rb_objc_define_method(*(VALUE *)cGzipReader, "alloc", rb_gzreader_s_allocate, 0);
    rb_objc_define_method(cGzipReader, "initialize", rb_gzreader_initialize, 1);
    rb_objc_define_method(cGzipReader, "rewind", rb_gzreader_rewind, 0);
    rb_objc_define_method(cGzipReader, "unused", rb_gzreader_unused, 0);
    rb_objc_define_method(cGzipReader, "read", rb_gzreader_read, -1);
    rb_objc_define_method(cGzipReader, "readpartial", rb_gzreader_readpartial, -1);
    rb_objc_define_method(cGzipReader, "getc", rb_gzreader_getc, 0);
    rb_objc_define_method(cGzipReader, "getbyte", rb_gzreader_getbyte, 0);
    rb_objc_define_method(cGzipReader, "readchar", rb_gzreader_readchar, 0);
    rb_objc_define_method(cGzipReader, "readbyte", rb_gzreader_readbyte, 0);
    rb_objc_define_method(cGzipReader, "each_byte", rb_gzreader_each_byte, 0);
    rb_objc_define_method(cGzipReader, "each_char", rb_gzreader_each_char, 0);
    rb_objc_define_method(cGzipReader, "bytes", rb_gzreader_each_byte, 0);
    rb_objc_define_method(cGzipReader, "ungetc", rb_gzreader_ungetc, 1);
    rb_objc_define_method(cGzipReader, "gets", rb_gzreader_gets, -1);
    rb_objc_define_method(cGzipReader, "readline", rb_gzreader_readline, -1);
    rb_objc_define_method(cGzipReader, "each", rb_gzreader_each, -1);
    rb_objc_define_method(cGzipReader, "each_line", rb_gzreader_each, -1);
    rb_objc_define_method(cGzipReader, "lines", rb_gzreader_each, -1);
    rb_objc_define_method(cGzipReader, "readlines", rb_gzreader_readlines, -1);

    rb_define_const(mZlib, "OS_CODE", INT2FIX(OS_CODE));
    rb_define_const(mZlib, "OS_MSDOS", INT2FIX(OS_MSDOS));
    rb_define_const(mZlib, "OS_AMIGA", INT2FIX(OS_AMIGA));
    rb_define_const(mZlib, "OS_VMS", INT2FIX(OS_VMS));
    rb_define_const(mZlib, "OS_UNIX", INT2FIX(OS_UNIX));
    rb_define_const(mZlib, "OS_ATARI", INT2FIX(OS_ATARI));
    rb_define_const(mZlib, "OS_OS2", INT2FIX(OS_OS2));
    rb_define_const(mZlib, "OS_MACOS", INT2FIX(OS_MACOS));
    rb_define_const(mZlib, "OS_TOPS20", INT2FIX(OS_TOPS20));
    rb_define_const(mZlib, "OS_WIN32", INT2FIX(OS_WIN32));

    rb_define_const(mZlib, "OS_VMCMS", INT2FIX(OS_VMCMS));
    rb_define_const(mZlib, "OS_ZSYSTEM", INT2FIX(OS_ZSYSTEM));
    rb_define_const(mZlib, "OS_CPM", INT2FIX(OS_CPM));
    rb_define_const(mZlib, "OS_QDOS", INT2FIX(OS_QDOS));
    rb_define_const(mZlib, "OS_RISCOS", INT2FIX(OS_RISCOS));
    rb_define_const(mZlib, "OS_UNKNOWN", INT2FIX(OS_UNKNOWN));

#endif /* GZIP_SUPPORT */

}

/* Document error classes. */

/*
 * Document-class: Zlib::Error
 *
 * The superclass for all exceptions raised by Ruby/zlib.
 *
 * The following exceptions are defined as subclasses of Zlib::Error. These
 * exceptions are raised when zlib library functions return with an error
 * status.
 *
 * - Zlib::StreamEnd
 * - Zlib::NeedDict
 * - Zlib::DataError
 * - Zlib::StreamError
 * - Zlib::MemError
 * - Zlib::BufError
 * - Zlib::VersionError
 *
 */

/*
 * Document-class: Zlib::GzipFile::Error
 *
 * Base class of errors that occur when processing GZIP files.
 */

/*
 * Document-class: Zlib::GzipFile::NoFooter
 *
 * Raised when gzip file footer is not found. 
 */

/*
 * Document-class: Zlib::GzipFile::CRCError
 *
 * Raised when the CRC checksum recorded in gzip file footer is not equivalent
 * to the CRC checksum of the actual uncompressed data. 
 */

/*
 * Document-class: Zlib::GzipFile::LengthError
 *
 * Raised when the data length recorded in the gzip file footer is not equivalent
 * to the length of the actual uncompressed data. 
 */



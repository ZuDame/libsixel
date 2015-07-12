/*
 * Copyright (c) 2014,2015 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#if HAVE_JPEG
# include <stdio.h>
# include <jpeglib.h>
#endif  /* HAVE_JPEG */

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if HAVE_IO_H
# include <io.h>
#endif

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(HAVE_MEMMOVE)
# define memmove(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(O_BINARY) && defined(_O_BINARY)
# define O_BINARY _O_BINARY
#endif  /* !defined(O_BINARY) && !defined(_O_BINARY) */

#include <assert.h>

#define STBI_NO_STDIO 1
#include "stb_image.h"

#ifdef HAVE_GDK_PIXBUF2
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wtypedef-redefinition"
# endif
# include <gdk-pixbuf/gdk-pixbuf.h>
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION
#   pragma GCC diagnostic pop
# endif
#endif

#ifdef HAVE_GD
# include <gd.h>
#endif

#ifdef HAVE_LIBCURL
# include <curl/curl.h>
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#ifdef HAVE_LIBPNG
# include <png.h>
#endif  /* HAVE_LIBPNG */

#include <stdio.h>
#include "frompnm.h"
#include "fromgif.h"
#include "frame.h"
#include <sixel.h>

#define STBI_NO_STDIO 1
#define STB_IMAGE_IMPLEMENTATION 1
#include "stb_image.h"


typedef struct chunk
{
    unsigned char *buffer;
    size_t size;
    size_t max_size;
} chunk_t;


static void
chunk_init(chunk_t * const pchunk,
           size_t initial_size)
{
    pchunk->max_size = initial_size;
    pchunk->size = 0;
    pchunk->buffer = (unsigned char *)malloc(pchunk->max_size);
}


# ifdef HAVE_LIBCURL
static size_t
memory_write(void *ptr,
             size_t size,
             size_t len,
             void *memory)
{
    size_t nbytes;
    chunk_t *chunk;

    if (ptr == NULL || memory == NULL) {
        return 0;
    }
    nbytes = size * len;
    if (nbytes == 0) {
        return 0;
    }

    chunk = (chunk_t *)memory;
    if (chunk->buffer == NULL) {
        return 0;
    }

    if (chunk->max_size <= chunk->size + nbytes) {
        do {
            chunk->max_size *= 2;
        } while (chunk->max_size <= chunk->size + nbytes);
        chunk->buffer = (unsigned char*)realloc(chunk->buffer, chunk->max_size);
        if (chunk->buffer == NULL) {
            return 0;
        }
    }

    memcpy(chunk->buffer + chunk->size, ptr, nbytes);
    chunk->size += nbytes;

    return nbytes;
}
# endif


static int
wait_file(int fd, int usec)
{
#if HAVE_SYS_SELECT_H
    fd_set rfds;
    struct timeval tv;
#endif  /* HAVE_SYS_SELECT_H */
    int ret = 1;

#if HAVE_SYS_SELECT_H
    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    ret = select(fd + 1, &rfds, NULL, NULL, &tv);
#else
    (void) fd;
    (void) usec;
#endif  /* HAVE_SYS_SELECT_H */
    if (ret == 0) {
        return (1);
    }
    if (ret < 0) {
        return ret;
    }

    return (0);
}


static SIXELSTATUS
open_binary_file(FILE **f, char const *filename)
{
    SIXELSTATUS status = SIXEL_FALSE;
    char buffer[1024];
#if HAVE_STAT
    struct stat sb;
#endif  /* HAVE_STAT */

    if (filename == NULL || strcmp(filename, "-") == 0) {
        /* for windows */
#if defined(O_BINARY)
# if HAVE__SETMODE
        _setmode(fileno(stdin), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdin), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* defined(O_BINARY) */
        *f = stdin;

        status = SIXEL_OK;
        goto end;
    }

#if HAVE_STAT
    if (stat(filename, &sb) != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        if (sprintf(buffer, "stat('%s') failed.", filename) != EOF) {
            sixel_helper_set_additional_message(buffer);
        }
        goto end;
    }
    if ((sb.st_mode & S_IFMT) == S_IFDIR) {
        status = SIXEL_BAD_INPUT;
        if (sprintf(buffer, "'%s' is directory.", filename) != EOF) {
            sixel_helper_set_additional_message(buffer);
        }
        goto end;
    }
#endif  /* HAVE_STAT */

    *f = fopen(filename, "rb");
    if (!*f) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        if (sprintf(buffer, "fopen('%s') failed.", filename) != EOF) {
            sixel_helper_set_additional_message(buffer);
        }
        goto end;
    }

    status = SIXEL_OK;
end:
    return status;
}


static SIXELSTATUS
get_chunk_from_file(
    char const *filename,
    chunk_t *pchunk,
    int const *cancel_flag
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int ret;
    FILE *f;
    int n;

    status = open_binary_file(&f, filename);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    chunk_init(pchunk, 64 * 1024);
    if (pchunk->buffer == NULL) {
        sixel_helper_set_additional_message(
            "get_chunk_from_file: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (;;) {
        if (pchunk->max_size - pchunk->size < 4096) {
            pchunk->max_size *= 2;
            pchunk->buffer = (unsigned char *)realloc(pchunk->buffer,
                                                      pchunk->max_size);
            if (pchunk->buffer == NULL) {
                sixel_helper_set_additional_message(
                    "get_chunk_from_file: realloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }

        if (isatty(fileno(f))) {
            for (;;) {
                if (*cancel_flag) {
                    status = SIXEL_INTERRUPTED;
                    goto end;
                }
                ret = wait_file(fileno(f), 10000);
                if (ret < 0) {
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                if (ret == 0) {
                    break;
                }
            }
        }
        n = fread(pchunk->buffer + pchunk->size, 1, 4096, f);
        if (n <= 0) {
            break;
        }
        pchunk->size += n;
    }

    if (f != stdin) {
        fclose(f);
    }

    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
get_chunk_from_url(
    char const *url,
    chunk_t *pchunk,
    int finsecure)
{
    SIXELSTATUS status = SIXEL_FALSE;

# ifdef HAVE_LIBCURL
    CURL *curl;
    CURLcode code;
    char buffer[1024];

    chunk_init(pchunk, 1024);
    if (pchunk->buffer == NULL) {
        sixel_helper_set_additional_message(
            "get_chunk_from_url: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (finsecure && strncmp(url, "https://", 8) == 0) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pchunk);
    code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        status = SIXEL_CURL_ERROR & (code & 0xff);
        if (sprintf(buffer, "curl_easy_perform('%s') failed.", url) != EOF) {
            sixel_helper_set_additional_message(buffer);
        }
        curl_easy_cleanup(curl);
        goto end;
    }
    curl_easy_cleanup(curl);

    status = SIXEL_OK;
# else
    (void) url;
    (void) pchunk;
    (void) finsecure;
    sixel_helper_set_additional_message(
        "To specify URI schemes, you have to "
        "configure this program with --with-libcurl "
        "option at compile time.\n");
    status = SIXEL_NOT_IMPLEMENTED;
    goto end;
# endif  /* HAVE_LIBCURL */

end:
    return status;
}


# if HAVE_JPEG
/* import from @uobikiemukot's sdump loader.h */
static SIXELSTATUS
load_jpeg(unsigned char **result,
          unsigned char *data,
          int datasize,
          int *pwidth,
          int *pheight,
          int *ppixelformat)
{
    SIXELSTATUS status = SIXEL_JPEG_ERROR;
    int row_stride, size;
    JSAMPARRAY buffer;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr pub;

    cinfo.err = jpeg_std_error(&pub);

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, datasize);
    jpeg_read_header(&cinfo, TRUE);

    /* disable colormap (indexed color), grayscale -> rgb */
    cinfo.quantize_colors = FALSE;
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    if (cinfo.output_components != 3) {
        sixel_helper_set_additional_message(
            "load_jpeg: unknown pixel format.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    *pwidth = cinfo.output_width;
    *pheight = cinfo.output_height;

    size = *pwidth * *pheight * cinfo.output_components;
    *result = (unsigned char *)malloc(size);
    if (*result == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "load_jpeg: malloc() failed.");
        goto end;
    }
    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(*result + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
    }

    status = SIXEL_OK;

end:
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return status;
}
# endif  /* HAVE_JPEG */


# if HAVE_LIBPNG
static void
read_png(png_structp png_ptr,
         png_bytep data,
         png_size_t length)
{
    chunk_t *pchunk = png_get_io_ptr(png_ptr);
    if (length > pchunk->size) {
        length = pchunk->size;
    }
    if (length > 0) {
        memcpy(data, pchunk->buffer, length);
        pchunk->buffer += length;
        pchunk->size -= length;
    }
}


static void
read_palette(png_structp png_ptr,
             png_infop info_ptr,
             unsigned char *palette,
             int ncolors,
             png_color *png_palette,
             png_color_16 *pbackground,
             int *transparent)
{
    png_bytep trans = NULL;
    int num_trans = 0;
    int i;

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
    }
    if (num_trans > 0) {
        *transparent = trans[0];
    }
    for (i = 0; i < ncolors; ++i) {
        if (pbackground && i < num_trans) {
            palette[i * 3 + 0] = ((0xff - trans[i]) * pbackground->red
                                   + trans[i] * png_palette[i].red) >> 8;
            palette[i * 3 + 1] = ((0xff - trans[i]) * pbackground->green
                                   + trans[i] * png_palette[i].green) >> 8;
            palette[i * 3 + 2] = ((0xff - trans[i]) * pbackground->blue
                                   + trans[i] * png_palette[i].blue) >> 8;
        } else {
            palette[i * 3 + 0] = png_palette[i].red;
            palette[i * 3 + 1] = png_palette[i].green;
            palette[i * 3 + 2] = png_palette[i].blue;
        }
    }
}


static SIXELSTATUS
load_png(unsigned char **result,
         unsigned char *buffer,
         int size,
         int *psx,
         int *psy,
         unsigned char **ppalette,
         int *pncolors,
         int reqcolors,
         int *pixelformat,
         unsigned char *bgcolor,
         int *transparent)
{
    SIXELSTATUS status = SIXEL_FALSE;
    chunk_t read_chunk;
    png_uint_32 bitdepth;
    png_uint_32 png_status;
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned char **rows = NULL;
    png_color *png_palette = NULL;
    png_color_16 background;
    png_color_16p default_background;
    int i;
    int depth;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        sixel_helper_set_additional_message(
            "png_create_read_struct() failed.");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        sixel_helper_set_additional_message(
            "png_create_info_struct() failed.");
        status = SIXEL_PNG_ERROR;
        png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
        goto cleanup;
    }
    read_chunk.buffer = buffer;
    read_chunk.size = size;

    png_set_read_fn(png_ptr,(png_voidp)&read_chunk, read_png);
    png_read_info(png_ptr, info_ptr);
    *psx = png_get_image_width(png_ptr, info_ptr);
    *psy = png_get_image_height(png_ptr, info_ptr);
    bitdepth = png_get_bit_depth(png_ptr, info_ptr);
    if (bitdepth == 16) {
#  if HAVE_DEBUG
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "stripping to 8bit...\n");
#  endif
        png_set_strip_16(png_ptr);
        bitdepth = 8;
    }

    if (bgcolor) {
        background.red = bgcolor[0];
        background.green = bgcolor[1];
        background.blue = bgcolor[2];
        background.gray = (bgcolor[0] + bgcolor[1] + bgcolor[2]) / 3;
    } else if (png_get_bKGD(png_ptr, info_ptr, &default_background) == PNG_INFO_bKGD) {
        memcpy(&background, default_background, sizeof(background));
    } else {
        background.red = 0;
        background.green = 0;
        background.blue = 0;
        background.gray = 0;
    }

    switch (png_get_color_type(png_ptr, info_ptr)) {
    case PNG_COLOR_TYPE_PALETTE:
#  if HAVE_DEBUG
        fprintf(stderr, "paletted PNG(PNG_COLOR_TYPE_PALETTE)\n");
#  endif
        png_status = png_get_PLTE(png_ptr, info_ptr,
                                  &png_palette, pncolors);
        if (png_status != PNG_INFO_PLTE || png_palette == NULL) {
            sixel_helper_set_additional_message(
                "PLTE chunk not found");
            status = SIXEL_PNG_ERROR;
            goto cleanup;
        }
#  if HAVE_DEBUG
        fprintf(stderr, "palette colors: %d\n", *pncolors);
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (ppalette == NULL || *pncolors > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than reqired(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand to RGB format...\n");
#  endif
            png_set_background(png_ptr, &background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
            png_set_palette_to_rgb(png_ptr);
            png_set_strip_alpha(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
                *ppalette = malloc(*pncolors * 3);
                if (*ppalette == NULL) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL1;
                break;
            case 2:
                *ppalette = malloc(*pncolors * 3);
                if (*ppalette == NULL) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL2;
                break;
            case 4:
                *ppalette = malloc(*pncolors * 3);
                if (*ppalette == NULL) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL4;
                break;
            case 8:
                *ppalette = malloc(*pncolors * 3);
                if (*ppalette == NULL) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL8;
                break;
            default:
                png_set_background(png_ptr, &background,
                                   PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                png_set_palette_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale PNG(PNG_COLOR_TYPE_GRAY)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (1 << bitdepth > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than reqired(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand into RGB format...\n");
#  endif
            png_set_background(png_ptr, &background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
            png_set_gray_to_rgb(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
            case 2:
            case 4:
                if (ppalette) {
#  if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_expand_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  else
#   if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
#   endif
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
#  endif
                } else {
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            case 8:
                if (ppalette) {
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
                } else {
#  if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
#  endif
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            default:
#  if HAVE_DEBUG
                fprintf(stderr, "expand into RGB format...\n");
#  endif
                png_set_background(png_ptr, &background,
                                   PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                png_set_gray_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale-alpha PNG(PNG_COLOR_TYPE_GRAY_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        png_set_gray_to_rgb(png_ptr);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "RGBA PNG(PNG_COLOR_TYPE_RGB_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB:
#  if HAVE_DEBUG
        fprintf(stderr, "RGB PNG(PNG_COLOR_TYPE_RGB)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    default:
        /* unknown format */
        goto cleanup;
    }
    depth = sixel_helper_compute_depth(*pixelformat);
    *result = malloc(*psx * *psy * depth);
    if (*result == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    rows = malloc(*psy * sizeof(unsigned char *));
    if (rows == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    switch (*pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
        for (i = 0; i < *psy; ++i) {
            rows[i] = *result + (depth * *psx * bitdepth + 7) / 8 * i;
        }
        break;
    default:
        for (i = 0; i < *psy; ++i) {
            rows[i] = *result + depth * *psx * i;
        }
        break;
    }
#if USE_SETJMP && HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr))) {
        free(*result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP */
    png_read_image(png_ptr, rows);

    status = SIXEL_OK;

cleanup:
    png_destroy_read_struct(&png_ptr, &info_ptr,(png_infopp)0);
    free(rows);

    return status;
}
# endif  /* HAVE_PNG */


static SIXELSTATUS
load_sixel(unsigned char **result,
           unsigned char *buffer,
           int size,
           int *psx,
           int *psy,
           unsigned char **ppalette,
           int *pncolors,
           int reqcolors,
           int *ppixelformat)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *p = NULL;
    unsigned char *palette = NULL;
    int colors;
    int i;

    /* sixel */
    status = sixel_decode(buffer, size,
                          &p, psx, psy,
                          &palette, &colors, malloc);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (ppalette == NULL || colors > reqcolors) {
        *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
        *result = malloc(*psx * *psy * 3);
        if (*result == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (i = 0; i < *psx * *psy; ++i) {
            (*result)[i * 3 + 0] = palette[p[i] * 3 + 0];
            (*result)[i * 3 + 1] = palette[p[i] * 3 + 1];
            (*result)[i * 3 + 2] = palette[p[i] * 3 + 2];
        }
    } else {
        *ppixelformat = SIXEL_PIXELFORMAT_PAL8;
        *result = p;
        *ppalette = palette;
        *pncolors = colors;
        p = NULL;
        palette = NULL;
    }

end:
    free(palette);
    free(p);

    return status;
}


static SIXELSTATUS
get_chunk(
    char const *filename,
    chunk_t *pchunk,
    int finsecure,
    int const *cancel_flag
)
{
    if (filename != NULL && strstr(filename, "://")) {
        return get_chunk_from_url(filename, pchunk, finsecure);
    }

    return get_chunk_from_file(filename, pchunk, cancel_flag);
}


static int
chunk_is_sixel(chunk_t const *chunk)
{
    unsigned char *p;
    unsigned char *end;

    p = chunk->buffer;
    end = p + chunk->size;

    if (chunk->size < 3) {
        return 0;
    }

    p++;
    if (p >= end) {
        return 0;
    }
    if (*(p - 1) == 0x90 || (*(p - 1) == 0x1b && *p == 0x50)) {
        while (p++ < end) {
            if (*p == 0x71) {
                return 1;
            } else if (*p == 0x18 || *p == 0x1a) {
                return 0;
            } else if (*p < 0x20) {
                continue;
            } else if (*p < 0x30) {
                return 0;
            } else if (*p < 0x40) {
                continue;
            }
        }
    }
    return 0;
}


static int
chunk_is_pnm(chunk_t const *chunk)
{
    if (chunk->size < 2) {
        return 0;
    }
    if (chunk->buffer[0] == 'P' &&
        chunk->buffer[1] >= '1' &&
        chunk->buffer[1] <= '6') {
        return 1;
    }
    return 0;
}


#if HAVE_LIBPNG
static int
chunk_is_png(chunk_t const *chunk)
{
    if (chunk->size < 8) {
        return 0;
    }
    if (png_check_sig(chunk->buffer, 8)) {
        return 1;
    }
    return 0;
}
#endif  /* HAVE_LIBPNG */


static int
chunk_is_gif(chunk_t const *chunk)
{
    if (chunk->size < 6) {
        return 0;
    }
    if (chunk->buffer[0] == 'G' &&
        chunk->buffer[1] == 'I' &&
        chunk->buffer[2] == 'F' &&
        chunk->buffer[3] == '8' &&
        (chunk->buffer[4] == '7' || chunk->buffer[4] == '9') &&
        chunk->buffer[5] == 'a') {
        return 1;
    }
    return 0;
}


#if HAVE_JPEG
static int
chunk_is_jpeg(chunk_t const *chunk)
{
    if (chunk->size < 2) {
        return 0;
    }
    if (memcmp("\xFF\xD8", chunk->buffer, 2) == 0) {
        return 1;
    }
    return 0;
}
#endif  /* HAVE_JPEG */



static SIXELSTATUS
load_with_builtin(
    chunk_t const             /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    char message[256];
    int nwrite;

    if (chunk_is_sixel(pchunk)) {
        frame = sixel_frame_create();
        if (frame == NULL) {
            sixel_helper_set_additional_message(
                "malloc() failed in load_with_builtin().");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = load_sixel(&frame->pixels,
                            pchunk->buffer,
                            pchunk->size,
                            &frame->width,
                            &frame->height,
                            fuse_palette ? &frame->palette: NULL,
                            &frame->ncolors,
                            reqcolors,
                            &frame->pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else if (chunk_is_pnm(pchunk)) {
        frame = sixel_frame_create();
        if (frame == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        /* pnm */
        frame->pixels = load_pnm(pchunk->buffer,
                                 pchunk->size,
                                 &frame->width,
                                 &frame->height,
                                 fuse_palette ? &frame->palette: NULL,
                                 &frame->ncolors,
                                 &frame->pixelformat);
        if (!frame->pixels) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("load_pnm() failed.");
            goto end;
        }
    }
#if HAVE_JPEG
    else if (chunk_is_jpeg(pchunk)) {
        frame = sixel_frame_create();
        if (frame == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = load_jpeg(&frame->pixels,
                           pchunk->buffer,
                           pchunk->size,
                           &frame->width,
                           &frame->height,
                           &frame->pixelformat);

        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
#endif  /* HAVE_JPEG */
#if HAVE_LIBPNG
    else if (chunk_is_png(pchunk)) {
        frame = sixel_frame_create();
        if (frame == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        status = load_png(&frame->pixels,
                          pchunk->buffer,
                          pchunk->size,
                          &frame->width,
                          &frame->height,
                          fuse_palette ? &frame->palette: NULL,
                          &frame->ncolors,
                          reqcolors,
                          &frame->pixelformat,
                          bgcolor,
                          &frame->transparent);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
#endif  /* HAVE_LIBPNG */
    else if (chunk_is_gif(pchunk)) {
        status = load_gif(pchunk->buffer,
                          pchunk->size,
                          bgcolor,
                          reqcolors,
                          fuse_palette,
                          fstatic,
                          loop_control,
                          fn_load,
                          context);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        goto end;
    } else {
        stbi__context s;
        int depth;

        frame = sixel_frame_create();
        if (frame == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        stbi__start_mem(&s, pchunk->buffer, pchunk->size);
        frame->pixels = stbi__load_main(&s, &frame->width, &frame->height, &depth, 3);
        if (!frame->pixels) {
            sixel_helper_set_additional_message(stbi_failure_reason());
            status = SIXEL_STBI_ERROR;
            goto end;
        }
        frame->loop_count = 1;

        switch (depth) {
        case 1:
        case 3:
        case 4:
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            break;
        default:
            nwrite = sprintf(message,
                             "load_with_builtin() failed.\n"
                             "reason: unknown pixel-format.(depth: %d)\n",
                             depth);
            if (nwrite > 0) {
                sixel_helper_set_additional_message(message);
            }
            goto end;
        }
    }

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    sixel_frame_unref(frame);

    return status;
}


#ifdef HAVE_GDK_PIXBUF2
static SIXELSTATUS
load_with_gdkpixbuf(
    chunk_t const             /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    GdkPixbuf *pixbuf;
    GdkPixbufAnimation *animation;
    GdkPixbufLoader *loader;
#if 1
    GdkPixbufAnimationIter *it;
    GTimeVal time;
#endif
    sixel_frame_t *frame;
    int stride;
    unsigned char *p;
    int i;
    int depth;

    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;

    frame = sixel_frame_create();
    if (frame == NULL) {
        return SIXEL_FALSE;
    }

#if (!GLIB_CHECK_VERSION(2, 36, 0))
    g_type_init();
#endif
    g_get_current_time(&time);
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, pchunk->buffer, pchunk->size, NULL);
    animation = gdk_pixbuf_loader_get_animation(loader);
    if (!animation || fstatic || gdk_pixbuf_animation_is_static_image(animation)) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf == NULL) {
            goto end;
        }
        frame->frame_no = 0;
        frame->width = gdk_pixbuf_get_width(pixbuf);
        frame->height = gdk_pixbuf_get_height(pixbuf);
        stride = gdk_pixbuf_get_rowstride(pixbuf);
        frame->pixels = malloc(frame->height * stride);
        if (frame->pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (stride / frame->width == 4) {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            depth = 4;
        } else {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            depth = 3;
        }
        p = gdk_pixbuf_get_pixels(pixbuf);
        if (stride == frame->width * depth) {
            memcpy(frame->pixels, gdk_pixbuf_get_pixels(pixbuf),
                   frame->height * stride);
        } else {
            for (i = 0; i < frame->height; ++i) {
                memcpy(frame->pixels + frame->width * depth * i,
                       p + stride * i,
                       frame->width * depth);
            }
        }
        status = fn_load(frame, context);
        if (status != SIXEL_OK) {
            goto end;
        }
    } else {
        g_get_current_time(&time);

        frame->frame_no = 0;

        it = gdk_pixbuf_animation_get_iter(animation, &time);
        for (;;) {
            while (!gdk_pixbuf_animation_iter_on_currently_loading_frame(it)) {
                frame->delay = gdk_pixbuf_animation_iter_get_delay_time(it);
                g_time_val_add(&time, frame->delay * 1000);
                frame->delay /= 10;
                pixbuf = gdk_pixbuf_animation_iter_get_pixbuf(it);
                if (pixbuf == NULL) {
                    break;
                }
                frame->width = gdk_pixbuf_get_width(pixbuf);
                frame->height = gdk_pixbuf_get_height(pixbuf);
                stride = gdk_pixbuf_get_rowstride(pixbuf);
                frame->pixels = malloc(frame->height * stride);
                if (frame->pixels == NULL) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                if (gdk_pixbuf_get_has_alpha(pixbuf)) {
                    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
                    depth = 4;
                } else {
                    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
                    depth = 3;
                }
                p = gdk_pixbuf_get_pixels(pixbuf);
                if (stride == frame->width * depth) {
                    memcpy(frame->pixels, gdk_pixbuf_get_pixels(pixbuf),
                           frame->height * stride);
                } else {
                    for (i = 0; i < frame->height; ++i) {
                        memcpy(frame->pixels + frame->width * depth * i,
                               p + stride * i,
                               frame->width * depth);
                    }
                }
                frame->multiframe = 1;
                gdk_pixbuf_animation_iter_advance(it, &time);
                status = fn_load(frame, context);
                if (status != SIXEL_OK) {
                    goto end;
                }
                frame->frame_no++;
            }

            ++frame->loop_count;

            if (loop_control == SIXEL_LOOP_DISABLE || frame->frame_no == 1) {
                break;
            }
            /* TODO: get loop property */
            if (loop_control == SIXEL_LOOP_AUTO && frame->loop_count == 1) {
                break;
            }
        }
    }

    status = SIXEL_OK;

end:
    gdk_pixbuf_loader_close(loader, NULL);
    g_object_unref(loader);
    free(frame->pixels);
    free(frame->palette);
    free(frame);

    return status;

}
#endif  /* HAVE_GDK_PIXBUF2 */

#ifdef HAVE_GD
static int
detect_file_format(int len, unsigned char *data)
{
    if (memcmp("TRUEVISION", data + len - 18, 10) == 0) {
        return SIXEL_FORMAT_TGA;
    }

    if (memcmp("GIF", data, 3) == 0) {
        return SIXEL_FORMAT_GIF;
    }

    if (memcmp("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", data, 8) == 0) {
        return SIXEL_FORMAT_PNG;
    }

    if (memcmp("BM", data, 2) == 0) {
        return SIXEL_FORMAT_BMP;
    }

    if (memcmp("\xFF\xD8", data, 2) == 0) {
        return SIXEL_FORMAT_JPG;
    }

    if (memcmp("\x00\x00", data, 2) == 0) {
        return SIXEL_FORMAT_WBMP;
    }

    if (memcmp("\x4D\x4D", data, 2) == 0) {
        return SIXEL_FORMAT_TIFF;
    }

    if (memcmp("\x49\x49", data, 2) == 0) {
        return SIXEL_FORMAT_TIFF;
    }

    if (memcmp("\033P", data, 2) == 0) {
        return SIXEL_FORMAT_SIXEL;
    }

    if (data[0] == 0x90  && (data[len-1] == 0x9C || data[len-2] == 0x9C)) {
        return SIXEL_FORMAT_SIXEL;
    }

    if (data[0] == 'P' && data[1] >= '1' && data[1] <= '6') {
        return SIXEL_FORMAT_PNM;
    }

    if (memcmp("gd2", data, 3) == 0) {
        return SIXEL_FORMAT_GD2;
    }

    if (memcmp("8BPS", data, 4) == 0) {
        return SIXEL_FORMAT_PSD;
    }

    if (memcmp("#?RADIANCE\n", data, 11) == 0) {
        return SIXEL_FORMAT_HDR;
    }

    return (-1);
}


static SIXELSTATUS
load_with_gd(
    chunk_t const             /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *p;
    gdImagePtr im;
    int x, y;
    int c;
    sixel_frame_t *frame;

    (void) fstatic;
    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;
    (void) loop_control;

    frame = sixel_frame_create();
    if (frame == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    switch (detect_file_format(pchunk->size, pchunk->buffer)) {
#if 0
# if HAVE_DECL_GDIMAGECREATEFROMGIFPTR
        case SIXEL_FORMAT_GIF:
            im = gdImageCreateFromGifPtr(pchunk->size, pchunk->buffer);
            break;
# endif  /* HAVE_DECL_GDIMAGECREATEFROMGIFPTR */
#endif
#if HAVE_DECL_GDIMAGECREATEFROMPNGPTR
        case SIXEL_FORMAT_PNG:
            im = gdImageCreateFromPngPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMPNGPTR */
#if HAVE_DECL_GDIMAGECREATEFROMBMPPTR
        case SIXEL_FORMAT_BMP:
            im = gdImageCreateFromBmpPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMBMPPTR */
        case SIXEL_FORMAT_JPG:
#if HAVE_DECL_GDIMAGECREATEFROMJPEGPTREX
            im = gdImageCreateFromJpegPtrEx(pchunk->size, pchunk->buffer, 1);
#elif HAVE_DECL_GDIMAGECREATEFROMJPEGPTR
            im = gdImageCreateFromJpegPtr(pchunk->size, pchunk->buffer);
#endif  /* HAVE_DECL_GDIMAGECREATEFROMJPEGPTREX */
            break;
#if HAVE_DECL_GDIMAGECREATEFROMTGAPTR
        case SIXEL_FORMAT_TGA:
            im = gdImageCreateFromTgaPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMTGAPTR */
#if HAVE_DECL_GDIMAGECREATEFROMWBMPPTR
        case SIXEL_FORMAT_WBMP:
            im = gdImageCreateFromWBMPPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMWBMPPTR */
#if HAVE_DECL_GDIMAGECREATEFROMTIFFPTR
        case SIXEL_FORMAT_TIFF:
            im = gdImageCreateFromTiffPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMTIFFPTR */
#if HAVE_DECL_GDIMAGECREATEFROMGD2PTR
        case SIXEL_FORMAT_GD2:
            im = gdImageCreateFromGd2Ptr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMGD2PTR */
        default:
            status = SIXEL_GD_ERROR;
            sixel_helper_set_additional_message(
                "unexpected image format detected.");
            goto end;
    }

    if (im == NULL) {
        status = SIXEL_GD_ERROR;
        /* TODO: retrieve error detail */
        goto end;
    }

    if (!gdImageTrueColor(im)) {
#if HAVE_DECL_GDIMAGEPALETTETOTRUECOLOR
        if (!gdImagePaletteToTrueColor(im)) {
            status = SIXEL_GD_ERROR;
            /* TODO: retrieve error detail */
            goto end;
        }
#else
        status = SIXEL_GD_ERROR;
        /* TODO: retrieve error detail */
        goto end;
#endif
    }

    frame->width = gdImageSX(im);
    frame->height = gdImageSY(im);
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    p = frame->pixels = malloc(frame->width * frame->height * 3);
    if (frame->pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        gdImageDestroy(im);
        goto end;
    }
    for (y = 0; y < frame->height; y++) {
        for (x = 0; x < frame->width; x++) {
            c = gdImageTrueColorPixel(im, x, y);
            *p++ = gdTrueColorGetRed(c);
            *p++ = gdTrueColorGetGreen(c);
            *p++ = gdTrueColorGetBlue(c);
        }
    }
    gdImageDestroy(im);

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}

#endif  /* HAVE_GD */


/* load image from file */

SIXELAPI SIXELSTATUS
sixel_helper_load_image_file(
    char const                /* in */     *filename,     /* source file name */
    int                       /* in */     fstatic,       /* whether to extract static image */
    int                       /* in */     fuse_palette,  /* whether to use paletted image */
    int                       /* in */     reqcolors,     /* requested number of colors */
    unsigned char             /* in */     *bgcolor,      /* background color */
    int                       /* in */     loop_control,  /* one of enum loopControl */
    sixel_load_image_function /* in */     fn_load,       /* callback */
    int                       /* in */     finsecure,     /* true if do not verify SSL */
    int const                 /* in */     *cancel_flag,  /* cancel flag */
    void                      /* in/out */ *context       /* private data */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    chunk_t chunk;
    chunk.buffer = NULL;
    chunk.size = chunk.max_size = 0;

    status = get_chunk(filename, &chunk, finsecure, cancel_flag);
    if (status != SIXEL_OK) {
        goto end;
    }

    /* if input date is empty or 1 byte LF, ignore it and return successfully */
    if (chunk.size == 0 || (chunk.size == 1 && *chunk.buffer == '\n')) {
        status = SIXEL_OK;
        goto end;
    }

    /* assertion */
    if (chunk.buffer == NULL || chunk.max_size == 0) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_FALSE;
#ifdef HAVE_GDK_PIXBUF2
    if (SIXEL_FAILED(status)) {
        status = load_with_gdkpixbuf(&chunk,
                                     fstatic,
                                     fuse_palette,
                                     reqcolors,
                                     bgcolor,
                                     loop_control,
                                     fn_load,
                                     context);
    }
#endif  /* HAVE_GDK_PIXBUF2 */
#if HAVE_GD
    if (SIXEL_FAILED(status)) {
        status = load_with_gd(&chunk,
                              fstatic,
                              fuse_palette,
                              reqcolors,
                              bgcolor,
                              loop_control,
                              fn_load,
                              context);
    }
#endif  /* HAVE_GD */
    if (SIXEL_FAILED(status)) {
        status = load_with_builtin(&chunk,
                                   fstatic,
                                   fuse_palette,
                                   reqcolors,
                                   bgcolor,
                                   loop_control,
                                   fn_load,
                                   context);
    }
    free(chunk.buffer);

    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return status;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    unsigned char *ptr = malloc(16);

#ifdef HAVE_LIBCURL
    chunk_t chunk = {0, 0, 0};
    int nread;

    nread = memory_write(NULL, 1, 1, NULL);
    if (nread != 0) {
        goto error;
    }

    nread = memory_write(ptr, 1, 1, &chunk);
    if (nread != 0) {
        goto error;
    }

    nread = memory_write(ptr, 0, 1, &chunk);
    if (nread != 0) {
        goto error;
    }
#else
    nret = EXIT_SUCCESS;
    goto error; 
#endif  /* HAVE_LIBCURL */
    nret = EXIT_SUCCESS;

error:
    free(ptr);
    return nret;
}


int
sixel_loader_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */



/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */

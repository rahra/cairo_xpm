/*! This file contains function to create XPM output from a Cairo surface. All
 * prototypes are defined in cairo_xpm.h. This code is independent of any Xlib
 * library.
 *
 * To compile as an object file to link against your code use the following statement:
 * gcc -Wall -g `pkg-config --cflags --libs cairo` -c cairo_xpm.c 
 *
 * To compile as standalone tool including the main() function compile as follows:
 * gcc -Wall -g `pkg-config --cflags --libs cairo` -DCAIRO_XPM_MAIN -o cairo_xpm cairo_xpm.c
 *
 * @author Bernhard R. Fischer, 4096R/8E24F29D bf@abenteuerland.at
 * @version 2018/05/18
 * @license This code is free software. Do whatever you like to do with it.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cairo.h>

#include "cairo_xpm.h"


// max number of colors = 3 * 8 bit
#define MAX_COL 0x1000000
// transparency threshold 50% (0xff = transparent, 0x00 = opaque)
#define TRANS_THRESH 0x80


/*! Function to return color value of pixel.
 */
static int cairo_image_surface_get_argb24_pixel(cairo_surface_t *sfc, int x, int y)
{
   return *((int*) (cairo_image_surface_get_data(sfc) + y * cairo_image_surface_get_stride(sfc) + x * 4));
}


/*! Function to calculate number of characters per pixel dependent on the
 * number of colors. The color index will be base64-encoded, thus 1 character
 * equals 6 bits.
 */
static int num_chars_per_pixel(unsigned ncols)
{
   int bpc, n;

   for (bpc = 0, n = ncols; n; n >>= 1, bpc++);

   n = bpc / 6;
   if (n * 6 < bpc)
      n++;

   return n;
}


/* This function Base64-encodes an integer number.
 * @param dst Pointer to destination. It must hold at least cpp + 1 bytes of
 * memory. The string will by \0-terminated.
 * @param pix Integer number to encode.
 * @param cpp Number of characters of the result.
 */
static void col_encode_base64(char *dst, int pix, int cpp)
{
   char *ctbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

   for (; cpp; cpp--)
      *dst++ = ctbl[(pix >> ((cpp - 1) * 6)) & 0x3f];
   *dst = '\0';
}


/*! This function converts a integer color to a hexadecimal color sting (as
 * typically used e.g. by HTML).
 * @param dst Pointer to destination. It must hold at least 7 bytes of memory.
 * The string will be \0-terminated.
 * @param c Color to convert.
 */
static void col_encode_html(char *dst, int c)
{
   snprintf(dst, 7, "%06x", c & 0xffffff);
}


/*! Function to estimate size of final XPM structure (the source code).
 */
static size_t xpm_size(int w, int h, int ncols, int cpp)
{
   int ctbls, ptbls;

   // size of color table 14 = "\" c #xxxxxx\",\n"
   ctbls = (cpp + 14) * ncols;
   // size of pixel table. 4 = "\"\",\n"
   ptbls = ((w * cpp) + 4) * h;

   // 256 bytes for header
   return ctbls + ptbls + 256;
}


/*! This function creates an XPM struct in memory from a Cairo image surface.
 * @param sfc Pointer to a Cairo surface. It should be an image surface of
 * either CAIRO_FORMAT_ARGB32 or CAIRO_FORMAT_RGB24. Other formats are
 * converted to CAIRO_FORMAT_RGB24 before creating the XPM.
 * Please note XPM supports just on transparent color, hence, a threshold (cpp
 * macro TRANS_THRESH) is used to convert transparent pixels.
 * @param data Pointer to a memory pointer. This parameter receives a pointer
 * to the memory area where the final XPM data is found in memory. This
 * function reserves the memory properly and it has to be freed by the caller
 * with free(3).
 * @param len Pointer to a variable of type size_t which will receive the final
 * lenght of the memory buffer.
 * @return On success the function returns CAIRO_STATUS_SUCCESS. In case of
 * error CAIRO_STATUS_INVALID_FORMAT is returned.
 */
cairo_status_t cairo_image_surface_write_to_xpm_mem(cairo_surface_t *sfc, unsigned char **data, size_t *len)
{
   int *colors, col, ncols, cpp, x, y, xpmsize;
   cairo_surface_t *other = NULL;
   char *xbuf;

   *data = NULL;
   *len = 0;

   // check valid input format (must be IMAGE_SURFACE && (ARGB32 || RGB24))
   if (cairo_surface_get_type(sfc) != CAIRO_SURFACE_TYPE_IMAGE ||
         (cairo_image_surface_get_format(sfc) != CAIRO_FORMAT_ARGB32 &&
         cairo_image_surface_get_format(sfc) != CAIRO_FORMAT_RGB24))
   {
      // create a similar surface with a proper format if supplied input format
      // does not fulfill the requirements
      double x1, y1, x2, y2;
      other = sfc;
      cairo_t *ctx = cairo_create(other);
      // get extents of original surface
      cairo_clip_extents(ctx, &x1, &y1, &x2, &y2);
      cairo_destroy(ctx);

      // create new image surface
      sfc = cairo_surface_create_similar_image(other, CAIRO_FORMAT_RGB24, x2 - x1, y2 - y1);
      if (cairo_surface_status(sfc) != CAIRO_STATUS_SUCCESS)
         return CAIRO_STATUS_INVALID_FORMAT;

      // paint original surface to new surface
      ctx = cairo_create(sfc);
      cairo_set_source_surface(ctx, other, 0, 0);
      cairo_paint(ctx);
      cairo_destroy(ctx);
   }

   // finish queued drawing operations
   cairo_surface_flush(sfc);

   // get memory for colors
   if ((colors = calloc(MAX_COL + 1, sizeof(*colors))) == NULL)
   {
      if (other != NULL)
         cairo_surface_destroy(sfc);
      return CAIRO_STATUS_NO_MEMORY;
   }

   // count number of different colors
   ncols = 0;
   for (y = 0; y < cairo_image_surface_get_height(sfc); y++)
      for (x = 0; x < cairo_image_surface_get_width(sfc); x++)
      {
         col = cairo_image_surface_get_argb24_pixel(sfc, x, y);
         if (((col >> 24) & 0xff) < TRANS_THRESH)
            col = MAX_COL;
         else
            col &= 0x00ffffff;
         if (!colors[col])
            colors[col] = ++ncols;
      }

   cpp = num_chars_per_pixel(ncols);
   xpmsize = xpm_size(cairo_image_surface_get_width(sfc), cairo_image_surface_get_height(sfc), ncols, cpp);

   if ((xbuf = malloc(xpmsize)) == NULL)
   {
      free(colors);
      if (other != NULL)
         cairo_surface_destroy(sfc);
      return CAIRO_STATUS_NO_MEMORY;
   }

   int size, pos, cur;
   pos = 0;
   size = xpmsize;
   cur = snprintf(xbuf, size, "/* XPM */\nstatic char *xpm_c%d_[] = {\n\"%d %d %d %d\"",
         ncols, cairo_image_surface_get_width(sfc),
         cairo_image_surface_get_height(sfc), ncols, cpp);
   pos += cur;
   size -= cur;

   // create XPM color table
   char cb[5], ch[7];
   for (int i = 0; i < MAX_COL + 1; i++)
      if (colors[i])
      {
         col_encode_base64(cb, colors[i] - 1, cpp);
         col_encode_html(ch, i);
         if (i < MAX_COL)
            cur = snprintf(xbuf + pos, size, ",\n\"%s c #%s\"", cb, ch);
         else
            cur = snprintf(xbuf + pos, size, ",\n\"%s c None\"", cb);
         pos += cur;
         size -= cur;
      }

   // create XPM pixel table
   for (y = 0; y < cairo_image_surface_get_height(sfc); y++)
   {
      cur = snprintf(xbuf + pos, size, ",\n\"");
      pos += cur;
      size -= cur;
      for (x = 0; x < cairo_image_surface_get_width(sfc); x++)
      {
         col = cairo_image_surface_get_argb24_pixel(sfc, x, y);
         if (((col >> 24) & 0xff) < TRANS_THRESH)
            col = MAX_COL;
         else
            col &= 0xffffff;
         col_encode_base64(xbuf + pos, colors[col] - 1, cpp);
         pos += cpp;
         size -= cpp;
      }
      cur = snprintf(xbuf + pos, size, "\"");
      pos += cur;
      size -= cur;
   }

   cur = snprintf(xbuf + pos, size, "\n};\n");
   pos += cur;
   size -= cur;

   free(colors);

   // free temporary surface if necessary
   if (other != NULL)
      cairo_surface_destroy(sfc);

   *len = pos;
   *data = (unsigned char*) xbuf;

   return CAIRO_STATUS_SUCCESS;
}


/*! This is the internal write function which is called by
 * cairo_image_surface_write_to_xpm(). It is not exported.
 */
static cairo_status_t cj_write(void *closure, const unsigned char *data, unsigned int length)
{
   return write((long) closure, data, length) < length ?
      CAIRO_STATUS_WRITE_ERROR : CAIRO_STATUS_SUCCESS;
}


/*! This function writes XPM data from a Cairo image surface by using the
 * user-supplied stream writer function write_func().
 * @param sfc Pointer to a Cairo *image* surface. Its format must either be
 * CAIRO_FORMAT_ARGB32 or CAIRO_FORMAT_RGB24. Other formats are not supported
 * by this function, yet.
 * @param write_func Function pointer to a function which is actually writing
 * the data.
 * @param closure Pointer to user-supplied variable which is directly passed to
 * write_func().
 * @return This function calles cairo_image_surface_write_to_jpeg_mem() and
 * returns its return value.
 */
cairo_status_t cairo_image_surface_write_to_xpm_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure)
{
   cairo_status_t e;
   unsigned char *data = NULL;
   size_t len = 0;

   // create XPM data in memory from surface
   if ((e = cairo_image_surface_write_to_xpm_mem(sfc, &data, &len)) != CAIRO_STATUS_SUCCESS)
      return e;

   // write whole memory block with stream function
   e = write_func(closure, data, len);

   // free XPM memory again and return the return value
   free(data);
   return e;

}


/*! This function creates a XPM file from a Cairo image surface.
 * @param sfc Pointer to a Cairo *image* surface. Its format must either be
 * CAIRO_FORMAT_ARGB32 or CAIRO_FORMAT_RGB24. Other formats are not supported
 * by this function, yet.
 * @param filename Pointer to the filename.
 * @return In case of success CAIRO_STATUS_SUCCESS is returned. If an error
 * occured while opening/creating the file CAIRO_STATUS_DEVICE_ERROR is
 * returned. The error can be tracked down by inspecting errno(3). The function
 * internally calles cairo_image_surface_write_to_xpm_stream() and returnes
 * its return value respectively (see there).
 */
cairo_status_t cairo_image_surface_write_to_xpm(cairo_surface_t *sfc, const char *filename)
{
   cairo_status_t e;
   int outfile;

   // Open/create new file
   if ((outfile = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
      return CAIRO_STATUS_DEVICE_ERROR;

   // write surface to file
   e = cairo_image_surface_write_to_xpm_stream(sfc, cj_write, (void*) (long) outfile);

   // close file again and return
   close(outfile);
   return e;
}


#ifdef CAIRO_XPM_MAIN
int main(int argc, char **argv)
{
   cairo_surface_t *sfc;
   unsigned char *data;
   size_t len;

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s <input PNG file> [<output XPM filename>]\n", argv[0]);
      return 1;
   }

   sfc = cairo_image_surface_create_from_png(argv[1]);

   if (argc < 3)
   {
      cairo_image_surface_write_to_xpm_mem(sfc, &data, &len);
      printf("%*s", (int) len, data);
   }
   else
   {
      cairo_image_surface_write_to_xpm(sfc, argv[2]);
   }

   cairo_surface_destroy(sfc);
   return 0;
}
#endif


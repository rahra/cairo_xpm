#ifndef CAIRO_XPM_H
#define CAIRO_XPM_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cairo.h>


cairo_status_t cairo_image_surface_write_to_xpm_mem(cairo_surface_t *sfc, unsigned char **data, size_t *len);
cairo_status_t cairo_image_surface_write_to_xpm_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure);
cairo_status_t cairo_image_surface_write_to_xpm(cairo_surface_t *sfc, const char *filename);


#endif


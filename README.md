# cairo_xpm

This is an implementation of the XPM 3 (color gradients only) format to export
a Cairo surface to XPM.
It uses the same function prototypes as Cairo's [PNG
support](http://www.cairographics.org/manual/cairo-PNG-Support.html).

The implementation is done on top of the Cairo API. It does not access
Cairo-internal functions.

The functions with the following prototypes are implemented:

```C
cairo_status_t cairo_image_surface_write_to_xpm_mem(cairo_surface_t *sfc, unsigned char **data, size_t *len);
cairo_status_t cairo_image_surface_write_to_xpm_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure);
cairo_status_t cairo_image_surface_write_to_xpm(cairo_surface_t *sfc, const char *filename);

```

To compile as an object file to link against your code use the following statement:
```Shell
gcc -Wall -g `pkg-config --cflags --libs cairo` -c cairo_xpm.c 
```

To compile as standalone tool including the main() function compile as follows:
```Shell
gcc -Wall -g `pkg-config --cflags --libs cairo` -DCAIRO_XPM_MAIN -o cairo_xpm cairo_xpm.c
```

Please have a look at the comments within the source files for further details.
Don't hesitate to contact me at [bf@abenteuerland.at](mailto:bf@abenteuerland.at).


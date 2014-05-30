#ifndef COLOR_H
#define COLOR_H

#ifdef PROVIDE_GDK_CONVERTION_FUNC
#include <gdk/gdk.h> // for GdkColor.
#endif /* PROVIDE_GDK_CONVERTION_FUNC */

#include <stdbool.h> // for bool.
#include <stdint.h> // for uint8_t.

// Defines a 32 bit color.
typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
} color_T;

// Creates a new color from individual integer components.
color_T color_create_with_rgba(uint8_t red, uint8_t green, uint8_t blue,
                             uint8_t alpha);

// Extract each color_t components.
void color_extract_components(const color_T* color,
                              double* red,
                              double* green,
                              double* blue,
                              double* alpha);

// Extract each color_t components to an double array.
void color_extract_components_to_array(const color_T* color,
                                        double components[4]);

// Creates a new color_t from a string with hexadecimal color notation.
color_T color_create(const char* hex_color);
/* // Destroys a color_t object. */


// Return a default color.
color_T color_default_color(void);

// Convert a color_t object to a GdkColor object.
#ifdef PROVIDE_GDK_CONVERTION_FUNC
void color_to_gdkcolor(const color_T* src, GdkColor* dest);
#endif /* PROVIDE_GDK_CONVERTION_FUNC */

#endif // COLOR_H

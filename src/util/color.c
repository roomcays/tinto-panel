#include <stdbool.h> // for bool.
#include <stdlib.h> // for malloc(), free().
#include <stddef.h> // for NULL.
#include <string.h> // for strlen().
#include <ctype.h> // for tolower().

#include <stdio.h>

#include "color.h" /* for color_t. */

static uint8_t hex_char_to_decimal(char character);

/**
 * @brief Return the default color.
 *
 * @return a default color.
 */
inline color_T color_default_color(void) {
  static color_T color = { .alpha = 204U, .red = 204U, .green = UINT8_MAX,
                           .blue = 127U,};

  return color;
}

/**
 * @brief Creates a new color_t from its components rgba.
 * A new color_t is created from individual components rgba, the caller must release the
 * resources allocated by color_t when done through <i>color_destroy_color()</i>.
 *
 * @param red red component.

 * @param blue blue component.

 * @param green green component.
 *
 * @param alpha alpha component.
 *
 * @return a new color in case of success or NULL if any errors occurred.
 *
 */
inline color_T color_create_with_rgba(uint8_t red, uint8_t green, uint8_t blue,
                             uint8_t alpha) {
  color_T color = { .alpha = alpha, .green = green, .red = red, .blue = blue };

  return color;
}

void color_extract_components(const color_T* color,
                              double* red,
                              double* green,
                              double* blue,
                              double* alpha) {
  if (red) *red = color->red / 255.0;
  if (green) *green = color->green / 255.0;
  if (blue) *blue = color->blue / 255.0;
  if (alpha) *alpha = color->alpha / 255.0;
}

inline void color_extract_components_to_array(const color_T* color,
                                       double components[4]) {
  color_extract_components(color,
                           components,
                           components + 1U,
                           components + 2U,
                           components + 3U);
}

/**
 * Creates a new color_t from a hexadecimal string.
 *
 * @param hex_color string with a hexadecimal color.
 *
 * @return a new color_t, note that caller should free the color_t after done using it by calling <i>collor_destroy</i>;
 */
color_T color_create(const char* hex_color) {
  if (!hex_color || *hex_color != '#') return color_default_color();

  const size_t len = strlen(hex_color);
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
  red = green = blue = alpha = UINT8_MAX;

  switch (len) {
  case 5:
    if (*(hex_color + 3U) != *(hex_color + 4U)) return color_default_color();
    alpha = hex_char_to_decimal(*(hex_color + 4U));
    alpha += alpha << 4;
  case 4:
    if (*(hex_color + 1U) != *(hex_color + 2U)
        || *(hex_color + 2U) != *(hex_color + 3U)) {
      return color_default_color();
    }
    red = hex_char_to_decimal(*(hex_color + 1U));

    char* end_ptr = strchr(hex_color + 2U, *(hex_color + 1U));
    if (!end_ptr) return color_default_color();
    if (end_ptr == strrchr(hex_color + 2U, *(hex_color + 1U))) return color_default_color();

    blue = green = red += red << 4;
    break;

  case 9:
    alpha = (hex_char_to_decimal(*(hex_color + 8)) << 4)
      + hex_char_to_decimal(*(hex_color + 7));
  case 7:
    blue = (hex_char_to_decimal(*(hex_color + 6)) << 4)
      + hex_char_to_decimal(*(hex_color + 5));
    green = (hex_char_to_decimal(*(hex_color + 4)) << 4)
      + hex_char_to_decimal(*(hex_color + 3));
    red = (hex_char_to_decimal(*(hex_color + 2)) << 4)
      + hex_char_to_decimal(*(hex_color + 1));
    break;

  default:
    return color_default_color();
  }

  color_T c = color_create_with_rgba(red, green, blue, alpha);

  return c;
}

/**
 * @brief Converts a color_t to a GdkColor color.
 *
 * @param src a color_t object to convert.
 *
 * @param dest a GdkColor object to store de convertion.
 */
#ifdef PROVIDE_GDK_CONVERTION_FUNC
void color_to_gdkcolor(const color_T* src, GdkColor* dest) {
  dest->red = (guint16)src->red;
  dest->green = (guint16)src->green;
  dest->red = (guint16)src->blue;
}
#endif /* PROVIDE_GDK_CONVERTION_FUNC */

/**
 * Convert a character in hexadecimal notation to its equivalent in decimal notation.
 *
 * @param c character to be converted.
 *
 * @return the equivalent decimal character or 0 if <i>c</i> is not a hexadecimal character.
 */
static inline uint8_t hex_char_to_decimal(char character) {
  uint8_t value = 0;
  character = tolower(character);
  if (character >= '0' && character <= '9') value = character - '0';
  else if (character >= 'a' && character <= 'f')
    value = character - 'a' + 10;

  return value;
}

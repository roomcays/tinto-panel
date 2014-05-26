/**************************************************************************
* Common declarations
*
**************************************************************************/

#ifndef COMMON_H
#define COMMON_H

#define WM_CLASS_TINT "panel"

#include <Imlib2.h>
#include "area.h"

typedef struct {
  int x;
  int y;
} point_T;

/*
void fxfree(void** ptr){
  if(*ptr){
    free(*ptr);
    *ptr=NULL;
    }
  }
FXint fxmalloc(void** ptr,unsigned long size){
  *ptr=NULL;
  if(size!=0){
    if((*ptr=malloc(size))==NULL) return FALSE;
    }
  return TRUE;
  }
*/

// mouse actions
enum {
  NONE = 0,
  CLOSE,
  TOGGLE,
  ICONIFY,
  SHADE,
  TOGGLE_ICONIFY,
  MAXIMIZE_RESTORE,
  MAXIMIZE,
  RESTORE,
  DESKTOP_LEFT,
  DESKTOP_RIGHT,
  NEXT_TASK,
  PREV_TASK
};

#define ALLDESKTOP 0xFFFFFFFF

// copy file source to file dest
void copy_file(const char* pathSrc, const char* pathDest);

// extract key = value
int parse_line(const char* line, char** key, char** value);

// execute a command by calling fork
void tint_exec(const char* command);

// conversion
int hex_char_to_int(char c);
int hex_to_rgb(char* hex, int* r, int* g, int* b);
void get_color(char* hex, double* rgb);

void extract_values(const char* value, char** value1, char** value2,
                    char** value3);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void adjust_asb(DATA32* data, int w, int h, int alpha, float satur,
                float bright);
void createHeuristicMask(DATA32* data, int w, int h);

void render_image(Drawable d, int x, int y, int w, int h);

#ifndef SWAP_INTEGER
/*
 * @brief Swap the values of the integers a and b.
 *
 * @param a a non const integer lvalue which will have the value of b when this
 * macro finishes.
 *
 * @param b a non const integer lvalue which will have the value of a when this
 * macro finishes.
 */
#define SWAP_INTEGER(a, b) \
  {                        \
    a ^= b;                \
    b ^= a;                \
    a ^= b;                \
  }
#endif  // SWAP_INTEGER
#endif

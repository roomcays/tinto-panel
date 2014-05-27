/**************************************************************************
*
* Copyright (C) 2008 Pål Staurland (staura@gmail.com)
* Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega
*distribution
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
*USA.
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>

#include "server.h"
#include "config.h"
#include "window.h"
#include "task.h"
#include "panel.h"
#include "tooltip.h"
#include "debug.h"
#include "common.h"

int signal_pending;
// --------------------------------------------------
// mouse events
int mouse_middle;
int mouse_right;
int mouse_scroll_up;
int mouse_scroll_down;
int mouse_tilt_left;
int mouse_tilt_right;

int panel_mode;
int wm_menu;
int panel_dock;
int panel_layer;
int panel_position;
int panel_horizontal;
int panel_refresh;
int task_dragged;

int panel_autohide;
int panel_autohide_show_timeout;
int panel_autohide_hide_timeout;
int panel_autohide_height;
int panel_strut_policy;
char* panel_items_order;

int max_tick_urgent;

// panel's initial config
Panel panel_config;
// panels (one panel per monitor)
Panel* panel1;
int nb_panel;

GArray* backgrounds;

Imlib_Image default_icon;

void default_panel(void) {
  panel1 = NULL;
  nb_panel = 0;
  default_icon = NULL;
  task_dragged = 0;
  panel_horizontal = 1;
  panel_position = CENTER;
  panel_items_order = NULL;
  panel_autohide = 0;
  panel_autohide_show_timeout = 0;
  panel_autohide_hide_timeout = 0;
  panel_autohide_height = 5;  // for vertical panels this is of course the width
  panel_strut_policy = STRUT_FOLLOW_SIZE;
  panel_dock = 0;  // default not in the dock
  panel_layer = BOTTOM_LAYER;  // default is bottom layer
  wm_menu = 0;
  max_tick_urgent = 14;
  backgrounds = g_array_new(0, 0, sizeof(Background));

  memset(&panel_config, 0, sizeof(Panel));

  // append full transparency background
  Background transparent_bg;
  memset(&transparent_bg, 0, sizeof(Background));
  g_array_append_val(backgrounds, transparent_bg);
}

void cleanup_panel(void) {
  if (!panel1) return;

  cleanup_taskbar();
  // taskbarname_font_desc freed here because cleanup_taskbarname() called on
  // _NET_NUMBER_OF_DESKTOPS
  if (taskbarname_font_desc) pango_font_description_free(taskbarname_font_desc);

  Panel* p = NULL;
  for (int i = 0; i < nb_panel; ++i) {
    p = panel1 + i;

    free_area(&p->area);
    if (p->temp_pmap) XFreePixmap(server.dsp, p->temp_pmap);
    if (p->hidden_pixmap) XFreePixmap(server.dsp, p->hidden_pixmap);
    if (p->main_win) XDestroyWindow(server.dsp, p->main_win);
  }

  if (panel_items_order) free(panel_items_order);
  if (panel1) free(panel1);
  if (backgrounds) g_array_free(backgrounds, 1);
  if (panel_config.g_task.font_desc)
    pango_font_description_free(panel_config.g_task.font_desc);
}

void init_panel(void) {
  Panel* p = NULL;

  if (panel_config.monitor > server.nb_monitor - 1) panel_config.monitor = 0;

  init_tooltip();
  init_systray();
  init_launcher();
  init_clock();
#ifdef ENABLE_BATTERY
  init_battery();
#endif
  init_taskbar();

  nb_panel = MAX(panel_config.monitor, server.nb_monitor);

  panel1 = malloc(nb_panel * sizeof(Panel));
  for (int j = 0; j < nb_panel; ++j)
    memcpy(panel1 + j, &panel_config, sizeof(Panel));

  for (int i = 0; i < nb_panel; ++i) {
    p = panel1 + i;

    if (panel_config.monitor < 0) p->monitor = i;
    if (!p->area.bg) p->area.bg = &g_array_index(backgrounds, Background, 0);
    p->area.parent = p;
    p->area.panel = p;
    p->area.on_screen = 1;
    p->area.resize = 1;
    p->area.size_mode = SIZE_BY_LAYOUT;
    p->area._resize = resize_panel;
    init_panel_size_and_position(p);
    // add childs according to panel_items
    for (int k = 0, len = strlen(panel_items_order); k < len; ++k) {
      if (panel_items_order[k] == 'L')
        init_launcher_panel(p);
      else if (panel_items_order[k] == 'T')
        init_taskbar_panel(p);
#ifdef ENABLE_BATTERY
      else if (panel_items_order[k] == 'B')
        init_battery_panel(p);
#endif
      else if (panel_items_order[k] == 'S' && i == 0) {
        // TODO : check systray is only on 1 panel
        // at the moment only on panel1[0] allowed
        init_systray_panel(p);
        refresh_systray = 1;
      } else if (panel_items_order[k] == 'C')
        init_clock_panel(p);
    }
    set_panel_items_order(p);

    // catch some events
    XSetWindowAttributes att = {
        .colormap = server.colormap, .background_pixel = 0, .border_pixel = 0};
    unsigned long mask = CWEventMask | CWColormap | CWBackPixel | CWBorderPixel;
    p->main_win = XCreateWindow(server.dsp, server.root_win, p->location.x, p->location.y,
                                p->area.width, p->area.height, 0, server.depth,
                                InputOutput, server.visual, mask, &att);

    long event_mask =
        ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask;
    if (p->g_task.tooltip_enabled || p->clock.area._get_tooltip_text ||
        (launcher_enabled && launcher_tooltip_enabled)) {
      event_mask |= PointerMotionMask | LeaveWindowMask;
    }

    if (panel_autohide) event_mask |= LeaveWindowMask | EnterWindowMask;
    XChangeWindowAttributes(server.dsp, p->main_win, CWEventMask,
                            &(XSetWindowAttributes) {.event_mask = event_mask});

    if (!server.gc) {
      XGCValues gcv;
      server.gc = XCreateGC(server.dsp, p->main_win, 0, &gcv);
    }

    set_panel_properties(p);
    set_panel_background(p);
    if (!snapshot_path) XMapWindow(server.dsp, p->main_win);

    if (panel_autohide)
      add_timeout(panel_autohide_hide_timeout, 0, autohide_hide, p);

    visible_taskbar(p);
  }

  task_refresh_tasklist();
  active_task();
}

void init_panel_size_and_position(Panel* panel) {
  if (panel_horizontal) {
    if (panel->pourcentx) {
      panel->area.width =
          server.monitor[panel->monitor].width * panel->area.width / 100.0F;
    }

    if (panel->pourcenty) {
      panel->area.height =
          server.monitor[panel->monitor].height * panel->area.height / 100.0F;
    }

    if (panel->area.width + margin_horizontal(&panel->margin) >
        server.monitor[panel->monitor].width) {
      panel->area.width = server.monitor[panel->monitor].width - margin_horizontal(&panel->margin);
    }

    if (panel->area.bg->border.rounded > panel->area.height >> 1) {
      g_array_append_val(backgrounds, *panel->area.bg);
      panel->area.bg =
          &g_array_index(backgrounds, Background, backgrounds->len - 1);
      panel->area.bg->border.rounded = panel->area.height >> 2;
    }
  } else {
    SWAP_INTEGER(panel->area.height, panel->area.width);
    SWAP_INTEGER(panel->margin.top, panel->margin.left);
    SWAP_INTEGER(panel->margin.bottom, panel->margin.right);

    if (panel->pourcentx) {
      panel->area.height =
          server.monitor[panel->monitor].height * panel->area.height / 100.0F;
    }

    if (panel->pourcenty) {
      panel->area.width =
          server.monitor[panel->monitor].width * panel->area.width / 100.0F;
    }

    if (panel->area.height + margin_vertical(&panel->margin) >
        server.monitor[panel->monitor].height) {
      panel->area.height =
        server.monitor[panel->monitor].height - margin_vertical(&panel->margin);
    }

    if (panel->area.bg->border.rounded > panel->area.width >> 1) {
      g_array_append_val(backgrounds, *panel->area.bg);
      panel->area.bg =
          &g_array_index(backgrounds, Background, backgrounds->len - 1);
      panel->area.bg->border.rounded = panel->area.width >> 2;
    }
  }

  // Horizontal contraints (LEFT, RIGHT and CENTER).
  if (panel_position & LEFT) {
    panel->location.x = server.monitor[panel->monitor].x + panel->margin.left;
  } else if (panel_position & RIGHT) {
    panel->location.x = server.monitor[panel->monitor].x +
                  server.monitor[panel->monitor].width - panel->area.width -
      panel->margin.right;
  } else {
    if (panel_horizontal)
      panel->location.x =
          server.monitor[panel->monitor].x +
          (server.monitor[panel->monitor].width - panel->area.width) / 2;
    else
      panel->location.x = server.monitor[panel->monitor].x + margin_horizontal(&panel->margin);
  }

  // Vertical constraints (TOP and LEFT).
  if (panel_position & TOP) {
    panel->location.y = server.monitor[panel->monitor].y + margin_vertical(&panel->margin);
  } else {
    panel->location.y = server.monitor[panel->monitor].y +
      server.monitor[panel->monitor].height - panel->area.height -
      margin_vertical(&panel->margin);
  }

  // autohide or strut_policy=minimum
  int diff = (panel_horizontal ? panel->area.height : panel->area.width) -
             panel_autohide_height;
  if (panel_horizontal) {
    panel->hidden_dimen.width = panel->area.width;
    panel->hidden_dimen.height = panel->area.height - diff;
  } else {
    panel->hidden_dimen.width = panel->area.width - diff;
    panel->hidden_dimen.height = panel->area.height;
  }
}

int resize_panel(void* obj) {
  resize_by_layout(obj, 0);

  // printf("resize_panel\n");
  if (panel_mode != MULTI_DESKTOP && taskbar_enabled) {
    // propagate width/height on hidden taskbar
    int i, width, height;
    Panel* panel = (Panel*)obj;
    width = panel->taskbar[server.desktop].area.width;
    height = panel->taskbar[server.desktop].area.height;
    for (i = 0; i < panel->nb_desktop; i++) {
      panel->taskbar[i].area.width = width;
      panel->taskbar[i].area.height = height;
      panel->taskbar[i].area.resize = 1;
    }
  }
  return 0;
}

void update_strut(Panel* p) {
  if (panel_strut_policy == STRUT_NONE) {
    XDeleteProperty(server.dsp, p->main_win, server.atom._NET_WM_STRUT);
    XDeleteProperty(server.dsp, p->main_win, server.atom._NET_WM_STRUT_PARTIAL);
    return;
  }

  // Reserved space
  unsigned int d1, screen_width, screen_height;
  Window d2;
  int d3;
  XGetGeometry(server.dsp, server.root_win, &d2, &d3, &d3, &screen_width,
               &screen_height, &d1, &d1);
  Monitor monitor = server.monitor[p->monitor];
  long struts[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  if (panel_horizontal) {
    int height = p->area.height + margin_vertical(&p->margin);
    if (panel_strut_policy == STRUT_MINIMUM ||
        (panel_strut_policy == STRUT_FOLLOW_SIZE && p->hidden))
      height = p->hidden_dimen.height;
    if (panel_position & TOP) {
      struts[2] = height + monitor.y;
      struts[8] = p->location.x;
      // p->area.width - 1 allowed full screen on monitor 2
      struts[9] = p->location.x + p->area.width - 1;
    } else {
      struts[3] = height + screen_height - monitor.y - monitor.height;
      struts[10] = p->location.x;
      // p->area.width - 1 allowed full screen on monitor 2
      struts[11] = p->location.x + p->area.width - 1;
    }
  } else {
    int width = p->area.width + margin_horizontal(&p->margin);
    if (panel_strut_policy == STRUT_MINIMUM ||
        (panel_strut_policy == STRUT_FOLLOW_SIZE && p->hidden))
      width = p->hidden_dimen.width;
    if (panel_position & LEFT) {
      struts[0] = width + monitor.x;
      struts[4] = p->location.y;
      // p->area.width - 1 allowed full screen on monitor 2
      struts[5] = p->location.y + p->area.height - 1;
    } else {
      struts[1] = width + screen_width - monitor.x - monitor.width;
      struts[6] = p->location.y;
      // p->area.width - 1 allowed full screen on monitor 2
      struts[7] = p->location.y + p->area.height - 1;
    }
  }
  // Old specification : fluxbox need _NET_WM_STRUT.
  XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_STRUT,
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&struts, 4);
  XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_STRUT_PARTIAL,
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&struts,
                  12);
}

void set_panel_items_order(Panel* p) {
  int k, j;

  if (p->area.list) {
    g_slist_free(p->area.list);
    p->area.list = NULL;
  }

  for (k = 0; k < strlen(panel_items_order); k++) {
    if (panel_items_order[k] == 'L')
      p->area.list = g_slist_append(p->area.list, &p->launcher);
    if (panel_items_order[k] == 'T') {
      for (j = 0; j < p->nb_desktop; j++)
        p->area.list = g_slist_append(p->area.list, &p->taskbar[j]);
    }
#ifdef ENABLE_BATTERY
    if (panel_items_order[k] == 'B')
      p->area.list = g_slist_append(p->area.list, &p->battery);
#endif
    if (panel_items_order[k] == 'S' && p == panel1) {
      // TODO : check systray is only on 1 panel
      // at the moment only on panel1[0] allowed
      p->area.list = g_slist_append(p->area.list, &systray);
    }
    if (panel_items_order[k] == 'C')
      p->area.list = g_slist_append(p->area.list, &p->clock);
  }
  init_rendering(&p->area, 0);
}

void set_panel_properties(Panel* p) {
  XStoreName(server.dsp, p->main_win, "tinto");

  gsize len;
  char* name = g_locale_to_utf8("tinto", -1, NULL, &len, NULL);
  if (name) {
    XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_NAME,
                    server.atom.UTF8_STRING, 8, PropModeReplace,
                    (unsigned char*)name, (int)len);
    free(name);
  }

  // Dock
  long val = server.atom._NET_WM_WINDOW_TYPE_DOCK;
  XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_WINDOW_TYPE,
                  XA_ATOM, 32, PropModeReplace, (unsigned char*)&val, 1);

  // Sticky and below other window
  val = ALLDESKTOP;
  XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_DESKTOP,
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&val, 1);
  Atom state[4];
  state[0] = server.atom._NET_WM_STATE_SKIP_PAGER;
  state[1] = server.atom._NET_WM_STATE_SKIP_TASKBAR;
  state[2] = server.atom._NET_WM_STATE_STICKY;
  state[3] = panel_layer == BOTTOM_LAYER ? server.atom._NET_WM_STATE_BELOW
                                         : server.atom._NET_WM_STATE_ABOVE;
  int nb_atoms = panel_layer == NORMAL_LAYER ? 3 : 4;
  XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_STATE, XA_ATOM,
                  32, PropModeReplace, (unsigned char*)state, nb_atoms);

  // Unfocusable
  XWMHints wmhints;
  if (panel_dock) {
    wmhints.icon_window = wmhints.window_group = p->main_win;
    wmhints.flags = StateHint | IconWindowHint;
    wmhints.initial_state = WithdrawnState;
  } else {
    wmhints.flags = InputHint;
    wmhints.input = False;
  }
  XSetWMHints(server.dsp, p->main_win, &wmhints);

  // Undecorated
  long prop[5] = {2, 0, 0, 0, 0};
  XChangeProperty(server.dsp, p->main_win, server.atom._MOTIF_WM_HINTS,
                  server.atom._MOTIF_WM_HINTS, 32, PropModeReplace,
                  (unsigned char*)prop, 5);

  // XdndAware - Register for Xdnd events
  Atom version = 4;
  XChangeProperty(server.dsp, p->main_win, server.atom.XdndAware, XA_ATOM, 32,
                  PropModeReplace, (unsigned char*)&version, 1);

  update_strut(p);

  // Fixed position and non-resizable window
  // Allow panel move and resize when tint2 reload config file
  int minwidth = panel_autohide ? p->hidden_dimen.width : p->area.width;
  int minheight = panel_autohide ? p->hidden_dimen.height : p->area.height;
  XSizeHints size_hints;
  size_hints.flags = PPosition | PMinSize | PMaxSize;
  size_hints.min_width = minwidth;
  size_hints.max_width = p->area.width;
  size_hints.min_height = minheight;
  size_hints.max_height = p->area.height;
  XSetWMNormalHints(server.dsp, p->main_win, &size_hints);

  // Set WM_CLASS
  XClassHint* classhint = XAllocClassHint();
  classhint->res_name = "tinto";
  classhint->res_class = "tinto";
  XSetClassHint(server.dsp, p->main_win, classhint);
  XFree(classhint);
}

void set_panel_background(Panel* p) {
  if (p->area.pix) XFreePixmap(server.dsp, p->area.pix);
  p->area.pix = XCreatePixmap(server.dsp, server.root_win, p->area.width,
                              p->area.height, server.depth);

  int xoff = 0, yoff = 0;
  if (panel_horizontal && panel_position & BOTTOM)
    yoff = p->area.height - p->hidden_dimen.height;
  else if (!panel_horizontal && panel_position & RIGHT)
    xoff = p->area.width - p->hidden_dimen.width;

  if (server.real_transparency) {
    clear_pixmap(p->area.pix, 0, 0, p->area.width, p->area.height);
  } else {
    get_root_pixmap();
    // copy background (server.root_pmap) in panel.area.pix
    Window dummy;
    int x = 0, y = 0;
    XTranslateCoordinates(server.dsp, p->main_win, server.root_win, 0, 0, &x,
                          &y, &dummy);
    if (panel_autohide && p->hidden) {
      x -= xoff;
      y -= yoff;
    }
    XSetTSOrigin(server.dsp, server.gc, -x, -y);
    XFillRectangle(server.dsp, p->area.pix, server.gc, 0, 0, p->area.width,
                   p->area.height);
  }

  // draw background panel
  cairo_surface_t* cs;
  cairo_t* c;
  cs = cairo_xlib_surface_create(server.dsp, p->area.pix, server.visual,
                                 p->area.width, p->area.height);
  c = cairo_create(cs);
  draw_background(&p->area, c);
  cairo_destroy(c);
  cairo_surface_destroy(cs);

  if (panel_autohide) {
    if (p->hidden_pixmap) XFreePixmap(server.dsp, p->hidden_pixmap);
    p->hidden_pixmap =
        XCreatePixmap(server.dsp, server.root_win, p->hidden_dimen.width,
                      p->hidden_dimen.height, server.depth);
    XCopyArea(server.dsp, p->area.pix, p->hidden_pixmap, server.gc, xoff, yoff,
              p->hidden_dimen.width, p->hidden_dimen.height, 0, 0);
  }

  // redraw panel's object
  GSList* l0;
  Area* a;
  for (l0 = p->area.list; l0; l0 = l0->next) {
    a = l0->data;
    set_redraw(a);
  }

  // reset task/taskbar 'state_pix'
  int i, k;
  Taskbar* tskbar;
  for (i = 0; i < p->nb_desktop; i++) {
    tskbar = &p->taskbar[i];
    for (k = 0; k < TASKBAR_STATE_COUNT; ++k) {
      if (tskbar->state_pix[k]) XFreePixmap(server.dsp, tskbar->state_pix[k]);
      tskbar->state_pix[k] = 0;
      if (tskbar->bar_name.state_pix[k])
        XFreePixmap(server.dsp, tskbar->bar_name.state_pix[k]);
      tskbar->bar_name.state_pix[k] = 0;
    }
    tskbar->area.pix = 0;
    tskbar->bar_name.area.pix = 0;
    l0 = tskbar->area.list;
    if (taskbarname_enabled) l0 = l0->next;
    for (; l0; l0 = l0->next) {
      set_task_redraw((Task*)l0->data);
    }
  }
}

Panel* get_panel(Window win) {
  for (int i = 0; i < nb_panel; ++i) {
    if (panel1[i].main_win == win) {
      return &panel1[i];
    }
  }
  return 0;
}

Taskbar* click_taskbar(Panel* panel, point_T point) {
  Taskbar* tskbar;
  int i;

  if (panel_horizontal) {
    for (i = 0; i < panel->nb_desktop; i++) {
      tskbar = &panel->taskbar[i];
      if (tskbar->area.on_screen && point.x >= tskbar->area.posx &&
          point.x <= (tskbar->area.posx + tskbar->area.width))
        return tskbar;
    }
  } else {
    for (i = 0; i < panel->nb_desktop; i++) {
      tskbar = &panel->taskbar[i];
      if (tskbar->area.on_screen && point.y >= tskbar->area.posy &&
          point.y <= (tskbar->area.posy + tskbar->area.height))
        return tskbar;
    }
  }
  return NULL;
}

Task* click_task(Panel* panel, point_T point) {
  GSList* l0;
  Taskbar* tskbar;

  if ((tskbar = click_taskbar(panel, point))) {
    if (panel_horizontal) {
      Task* tsk;
      l0 = tskbar->area.list;
      if (taskbarname_enabled) l0 = l0->next;
      for (; l0; l0 = l0->next) {
        tsk = l0->data;
        if (tsk->area.on_screen && point.x >= tsk->area.posx &&
            point.x <= (tsk->area.posx + tsk->area.width)) {
          return tsk;
        }
      }
    } else {
      Task* tsk;
      l0 = tskbar->area.list;
      if (taskbarname_enabled) l0 = l0->next;
      for (; l0; l0 = l0->next) {
        tsk = l0->data;
        if (tsk->area.on_screen && point.y >= tsk->area.posy &&
            point.y <= (tsk->area.posy + tsk->area.height)) {
          return tsk;
        }
      }
    }
  }
  return NULL;
}

Launcher* click_launcher(Panel* panel, point_T point) {
  Launcher* launcher = &panel->launcher;

  if (panel_horizontal) {
    if (launcher->area.on_screen && point.x >= launcher->area.posx &&
        point.x <= (launcher->area.posx + launcher->area.width)) {
      return launcher;
    }
  } else {
    if (launcher->area.on_screen && point.y >= launcher->area.posy &&
        point.y <= (launcher->area.posy + launcher->area.height)) {
      return launcher;
    }
  }
  return NULL;
}

LauncherIcon* click_launcher_icon(Panel* panel, point_T point) {
  GSList* l0;
  Launcher* launcher;

  // printf("Click x=%d y=%d\n", x, y);
  if ((launcher = click_launcher(panel, point))) {
    LauncherIcon* icon;
    for (l0 = launcher->list_icons; l0; l0 = l0->next) {
      icon = l0->data;
      if (point.x >= (launcher->area.posx + icon->x) &&
          point.x <= (launcher->area.posx + icon->x + icon->icon_size) &&
          point.y >= (launcher->area.posy + icon->y) &&
          point.y <= (launcher->area.posy + icon->y + icon->icon_size)) {
        return icon;
      }
    }
  }
  return NULL;
}

/* int click_padding(Panel *panel, int x, int y) */
/* { */
/* 	if (panel_horizontal) { */
/* 		if (x < panel->area.paddingxlr || x >
 * panel->area.width-panel->area.paddingxlr) */
/* 		return 1; */
/* 	} */
/* 	else { */
/* 		if (y < panel->area.paddingxlr || y >
 * panel->area.height-panel->area.paddingxlr) */
/* 		return 1; */
/* 	} */
/* 	return 0; */
/* } */

bool click_clock(Panel* panel, point_T point) {
  Clock clk = panel->clock;
  if (panel_horizontal) {
    if (clk.area.on_screen && point.x >= clk.area.posx &&
        point.x <= (clk.area.posx + clk.area.width))
      return true;
  } else {
    if (clk.area.on_screen && point.y >= clk.area.posy &&
        point.y <= (clk.area.posy + clk.area.height))
      return true;
  }
  return false;
}

Area* click_area(Panel* panel, point_T point) {
  Area* result = &panel->area;
  Area* new_result = result;
  do {
    result = new_result;
    GSList* it = result->list;
    while (it) {
      Area* a = it->data;
      if (a->on_screen && point.x >= a->posx &&
          point.x <= (a->posx + a->width) && point.y >= a->posy &&
          point.y <= (a->posy + a->height)) {
        new_result = a;
        break;
      }
      it = it->next;
    }
  } while (new_result != result);
  return result;
}

void stop_autohide_timeout(Panel* p) {
  if (p->autohide_timeout) {
    stop_timeout(p->autohide_timeout);
    p->autohide_timeout = 0;
  }
}

void autohide_show(void* p) {
  Panel* panel = (Panel*)p;
  stop_autohide_timeout(panel);
  panel->hidden = false;
  if (panel_strut_policy == STRUT_FOLLOW_SIZE) update_strut(p);

  XMapSubwindows(server.dsp, panel->main_win);  // systray windows
  if (panel_horizontal) {
    if (panel_position & TOP) {
      XResizeWindow(server.dsp, panel->main_win, panel->area.width,
                    panel->area.height);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win, panel->location.x, panel->location.y,
                        panel->area.width, panel->area.height);
    }
  } else {
    if (panel_position & LEFT) {
      XResizeWindow(server.dsp, panel->main_win, panel->area.width,
                    panel->area.height);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win, panel->location.x, panel->location.y,
                        panel->area.width, panel->area.height);
    }
  }
  refresh_systray = 1;  // ugly hack, because we actually only need to call
  // XSetBackgroundPixmap
  panel_refresh = 1;
}

void autohide_hide(void* p) {
  Panel* panel = (Panel*)p;
  stop_autohide_timeout(panel);
  panel->hidden = true;
  if (panel_strut_policy == STRUT_FOLLOW_SIZE) update_strut(p);

  XUnmapSubwindows(server.dsp, panel->main_win);  // systray windows
  int diff = (panel_horizontal ? panel->area.height : panel->area.width) -
             panel_autohide_height;

  if (panel_horizontal) {
    if (panel_position & TOP) {
      XResizeWindow(server.dsp, panel->main_win, panel->hidden_dimen.width,
                    panel->hidden_dimen.height);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win, panel->location.x,
                        panel->location.y + diff, panel->hidden_dimen.width,
                        panel->hidden_dimen.height);
    }
  } else {
    if (panel_position & LEFT) {
      XResizeWindow(server.dsp, panel->main_win, panel->hidden_dimen.width,
                    panel->hidden_dimen.height);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win, panel->location.x + diff,
                        panel->location.y, panel->hidden_dimen.width, panel->hidden_dimen.height);
    }
  }
  panel_refresh = 1;
}

void autohide_trigger_show(Panel* p) {
  if (!p) return;

  if (p->autohide_timeout) {
    change_timeout(p->autohide_timeout, panel_autohide_show_timeout, 0,
                   autohide_show, p);
  } else {
    p->autohide_timeout =
        add_timeout(panel_autohide_show_timeout, 0, autohide_show, p);
  }
}

void autohide_trigger_hide(Panel* p) {
  if (!p) return;

  Window root, child;
  int xr, yr, xw, yw;
  unsigned int mask;
  if (XQueryPointer(server.dsp, p->main_win, &root, &child, &xr, &yr, &xw, &yw,
                    &mask)) {
    if (child) return;  // mouse over one of the system tray icons
  }

  if (p->autohide_timeout) {
    change_timeout(p->autohide_timeout, panel_autohide_hide_timeout, 0,
                   autohide_hide, p);
  } else {
    p->autohide_timeout =
        add_timeout(panel_autohide_hide_timeout, 0, autohide_hide, p);
  }
}

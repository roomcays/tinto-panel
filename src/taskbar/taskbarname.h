/**************************************************************************
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr)
**************************************************************************/

#ifndef TASKBARNAME_H
#define TASKBARNAME_H

#include "common.h"
#include "area.h"
#include "color.h"

extern int taskbarname_enabled;
extern PangoFontDescription* taskbarname_font_desc;
extern color_T taskbarname_font;
extern color_T taskbarname_active_font;

void default_taskbarname(void);
void cleanup_taskbarname(void);

void init_taskbarname_panel(void* p);

void draw_taskbarname(void* obj, cairo_t* c);

int resize_taskbarname(void* obj);

#endif

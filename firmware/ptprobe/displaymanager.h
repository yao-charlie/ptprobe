#ifndef _displaymanager_h
#define _displaymanager_h

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "datatile.h"

class DisplayManager
{
public:

  DisplayManager(int w=128, int h=64, int addr=0x3C);

  void set_title_font(GFXfont const* f) { title_canvas_.setFont(f); }
  void set_subtitle_font(GFXfont const* f) { subtitle_canvas_.setFont(f); }

  bool begin(const char* title, const char* subtitle);
  void clear_all();

  void update_title(const char* msg);
  void update_subtitle(const char* msg);

  DataTile& data_rect(int row, int col) { return data_canvas_[row + 4*col]; }

  void show() { display_.display(); }

private:
  void update_rect(int x, int y, const char* msg, int lmargin, GFXcanvas1& canvas);

  GFXcanvas1 title_canvas_;
  GFXcanvas1 subtitle_canvas_;

  DataTile data_canvas_[8];
  
  Adafruit_SSD1306 display_;
  int addr_;
  int left_margin_;
};


#endif //_displaymanager_h

#ifndef datatile_h_
#define datatile_h_

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DataTile 
{
public:
  DataTile();
  
  void init(int ulx, int uly, Adafruit_SSD1306* disp=nullptr);

  void update_data(float val, bool outline=false);
  void update_data(char const* msg, bool outline=false);

  void update_lbl_hi(char const* msg, bool outline=false);
  void update_lbl_lo(char const* msg, bool outline=false);

private:
  void update_rect(int x, int y, const char* msg, int lmargin, bool outline, GFXcanvas1& canvas);

  int ulx_; // upper left corner x
  int uly_; // upper left corner y
  
  GFXcanvas1 lbl_hi_canvas_;  // upper label canvas
  GFXcanvas1 lbl_lo_canvas_;  // lower label canvas
  GFXcanvas1 data_canvas_;    // data value canvas
  
  Adafruit_SSD1306* display_;

  
};


#endif //datatile_h_

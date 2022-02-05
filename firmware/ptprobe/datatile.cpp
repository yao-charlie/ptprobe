#include "datatile.h"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/Picopixel.h>

DataTile::DataTile()
: ulx_(0), uly_(0), display_(nullptr), 
  lbl_hi_canvas_(18,8), lbl_lo_canvas_(18,8), data_canvas_(46,16)
{
  lbl_hi_canvas_.setFont(&Picopixel);
  lbl_hi_canvas_.setTextSize(1);
  lbl_hi_canvas_.setTextColor(SSD1306_WHITE);

  lbl_lo_canvas_.setFont(&Picopixel);
  lbl_lo_canvas_.setTextSize(1);
  lbl_lo_canvas_.setTextColor(SSD1306_WHITE);

  data_canvas_.setFont(&FreeSans9pt7b);
  data_canvas_.setTextSize(1);
  data_canvas_.setTextColor(SSD1306_WHITE);
}

void DataTile::init(int ulx, int uly, Adafruit_SSD1306* disp)
{
  ulx_ = ulx;
  uly_ = uly;
  display_ = disp; 
}

void DataTile::update_rect(
    int const x, 
    int const y, 
    const char* msg, 
    int const lmargin,
    bool const outline,  
    GFXcanvas1& canvas)
{
  canvas.fillScreen(SSD1306_BLACK);
  if (outline) {
    canvas.drawFastHLine(lmargin, canvas.height()-1, canvas.width()-3,SSD1306_WHITE);
  }
  canvas.setCursor(lmargin, canvas.height()-3);
  canvas.print(msg);

  display_->drawBitmap(x,y,canvas.getBuffer(),canvas.width(),canvas.height(),
      SSD1306_WHITE,SSD1306_BLACK);
}

void DataTile::update_data(const char* msg, bool outline /* = false*/)
{
  update_rect(ulx_ + lbl_hi_canvas_.width(), uly_, msg, 1, outline, data_canvas_);
}

void DataTile::update_data(float val, bool outline /* = false*/)
{
  String sval (val,1);
  update_data(sval.c_str(), outline);
}

void DataTile::update_lbl_hi(const char* msg, bool outline /* = false*/)
{
  update_rect(ulx_, uly_, msg, 2, outline, lbl_hi_canvas_);
}

void DataTile::update_lbl_lo(const char* msg, bool outline /* = false*/)
{
  update_rect(ulx_, uly_ + lbl_hi_canvas_.height(), msg, 2, outline, lbl_lo_canvas_);
}

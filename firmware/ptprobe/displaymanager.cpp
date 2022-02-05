#include "displaymanager.h"
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

DisplayManager::DisplayManager(
    int const w /*=128*/,
    int const h /*=64*/,
    int const addr /*=0x3C*/) 
    : title_canvas_(w,FreeSans12pt7b.yAdvance-6), subtitle_canvas_(w,FreeSans9pt7b.yAdvance), 
    display_(w,h,&Wire,OLED_RESET), addr_(addr), left_margin_(0)
{
  title_canvas_.setFont(&FreeSans12pt7b);
  title_canvas_.setTextSize(1);
  title_canvas_.setTextColor(SSD1306_WHITE);

  subtitle_canvas_.setFont(&FreeSans9pt7b);
  subtitle_canvas_.setTextSize(1);
  subtitle_canvas_.setTextColor(SSD1306_WHITE);

  for (int ico = 0; ico < 2; ++ico) {
    for (int iro = 0; iro < 4; ++iro) {
      data_canvas_[iro + 4*ico].init((w/2)*ico, (h/4)*iro, &display_);
    }
  }
}

bool DisplayManager::begin(const char* title, const char* subtitle)
{
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display_.begin(SSD1306_SWITCHCAPVCC, addr_)) {
    return false;
  }
  display_.clearDisplay();
  update_title(title);
  update_subtitle(subtitle);
  display_.display();
  return true;
}

void DisplayManager::clear_all() 
{
  display_.clearDisplay();
}

void DisplayManager::update_rect(
    int const x, 
    int const y, 
    const char* msg, 
    int const lmargin,  
    GFXcanvas1& canvas)
{
  canvas.fillScreen(SSD1306_BLACK);
  canvas.setCursor(lmargin, canvas.height()-4);
  canvas.print(msg);

  display_.drawBitmap(x,y,canvas.getBuffer(),canvas.width(),canvas.height(),
      SSD1306_WHITE,SSD1306_BLACK);
}

void DisplayManager::update_title(const char* msg)
{
  update_rect(0,0,msg,left_margin_,title_canvas_);
}

void DisplayManager::update_subtitle(const char* msg)
{
  update_rect(0,title_canvas_.height(),msg,left_margin_+4,subtitle_canvas_);
}

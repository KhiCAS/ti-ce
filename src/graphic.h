#ifndef GRAPHIC_H
#define GRAPHIC_H


#define VIR_LCD_PIX_W   320
#define VIR_LCD_PIX_H   240
#define GL_WHITE        127

#ifdef __cplusplus
extern "C" {
#endif
  void vGL_Initialize() ;
  inline void gfx_Begin(){ vGL_Initialize(); }
  void vGL_End() ;
  inline void gfx_End(){ vGL_End(); }

  void vGL_putString(int x0, int y0, const char *s, unsigned char fg, unsigned char bg, int fontSize);
  void vGL_reverseArea(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1) ;
  void vGL_clearArea(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1) ;
  void vGL_set_pixel(unsigned x,unsigned y,int c);
  int vGL_get_pixel(unsigned int x,unsigned int y);
  void vGL_setArea(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned int color) ;
  
#ifdef __cplusplus
}
#endif


#endif // GRAPHIC_H

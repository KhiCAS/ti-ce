// Device side of the MathPrint-style input (mathinput.cc): draws a layout with KhiCAS's fonts,
// in the style of the results (kdisplay.cc), so input and output look the same.
#include <string.h>
#include "console.h"
#include "main.h"
#include "mathinput.h" // last: it maps std to ustl on the calculator

namespace xcas {
  void text_print(int fontsize,const char * s,int x,int y,int c,int bg,int mode); // kdisplay.cc
}

// KhiCAS fonts: big = text_print size 18 (8 px per char; glyphs up to 11 px above and 3 below
// the baseline, so a plain line fits a 15 px console row), small = size 12 (exponents, bounds:
// 5 px per char). text_print's y is not the baseline: it is 4 (big) / 3 (small) px below it.
extern const mi_metrics mi_device_metrics={{8,11,3},{5,7,2},2,0,0,0,0,0,0}; // extern: a const is internal otherwise

static const int MI_INK=COLOR_BLACK;

// line clipped to the box [x0,x1) x [y0,y1) (the input area); only axis-aligned clipping
// of the endpoints is needed: shapes are drawn whole or not at all
static void mi_line(int xa,int ya,int xb,int yb,int x0,int y0,int x1,int y1,int c){
  if (xa<x0 || xb<x0 || xa>=x1 || xb>=x1 || ya<y0 || yb<y0 || ya>=y1 || yb>=y1)
    return;
  draw_line(xa,ya,xb,yb,c);
}

// draws layout L of buffer s with its baseline at screen y=by and its left edge at x=bx,
// inside the box [x0,x1) x [y0,y1); caret: draw the caret too; selbg >= 0: the selection's box
// (L.sx..sh) on a background of that color
void mi_draw(const mi_layout & L,const char * s,int bx,int by,int x0,int y0,int x1,int y1,bool caret,int selbg){
  const mi_metrics & m=mi_device_metrics;
  int sx0=0,sy0=0,sx1=0,sy1=0;
  if (selbg>=0 && L.sw>0){ // 1 px around the box, clipped to the area
    sx0=bx+L.sx-1; sy0=by+L.sy-1; sx1=bx+L.sx+L.sw+1; sy1=by+L.sy+L.sh+1;
    if (sx0<x0) sx0=x0;
    if (sy0<y0) sy0=y0;
    if (sx1>x1) sx1=x1;
    if (sy1>y1) sy1=y1;
    if (sx1>sx0 && sy1>sy0)
      drawRectangle(sx0,sy0,sx1-sx0,sy1-sy0,selbg);
  }
  for (size_t k=0;k<L.ops.size();++k){
    const mi_op & o=L.ops[k];
    const int x=bx+o.x,y=by+o.y,w=o.w,h=o.h;
    switch (o.code){
    case MI_TEXT: {
      const mi_font & f=o.small?m.small:m.big;
      char buf[40];
      const char * t=o.lit;
      if (!t){
        const int n=o.len<int(sizeof(buf))-1?o.len:int(sizeof(buf))-1;
        memcpy(buf,s+o.pos,n);
        buf[n]=0;
        t=buf;
      }
      if (x<x0 || x+o.w>x1 || y-f.asc<y0 || y+f.desc>y1)
        break; // text is drawn whole or not at all
      if (o.lit && (!strcmp(o.lit,"pi") || !strcmp(o.lit,"oo"))){
        // drawn here in one glyph cell: text_print's pi is ~12 px wide and overlapped
        // the next glyph, oo would be two letters
        const int a=f.adv,s2=o.small?1:2,top=y-(o.small?6:8);
        if (o.lit[0]=='p'){ // pi: a bar on two legs, digit height
          drawRectangle(x,top,a-1,s2,MI_INK);
          drawRectangle(x+(o.small?1:1),top,s2,y-top,MI_INK);
          drawRectangle(x+a-1-s2-(o.small?0:1),top,s2,y-top,MI_INK);
        }
        else { // infinity: two loops crossing in the middle (7x5, small: 5x3), on the math axis
          static const char * const big[5]={".#...#.","#.#.#.#","#..#..#","#.#.#.#",".#...#."};
          static const char * const sml[3]={"##.##","#.#.#","##.##"};
          const char * const * pat=o.small?sml:big;
          const int nr=o.small?3:5,m=y-(o.small?3:4);
          for (int r=0;r<nr;++r){
            for (int c=0;pat[r][c];++c){
              if (pat[r][c]=='#')
                drawRectangle(x+c,m-nr/2+r,1,1,MI_INK);
            }
          }
        }
        break;
      }
      const bool insel=sx1>sx0 && x>=sx0 && x+o.w<=sx1 && y-f.asc>=sy0 && y+f.desc<=sy1;
      int ty=y+(o.small?3:4); // text_print's y: 4 (big) / 3 (small) px below the baseline
      if (t[0]==0x1e && !t[1]) // the arrow (lim, taylor): its shaft on the math axis, not on the baseline
        ty-=o.small?2:4;
      xcas::text_print(o.small?12:18,t,x,ty,MI_INK,insel?selbg:COLOR_WHITE,0);
      break;
    }
    case MI_DOT:
      if (x>=x0 && x+w<=x1 && y>=y0 && y+h<=y1)
        drawRectangle(x+w/2-1,y+h/2-1,2,2,MI_INK);
      break;
    case MI_HLINE: // fraction bar, 2 px like the results'
      mi_line(x,y,x+w-1,y,x0,y0,x1,y1,MI_INK);
      mi_line(x,y+1,x+w-1,y+1,x0,y0,x1,y1,MI_INK);
      break;
    case MI_SQRT: { // radical sign (6 px) and overbar, doubled like the results'
      const int ym=y+(2*h)/3;
      for (int d=0;d<2;++d){
        mi_line(x,ym+d,x+2,y+h-1,x0,y0,x1,y1,MI_INK);
        mi_line(x+2,y+h-1,x+5,y+d,x0,y0,x1,y1,MI_INK);
        mi_line(x+5,y+d,x+w-1,y+d,x0,y0,x1,y1,MI_INK);
      }
      break;
    }
    case MI_INTEGRAL: { // a tall S: stem, hooks at both ends, 3 px beyond the box each way
      const int ya=y-3>=y0?y-3:y0,yz=y+h+2<y1?y+h+2:y1-1;
      mi_line(x+3,ya+2,x+3,yz-2,x0,y0,x1,y1,MI_INK);
      mi_line(x+4,ya+2,x+4,yz-2,x0,y0,x1,y1,MI_INK);
      mi_line(x+4,ya+1,x+5,ya,x0,y0,x1,y1,MI_INK);
      mi_line(x+5,ya,x+7,ya,x0,y0,x1,y1,MI_INK);
      mi_line(x+2,yz-1,x+3,yz-1,x0,y0,x1,y1,MI_INK);
      mi_line(x,yz,x+2,yz,x0,y0,x1,y1,MI_INK);
      break;
    }
    case MI_SIGMA:
      mi_line(x,y,x+w-1,y,x0,y0,x1,y1,MI_INK);
      mi_line(x,y+h-1,x+w-1,y+h-1,x0,y0,x1,y1,MI_INK);
      mi_line(x,y,x+w/2,y+h/2,x0,y0,x1,y1,MI_INK);
      mi_line(x+w/2,y+h/2,x,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    case MI_LPAREN: case MI_RPAREN: { // curved: 3 segments, 2 px apart at the middle
      const bool l=o.code==MI_LPAREN;
      const int xo=l?x+w-2:x+1,xi=l?x+1:x+w-2,q=h/4;
      mi_line(xo,y,xi,y+q,x0,y0,x1,y1,MI_INK);
      mi_line(xi,y+q,xi,y+h-1-q,x0,y0,x1,y1,MI_INK);
      mi_line(xi,y+h-1-q,xo,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    }
    case MI_LBRACKET: case MI_RBRACKET: {
      const bool l=o.code==MI_LBRACKET;
      const int xs=l?x+1:x+w-2,xe=l?x+w-1:x;
      mi_line(xs,y,xs,y+h-1,x0,y0,x1,y1,MI_INK);
      mi_line(xs,y,xe,y,x0,y0,x1,y1,MI_INK);
      mi_line(xs,y+h-1,xe,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    }
    case MI_LBRACE: case MI_RBRACE: {
      const bool l=o.code==MI_LBRACE;
      const int xo=l?x+w-1:x,xm=l?x+w/2:x+w/2,xi=l?x:x+w-1,ym=y+h/2;
      mi_line(xo,y,xm,y+1,x0,y0,x1,y1,MI_INK);
      mi_line(xm,y+1,xm,ym-1,x0,y0,x1,y1,MI_INK);
      mi_line(xm,ym-1,xi,ym,x0,y0,x1,y1,MI_INK);
      mi_line(xi,ym,xm,ym+1,x0,y0,x1,y1,MI_INK);
      mi_line(xm,ym+1,xm,y+h-2,x0,y0,x1,y1,MI_INK);
      mi_line(xm,y+h-2,xo,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    }
    case MI_BAR:
      mi_line(x+w/2,y,x+w/2,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    case MI_BOX: // an empty slot: a light frame, like an online calculator's placeholder
      mi_line(x,y,x+w-1,y,x0,y0,x1,y1,COLOR_BLUE);
      mi_line(x,y+h-1,x+w-1,y+h-1,x0,y0,x1,y1,COLOR_BLUE);
      mi_line(x,y,x,y+h-1,x0,y0,x1,y1,COLOR_BLUE);
      mi_line(x+w-1,y,x+w-1,y+h-1,x0,y0,x1,y1,COLOR_BLUE);
      break;
    }
  }
  if (caret){
    const int cx=bx+L.cx,cy=by+L.cy;
    if (cx>=x0 && cx+2<=x1 && cy>=y0 && cy+L.ch<=y1)
      drawRectangle(cx,cy,2,L.ch,MI_INK);
  }
}

// ---------------------------------------------------------------- the console history in 2D
// (console.cc): an input or a result of k rows is its line (the giac text, for copy, recall and
// the saved session) followed by k-1 LINE_TYPE_CONT lines. Results: implicit products drawn as
// such (2*x: 2x), the user's groups around a numerator or a denominator not drawn.
bool mi_python_mode(); // mathinput_eval.cc
bool mi_hist2d=true; // the history drawn in 2D (Config menu, console.cc); 1D: more lines visible
static void mi_hist_metrics(mi_metrics & m,bool output){
  m=mi_device_metrics;
  if (output)
    m.flags|=MI_F_IMPLDOT|MI_F_TIDY;
}
// rows of rowh pixels that history line s takes in 2D, at most maxrows; 1: drawn as text
int mi_hist_rows(const char * s,bool output,int rowh,int maxrows){
  if (!s || !s[0] || !mi_hist2d)
    return 1;
  if (mi_python_mode())
    return 1;
  const int n=strlen(s);
  if (n>512)
    return 1;
  for (int i=0;i<n;++i){
    if (s[i]=='\n' || s[i]=='"' || s[i]==1) // programs, strings: as text
      return 1;
  }
  mi_metrics m;
  mi_hist_metrics(m,output);
  mi_layout L;
  mi_build(s,n,-1,m,L);
  if (L.width>LCD_WIDTH_PX-6)
    return 1; // too wide: the text, scrolled with the arrows
  int k=(L.asc+L.desc+1+rowh-1)/rowh; // 1 px margin: a plain line takes one row
  if (k<1)
    k=1;
  if (k>maxrows)
    k=1;
  return k;
}
// draws history line s in 2D in the block [top,top+height) (an input left-aligned, a result
// right-aligned), nothing outside [ymin,ymax); false if it does not fit (drawn as text then)
bool mi_hist_draw(const char * s,bool output,int top,int height,int ymin,int ymax){
  mi_metrics m;
  mi_hist_metrics(m,output);
  mi_layout L;
  mi_build(s,strlen(s),-1,m,L);
  if (L.asc+L.desc+1>height || L.width>LCD_WIDTH_PX-4)
    return false;
  const int bx=output?LCD_WIDTH_PX-4-L.width:2,by=top+(height-(L.asc+L.desc))/2+L.asc;
  mi_draw(L,s,bx,by,0,ymin,LCD_WIDTH_PX,ymax,false,-1);
  return true;
}

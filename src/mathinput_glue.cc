// Device side of the MathPrint-style input (mathinput.cc): draws a layout with KhiCAS's fonts,
// in the style of the results (kdisplay.cc), so input and output look the same.
#include <string.h>
#include "console.h"
#include "main.h"
#include <ctype.h>
#include "mathinput.h" // last: it maps std to ustl on the calculator
#include <sys/lcd.h> // lcd_Ram: the screen, 8 bits per pixel (graph thumbnails)

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
      // narrow (the small font: 3 px) the curve uses the whole width, else it would be a bar
      const int xo=w<5?(l?x+w-1:x):(l?x+w-2:x+1),xi=w<5?(l?x:x+w-1):(l?x+1:x+w-2),q=h/4;
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

// ---------------------------------------------------------------- graph thumbnails
// A graph in the history is a line: 0x80, the format 0x01 (1 bit per pixel), the width and the
// height on 3 digits each ("160105"), the text to copy (the command) ended by 0xFF, then the
// pixels (row by row, no 0 byte): 0x80..0xff: 7 pixels (bit 6 first, 1: black); 0x07..0x7f:
// v-7 white pixels then a black one; 0x01..0x06: v*128 white pixels; the last white ones omitted.
// The graph on the screen (screen rows MI_TH_Y0.. below the status area) is reduced 2x2.
static const int MI_TH_W=160,MI_TH_H=105,MI_TH_Y0=30;
// a pixel of the screen (rgb 3-3-2 palette) drawn black in a thumbnail: not white nor light grey
static inline bool mi_th_dark(unsigned char c){
  return (c&0x60)!=0x60 || (c&0x1c)<0x18 || (c&3)!=3;
}
// pixel p of the thumbnail dark; the row (y: screen row of row ry) moves with p, no division
struct mi_th_scan {
  const unsigned char * y;
  int ry;
};
static bool mi_th_px(mi_th_scan & S,int p){
  while (p>=S.ry+MI_TH_W){
    S.ry+=MI_TH_W;
    S.y+=2*LCD_WIDTH_PX;
  }
  while (p<S.ry){
    S.ry-=MI_TH_W;
    S.y-=2*LCD_WIDTH_PX;
  }
  const unsigned char * r=S.y+2*(p-S.ry);
  return mi_th_dark(r[0]) || mi_th_dark(r[1]) || mi_th_dark(r[LCD_WIDTH_PX]) || mi_th_dark(r[LCD_WIDTH_PX+1]);
}
// size of the thumbnail s; false if s is not a thumbnail
bool mi_thumb_size(const char * s,int * w,int * h){
  const unsigned char * u=(const unsigned char *)s;
  if (!u || u[0]!=0x80 || u[1]!=1)
    return false;
  for (int i=2;i<8;++i){
    if (u[i]<'0' || u[i]>'9')
      return false;
  }
  *w=(u[2]-'0')*100+(u[3]-'0')*10+(u[4]-'0');
  *h=(u[5]-'0')*100+(u[6]-'0')*10+(u[7]-'0');
  return *w>0 && *h>0;
}
// the text of the thumbnail s (its command): its start, its length in *len; 0 if not a thumbnail
const char * mi_thumb_text(const char * s,int * len){
  int w,h;
  if (!mi_thumb_size(s,&w,&h))
    return 0;
  const char * t=s+8,* e=t;
  while (*e && (unsigned char)*e!=0xff)
    ++e;
  *len=e-t;
  return t;
}
// the graph on the screen as a thumbnail with no text (malloc'ed, 0 ended); 0 if no memory
char * mi_thumb_capture(){
  const int n=MI_TH_W*MI_TH_H;
  unsigned char * o=(unsigned char *)malloc(n/7+14); // the worst case: 7 pixels per byte
  if (!o)
    return 0;
  o[0]=0x80;
  o[1]=1;
  o[2]='0'+MI_TH_W/100; o[3]='0'+MI_TH_W/10%10; o[4]='0'+MI_TH_W%10;
  o[5]='0'+MI_TH_H/100; o[6]='0'+MI_TH_H/10%10; o[7]='0'+MI_TH_H%10;
  o[8]=0xff; // no text yet (console_output_thumb)
  int k=9;
  // pixel p dark: one of its 4 screen pixels
  mi_th_scan scr={(const unsigned char *)lcd_Ram+MI_TH_Y0*LCD_WIDTH_PX,0};
  for (int p=0;p<n;){
    int w=0;
    while (p+w<n && !mi_th_px(scr,p+w))
      ++w;
    if (p+w>=n) // the last white pixels
      break;
    if (w>=128){
      const int c=w/128>6?6:w/128;
      o[k++]=c;
      p+=c*128;
      continue;
    }
    if (w>=6 && w<=120){
      o[k++]=7+w;
      p+=w+1;
      continue;
    }
    int v=0x80;
    for (int j=0;j<7;++j){
      if (p+j<n && mi_th_px(scr,p+j))
        v|=0x40>>j;
    }
    o[k++]=v;
    p+=7;
  }
  if (is_wchar(o[k-1])) // the console's length count steps over 2 bytes there: not past the end
    o[k++]=0x80;
  o[k]=0;
  unsigned char * r=(unsigned char *)realloc(o,k+1); // the heap back
  return (char *)(r?r:o);
}
// draws the thumbnail s with its top left corner at (x0,y0), its rows outside [ymin,ymax) not
void mi_thumb_draw(const char * s,int x0,int y0,int ymin,int ymax){
  int w,h,len;
  if (!mi_thumb_size(s,&w,&h) || !mi_thumb_text(s,&len))
    return;
  const unsigned char * u=(const unsigned char *)s+8+len;
  if (*u!=0xff)
    return;
  ++u;
  // written in the screen memory (8 bits per pixel), the position followed without division
  unsigned char * const scr=(unsigned char *)lcd_Ram;
  const unsigned char ink=convertcolor(COLOR_BLACK);
  int x=0,y=0; // the current pixel
  for (;*u && y<h;++u){
    int v=*u,k; // k: white pixels before a black one (-1: none)
    if (v>=0x80){ // 7 pixels
      for (int j=0;j<7;++j){
        if (v&(0x40>>j)){
          const int Y=y0+y,X=x0+x;
          if (y<h && Y>=ymin && Y<ymax && X>=0 && X<LCD_WIDTH_PX)
            scr[Y*LCD_WIDTH_PX+X]=ink;
        }
        if (++x==w){
          x=0;
          ++y;
        }
      }
      continue;
    }
    k=v>=7?v-7:v*128;
    for (x+=k;x>=w;x-=w)
      ++y;
    if (v>=7){ // the black pixel
      const int Y=y0+y,X=x0+x;
      if (y<h && Y>=ymin && Y<ymax && X>=0 && X<LCD_WIDTH_PX)
        scr[Y*LCD_WIDTH_PX+X]=ink;
      if (++x==w){
        x=0;
        ++y;
      }
    }
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
// the layout of history line s: a result too wide in the normal font is laid out in the small
// one (MI_F_SMALL); still too wide, it is scrolled with the arrows (mi_hist_hscroll)
static const int MI_HSTEP=16; // pixels per arrow press
// a large result costs up to 4 layouts (the static pool overflows: again in the big one; too
// wide: again in the small font), at each redraw: what the last such results needed is
// remembered (by a hash of the text), they are laid out once
struct mi_hist_memo { unsigned long h; int n; int flags, ad; }; // ad: asc+desc
static mi_hist_memo mi_memo[4];
static int mi_memo_next;
static unsigned long mi_hash(const char * s,int n){
  unsigned long h=2166136261UL;
  for (int i=0;i<n;++i)
    h=((h^(unsigned char)s[i])*16777619UL)&0xffffffffUL;
  return h;
}
static void mi_hist_layout(const char * s,int n,bool output,mi_metrics & m,mi_layout & L){
  mi_hist_metrics(m,output);
  if (!output){
    mi_build(s,n,-1,m,L);
    return;
  }
  const unsigned long h=mi_hash(s,n);
  for (int i=0;i<4;++i){
    if (mi_memo[i].n==n && mi_memo[i].h==h){
      m.flags|=mi_memo[i].flags;
      mi_build(s,n,-1,m,L);
      return;
    }
  }
  mi_build(s,n,-1,m,L);
  if (L.width>LCD_WIDTH_PX-6){
    m.flags|=MI_F_SMALL;
    mi_build(s,n,-1,m,L);
  }
  const int f=(m.flags & MI_F_SMALL)|(mi_build_big?MI_F_BIG:0);
  if (f){
    mi_memo[mi_memo_next].h=h;
    mi_memo[mi_memo_next].n=n;
    mi_memo[mi_memo_next].flags=f;
    mi_memo[mi_memo_next].ad=L.asc+L.desc;
    mi_memo_next=(mi_memo_next+1)%4;
  }
}
// rows of rowh pixels that line s takes in 2D (at most maxrows), 0: drawn as text (Python,
// programs, strings, too wide or too tall)
// a plain text (a message, a print output: "Session saved") and not a result: two words separated
// by blanks, outside the strings; giac's results have blanks only around a few operators
static bool mi_hist_words(const char * s,int n){
  static const char * const kw[]={"and","or","xor","not","mod","div","union","intersect","minus",0};
  bool inq=false;
  for (int i=0;i<n;++i){
    if (s[i]=='"')
      inq=!inq;
    if (inq || s[i]!=' ' || i==0)
      continue;
    int a=i,b=i;
    while (a>0 && (isalnum((unsigned char)s[a-1]) || s[a-1]=='_'))
      --a;
    while (b<n && s[b]==' ')
      ++b;
    int e=b;
    while (e<n && (isalnum((unsigned char)s[e]) || s[e]=='_'))
      ++e;
    if (a==i || e==b || isdigit((unsigned char)s[a]) || isdigit((unsigned char)s[b]))
      continue; // not two words (a blank near an operator, a number)
    bool op=false;
    for (int k=0;kw[k];++k){
      const int l=strlen(kw[k]);
      if ((i-a==l && !strncmp(s+a,kw[k],l)) || (e-b==l && !strncmp(s+b,kw[k],l)))
        op=true;
    }
    if (!op)
      return true;
  }
  return false;
}
static int mi_hist_k(const char * s,bool output,int rowh,int maxrows){
  if (!s || !s[0] || !mi_hist2d || mi_python_mode())
    return 0;
  const int n=strlen(s);
  if (n>1024)
    return 0;
  for (int i=0;i<n;++i){
    if (s[i]==1 || (s[i]=='\n' && !(output && s[0]=='['))) // programs: as text (giac prints a
      return 0;                                    // matrix result with a newline after each row)
    if (s[i]=='"' && (!output || i==0)) // an input with a string, a string result: as text; strings
      return 0;                         // inside a result (tabvar's cells): drawn, without quotes
  }
  int b0=0; // the first non blank
  while (b0<n && (s[b0]==' ' || s[b0]=='\t'))
    ++b0;
  if ((s[b0]=='/' && s[b0+1]=='/') || mi_hist_words(s,n) || (output && s[b0]=='\'')) // a comment
    return 0; // (// Success, also loaded as an input from a session), a message, a Python string: as text
  mi_metrics m;
  mi_layout L;
  mi_hist_layout(s,n,output,m,L);
  if (!output && L.width>LCD_WIDTH_PX-6)
    return 0; // an input too wide: the text, scrolled with the arrows (a result: scrolled in 2D)
  int k=(L.asc+L.desc+1+rowh-1)/rowh; // 1 px margin: a plain line takes one row
  if (k<1)
    k=1;
  return k>maxrows?0:k;
}
// rows of rowh pixels that history line s takes in 2D, at most maxrows; 1: drawn as text
int mi_hist_rows(const char * s,bool output,int rowh,int maxrows){
  if (!s || !s[0] || !mi_hist2d)
    return 1;
  int tw,th;
  if (mi_thumb_size(s,&tw,&th)) // a graph thumbnail
    return (th+rowh-1)/rowh<=maxrows?(th+rowh-1)/rowh:1;
  const int k=mi_hist_k(s,output,rowh,maxrows);
  return k?k:1;
}
// result s is drawn in 2D in the history (check_do_graph: the 2D editor need not show it)
bool mi_hist_shows(const char * s,int rowh,int maxrows){
  return mi_hist_k(s,true,rowh,maxrows)>0;
}
// draws history line s in 2D in the block [top,top+height) (an input left-aligned, a result
// right-aligned), nothing outside [ymin,ymax); false if it does not fit (drawn as text then)
// 2D line s drawn on height px reaches its first or last row (where the selection's frame is)
bool mi_hist_tight(const char * s,bool output,int height){
  int tw,th;
  if (mi_thumb_size(s,&tw,&th))
    return true;
  const int n=strlen(s);
  if (output){ // a large result: known without laying it out
    const unsigned long h=mi_hash(s,n);
    for (int i=0;i<4;++i){
      if (mi_memo[i].n==n && mi_memo[i].h==h)
        return mi_memo[i].ad+1>=height;
    }
  }
  mi_metrics m;
  mi_layout L;
  mi_hist_layout(s,n,output,m,L);
  return L.asc+L.desc+1>=height;
}
// arrow presses that scroll result s, drawn in 2D, to its right end (0: it fits the screen)
int mi_hist_hscroll(const char * s,bool output){
  int tw,th;
  if (!s || !s[0] || !output || mi_thumb_size(s,&tw,&th))
    return 0;
  mi_metrics m;
  mi_layout L;
  mi_hist_layout(s,strlen(s),output,m,L);
  const int over=L.width-(LCD_WIDTH_PX-16);
  return over>0?(over+MI_HSTEP-1)/MI_HSTEP:0;
}
// scroll: arrow presses to the right (a result wider than the screen; clamped)
bool mi_hist_draw(const char * s,bool output,int top,int height,int ymin,int ymax,int scroll){
  int tw,th;
  if (mi_thumb_size(s,&tw,&th)){
    if (th>height || tw>LCD_WIDTH_PX)
      return false;
    mi_thumb_draw(s,LCD_WIDTH_PX-4-tw,top+(height-th)/2,ymin,ymax); // right-aligned as the results
    return true;
  }
  mi_metrics m;
  mi_layout L;
  mi_hist_layout(s,strlen(s),output,m,L);
  if (L.asc+L.desc+1>height || (!output && L.width>LCD_WIDTH_PX-4))
    return false;
  const int by=top+(height-(L.asc+L.desc))/2+L.asc;
  if (L.width<=LCD_WIDTH_PX-6){
    const int bx=output?LCD_WIDTH_PX-4-L.width:2;
    mi_draw(L,s,bx,by,0,ymin,LCD_WIDTH_PX,ymax,false,-1);
    return true;
  }
  // wider than the screen: left-aligned, shifted by the scroll, clipped inside the frame; a
  // chevron at each side where there is more
  const int maxoff=L.width-(LCD_WIDTH_PX-16);
  int off=scroll*MI_HSTEP;
  if (off>maxoff)
    off=maxoff;
  if (off<0)
    off=0;
  mi_draw(L,s,8-off,by,7,ymin,LCD_WIDTH_PX-7,ymax,false,-1); // the chevrons outside
  const int ym=top+height/2;
  if (ym-3>=ymin && ym+3<ymax){
    if (off>0){
      mi_line(4,ym-3,1,ym,0,ymin,LCD_WIDTH_PX,ymax,COLOR_BLUE);
      mi_line(1,ym,4,ym+3,0,ymin,LCD_WIDTH_PX,ymax,COLOR_BLUE);
    }
    if (off<maxoff){
      mi_line(LCD_WIDTH_PX-5,ym-3,LCD_WIDTH_PX-2,ym,0,ymin,LCD_WIDTH_PX,ymax,COLOR_BLUE);
      mi_line(LCD_WIDTH_PX-2,ym,LCD_WIDTH_PX-5,ym+3,0,ymin,LCD_WIDTH_PX,ymax,COLOR_BLUE);
    }
  }
  return true;
}

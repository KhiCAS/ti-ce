#ifndef __TEXTGUI_H
#define __TEXTGUI_H
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#if defined TICE && !defined std
#define std ustl
#endif
#if defined FX || defined FXCG
typedef unsigned char Char;
#else
typedef char Char;
#endif


const Char *toksplit(const Char *src, /* Source of tokens */
			      char tokchar, /* token delimiting char */
			      Char *token, /* receiver of parsed token */
			      int lgh); /* length token can receive */

  enum color_values {
    _BLACK=0,
    _RED=0xf800,
    _GREEN=0x0400,
    _YELLOW=0xffe0,
    _BLUE=0x001f,
    _MAGENTA=0xf81f,
    _CYAN=0x07ff,
    _WHITE=0xffff,
#ifndef TICE
    _POINT_LOSANGE= 1 << 25,
    _POINT_PLUS = 1 << 26,
    _POINT_INVISIBLE = 1 << 27,
    _POINT_CARRE = 100663296,
    _POINT_TRIANGLE = 167772160,
    _POINT_ETOILE = 201326592, 
    _POINT_POINT = 234881024,
    _FILL_POLYGON = 1 << 30,
    _QUADRANT1 = 0,
    _QUADRANT2 = 1 << 28,
    _QUADRANT3 = 1 << 29,
    _QUADRANT4 = 805306368,
    _DASH_LINE = 1 << 22,
    _DOT_LINE = 2 << 22,
    _DASHDOT_LINE = 3 << 22,
    _DASHDOTDOT_LINE = 4 << 22,
    _CAP_FLAT_LINE = 5 << 22,
    _CAP_ROUND_LINE = 6 << 22,
    _CAP_SQUARE_LINE = 7 << 22,
    _HIDDEN_NAME = 1 << 31
#endif
    _LINE_WIDTH_1 = 0,
    _LINE_WIDTH_2 = 1 << 16,
    _LINE_WIDTH_3 = 2 << 16,
    _LINE_WIDTH_4 = 3 << 16,
    _LINE_WIDTH_5 = 4 << 16,
    _LINE_WIDTH_6 = 5 << 16,
    _LINE_WIDTH_7 = 6 << 16,
    _LINE_WIDTH_8 = 7 << 16,
    _POINT_WIDTH_1 = 0,
    _POINT_WIDTH_2 = 1 << 19,
    _POINT_WIDTH_3 = 2 << 19,
    _POINT_WIDTH_4 = 3 << 19,
    _POINT_WIDTH_5 = 4 << 19,
    _POINT_WIDTH_6 = 5 << 19,
    _POINT_WIDTH_7 = 6 << 19,
    _POINT_WIDTH_8 = 7 << 19,
  };

bool is_alphanum(char c);
void chk_restart();
#ifdef FX
typedef Char color_t;
#else
typedef short unsigned int color_t;
#endif
typedef struct textElement
{
  std::string s;
  color_t color=COLOR_BLACK;
  short int newLine=0; // if 1, new line will be drawn before the text
  short int spaceAtEnd=0;
  short int lineSpacing=0;
  short int minimini=0; // 0 inherit, 1 font 6x4, -1 font 8x6
  int nlines=1;
} textElement;

#define TEXTAREATYPE_NORMAL 0
#define TEXTAREATYPE_INSTANT_RETURN 1
typedef struct textArea
{
  int x=0;
  int y=0;
  int line=0,undoline=0;
  int pos=0,undopos=0;
  int clipline,undoclipline;
  int clippos,undoclippos;
  int width=LCD_WIDTH_PX;
#ifdef FX
  int lineHeight=8; // -2 if minimini is true
#else
  int lineHeight=16; // -2 if minimini is true  
#endif
  std::vector<textElement> elements,undoelements;
  char* title = NULL;
  std::string filename;
  int scrollbar=1;
  bool allowEXE=0; //whether to allow EXE to exit the screen
  bool allowF1=0; //whether to allow F1 to exit the screen
  bool editable=false;
  bool changed=false;
  bool minimini=false; // global font setting
  bool longlinescut=true;
  bool nostatus=false;
  int python=0;
  int type=TEXTAREATYPE_NORMAL;
  int cursorx=0,cursory=0; // position where the cursor should be displayed
} textArea;


#define TEXTAREA_RETURN_EXIT 0
#define TEXTAREA_RETURN_EXE 1
#define TEXTAREA_RETURN_F1 2
int doTextArea(textArea* text); //returns 0 when user EXITs, 1 when allowEXE is true and user presses EXE, 2 when allowF1 is true and user presses F1.
std::string merge_area(const std::vector<textElement> & v);
bool save_script(const char * filename,const std::string & s);
void add(textArea *edptr,const std::string & s);

extern textArea * edptr;
std::string get_searchitem(std::string & replace);
int check_leave(textArea * text);
void warn_python(int mode,bool autochange);
#endif 

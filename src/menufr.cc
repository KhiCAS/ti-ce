#include <string>
#include <giac/giacPCH.h>
#ifndef FAKE_GIAC
#include <giac/input_lexer.h>
#endif
#include "calc.h"

#include <cstring>
#include <ctype.h>

#include "menuGUI.h"
#include "textGUI.h"
#include "console.h"
#if !defined std
#define std ustl
#endif
using namespace std;

void fix_f(int & key){
  if (key>=KEY_CTRL_F7 && key<=KEY_CTRL_F12)
    key -= 906;
  if (key>=KEY_CTRL_F13 && key<=KEY_CTRL_F18)
    key -= 912;
}

#define MB_ElementCount strlen

int doMenu(Menu* menu, MenuItemIcon* icontable) { // returns code telling what user did. selection is on menu->selection. menu->selection starts at 1!
  int itemsStartY=menu->startY; // char Y where to start drawing the menu items. Having a title increases this by one
  int itemsHeight=menu->height;
  int showtitle = menu->title != NULL;
  int fontwidth=8;
  if (showtitle) {
    itemsStartY++;
    itemsHeight--;
  }
  char keyword[5];
  keyword[0]=0;
  if(menu->selection > menu->scroll+(menu->numitems>itemsHeight ? itemsHeight : menu->numitems))
    menu->scroll = menu->selection -(menu->numitems>itemsHeight ? itemsHeight : menu->numitems);
  if(menu->selection-1 < menu->scroll)
    menu->scroll = menu->selection -1;
  
  while(1) {
#ifdef CURSOR
    Cursor_SetFlashOff();
#endif
    if (menu->selection <=1)
      menu->selection=1;
    if (menu->selection > menu->scroll+(menu->numitems>itemsHeight ? itemsHeight : menu->numitems))
      menu->scroll = menu->selection -(menu->numitems>itemsHeight ? itemsHeight : menu->numitems);
    if (menu->selection-1 < menu->scroll)
      menu->scroll = menu->selection -1;
    //if(menu->statusText != NULL) DefineStatusMessage(menu->statusText, 1, 0, 0);
    // Clear the area of the screen we are going to draw on
    if (0 == menu->pBaRtR){
      int x=fontwidth*(menu->startX-2), y=C10*(menu->miniMiniTitle ? itemsStartY:menu->startY)+STATUS_AREA_PX, w=fontwidth*(menu->width+2)+((menu->scrollbar && menu->scrollout)?C6:0), h=C10*menu->height-(menu->miniMiniTitle ? C10:0);
      //dbg_printf("menu clear area x=%i y=%i w=%i h=%i\n",x,y,w,h);
      int ybot=y+h;
      bool recadre=ybot>LCD_HEIGHT_PX-8;
      drawRectangle(x,giacmax(y,STATUS_AREA_PX),w,recadre?h-(ybot-(LCD_HEIGHT_PX-8)):h, _WHITE);
      draw_line(x,y,x+w,y,_BLACK);
      if (recadre)
        ybot=LCD_HEIGHT_PX-8;
      else
        draw_line(x,ybot,x+w,ybot,_BLACK);
      draw_line(x,y,x,ybot,_BLACK);
      draw_line(x+w,y,x+w,ybot,_BLACK);
    }
    if (menu->numitems>0) {
      for(int curitem=0; curitem < menu->numitems; curitem++) {
        // print the menu item only when appropriate
        if (menu->scroll < curitem+1 && menu->scroll > curitem-itemsHeight) {
          char menuitem[256] = "";
          if (menu->numitems>=100 || menu->type == MENUTYPE_MULTISELECT){
            strcpy(menuitem, "  "); //allow for the folder and selection icons on MULTISELECT menus (e.g. file browser)
            strcpy(menuitem+2,menu->items[curitem].text);
          }
          else {
            int cur=curitem+1;
            if (menu->numitems<10){
              menuitem[0]='0'+cur;
              menuitem[1]=' ';
              menuitem[2]=0;
              strcpy(menuitem+2,menu->items[curitem].text);
            }
            else {
              menuitem[0]=cur>=10?('0'+(cur/10)):' ';
              menuitem[1]='0'+(cur%10);
              menuitem[2]=' ';
              menuitem[3]=0;
              strcpy(menuitem+3,menu->items[curitem].text);
            }
          }
          //strncat(menuitem, menu->items[curitem].text, 68);
          if (menu->items[curitem].type != MENUITEM_SEPARATOR) {
            //make sure we have a string big enough to have background when item is selected:          
            // MB_ElementCount is used instead of strlen because multibyte chars count as two with strlen, while graphically they are just one char, making fillerRequired become wrong
            int fillerRequired = menu->width - MB_ElementCount(menu->items[curitem].text) - (menu->type == MENUTYPE_MULTISELECT ? 2 : 3);
            for(int i = 0; i < fillerRequired; i++)
              strcat(menuitem, " ");
            Printxy(C6*menu->startX+1,C10*(curitem+itemsStartY-menu->scroll)+1,menuitem, (menu->selection == curitem+1 ? TEXT_MODE_INVERT : TEXT_MODE_NORMAL));
          } else {
            /*int textX = (menu->startX-1) * fontwidth;
            int textY = curitem*C10+itemsStartY*C10-menu->scroll*C10-C10+C6;
            clearLine(menu->startX, curitem+itemsStartY-menu->scroll, (menu->selection == curitem+1 ? textColorToFullColor(menu->items[curitem].color) : _WHITE));
            drawLine(textX, textY+C10-4, LCD_WIDTH_PX-2, textY+C10-4, COLOR_GRAY);
            Printmini(&textX, &textY, (unsigned char*)menuitem, 0, 0xFFFFFFFF, 0, 0, (menu->selection == curitem+1 ? _WHITE : textColorToFullColor(menu->items[curitem].color)), (menu->selection == curitem+1 ? textColorToFullColor(menu->items[curitem].color) : _WHITE), 1, 0);*/
          }
          // deal with menu items of type MENUITEM_CHECKBOX
          if(menu->items[curitem].type == MENUITEM_CHECKBOX) {
            Printxy(C6*(menu->startX+menu->width-1)+1,C10*(curitem+itemsStartY-menu->scroll)+1,
              (menu->items[curitem].value == MENUITEM_VALUE_CHECKED ? "x" : "-"),
              (menu->selection == curitem+1 ? TEXT_MODE_INVERT : (menu->pBaRtR == 1? TEXT_MODE_NORMAL : TEXT_MODE_NORMAL)));
          }
          // deal with multiselect menus
          if(menu->type == MENUTYPE_MULTISELECT) {
            if (menu->items[curitem].isselected) {
              if (menu->selection == curitem+1) {
                Printxy(C6*menu->startX+1,C10*(curitem+itemsStartY-menu->scroll)+1,"+", TEXT_MODE_NORMAL);
              } else {
                Printxy(C6*menu->startX+1,C10*(curitem+itemsStartY-menu->scroll)+1,"-", TEXT_MODE_NORMAL);
              }
            }
          }
        }
      }
      if (menu->scrollbar) {
#ifdef SCROLLBAR
        TScrollbar sb;
        sb.I1 = 0;
        sb.I5 = 0;
        sb.indicatormaximum = menu->numitems;
        sb.indicatorheight = itemsHeight;
        sb.indicatorpos = menu->scroll;
        sb.barheight = itemsHeight*C10;
        sb.bartop = (itemsStartY-1)*C10;
        sb.barleft = menu->startX*fontwidth+menu->width*fontwidth - fontwidth - (menu->scrollout ? 0 : 5);
        sb.barwidth = C6;
        Scrollbar(&sb);
#endif
      }
      //if(menu->type==MENUTYPE_MULTISELECT && menu->fkeypage == 0) drawFkeyLabels(0x0037); // SELECT (white)
    } else {
      printCentered(menu->nodatamsg, (itemsStartY*C10)+(itemsHeight*C10)/2-12);
    }
    if (showtitle){
      int textX = fontwidth*menu->startX, textY=(menu->startY-1)*C10;
      if (menu->miniMiniTitle){
        Printmini( textX+1, textY+1, menu->title, 0 );
      } else
        Printxy(C6*menu->startX+1, C10*menu->startY+1, menu->title, TEXT_MODE_NORMAL);
      if (menu->subtitle != NULL){
        int textX=(MB_ElementCount(menu->title)+menu->startX-1)*fontwidth+C10, textY=C6;
        Printmini(textX+1, textY+1, menu->subtitle, 0);
      }
      int x=fontwidth*(menu->startX+menu->width-6);
      Printxy(x, textY+C10+1, "____", 0);
      Printxy(x, textY+C10+1, keyword, 0);
    }
    /*if(menu->darken) {
      DrawFrame(_BLACK);
      VRAMInvertArea(menu->startX*fontwidth-fontwidth, menu->startY*C10, menu->width*fontwidth-(menu->scrollout || !menu->scrollbar ? 0 : 5), menu->height*C10);
    }*/
    if(menu->type == MENUTYPE_NO_KEY_HANDLING) return MENU_RETURN_INSTANT; // we don't want to handle keys
    int key;
    ck_getkey(&key);
    while (key==KEY_CTRL_SHIFT || key==KEY_CTRL_ALPHA)
      ck_getkey(&key);      
    //fix_f(key);
    if (key<256 && isalpha(key)){
      key=tolower(key);
      int pos=strlen(keyword);
      if (pos>=4)
	pos=0;
      keyword[pos]=key;
      keyword[pos+1]=0;
      int cur=0;
      for (;cur<menu->numitems;++cur){
#if 1
	if (strcmp(menu->items[cur].text,keyword)>=0)
	  break;
#else
	char c=menu->items[cur].text[0];
	if (key<=c)
	  break;
#endif
      }
      if (cur<menu->numitems){
	menu->selection=cur+1;
	if(menu->selection > menu->scroll+(menu->numitems>itemsHeight ? itemsHeight : menu->numitems))
	  menu->scroll = menu->selection -(menu->numitems>itemsHeight ? itemsHeight : menu->numitems);
	if(menu->selection-1 < menu->scroll)
	  menu->scroll = menu->selection -1;
      }
      continue;
    }
    if (key==KEY_CTRL_SOLVE) key=KEY_CHAR_COS;
    if (key==KEY_EQW_TEMPLATE) key=KEY_CHAR_TAN;
    switch(key) {
    case KEY_CTRL_PAGEDOWN:
      menu->selection+=6;
      if (menu->selection >= menu->numitems)
	menu->selection=menu->numitems;
      if(menu->selection > menu->scroll+(menu->numitems>itemsHeight ? itemsHeight : menu->numitems))
	menu->scroll = menu->selection -(menu->numitems>itemsHeight ? itemsHeight : menu->numitems);
      break;
    case KEY_CTRL_DOWN:
      if(menu->selection == menu->numitems)
	{
          if(menu->returnOnInfiniteScrolling) {
            return MENU_RETURN_SCROLLING;
          } else {
            menu->selection = 1;
            menu->scroll = 0;
          }
        }
      else
        {
	  menu->selection++;
          if(menu->selection > menu->scroll+(menu->numitems>itemsHeight ? itemsHeight : menu->numitems))
            menu->scroll = menu->selection -(menu->numitems>itemsHeight ? itemsHeight : menu->numitems);
        }
      if(menu->pBaRtR==1) return MENU_RETURN_INSTANT;
      break;
    case KEY_CTRL_PAGEUP:
      menu->selection-=6;
      if (menu->selection <=1)
	menu->selection=1;
      if(menu->selection-1 < menu->scroll)
	menu->scroll = menu->selection -1;
      break;
    case KEY_CTRL_UP:
      if(menu->selection == 1)
	{
	  if(menu->returnOnInfiniteScrolling) {
	    return MENU_RETURN_SCROLLING;
	  } else {
	    menu->selection = menu->numitems;
	    menu->scroll = menu->selection-(menu->numitems>itemsHeight ? itemsHeight : menu->numitems);
	  }
	}
      else
	{
	  menu->selection--;
	  if(menu->selection-1 < menu->scroll)
	    menu->scroll = menu->selection -1;
	}
      if(menu->pBaRtR==1) return MENU_RETURN_INSTANT;
      break;
    case KEY_CTRL_F1: 
      if(menu->type==MENUTYPE_MULTISELECT && menu->fkeypage == 0 && menu->numitems > 0) {
          /*if(menu->items[menu->selection-1].isselected) {
            menu->items[menu->selection-1].isselected=0;
            menu->numselitems = menu->numselitems-1;
	    } else {
            menu->items[menu->selection-1].isselected=1;
            menu->numselitems = menu->numselitems+1;
	    }
	    return key; //return on F1 too so that parent subroutines have a chance to e.g. redraw fkeys*/
      } else if (menu->type == MENUTYPE_FKEYS) {
	return key;
      }
      break;
    case KEY_CTRL_F2:
    case KEY_CTRL_F3:
    case KEY_CTRL_F4:
    case KEY_CTRL_F5:
    case KEY_CTRL_F6:
    case KEY_CTRL_F7:
    case KEY_CTRL_F8:
    case KEY_CTRL_F9:
    case KEY_CTRL_F10:
    case KEY_CTRL_F11:
    case KEY_CTRL_F12:
    case KEY_CTRL_F13:
    case KEY_CTRL_F14:
    case KEY_CTRL_F15:
      //dbg_printf("doMenu key=%i (F1=%i)\n",key,KEY_CTRL_F1);
      if (menu->type == MENUTYPE_FKEYS || menu->type==MENUTYPE_MULTISELECT) return key; // MULTISELECT also returns on Fkeys
      break;
    case KEY_CTRL_PASTE:
      if (menu->type==MENUTYPE_MULTISELECT) return key; // MULTISELECT also returns on paste
    case KEY_CTRL_OPTN:
      if (menu->type==MENUTYPE_FKEYS || menu->type==MENUTYPE_MULTISELECT) return key;
      break;
    case KEY_CTRL_FORMAT:
      if (menu->type==MENUTYPE_FKEYS) return key; // return on the Format key so that event lists can prompt to change event category
      break;
    case KEY_CTRL_RIGHT:
      if(menu->type != MENUTYPE_MULTISELECT) break;
      // else fallthrough
    case KEY_CTRL_EXE:
      if(menu->numitems>0) return MENU_RETURN_SELECTION;
      break;
    case KEY_CTRL_LEFT:
      if(menu->type != MENUTYPE_MULTISELECT) break;
      // else fallthrough
    case KEY_CTRL_DEL:
      if (strlen(keyword))
	keyword[strlen(keyword)-1]=0;
      else {
	if (strncmp(menu->title,"VARS",4)==0)
	  return key;
      }
      break;
    case KEY_CTRL_AC:
      if (strlen(keyword)){
	keyword[0]=0;
	lock_alpha(); //
	//DisplayStatusArea();
	break;
      }
    case KEY_CTRL_EXIT: 
      return MENU_RETURN_EXIT;
      break;
    case KEY_CHAR_1:
    case KEY_CHAR_2:
    case KEY_CHAR_3:
    case KEY_CHAR_4:
    case KEY_CHAR_5:
    case KEY_CHAR_6:
    case KEY_CHAR_7:
    case KEY_CHAR_8:
    case KEY_CHAR_9:
      if(menu->numitems>=(key-0x30)) {
	menu->selection = (key-0x30);
	if (menu->type != MENUTYPE_FKEYS) return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CHAR_0:
      if(menu->numitems>=10) {
	menu->selection = 10;
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CTRL_XTT:
      if(menu->numitems>=11) {
	menu->selection = 11;
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CHAR_LOG: case KEY_CHAR_MAT:
      if(menu->numitems>=12) {
	menu->selection = 12;
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CHAR_LN: case KEY_CTRL_F16:
      if(menu->numitems>=13) {
	menu->selection = 13;
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CHAR_SIN:
    case KEY_CHAR_COS:
    case KEY_CHAR_TAN:
      if(menu->numitems>=(key-115)) {
	menu->selection = (key-115);
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CHAR_FRAC: case KEY_CHAR_POW:
      if(menu->numitems>=17) {
	menu->selection = 17;
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CTRL_FD: 
      if(menu->numitems>=18) {
	menu->selection = 18;
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CHAR_LPAR:
    case KEY_CHAR_RPAR:
      if(menu->numitems>=(key-21)) {
	menu->selection = (key-21);
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CHAR_COMMA:
      if(menu->numitems>=21) {
	menu->selection = 21;
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    case KEY_CHAR_STORE:
      if(menu->numitems>=22) {
	menu->selection = 22;
	if (menu->type != MENUTYPE_FKEYS)  return MENU_RETURN_SELECTION;
      }
      break;
    }
  }
  return MENU_RETURN_EXIT;
}

void reset_alpha(){
  reset_kbd();
  //DisplayStatusArea();
}

#define XCAS_ONLY 0
#define CAT_CATEGORY_ALL 0
#define CAT_CATEGORY_ALGEBRA 1
#define CAT_CATEGORY_LINALG 2
#define CAT_CATEGORY_CALCULUS 3
#define CAT_CATEGORY_ARIT 4
#define CAT_CATEGORY_COMPLEXNUM 5
#define CAT_CATEGORY_POLYNOMIAL 6
#define CAT_CATEGORY_PROG 7
#define CAT_CATEGORY_PROGCMD 8
#define CAT_CATEGORY_REAL 9
#define CAT_CATEGORY_OPTIONS 10
#define CAT_CATEGORY_MATRIX 11
#define CAT_CATEGORY_LIST 12
#define CAT_CATEGORY_TRIG 13
#define CAT_CATEGORY_SOLVE 14
#define CAT_CATEGORY_PHYS 15
#define CAT_CATEGORY_UNIT 16
#define CAT_CATEGORY_PLOT 17

void init_locale(){
#ifndef XLIGHT
  lang=1;
#endif
}

const catalogFunc completeCatfr[] = { // list of all functions (including some not in any category)
  {" boucle for (pour)", "for ", "Boucle definie pour un indice variant entre 2 valeurs fixees", "#\nfor ", 0, CAT_CATEGORY_PROG},
  // {" boucle liste", "for in", "Boucle sur tous les elements d'une liste.", "#\nfor in :", 0, CAT_CATEGORY_PROG},
  {" boucle while (tantque)", "while :", "Boucle indefinie tantque.", "#\nwhile :", 0, CAT_CATEGORY_PROG},
  {" test si alors", "if :", "Test", "#\nif :", 0, CAT_CATEGORY_PROG},
  {" test sinon", "else :", "Clause fausse du test", 0, 0, CAT_CATEGORY_PROG},
  {" fonction def.", "f(x)", "Definition de fonction.", "#\ndef f(x):", 0, CAT_CATEGORY_PROG},
  // {" local j,k;", "local ", "Declaration de variables locales Xcas", 0, 0, CAT_CATEGORY_PROG},
  {" range(a,b)", "in range(", "Dans l'intervalle [a,b[ (a inclus, b exclus)", "# in range(1,10):", 0, CAT_CATEGORY_PROG},
  {" return res;", "return ", "return quitte la fonction et renvoie le resultat res", 0, 0, CAT_CATEGORY_PROG},
  //{" edit list ", "list ", "Assistant creation de liste.", 0, 0, CAT_CATEGORY_LIST},
  //{" edit matrix ", "matrix ", "Assistant creation de matrice.", 0, 0, CAT_CATEGORY_MATRIX},
  //{"fonction def Xcas", "fonction f(x) local y;   ffonction:;", "Definition de fonction.", "#fonction f(x) local y; y:=x^2; return y; ffonction:;", 0, CAT_CATEGORY_PROG},
  {"!", "!", "Non logique (prefixe) ou factorielle de n (suffixe).", "#7!", "#!b", CAT_CATEGORY_PROGCMD},
  {"#", "#", "Commentaire Python (// en Xcas)", 0, 0, CAT_CATEGORY_PROG},
  {"%", "%", "a % b signifie a modulo b", 0, 0, CAT_CATEGORY_ARIT | (CAT_CATEGORY_PROGCMD << 8)},
  {"&", "&", "Et logique ou +", "#1&2", 0, CAT_CATEGORY_PROGCMD},
  {":=", ":=", "Affectation vers la gauche (inverse de =>).", "#a:=3", 0, CAT_CATEGORY_PROGCMD},
  {"<", "<", "Inferieur strict.", 0, 0, CAT_CATEGORY_PROGCMD},
#ifdef WITH_UNITS
  {"=>", "=>", "Affectation vers la droite ou conversion en (touche sto). Exemple 5=>a ou x^4-1=>* ou (x+1)^2=>+ ou sin(x)^2=>cos.", "#5=>a", "#15_m=>_cm", CAT_CATEGORY_PROGCMD | (CAT_CATEGORY_PHYS <<8) | (CAT_CATEGORY_UNIT << 16)},
#else
  {"=>", "=>", "Affectation vers la droite ou conversion en (touche sto). Exemple 5=>a ou x^4-1=>* ou (x+1)^2=>+ ou sin(x)^2=>cos.", "#5=>a", "#x^4-1=>*", CAT_CATEGORY_PROGCMD},
#endif
  {">", ">", "Superieur strict.", 0, 0, CAT_CATEGORY_PROGCMD},
  {"\\", "\\", "Caractere \\", 0, 0, CAT_CATEGORY_PROGCMD},
  {"_", "_", "Caractere _.", 0, 0, CAT_CATEGORY_PROGCMD},
#ifdef WITH_UNITS
  {" mksa(x)", 0, "Conversion en unites MKSA", 0, 0, CAT_CATEGORY_PHYS | (CAT_CATEGORY_UNIT << 8) },
  {" ufactor(a,b)", 0, "Factorise l'unite b dans a", "100_J,1_kW", 0, CAT_CATEGORY_PHYS | (CAT_CATEGORY_UNIT << 8) },
  {" usimplify(a)", 0, "Simplifie l'unite dans a", "100_l/10_cm^2", 0, CAT_CATEGORY_PHYS | (CAT_CATEGORY_UNIT << 8) },
    {"_", "_", "Caractere _. Prefixe d'unites.", 0, 0, CAT_CATEGORY_PROGCMD},
    {"_(km/h)", "_(km/h)", "Vitesse en kilometre/heure", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_(m/s)", "_(m/s)", "Vitesse en metre/seconde", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_(m/s^2)", "_(m/s^2)", "Acceleration en metre par seconde au carre", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_(m^2/s)", "_(m^2/s)", "Viscosite", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_A", 0, "Intensite electrique en Ampere", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Bq", 0, "Radioactivite: Becquerel", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_C", 0, "Charge electrique en Coulomb", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Ci", 0, "Radioactivite: Curie", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_F", 0, "Farad", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_F_", 0, "constante de Faraday (charge globale d'une mole de charges élémentaires).", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_G_", 0, "constante de gravitation universelle. Force=_G_*m1*m2/r^2", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_H", 0, "Henry", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Hz", 0, "Hertz", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_J", 0, "Energie en Joule=kg*m^2/s^2", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_K", 0, "Temperature en Kelvin", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Kcal", 0, "Energie en kilo-calorier", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_MeV", 0, "Energie en mega-electron-Volt", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_N", 0, "Force en Newton=kg*m/s^2", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_NA_", 0, "Avogadro", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_Ohm", 0, "Resistance electrique en Ohm", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_PSun_", 0, "puissance du Soleil", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_Pa", 0, "Pression en Pascal=kg/m/s^2", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_REarth_", 0, "Rayon de la Terre", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_RSun_", 0, "rayon du Soleil", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_R_", 0, "constante des gaz (de Boltzmann par mole)", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_S", 0, "", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_StdP_", 0, "Pression standard (au niveau de la mer)", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_StdT_", 0, "Temperature standard (0 degre Celsius exprimes en Kelvins)", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_Sv", 0, "Radioactivite: Sievert", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_T", 0, "Tesla", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_V", 0, "Tension electrique en Volt", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Vm_", 0, "Volume molaire", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_W", 0, "Puissance en Watt=kg*m^2/s^3", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Wb", 0, "Weber", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_alpha_", 0, "constante de structure fine", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_c_", 0, "vitesse de la lumiere", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_cd", 0, "Luminosite en candela", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_d", 0, "Temps: jour", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_deg", 0, "Angle en degres", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_eV", 0, "Energie en electron-Volt", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_epsilon0_", 0, "permittivite du vide", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_ft", 0, "Longueur en pieds", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_g_", 0, "gravite au sol", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_grad", 0, "Angle en grades", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_h", 0, "Heure", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_h_", 0, "constante de Planck", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_ha", 0, "Aire en hectare", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_hbar_", 0, "constante de Planck/(2*pi)", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_inch", 0, "Longueur en pouces", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_kWh", 0, "Energie en kWh", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_k_", 0, "constante de Boltzmann", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_kg", 0, "Masse en kilogramme", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_l", 0, "Volume en litre", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_m", 0, "Longueur en metre", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_mEarth_", 0, "masse de la Terre", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_m^2", 0, "Aire en m^2", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_m^3", 0, "Volume en m^3", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_me_", 0, "masse electron", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_miUS", 0, "Longueur en miles US", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_mn", 0, "Temps: minute", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_mp_", 0, "masse proton", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_mpme_", 0, "ratio de masse proton/electron", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_mu0_", 0, "permeabilite du vide", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_phi_", 0, "quantum flux magnetique", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_qe_", 0, "charge de l'electron", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_qme_", 0, "_q_/_me_", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_rad", 0, "Angle en radians", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_rem", 0, "Radioactivite: rem", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_s", 0, "Temps: seconde", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_sd_", 0, "Jour sideral", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_syr_", 0, "Annee siderale", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_tr", 0, "Angle en tours", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_yd", 0, "Longueur en yards", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
#endif
  {"a and b", " and ", "Et logique", 0, 0, CAT_CATEGORY_PROGCMD},
  {"a or b", " or ", "Ou logique", 0, 0, CAT_CATEGORY_PROGCMD},
#ifdef WITH_QUAD
  {"a2q(A,[vars])", 0, "Matrice vers forme quadratique", "[[1,2],[2,3]]","[[1,2],[2,3]],[x,y]", CAT_CATEGORY_LINALG},
#endif
  {"abcuv(a,b,c)", 0, "Cherche 2 polynomes u,v tels que a*u+b*v=c","x+1,x^2-2,x", 0, CAT_CATEGORY_POLYNOMIAL},
  {"abs(x)", 0, "Valeur absolue, module ou norme de x", "-3", "[1,2,3]", CAT_CATEGORY_COMPLEXNUM | (CAT_CATEGORY_REAL<<8)},
  {"append", 0, "Ajoute un element en fin de liste l","#l.append(x)", 0, CAT_CATEGORY_LIST},
  {"approx(x)", 0, "Valeur approchee de x.", "pi", 0, CAT_CATEGORY_REAL},
  {"arg(z)", 0, "Argument du complexe z.", "1+i", 0, CAT_CATEGORY_COMPLEXNUM},
  {"asc(string)", 0, "Liste des codes ASCII d'une chaine", "\"Bonjour\"", 0, CAT_CATEGORY_ARIT},
  {"assume(hyp)", 0, "Hypothese sur une variable.", "x>1", "x>-1 and x<1", CAT_CATEGORY_PROGCMD },
  {"barplot(list)", 0, "Diagramme en batons d'une serie statistique 1d.", "[3/2,2,1,1/2,3,2,3/2]", 0, (CAT_CATEGORY_PLOT<<8)},
  {"binomial(n,p,k)", 0, "binomial(n,p,k) probabilite de k succes avec n essais ou p est la proba de succes d'un essai.", "10,5,.4", 0, CAT_CATEGORY_REAL},
  {"bitxor", "bitxor", "Ou exclusif", "#bitxor(1,2)", 0, CAT_CATEGORY_PROGCMD},
  {"black", "black", "Option d'affichage", "#display=black", 0, CAT_CATEGORY_OPTIONS},
  {"blue", "blue", "Option d'affichage", "#display=blue", 0, CAT_CATEGORY_OPTIONS},
  {"ceiling(x)", 0, "Partie entiere superieure", "1.2", 0, CAT_CATEGORY_REAL},
  {"cfactor(p)", 0, "Factorisation sur C.", "x^4-1", 0, CAT_CATEGORY_ALGEBRA | (CAT_CATEGORY_COMPLEXNUM << 8)},
  {"char(liste)", 0, "Chaine donnee par une liste de code ASCII", "[97,98,99]", 0, CAT_CATEGORY_ARIT},
  {"charpoly(M,x)", 0, "Polynome caracteristique de la matrice M en la variable x.", "[[1,2],[3,4]],x", 0, CAT_CATEGORY_MATRIX},
  {"circle(centre,rayon)", 0, "Cercle donne par centre et rayon ou par un diametre", "2+i,3", "1-i,1+i", CAT_CATEGORY_PLOT},
  {"coeff(p,x,n)", 0, "Coefficient de x^n dans le polynome p.", "(1+x)^6,x,3", 0, CAT_CATEGORY_POLYNOMIAL},
  {"comb(n,k)", 0, "Renvoie k parmi n.", "10,4", 0, CAT_CATEGORY_REAL},
  //{"cond(A,[1,2,inf])", 0, "Nombre de condition d'une matrice par rapport a la norme specifiee (par defaut 1)", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"conj(z)", 0, "Conjugue complexe de z.", "1+i", 0, CAT_CATEGORY_COMPLEXNUM},
  {"cpartfrac(p,x)", 0, "Decomposition en elements simples sur C.", "1/(x^4-1)", 0, CAT_CATEGORY_ALGEBRA | (CAT_CATEGORY_COMPLEXNUM << 8)},
  {"cross(u,v)", 0, "Produit vectoriel de u et v.","[1,2,3],[0,1,3]", 0, CAT_CATEGORY_LINALG},
  {"csolve(equation,x)", 0, "Resolution exacte dans C d'une equation en x (ou d'un systeme polynomial).","x^2+x+1=0", 0, CAT_CATEGORY_SOLVE | (CAT_CATEGORY_COMPLEXNUM << 8)},
  {"cyan", "cyan", "Option d'affichage", "#display=cyan", 0, CAT_CATEGORY_OPTIONS},
  {"debug(f(args))", 0, "Execute la fonction f en mode pas a pas.", 0, 0, CAT_CATEGORY_PROG},
  {"degree(p,x)", 0, "Degre du polynome p en x.", "x^4-1", 0, CAT_CATEGORY_POLYNOMIAL},
  {"denom(x)", 0, "Denominateur de l'expression x.", "3/4", 0, CAT_CATEGORY_POLYNOMIAL},
#ifdef WITH_DESOLVE
  {"desolve(equation,t,y)", 0, "Resolution exacte d'equation differentielle ou de systeme differentiel lineaire a coefficients constants.", "[y'+y=exp(x),y(0)=1]", "[y'=[[1,2],[2,1]]*y+[x,x+1],y(0)=[1,2]]", CAT_CATEGORY_SOLVE | (CAT_CATEGORY_CALCULUS << 8)},
#endif
  {"det(A)", 0, "Determinant de la matrice A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX |  (CAT_CATEGORY_LINALG<<8)},
  {"diff(f,var,[n])", 0, "Derivee de l'expression f par rapport a var (a l'ordre n, n=1 par defaut), par ex. diff(sin(x),x) ou diff(x^3,x,2). Pour deriver f par rapport a x, utiliser f' (F3). Pour le gradient de f, var est la liste des variables.", "sin(x),x", "sin(x^2),x,3", CAT_CATEGORY_CALCULUS},
  {"dot(a,b)", 0, "Produit scalaire de 2 vecteurs.", "[1,2,3,4,5],[0,1,3,4,4]", 0, CAT_CATEGORY_LINALG},
  {"egcd(A,B)", 0, "Cherche des polynomes U,V,D tels que A*U+B*V=D=gcd(A,B)","x^2+3x+1,x^2-5x-1", 0, CAT_CATEGORY_POLYNOMIAL},
  {"egvl(A)", 0, "Valeurs propres de la matrice A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX |  (CAT_CATEGORY_LINALG<<8)},
  {"egv(A)", 0, "Vecteurs propres de la matrice A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX |  (CAT_CATEGORY_LINALG<<8)},
  {"elif (test)", "elif", "Tests en cascade", 0, 0, CAT_CATEGORY_PROG},
  //{"end", "end", "Fin de bloc", 0, 0, CAT_CATEGORY_PROG},
  {"euler(n)",0,"Indicatrice d'Euler: nombre d'entiers < n premiers avec n","25",0,CAT_CATEGORY_ARIT},
  {"eval(f)", 0, "Evalue f.", 0, 0, CAT_CATEGORY_PROGCMD},
  {"evalc(z)", 0, "Ecrit z=x+i*y.", "1/(1+i*sqrt(3))", 0, CAT_CATEGORY_COMPLEXNUM},
  {"exact(x)", 0, "Convertit x en rationnel.", "1.2", 0, CAT_CATEGORY_REAL},
  {"exp2trig(expr)", 0, "Conversion d'exponentielles complexes en sin/cos", "exp(i*x)", 0, CAT_CATEGORY_TRIG},
  {"extend", 0, "Concatene 2 listes. Attention + effectue l'addition de 2 vecteurs.","#l1.extend(l2)", 0, CAT_CATEGORY_LIST},
  {"factor(p,[x])", 0, "Factorisation du polynome p (utiliser ifactor pour un entier). Raccourci: p=>*", "x^4-1", "x^6+1,sqrt(3)", CAT_CATEGORY_ALGEBRA | (CAT_CATEGORY_POLYNOMIAL << 8)},
  {"factorial(n)", "factorial", "Factorielle de n", "7", "77", CAT_CATEGORY_REAL},
  {"filled", "filled", "Option de remplissage", "#display=red+filled", 0, CAT_CATEGORY_OPTIONS},
  {"float(x)", 0, "Convertit x en nombre approche (flottant).", "pi", 0, CAT_CATEGORY_REAL},
  {"floor(x)", 0, "Partie entiere de x", "pi", 0, CAT_CATEGORY_REAL},
#ifndef WITH_QUAD
  {"fonction f(x)", "fonction", "Definition de fonction (Xcas). Par exemple\nfonction f(x)\n local y;\ny:=x*x;\nreturn y;\nffonction", 0, 0, CAT_CATEGORY_PROG},
#endif
  {"fourier_an(f,x,T,n,a)", 0, "Coeffs de Fourier en cosinus de f", "x^2,n","x^2,x,2*pi,n,-pi", CAT_CATEGORY_CALCULUS},
  {"fourier_bn(f,x,T,n,a)", 0, "Coeffs de Fourier en sinus de f", "x^2,n","x^2,x,2*pi,n,-pi", CAT_CATEGORY_CALCULUS},
  {"fourier_cn(f,x,T,n,a)", 0, "Coeffs de Fourier exponentiels de f", "x^2,n","x^2,x,2*pi,n,-pi", CAT_CATEGORY_CALCULUS},
  {"fsolve(equation,x=a[..b])", 0, "Resolution approchee de equation pour x dans l'intervalle a..b ou en partant de x=a.","cos(x)=x,x=0..1", "cos(x)-x,x=0.0", CAT_CATEGORY_SOLVE},
#ifdef WITH_QUAD
  {"gauss(q)", 0, "Reduction d'une forme quadratique", "x^2+x*y+x*z+y^2+z^2,[x,y,z]", 0, CAT_CATEGORY_LINALG},
#endif  
  {"gcd(a,b,...)", 0, "Plus grand commun diviseur. Voir iegcd ou egcd pour Bezout.", "23,13", "x^2-1,x^3-1", CAT_CATEGORY_ARIT | (CAT_CATEGORY_POLYNOMIAL << 8)},
  {"green", "green", "Option d'affichage", "#display=green", 0, CAT_CATEGORY_OPTIONS},
  {"halftan(expr)", 0, "Exprime cos, sin, tan avec tan(angle/2).","cos(x)", 0, CAT_CATEGORY_TRIG},
  {"histogram(list,min,size)", 0, "Histogramme d'une liste de donneees, classes commencant a min de taille size.","ranv(100)/100.,0,0.1", 0, (CAT_CATEGORY_PLOT<<8)},
  {"iabcuv(a,b,c)", 0, "Cherche 2 entiers u,v tels que a*u+b*v=c","23,13,15", 0, CAT_CATEGORY_ARIT},
  {"ichinrem([a,m],[b,n])", 0,"Restes chinois entiers de a mod m et b mod n.", "[3,13],[2,7]", 0, CAT_CATEGORY_ARIT},
  {"idivis(n)", 0, "Liste des diviseurs d'un entier n.", "10", 0, CAT_CATEGORY_ARIT},
  {"idn(n)", 0, "matrice identite n * n", "4", 0, CAT_CATEGORY_MATRIX},
  {"iegcd(a,b)", 0, "Determine les entiers u,v,d tels que a*u+b*v=d=gcd(a,b)","23,13", 0, CAT_CATEGORY_ARIT},
  {"ifactor(n)", 0, "Factorisation d'un entier (pas trop grand!). Raccourci n=>*", "1234", 0, CAT_CATEGORY_ARIT},
  {"im(z)", 0, "Partie imaginaire.", "1+i", 0, CAT_CATEGORY_COMPLEXNUM},
  {"inf", "inf", "Plus l'infini. Utiliser -inf pour moins l'infini ou infinity pour l'infini complexe.", "-inf", "infinity", CAT_CATEGORY_CALCULUS},
  {"input()", "input()", "Lire une chaine au clavier", "\"Valeur ?\"", 0, CAT_CATEGORY_PROG},
  {"integrate(f,x,[,a,b])", 0, "Primitive de f par rapport a la variable x, par ex. integrate(x*sin(x),x). Pour calculer une integrale definie, entrer les arguments optionnels a et b, par ex. integrate(x*sin(x),x,0,pi).", "x*sin(x),x", "cos(x)/(1+x^4),x,0,inf", CAT_CATEGORY_CALCULUS},
  {"interp(X,Y[,interp])", 0, "Interpolation de Lagrange aux points (xi,yi) avec X la liste des xi et Y des yi. Renvoie la liste des differences divisees si interp est passe en parametre.", "[1,2,3,4,5],[0,1,3,4,4]", "[1,2,3,4,5],[0,1,3,4,4],interp", CAT_CATEGORY_POLYNOMIAL},
  {"inv(A)", 0, "Inverse de A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX |  (CAT_CATEGORY_LINALG<<8)},
  // {"inverser(v)", "inverser ", "La variable v est remplacee par son inverse", "#v:=3; inverser v", 0, CAT_CATEGORY_SOFUS},
  {"iquo(a,b)", 0, "Quotient euclidien de deux entiers.", "23,13", 0, CAT_CATEGORY_ARIT},
  {"irem(a,b)", 0,"Reste euclidien de deux entiers", "23,13", 0, CAT_CATEGORY_ARIT},
  {"isprime(n)", 0, "Renvoie 1 si n est premier, 0 sinon.", "11", "10", CAT_CATEGORY_ARIT},
  {"jordan(A)", 0, "Forme normale de Jordan de la matrice A, renvoie P et D tels que P^-1*A*P=D", "[[1,2],[3,4]]", "[[1,1,-1,2,-1],[2,0,1,-4,-1],[0,1,1,1,1],[0,1,2,0,1],[0,0,-3,3,-1]]", CAT_CATEGORY_MATRIX},
  {"lcm(a,b,...)", 0, "Plus petit commun multiple.", "23,13", "x^3-1,x^2-1", CAT_CATEGORY_ARIT | (CAT_CATEGORY_POLYNOMIAL << 8)},
  {"lcoeff(p,x)", 0, "Coefficient dominant du polynome p.", "x^4-1", 0, CAT_CATEGORY_POLYNOMIAL},
  {"limit(f,x=a)", 0, "Limite de f en x = a. Ajouter 1 ou -1 pour une limite a droite ou a gauche, limit(sin(x)/x,x=0) ou limit(abs(x)/x,x=0,1).", "sin(x)/x,x=0", "exp(-1/x),x=0,1", CAT_CATEGORY_CALCULUS},
  {"line(equation)", 0, "Droite donnee par une equation ou 2 points", "y=2x+1", "[1,1],[2,-1]", CAT_CATEGORY_PLOT },
  {"linsolve([eq1,eq2,..],[x,y,..])", 0, "Resolution de systeme lineaire. Peut utiliser le resultat de lu pour resolution en O(n^2).","[x+y=1,x-y=2],[x,y]", "#p,l,u:=lu([[1,2],[3,4]]); linsolve(p,l,u,[5,6])", CAT_CATEGORY_SOLVE | (CAT_CATEGORY_LINALG <<8) | (CAT_CATEGORY_MATRIX << 16)},
  //{"lu(A)", 0, "decomposition LU de la matrice A, P*A=L*U, renvoie P permutation, L et U triangulaires inferieure et superieure", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"magenta", "magenta", "Option d'affichage", "#display=magenta", 0, CAT_CATEGORY_OPTIONS},
  {"map(l,f)", 0, "Applique f aux elements de la liste l.","[1,2,3],x->x^2", 0, CAT_CATEGORY_LIST},
  {"matpow(A,n)", 0, "Renvoie A^n, la matrice A la puissance n", "[[1,-1],[2,4]],n","#assume(n>=1);matpow([[0,2],[0,4]],n)", CAT_CATEGORY_MATRIX},
  {"matrix(l,c,func)", 0, "Matrice de terme general donne.", "2,3,(j,k)->(j+1)^k", 0, CAT_CATEGORY_MATRIX},
  {"mult_c_conjugate", 0, "Multiplier par le conjugue complexe.", "1+2*i", 0,  (CAT_CATEGORY_COMPLEXNUM << 8)},
#ifndef WITH_QUAD
  {"mult_conjugate", 0, "Multiplier par le conjugue (sqrt).", "sqrt(2)-sqrt(3)", 0, CAT_CATEGORY_ALGEBRA},
#endif
  {"not(x)", 0, "Non logique.", 0, 0, CAT_CATEGORY_PROGCMD},
  {"numer(x)", 0, "Numerateur de x.", "3/4", 0, CAT_CATEGORY_POLYNOMIAL},
#ifdef WITH_DESOLVE
  {"odesolve(f(t,y),[t,y],[t0,y0],t1)", 0, "Solution approchee d'equation differentielle y'=f(t,y) et y(t0)=y0, valeur en t1 (ajouter curve pour les valeurs intermediaires de y)", "sin(t*y),[t,y],[0,1],2", "0..pi,(t,v)->{[-v[1],v[0]]},[0,1]", CAT_CATEGORY_SOLVE},
#endif
  {"partfrac(p,x)", 0, "Decomposition en elements simples. Raccourci p=>+", "1/(x^4-1)", 0, CAT_CATEGORY_ALGEBRA},
#ifdef WITH_PLOT
  {"plot(expr,x)", 0, "Graphe de fonction.", "ln(x),x=0..5", "1/x,x=1..5,xstep=1", CAT_CATEGORY_PLOT},
  //{"plotarea(expr,x=a..b,[n,meth])", 0, "Aire sous la courbe selon une methode d'integration.", "1/x,x=1..3,2,trapeze", 0, CAT_CATEGORY_PLOT},
  {"plotfield(f(t,y), [t=tmin..tmax,y=ymin..ymax])", 0, "Champ des tangentes de y'=f(t,y), optionnellement graphe avec arg. optionnel plotode=[t0,y0]", "sin(t*y), [t=-3..3,y=-3..3]", "5*[-y,x], [x=-1..1,y=-1..1]", CAT_CATEGORY_PLOT},
  {"plotode(f(t,y), [t=tmin..tmax,y],[t0,y0])", 0, "Graphe de solution d'equation differentielle y'=f(t,y), y(t0)=y0.", "sin(t*y),[t=-3..3,y],[0,1]", 0, CAT_CATEGORY_PLOT},
  {"plotparam([x,y],t)", 0, "Graphe en parametriques. Exemples plotparam([sin(3t),cos(2t)],t,0,pi) ou plotparam(exp(i*t),t,0,pi)", "[cos(3t),sin(2t)],t,0,2*pi", "[t^2,t^3],t=-1..1,tstep=0.1", CAT_CATEGORY_PLOT},
  {"plotpolar(r,theta)", 0, "Graphe en polaire.","cos(3*x),x,0,pi", "1/(1+cos(x)),x=0..pi,xstep=0.05", CAT_CATEGORY_PLOT},
  //{"plotcontour(expr,[x=xm..xM,y=ym..yM],niveaux)", 0, "Lignes de niveau de expr.", "x^2+2y^2, [x=-2..2,y=-2..2],[1,2]", 0, CAT_CATEGORY_PLOT},
  //{"plotlist(list)", 0, "Graphe d'une liste", "[3/2,2,1,1/2,3,2,3/2]", "[1,13],[2,10],[3,15],[4,16]", CAT_CATEGORY_PLOT},
  {"plotseq(f(x),x=[u0,m,M],n)", 0, "Trace f(x) sur [m,M] et n termes de la suite recurrente u_{n+1}=f(u_n) de 1er terme u0.","sqrt(2+x),x=[6,0,7],5", 0, CAT_CATEGORY_PLOT},
#endif
  {"point(x,y)", 0, "Point", "1,2", "1+2i", CAT_CATEGORY_PLOT },
  // {"pour (boucle Xcas)", "pour  de  jusque  faire  fpour;", "Boucle definie.","#pour j de 1 jusque 10 faire print(j,j^2); fpour;", 0, CAT_CATEGORY_PROG},
  {"powmod(a,n,p[,P,x])", 0, "Renvoie a^n mod p, ou a^n mod un entier p et un polynome P en x.","123,456,789", "x+1,452,19,x^4+x+1,x", CAT_CATEGORY_ARIT},
  {"print(expr)", 0, "Afficher dans la console", 0, 0, CAT_CATEGORY_PROG},
  {"proot(p)", 0, "Racines reelles et complexes approchees d'un polynome.", "x^3+2.1*x^2+3x+4.2", "[1,2.1,3,4.2]", CAT_CATEGORY_POLYNOMIAL},
  {"purge(x)", 0, "Purge le contenu de la variable x.", 0, 0, CAT_CATEGORY_PROGCMD },
#ifdef WITH_QUAD
  {"q2a(expr,vars)", 0, "Matrice d'une forme quadratique", "x^2+3*x*y","x^2+3*x*y,[x,y]", CAT_CATEGORY_LINALG},
#endif
  //{"qr(A)", 0, "Factorisation A=Q*R avec Q orthogonale et R triangulaire superieure", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"quo(p,q,x)", 0, "Quotient de division euclidienne polynomiale en x.", "x^3-1,x^2-1", 0, CAT_CATEGORY_POLYNOMIAL},
  //{"quote(x)", 0, "Renvoie l'expression x non evaluee.", 0, 0, CAT_CATEGORY_ALGEBRA},
  {"rand()", "rand()", "Reel aleatoire entre 0 et 1", 0, 0, CAT_CATEGORY_REAL},
  {"randint(n)", 0, "Entier aleatoire entre 1 et n ou entre a et b si on fournit 2 arguments", "6", "5,20", CAT_CATEGORY_REAL},
  {"range(n)", 0, "Liste d'entiers de 0 à n-1 ou de a inclus a b exclu", "10", "1,9", CAT_CATEGORY_MATRIX},
  {"ranm(n,m)", 0, "Matrice aleatoire", "4,2", "3,3,10", CAT_CATEGORY_MATRIX},
  {"ranv(n)", 0, "Vecteur aleatoire", "4", "10,30", CAT_CATEGORY_LINALG},
  {"ratnormal(x)", 0, "Ecrit sous forme de fraction irreductible.", "(x+1)/(x^2-1)^2", 0, CAT_CATEGORY_ALGEBRA},
  {"re(z)", 0, "Partie reelle.", "1+i", 0, CAT_CATEGORY_COMPLEXNUM},
  //{"read(\"filename\")", "read(\"", "Lire un fichier. Voir aussi write", 0, 0, CAT_CATEGORY_PROGCMD},
  {"red", "red", "Option de couleur", "#display=red", 0, CAT_CATEGORY_OPTIONS},
  {"rem(p,q,x)", 0, "Reste de division euclidienne polynomiale en x", "x^3-1,x^2-1", 0, CAT_CATEGORY_POLYNOMIAL},
  {"resultant(p,q,x)", 0, "Resultant en x des polynomes p et q.", "(P:=x^3+p*x+q),P',x", 0, CAT_CATEGORY_POLYNOMIAL},
  {"rref(A)", 0, "Pivot de Gauss", "[[1,2,3],[4,5,6]]", 0, CAT_CATEGORY_MATRIX|  (CAT_CATEGORY_LINALG<<8)},
  //{"rsolve(equation,u(n),[init])", 0, "Expression d'une suite donnee par une recurrence.","u(n+1)=2*u(n)+3,u(n),u(0)=1", "([u(n+1)=3*v(n)+u(n),v(n+1)=v(n)+u(n)],[u(n),v(n)],[u(0)=1,v(0)=2]", CAT_CATEGORY_SOLVE},
  {"seq(expr,var,a,b[,pas])", 0, "Liste de terme general donne.","j^2,j,1,10", "j^2,j,1,10,2", CAT_CATEGORY_LIST},
  //{"si (test Xcas)", "si  alors  sinon  fsi;", "Test.", "#f(x):=si x>0 alors x; sinon -x; fsi;// valeur absolue", 0, CAT_CATEGORY_PROG},
  {"series(f,x=a,n,[polynom])", 0, "Developpement de Taylor de l'expression f en x=a a l'ordre n, ajouter le parametre polynom pour enlever le terme de reste.","sin(x),x=0,5", "sin(x),x=0,5,polynom", CAT_CATEGORY_CALCULUS},
  //{"sign(x)", 0, "Renvoie -1 si x est negatif, 0 si x est nul et 1 si x est positif.", 0, 0, CAT_CATEGORY_REAL},
  {"simplify(expr)", 0, "Renvoie en general expr sous forme simplifiee. Raccourci expr=>/", "sin(3x)/sin(x)", "ln(4)-ln(2)", CAT_CATEGORY_ALGEBRA},
  {"solve(equation,x)", 0, "Resolution exacte d'une equation en x. Cf. csolve pour les solutions complexes, linsolve pour un systeme lineaire.", "x^2-x-1=0,x", "[x^2-y^2=0,x^2-z^2=0],[x,y,z]", CAT_CATEGORY_SOLVE},
  {"sort(l)", 0, "Trie une liste.","[3/2,2,1,1/2,3,2,3/2]", "[[1,2],[2,3],[4,3]],(x,y)->when(x[1]==y[1],x[0]>y[0],x[1]>y[1]", CAT_CATEGORY_LIST},
  {"subst(a,b=c)", 0, "Remplace b par c dans a. Raccourci a(b=c). Pour faire plusieurs remplacements, saisir subst(expr,[b1,b2...],[c1,c2...])", "x^2,x=3", "x+y^2,[x,y],[1,2]", CAT_CATEGORY_ALGEBRA},
  {"sum(f,k,m,M)", 0, "Somme de l'expression f dependant de k pour k variant de m a M. Exemple sum(k^2,k,1,n)=>*.", "k,k,1,n", "k^2,k", CAT_CATEGORY_CALCULUS},
#if defined WITH_SHEET && !defined WITH_QUAD
  //{"tablefunc(f(x),x[,xmin,xstep])", 0, "Tableau de valeurs de f(x)", "sqrt(x^2+x+1),x,-1,0.1",  0, CAT_CATEGORY_CALCULUS},
  {"tableseq(f(x),x,u0)", 0, "Tableau de valeurs de la suite recurrente u_{n+1}=f(u_n)", "cos(x),x,0.0",  0, CAT_CATEGORY_CALCULUS},
#endif
  {"tabvar(f,[x=a..b])", 0, "Tableau de variations de l'expression f, arguments optionnels la variable x dans l'intervalle a..b.", "sqrt(x^2+x+1)", "[cos(t),sin(3t)],t", CAT_CATEGORY_CALCULUS},
  //{"tantque (boucle Xcas)", "tantque  faire  ftantque;", "Boucle indefinie.", "#j:=13; tantque j!=1 faire j:=when(even(j),j/2,3j+1); print(j); ftantque;", 0, CAT_CATEGORY_PROG},
  //{"tchebyshev1(n)", 0, "Polynome de Tchebyshev de 1ere espece: cos(n*x)=T_n(cos(x))", "10", 0, CAT_CATEGORY_POLYNOMIAL},
  //{"tchebyshev2(n)", 0, "Polynome de Tchebyshev de 2eme espece: sin((n+1)*x)=sin(x)*U_n(cos(x))", "10", 0, CAT_CATEGORY_POLYNOMIAL},
  {"tcollect(expr)", 0, "Linearisation trigonometrique et regroupement.","sin(x)+cos(x)", 0, CAT_CATEGORY_TRIG},
  {"texpand(expr)", 0, "Developpe les fonctions trigonometriques, exp et ln.","sin(3x)", "ln(x*y)", CAT_CATEGORY_TRIG},
  //{"time(cmd)", 0, "Temps pour effectuer une commande ou mise a l'heure de horloge","int(1/(x^4+1),x)","8,0", CAT_CATEGORY_PROG},
  {"tlin(expr)", 0, "Linearisation trigonometrique de l'expression.","sin(x)^3", 0, CAT_CATEGORY_TRIG},
  {"trace(A)", 0, "Trace de la matrice A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"tran(A)", 0, "Transposee de la matrice A. Pour la transconjuguee utiliser trn(A) ou A^*.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX |  (CAT_CATEGORY_LINALG<<8)},
  {"trig2exp(expr)", 0, "Convertit les fonctions trigonometriques en exponentielles.","cos(x)^3", 0, CAT_CATEGORY_TRIG},
  {"trigcos(expr)", 0, "Exprime sin^2 et tan^2 avec cos^2.","sin(x)^4", 0, CAT_CATEGORY_TRIG},
  {"trigsin(expr)", 0, "Exprime cos^2 et tan^2 avec sin^2.","cos(x)^4", 0, CAT_CATEGORY_TRIG},
  {"trigtan(expr)", 0, "Exprime cos^2 et sin^2 avec tan^2.","cos(x)^4", 0, CAT_CATEGORY_TRIG},
  {"tstep",0, "Pas (graphe parametrique)", "tstep=0.2", 0, CAT_CATEGORY_OPTIONS},
  {"xstep",0, "Pas (graphe de fonction)", "xstep=0.2", 0, CAT_CATEGORY_OPTIONS},
  {"yellow", "yellow", "Display option", "#display=yellow", 0, CAT_CATEGORY_OPTIONS},
  {"|", "|", "Ou logique", "#1|2", 0, CAT_CATEGORY_PROGCMD},
  {"~", "~", "Complement", "#~7", 0, CAT_CATEGORY_PROGCMD},
};

#ifdef FRANCAIS
const char aide_khicas_string[]="Aide Khicas";
const char shortcuts_string[]="Pour mettre a l'heure l'horloge, tapez heure,minute puis touche STO puis , par exemple 13,10=>,\n\nRaccourcis clavier (shell et editeur)\n2nd quit: quitter\nmode: setup\nechanger: undo/redo\nF1-F4: menu selon legende\nF5: menu fichier\nshift-F1-F5 et alpha-F1-F5: selon legendes\nprgm: bascule entre editeur et shell\nmatrice, var, listes: menu rapide\nShell\n\n=>+: partfrac\n=>*: factor\n=>sin/cos/tan\n=>=>: solve\nannul: efface ligne ou historique\n\nEditeur\n\ntouche <>: indentation\n2nd enter:saut ligne\nenter: teste syntaxe";
const char apropos_string[]="Khicas 1.x, (c) 2024 B. Parisse et al. xcas.univ-grenoble-alpes.fr/.\nLicense GPL version 2.\nInterface adaptee d'Eigenmath pour Casio, G. Maia (http://gbl08ma.com), Mike Smith, Nemhardy, LePhenixNoir. Merci a Adrien Bertrand, Xavier Andreani et a toute la communaute de developpement ti83/84 pour l'aide apportee, en particulier Jacob Young, commandblockguy and Matt Waltz";
#else
constexpr char aide_khicas_string[]="Khicas help";
const char shortcuts_string[]="Clock setup: type hour,minute then sto key then , for example 13,10=>,\n\nKeyboard shortcuts\n2nd quit: quit\nmode: setup\nlink: undo/redo\nF1-F4: menu corresponding to legende\nF5: file menu\nshift-F1-F5 and alpha-F1-F5: cf. legendes\nprgm: switch between shell and editor\nmatrix, var, lists: fast menu\n\nShell\n\n=>+: partfrac\n=>*: factor\n=>sin/cos/tan\n=>=>: solve\ncancel: erase line or history\n\nEditor\n\n<> key: indentation\n2nd enter: newline\nenter: test syntax";
const char apropos_string[]="Khicas 1.x, (c) 2024 B. Parisse et al. xcas.univ-grenoble-alpes.fr/.\nLicense GPL version 2.\nInterface adapted from Eigenmath for Casio Prizm, G. Maia (http://gbl08ma.com), Mike Smith, Nemhardy, LePhenixNoir. Thanks to Adrien Bertrand, Xavier Andreani and all the ti83/84 development community, especially Jacob Young, commandblockguy and Matt Waltz contributions";
#endif

constexpr int CAT_COMPLETE_COUNT_FR=sizeof(completeCatfr)/sizeof(catalogFunc);

const catalogFunc completeCaten[] = { // list of all functions (including some not in any category)
  {" loop for", "for ", "Defined loop.", "#\nfor ", 0, CAT_CATEGORY_PROG},
  {" loop in list", "for in", "Loop on all elements of a list.", "#\nfor in", 0, CAT_CATEGORY_PROG},
  {" loop while", "while ", "Undefined loop.", "#\nwhile ", 0, CAT_CATEGORY_PROG},
  {" test if", "if ", "Test", "#\nif ", 0, CAT_CATEGORY_PROG},
  {" test else", "else ", "Test false case", 0, 0, CAT_CATEGORY_PROG},
  {" function def", "f(x):=", "Definition of function.", "#\nf(x):=", 0, CAT_CATEGORY_PROG},
  {" range(a,b)", 0, "In range [a,b) (a included, b excluded)", "# in range(1,10)", 0, CAT_CATEGORY_PROG},
  {" return res", "return ", "Leaves current function and returns res.", 0, 0, CAT_CATEGORY_PROG},
  {"!", "!", "Logical not (prefix) or factorial of n (suffix).", "#7!", "~!b", CAT_CATEGORY_PROGCMD},
  {"#", "#", "Python comment, for Xcas comment type //. Shortcut ALPHA F2", 0, 0, CAT_CATEGORY_PROG},
  {"%", "%", "a % b means a modulo b", 0, 0, CAT_CATEGORY_ARIT | (CAT_CATEGORY_PROGCMD << 8)},
  {"&", "&", "Logical and or +", "#1&2", 0, CAT_CATEGORY_PROGCMD},
#ifdef WITH_UNITS
  {"=>", "=>", "Store value in variable or conversion (touche ->). For example 5=>a or x^4-1=>* or (x+1)^2=>+ or sin(x)^2=>cos.", "#5=>a", "#x^4-1=>*", CAT_CATEGORY_PROGCMD },
  {" mksa(x)", 0, "Convert to MKSA units", 0, 0, CAT_CATEGORY_PHYS | (CAT_CATEGORY_UNIT << 8) },
  {" ufactor(a,b)", 0, "Factorizes unit b in a", "100_J,1_kW", 0, CAT_CATEGORY_PHYS | (CAT_CATEGORY_UNIT << 8) },
  {" usimplify(a)", 0, "Simplifies unit in a", "100_l/10_cm^2", 0, CAT_CATEGORY_PHYS | (CAT_CATEGORY_UNIT << 8) },
    {"_(km/h)", "_(km/h)", "Speed kilometer per hour", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_(m/s)", "_(m/s)", "Speed meter/second", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_(m/s^2)", "_(m/s^2)", "Acceleration", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_(m^2/s)", "_(m^2/s)", "Viscosity", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_A", 0, "Ampere", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Bq", 0, "Becquerel", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_C", 0, "Coulomb", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Ci", 0, "Curie", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_F", 0, "Farad", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_F_", 0, "Faraday constant", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_G_", 0, "Gravitation force=_G_*m1*m2/r^2", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_H", 0, "Henry", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Hz", 0, "Hertz", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_J", 0, "Joule=kg*m^2/s^2", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_K", 0, "Temperature in Kelvin", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Kcal", 0, "Energy kilo-calorie", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_MeV", 0, "Energy mega-electron-Volt", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_N", 0, "Force Newton=kg*m/s^2", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_NA_", 0, "Avogadro constant", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_Ohm", 0, "Ohm", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_PSun_", 0, "Sun power", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_Pa", 0, "Pressure in Pascal=kg/m/s^2", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_REarth_", 0, "Earth radius", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_RSun_", 0, "Sun radius", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_R_", 0, "Boltzmann constant (per mol)", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_S", 0, "", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_StdP_", 0, "Standard pressure", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_StdT_", 0, "Standard temperature (0 degre Celsius in Kelvins)", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_Sv", 0, "Sievert", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_T", 0, "Tesla", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_V", 0, "Volt", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Vm_", 0, "Volume molaire", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_W", 0, "Watt=kg*m^2/s^3", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_Wb", 0, "Weber", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_alpha_", 0, "fine structure constant", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_c_", 0, "speed of light", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_cd", 0, "candela", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_d", 0, "day", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_deg", 0, "degree", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_eV", 0, "electron-Volt", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_epsilon0_", 0, "vacuum permittivity", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_ft", 0, "feet", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_g_", 0, "Earth gravity (ground)", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_grad", 0, "grades (angle unit(", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_h", 0, "Hour", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_h_", 0, "Planck constant", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_ha", 0, "hectare", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_hbar_", 0, "Planck constant/(2*pi)", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_inch", 0, "inches", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_kWh", 0, "kWh", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_k_", 0, "Boltzmann constant", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_kg", 0, "kilogram", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_l", 0, "liter", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_m", 0, "meter", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_mEarth_", 0, "Earth mass", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_m^2", 0, "Area in m^2", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_m^3", 0, "Volume in m^3", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_me_", 0, "electron mass", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_miUS", 0, "US miles", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_mn", 0, "minute", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_mp_", 0, "proton mass", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_mpme_", 0, "proton/electron mass-ratio", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_mu0_", 0, "", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_phi_", 0, "magnetic flux quantum", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_qe_", 0, "electron charge", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_qme_", 0, "_q_/_me_", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_rad", 0, "radians", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_rem", 0, "rem", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_s", 0, "second", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_sd_", 0, "Sideral day", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_syr_", 0, "Sideral year", 0, 0, CAT_CATEGORY_PHYS | XCAS_ONLY},
    {"_tr", 0, "tour (angle unit)", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
    {"_yd", 0, "yards", 0, 0, CAT_CATEGORY_UNIT | XCAS_ONLY},
#else
  {"=>", "=>", "Store value in variable or conversion (touche ->). For example 5=>a or x^4-1=>* or (x+1)^2=>+ or sin(x)^2=>cos.", "#5=>a", "#15_ft=>_cm", CAT_CATEGORY_PROGCMD | (CAT_CATEGORY_PHYS <<8) | (CAT_CATEGORY_UNIT << 16) },
#endif
  {"a and b", " and ", "Logical and", 0, 0, CAT_CATEGORY_PROGCMD},
  {"a or b", " or ", "Logical or", 0, 0, CAT_CATEGORY_PROGCMD},
#ifdef WITH_QUAD
  {"a2q(A,[vars])", 0, "Matrix to quadratic form", "[[1,2],[2,3]]","[[1,2],[2,3]],[x,y]", CAT_CATEGORY_LINALG},
#endif
  {"abcuv(a,b,c)", 0, "Find 2 polynomial u,v such that a*u+b*v=c","x+1,x^2-2,x", 0, CAT_CATEGORY_POLYNOMIAL},
  {"abs(x)", 0, "Absolute value or norm of x x", "-3", "[1,2,3]", CAT_CATEGORY_COMPLEXNUM | (CAT_CATEGORY_REAL<<8)},
  {"append", 0, "Adds an element at the end of a list","#l.append(x)", 0, CAT_CATEGORY_LIST},
  {"approx(x)", 0, "Approx. value x. Shortcut S-D", "pi", 0, CAT_CATEGORY_REAL},
  {"arg(z)", 0, "Angle of complex z.", "1+i", 0, CAT_CATEGORY_COMPLEXNUM},
  {"asc(string)", 0, "List of ASCII codes os a string", "\"Hello\"", 0, CAT_CATEGORY_ARIT},
  {"assume(hyp)", 0, "Assumption on variable.", "x>1", "x>-1 and x<1", CAT_CATEGORY_PROGCMD},
  {"axes", "axes", "Axes visible or not axes=1 or 0", "#axes=0", 0, CAT_CATEGORY_PROGCMD << 8},
  //{"binomial(n,p,k)", 0, "binomial(n,p,k) probability to get k success with n trials where p is the probability of success of 1 trial. binomial_cdf(n,p,k) is the probability to get at most k successes. binomial_icdf(n,p,t) returns the smallest k such that binomial_cdf(n,p,k)>=t", "10,.5,4", 0, CAT_CATEGORY_PROBA},
  {"barplot(list)", 0, "Bar plot of 1-d statistic series data in list.", "[3/2,2,1,1/2,3,2,3/2]", 0, CAT_CATEGORY_PLOT},
  {"black", "black", "Display option", "#display=black", 0, CAT_CATEGORY_OPTIONS},
  {"blue", "blue", "Display option", "#display=blue", 0, CAT_CATEGORY_OPTIONS},
  {"ceil(x)", 0, "Smallest integer not less than x", "1.2", 0, CAT_CATEGORY_REAL},
  {"cfactor(p)", 0, "Factorization over C.", "x^4-1", 0, CAT_CATEGORY_ALGEBRA | (CAT_CATEGORY_COMPLEXNUM << 8)},
  {"char(liste)", 0, "Converts a list of ASCII codes to a string.", "[97,98,99]", 0, CAT_CATEGORY_ARIT},
  {"charpoly(M,x)", 0, "Characteristic polynomial of matrix M in variable x.", "[[1,2],[3,4]],x", 0, CAT_CATEGORY_MATRIX},
  {"circle(center,radius)", 0, "Circle", "2+i,3", "1-i,1+i", CAT_CATEGORY_PLOT },
  {"clearscreen()", "clearscreen()", "Clear screen.", 0, 0, CAT_CATEGORY_PROGCMD},
  {"coeff(p,x,n)", 0, "Coefficient of x^n in polynomial p.", 0, 0, CAT_CATEGORY_POLYNOMIAL},
  //{"comb(n,k)", 0, "Returns nCk", "10,4", 0, CAT_CATEGORY_PROBA},
  {"conj(z)", 0, "Complex conjugate of z.", "1+i", 0, CAT_CATEGORY_COMPLEXNUM},
  {"cpartfrac(p,x)", 0, "Partial fraction decomposition over C.", "1/(x^4-1)", 0, CAT_CATEGORY_ALGEBRA | (CAT_CATEGORY_COMPLEXNUM << 8)},
  {"csolve(equation,x)", 0, "Solve equation (or polynomial system) in exact mode over the complex numbers.","x^2+x+1=0", 0, CAT_CATEGORY_SOLVE| (CAT_CATEGORY_COMPLEXNUM << 8)},
  {"cyan", "cyan", "Display option", "#display=cyan", 0, CAT_CATEGORY_OPTIONS},
  {"debug(f(args))", 0, "Runs user function f in step by step mode.", 0, 0, CAT_CATEGORY_PROG},
  {"degree(p,x)", 0, "Degre of polynomial p in x.", "x^4-1", 0, CAT_CATEGORY_POLYNOMIAL},
  {"desolve(equation,t,y)", 0, "Exact differential equation solving.", "desolve([y'+y=exp(x),y(0)=1])", "[y'=[[1,2],[2,1]]*y+[x,x+1],y(0)=[1,2]]", CAT_CATEGORY_SOLVE | (CAT_CATEGORY_CALCULUS << 8)},
  {"det(A)", 0, "Determinant of matrix A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"diff(f,var,[n])", 0, "Derivative of expression f with respect to var (order n, n=1 by default), for example diff(sin(x),x) or diff(x^3,x,2). For derivation with respect to x, run f' (shortcut F3). For the gradient of f, var is the list of variables.", "sin(x),x", "sin(x^2),x,3", CAT_CATEGORY_CALCULUS},
  {"draw_arc(x1,y1,rx,ry,theta1,theta2,c)", 0, "Pixelised arc of ellipse.", "100,100,60,80,0,pi,magenta", 0, CAT_CATEGORY_PROGCMD},
  {"draw_circle(x1,y1,r,c)", 0, "Pixelised circle. Option: filled", "100,100,60,cyan+filled", 0, CAT_CATEGORY_PROGCMD},
  {"draw_line(x1,y1,x2,y2,c)", 0, "Pixelised line.", "100,50,300,200,blue", 0, CAT_CATEGORY_PROGCMD},
  {"draw_pixel(x,y,color)", 0, "Colors pixel x,y. Run draw_pixel() to synchronise screen.", 0, 0, CAT_CATEGORY_PROGCMD},
  {"draw_polygon([[x1,y1],...],c)", 0, "Pixelised polygon.", "[[100,50],[30,20],[60,70]],red+filled", 0, CAT_CATEGORY_PROGCMD},
  {"draw_rectangle(x,y,w,h,c)", 0, "Rectangle.", "100,50,30,20,red+filled", 0, CAT_CATEGORY_PROGCMD},
  {"draw_string(s,x,y,c)", 0, "Draw string s at pixel x,y", "\"Bonjour\",80,60", 0, CAT_CATEGORY_PROGCMD},
  {"egcd(A,B)", 0, "Find polynomials U,V,D such that A*U+B*V=D=gcd(A,B)","x^2+3x+1,x^2-5x-1", 0, CAT_CATEGORY_POLYNOMIAL},
  //{"elif test", "elif ", "Test cascade", 0, 0, CAT_CATEGORY_PROG},
  {"egv(A)", 0, "Eigenvectors of matrix A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"egvl(A)", 0, "Eigenvalues of matrix  A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX },
  {"euler(n)",0,"Euler indicatrix: number of integers < n coprime with n","25",0,CAT_CATEGORY_ARIT},
  {"eval(f)", 0, "Evals f.", 0, 0, CAT_CATEGORY_PROGCMD},
  {"evalc(z)", 0, "Write z=x+i*y.", "1/(1+i*sqrt(3))", 0, CAT_CATEGORY_COMPLEXNUM},
  {"exact(x)", 0, "Converts x to a rational. Shortcut shift S-D", "1.2", 0, CAT_CATEGORY_REAL},
  {"exp2trig(expr)", 0, "Convert complex exponentials to sin/cos", "exp(i*x)", 0, CAT_CATEGORY_TRIG},
  {"extend", 0, "Merge 2 lists. Note that + does not merge lists, it adds vectors","#l1.extend(l2)", 0, CAT_CATEGORY_LIST},
  {"factor(p,[x])", 0, "Factors polynomial p (run ifactor for an integer). Shortcut: p=>*", "x^4-1", "x^6+1,sqrt(3)", CAT_CATEGORY_ALGEBRA| (CAT_CATEGORY_POLYNOMIAL << 8)},
  {"filled", "filled", "Display option", 0, 0, CAT_CATEGORY_PROGCMD},
  {"float(x)", 0, "Converts x to a floating point value.", "pi", 0, CAT_CATEGORY_REAL},
  {"floor(x)", 0, "Largest integer not greater than x", "pi", 0, CAT_CATEGORY_REAL},
  {"fourier_an(f,x,T,n,a)", 0, "Cosine Fourier coefficients of f", "x^2,x,2*pi,n,-pi", 0, CAT_CATEGORY_CALCULUS},
  {"fourier_bn(f,x,T,n,a)", 0, "Sine Fourier coefficients of f", "x^2,x,2*pi,n,-pi", 0, CAT_CATEGORY_CALCULUS},
  {"fourier_cn(f,x,T,n,a)", 0, "Exponential Fourier coefficients of f", "x^2,x,2*pi,n,-pi", 0, CAT_CATEGORY_CALCULUS},
  //{"from math/... import *", "from math import *", "Access to math or to random functions ([random]) or turtle with English commandnames [turtle]. Math import is not required in KhiCAS", "#from random import *", "#from turtle import *", CAT_CATEGORY_PROG},
  {"fsolve(equation,x=a..b)", 0, "Approx equation solving in interval a..b.","cos(x)=x,x=0..1", "cos(x)-x,x=0.0", CAT_CATEGORY_SOLVE},
  // {"function f(x):...", "function f(x) local y;   ffunction:;", "Function definition.", "#function f(x) local y; y:=x^2; return y; ffunction:;", 0, CAT_CATEGORY_PROG},
#ifdef WITH_QUAD
  {"gauss(q)", 0, "Quadratic form reduction", "x^2+x*y+x*z+y^2+z^2,[x,y,z]", 0, CAT_CATEGORY_LINALG},
#endif
  {"gcd(a,b,...)", 0, "Greatest common divisor. See also iegcd and egcd for extended GCD.", "23,13", "x^2-1,x^3-1", CAT_CATEGORY_ARIT | (CAT_CATEGORY_POLYNOMIAL << 8)},
  {"green", "green", "Display option", "#display=green", 0, CAT_CATEGORY_OPTIONS},
  {"halftan(expr)", 0, "Convert cos, sin, tan with tan(angle/2).","cos(x)", 0, CAT_CATEGORY_TRIG},
  {"histogram(list,min,size)", 0, "Histogram of data in list, classes begin at min of size size.","ranv(100)/100.,0,0.1", 0, CAT_CATEGORY_PLOT},
  {"iabcuv(a,b,c)", 0, "Find 2 integers u,v such that a*u+b*v=c","23,13,15", 0, CAT_CATEGORY_ARIT},
  {"ichinrem([a,m],[b,n])", 0,"Integer chinese remainder of a mod m and b mod n.", "[3,13],[2,7]", 0, CAT_CATEGORY_ARIT},
   {"idivis(n)", 0, "Returns the list of divisors of an integer n.", "10", 0, CAT_CATEGORY_ARIT},
  {"idn(n)", 0, "Identity matrix of order n", "4", 0, CAT_CATEGORY_MATRIX},
  {"iegcd(a,b)", 0, "Find integers u,v,d such that a*u+b*v=d=gcd(a,b)","23,13", 0, CAT_CATEGORY_ARIT},
  {"ifactor(n)", 0, "Factorization of an integer (not too large!). Shortcut n=>*", 0, 0, CAT_CATEGORY_ARIT},
  {"ilaplace(f,s,x)", 0, "Inverse Laplace transform of f", "s/(s^2+1),s,x", 0, CAT_CATEGORY_CALCULUS},
  {"im(z)", 0, "Imaginary part.", "1+i", 0, CAT_CATEGORY_COMPLEXNUM},
  {"inf", "inf", "Plus infinity. -inf for minus infinity and infinity for unsigned/complex infinity. Shortcut shift INS.", "oo", 0, CAT_CATEGORY_CALCULUS},
  {"input()", "input()", "Read a string from keyboard", 0, 0, CAT_CATEGORY_PROG},
  {"integrate(f,x,[a,b])", 0, "Antiderivative of f with respect to x, like integrate(x*sin(x),x). For definite integral enter optional arguments a and b, like integrate(x*sin(x),x,0,pi). For line integral, integrate([field_x,field_y],[x,y],courbe,tmin,tmax), e.g.. ellipse area G:=plotparam([2*cos(t),sin(t)],t):; integrate([0,x],[x,y],G,0,2*pi). Shortcut SHIFT F3.", "x*sin(x),x", "cos(x)/(1+x^4),x,0,inf", CAT_CATEGORY_CALCULUS},
  {"interp(X,Y)", 0, "Lagrange interpolation at points (xi,yi) where X is the list of xi and Y of yi. If interp is passed as 3rd argument, returns the divided differences list.", "[1,2,3,4,5],[0,1,3,4,4]", "[1,2,3,4,5],[0,1,3,4,4],interp", CAT_CATEGORY_POLYNOMIAL},
  {"inv(A)", 0, "Inverse of A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"iquo(a,b)", 0, "Integer quotient of a and b.", "23,13", 0, CAT_CATEGORY_ARIT},
  {"irem(a,b)", 0,"Integer remainder of a and b.", "23,13", 0, CAT_CATEGORY_ARIT},
  {"isprime(n)", 0, "Returns 1 if n is prime, 0 otherwise.", "11", "10", CAT_CATEGORY_ARIT},
  //{"jordan(A)", 0, "Jordan normal form of matrix A, returns P and D such that P^-1*A*P=D", "[[1,2],[3,4]]", "[[1,1,-1,2,-1],[2,0,1,-4,-1],[0,1,1,1,1],[0,1,2,0,1],[0,0,-3,3,-1]]", CAT_CATEGORY_MATRIX},
  //{"laguerre(n,a,x)", 0, "n-ieme Laguerre polynomial (default a=0).", "10", 0, CAT_CATEGORY_POLYNOMIAL},
  {"laplace(f,x,s)", 0, "Laplace transform of f","sin(x),x,s", 0, CAT_CATEGORY_CALCULUS},
  {"lcm(a,b,...)", 0, "Least common multiple.", "23,13", "x^2-1,x^3-1", CAT_CATEGORY_ARIT | (CAT_CATEGORY_POLYNOMIAL << 8)},
  {"lcoeff(p,x)", 0, "Leading coefficient of polynomial p in x.", "x^4-1", 0, CAT_CATEGORY_POLYNOMIAL},
  //{"legendre(n)", 0, "n-the Legendre polynomial.", "10", "10,t", CAT_CATEGORY_POLYNOMIAL},
  {"len(l)", 0, "Size of a list.", "[3/2,2,1,1/2,3,2,3/2]", 0, CAT_CATEGORY_LIST},
  {"limit(f,x=a)", 0, "Limit of f at x = a. Add 1 or -1 for unidirectional limits, e.g. limit(sin(x)/x,x=0) or limit(abs(x)/x,x=0,1). Shortcut: SHIFT MIXEDFRAC", "sin(x)/x,x=0", "exp(-1/x),x=0,1", CAT_CATEGORY_CALCULUS},
  {"line(equation)", 0, "Line of equation or from 2 points", "y=2x+1", "[1,2],[3,1]", CAT_CATEGORY_PLOT},
  {"linsolve([eq1,eq2,..],[x,y,..])", 0, "Linear system solving. May use the output of lu for O(n^2) solving (see example 2).","[x+y=1,x-y=2],[x,y]", "#p,l,u:=lu([[1,2],[3,4]]); linsolve(p,l,u,[5,6])", CAT_CATEGORY_SOLVE | (CAT_CATEGORY_LINALG <<8) | (CAT_CATEGORY_MATRIX << 16)},
  {"lu(A)", 0, "LU decomposition LU of matrix A, P*A=L*U", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"magenta", "magenta", "Display option", "#display=magenta", 0, CAT_CATEGORY_OPTIONS},
  {"map(f,l)", 0, "Maps f on element of list l.","lambda x:x*x,[1,2,3]", 0, CAT_CATEGORY_LIST},
  {"matpow(A,n)", 0, "Returns matrix A^n", "[[1,2],[3,4]],n","#assume(n>=1);matpow([[0,2],[0,4]],n)",  CAT_CATEGORY_MATRIX},
  {"matrix(r,c,func)", 0, "Matrix from a defining function.", "2,3,(j,k)->j^k", 0, CAT_CATEGORY_MATRIX},
  {"mult_c_conjugate", 0, "Multiplier par le conjugue complexe.", "1+2*i", 0,  (CAT_CATEGORY_COMPLEXNUM << 8)},
  {"mult_conjugate", 0, "Multiplier par le conjugue (sqrt).", "sqrt(2)-sqrt(3)", 0, CAT_CATEGORY_ALGEBRA},
  {"not(x)", 0, "Logical not.", 0, 0, CAT_CATEGORY_PROGCMD},
  {"numer(x)", 0, "Numerator of x.", "3/4", 0, CAT_CATEGORY_POLYNOMIAL},
#ifdef WITH_DESOLVE
  {"odesolve(f(t,y),[t,y],[t0,y0],t1)", 0, "Approx. solution of differential equation y'=f(t,y) and y(t0)=y0, value for t=t1 (add curve to get intermediate values of y)", "sin(t*y),[t,y],[0,1],2", "0..pi,(t,v)->{[-v[1],v[0]]},[0,1]", CAT_CATEGORY_SOLVE},
#endif
  {"partfrac(p,x)", 0, "Partial fraction expansion. Shortcut p=>+", "1/(x^4-1)", 0, CAT_CATEGORY_ALGEBRA},
#ifdef WITH_PLOT
  {"plot(expr,x)", 0, "Plot an expression. For example plot(sin(x)), plot(ln(x),x.0,5)", "ln(x),x,0,5", "1/x,x=1..5,xstep=1", CAT_CATEGORY_PLOT},
  //{"plotarea(expr,x=a..b,[n,meth])", 0, "Area under curve with specified quadrature.", "1/x,x=1..3,2,trapezoid", 0, CAT_CATEGORY_PLOT},
  //{"plotcontour(expr,[x=xm..xM,y=ym..yM],levels)", 0, "Levels of expr.", "x^2+2y^2,[x=-2..2,y=-2..2],[1,2]", 0, CAT_CATEGORY_PLOT},
  {"plotfield(f(t,y),[t=tmin..tmax,y=ymin..ymax])", 0, "Plot field of differential equation y'=f(t,y), an optionnally one solution by adding plotode=[t0,y0]", "sin(t*y),[t=-3..3,y=-3..3],plotode=[0,1]", 0, CAT_CATEGORY_PLOT},
  //{"plotlist(list)", 0, "Plot a list", "[3/2,2,1,1/2,3,2,3/2]", 0, CAT_CATEGORY_PLOT},
  {"plotode(f(t,y),[t=tmin..tmax,y],[t0,y0])", 0, "Plot solution of differential equation y'=f(t,y), y(t0)=y0.", "sin(t*y),[t=-3..3,y],[0,1]", 0, CAT_CATEGORY_PLOT},
  {"plotparam([x,y],t)", 0, "Parametric plot. For example plotparam([sin(3t),cos(2t)],t,0,pi) or plotparam(exp(i*t),t,0,pi)", "[cos(3t),sin(2t)],t,0,2*pi", "[t^2,t^3],t=-1..1,tstep=0.1", CAT_CATEGORY_PLOT},
  {"plotpolar(r,theta)", 0, "Polar plot.","cos(3*x),x,0,pi", "1/(1+cos(x)),x=0..pi,xstep=0.05", CAT_CATEGORY_PLOT},
  {"plotseq(f(x),x=[u0,m,M],n)", 0, "Plot f(x) on [m,M] and n terms of the sequence defined by u_{n+1}=f(u_n) and u0.","sqrt(2+x),x=[6,0,7],5", 0, CAT_CATEGORY_PLOT},
#endif
  {"point(x,y)", 0, "Point", "1,2", "1+2i", CAT_CATEGORY_PLOT },
  {"powmod(a,n,p)", 0, "Returns a^n mod p.","123,456,789", 0, CAT_CATEGORY_ARIT},
  {"print(expr)", 0, "Print expr in console", 0, 0, CAT_CATEGORY_PROG},
  {"proot(p)", 0, "Returns real and complex roots, of polynomial p. Exemple proot([1,2.1,3,4.2]) or proot(x^3+2.1*x^2+3x+4.2)", "x^3+2.1*x^2+3x+4.2", 0, CAT_CATEGORY_POLYNOMIAL},
  {"purge(x)", 0, "Clear assigned variable x. Shortcut SHIFT-FORMAT", 0, 0, CAT_CATEGORY_PROGCMD},
  //{"python(f)", 0, "Displays f in Python syntax.", 0, 0, CAT_CATEGORY_PROGCMD},
  //{"python_compat(0|1|2)", 0, "python_compat(0) Xcas syntax, python_compat(1) Python syntax with ^ interpreted as power, python_compat(2) ^ as bit xor", "0", "1", CAT_CATEGORY_PROG},
#ifdef WITH_QUAD
  {"q2a(expr,vars)", 0, "Matrix of a quadratic form", "x^2+3*x*y","x^2+3*x*y,[x,y]", CAT_CATEGORY_LINALG},
#endif  
  {"quo(p,q,x)", 0, "Quotient of synthetic division of polynomials p and q (variable x).", 0, 0, CAT_CATEGORY_POLYNOMIAL},
  {"quote(x)", 0, "Returns expression x unevaluated.", 0, 0, CAT_CATEGORY_ALGEBRA},
  //{"rand()", "rand()", "Random real between 0 and 1", 0, 0, CAT_CATEGORY_PROBA},
  //{"randint(a,b)", 0, "Random integer between a and b. With 1 argument in Xcas, random integer between 1 and n.", "5,25", "6", CAT_CATEGORY_PROBA},
  {"ranm(n,m,[loi,parametres])", 0, "Random matrix with integer coefficients or according to a probability law (ranv for a vector). Examples ranm(2,3), ranm(3,2,binomial,20,.3), ranm(4,2,normald,0,1)", "3,3","4,2,normald,0,1",  CAT_CATEGORY_MATRIX},
  {"ranv(n,[loi,parametres])", 0, "Random vector.", "10","4,normald,0,1", CAT_CATEGORY_LINALG},
  {"ratnormal(x)", 0, "Puts everything over a common denominator.", 0, 0, CAT_CATEGORY_ALGEBRA},
  {"re(z)", 0, "Real part.", "1+i", 0, CAT_CATEGORY_COMPLEXNUM},
  {"red", "red", "Display option", "#display=red", 0, CAT_CATEGORY_OPTIONS},
  {"rem(p,q,x)", 0, "Remainder of synthetic division of polynomials p and q (variable x)", 0, 0, CAT_CATEGORY_POLYNOMIAL},
  {"resultant(p,q,x)", 0, "Resultant in x of polynomials p and q.", "#P:=x^3+p*x+q;resultant(P,P',x);", 0, CAT_CATEGORY_POLYNOMIAL},
  //{"rsolve(equation,u(n),[init])", 0, "Solve a recurrence relation.","u(n+1)=2*u(n)+3,u(n),u(0)=1", "([u(n+1)=3*v(n)+u(n),v(n+1)=v(n)+u(n)],[u(n),v(n)],[u(0)=1,v(0)=2]", CAT_CATEGORY_SOLVE},
  {"seq(expr,var,a,b)", 0, "Generates a list from an expression.","j^2,j,1,10", 0, CAT_CATEGORY_PROGCMD},
  //{"si", "si  alors  sinon  fsi;", "Test.", "#f(x):=si x>0 alors x; sinon -x; fsi;// valeur absolue", 0, CAT_CATEGORY_PROG},
  {"sign(x)", 0, "Returns -1 if x is negative, 0 if x is zero and 1 if x is positive.", 0, 0, CAT_CATEGORY_REAL},
  {"simplify(expr)", 0, "Returns x in a simpler form. Shortcut expr=>/", "sin(3x)/sin(x)", 0, CAT_CATEGORY_ALGEBRA},
  {"solve(equation,x)", 0, "Exact solving of equation w.r.t. x (or of a polynomial system). Run csolve for complex solutions, linsolve for a linear system. Shortcut SHIFT XthetaT", "x^2-x-1=0,x", "[x^2-y^2=0,x^2-z^2=0],[x,y,z]", CAT_CATEGORY_SOLVE},
  {"sorted(l)", 0, "Sorts a list.","[3/2,2,1,1/2,3,2,3/2]", "[[1,2],[2,3],[4,3]],(x,y)->when(x[1]==y[1],x[0]>y[0],x[1]>y[1]", CAT_CATEGORY_LIST},
  {"subst(a,b=c)", 0, "Substitutes b for c in a. Shortcut a(b=c).", "x^2,x=3", 0, CAT_CATEGORY_ALGEBRA},
  {"sum(f,k,m,M)", 0, "Summation of expression f for k from m to M. Exemple sum(k^2,k,1,n)=>*. Shortcut ALPHA F3", "k,k,1,n", 0, CAT_CATEGORY_CALCULUS},
#if defined WITH_SHEET
  {"tablefunc(f(x),x[,xmin,xstep])", 0, "Table of values of expression f", "sqrt(x^2+x+1),x,-1,0.1",  0, CAT_CATEGORY_CALCULUS},
  {"tableseq(f(x),x,u0)", 0, "Table of values of recurrent sequence u_{n+1}=f(u_n)", "cos(x),x,0.0",  0, CAT_CATEGORY_CALCULUS},
#endif
  {"tabvar(f,[x=a..b])", 0, "Table of variations of expression f, optional arguments variable x in interval a..b", "sqrt(x^2+x+1)",  "[cos(t),sin(3t)],t", CAT_CATEGORY_CALCULUS},
  //{"tantque", "tantque  faire   ftantque;", "While loop.", "#j:=13; tantque j!=1 faire j:=when(even(j),j/2,3j+1); print(j); ftantque;", 0, CAT_CATEGORY_PROG},
  {"taylor(f,x=a,n,[polynom])", 0, "Taylor expansion of f of x at a order n, add parameter polynom to remove remainder term.","sin(x),x=0,5", "sin(x),x=0,5,polynom", CAT_CATEGORY_CALCULUS},
  {"tchebyshev1(n)", 0, "Tchebyshev polynomial 1st kind: cos(n*x)=T_n(cos(x))", "10", 0, CAT_CATEGORY_POLYNOMIAL},
  {"tchebyshev2(n)", 0, "Tchebyshev polynomial 2nd kind: sin((n+1)*x)=sin(x)*U_n(cos(x))", "10", 0, CAT_CATEGORY_POLYNOMIAL},
  {"tcollect(expr)", 0, "Linearize and collect trig functions.","sin(x)+cos(x)", 0, CAT_CATEGORY_TRIG},
  {"texpand(expr)", 0, "Expand trigonometric, exp and ln functions.","sin(3x)", 0, CAT_CATEGORY_TRIG},
  {"time(cmd)", 0, "Time to run a command or set the clock","int(1/(x^4+1),x)","8,0", CAT_CATEGORY_PROG},
  {"tlin(expr)", 0, "Trigonometric linearization of expr.","sin(x)^3", 0, CAT_CATEGORY_TRIG},
  {"trace(A)", 0, "Trace of the matrix A.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"transpose(A)", 0, "Transposes matrix A. Transconjugate command is trn(A) or A^*.", "[[1,2],[3,4]]", 0, CAT_CATEGORY_MATRIX},
  {"trig2exp(expr)", 0, "Convert complex exponentials to trigonometric functions","cos(x)^3", 0, CAT_CATEGORY_TRIG},
  {"trigcos(expr)", 0, "Convert sin^2 and tan^2 to cos^2.","sin(x)^4", 0, CAT_CATEGORY_TRIG},
  {"trigsin(expr)", 0, "Convert cos^2 and tan^2 to sin^2.","cos(x)^4", 0, CAT_CATEGORY_TRIG},
  {"trigtan(expr)", 0, "Convert cos^2 and sin^2 to tan^2.","cos(x)^4", 0, CAT_CATEGORY_TRIG},
  {"tstep",0, "Parametric graph step", "tstep=0.2", 0, CAT_CATEGORY_OPTIONS},
  {"xstep",0, "Funciton graph step", "xstep=0.2", 0, CAT_CATEGORY_OPTIONS},
  //{"version", "version()", "Khicas 1.5.0, (c) B. Parisse et al. www-fourier.ujf-grenoble.fr/~parisse\nLicense GPL version 2. Interface adapted from Eigenmath for Casio, G. Maia, http://gbl08ma.com. Do not use if CAS calculators are forbidden.", 0, 0, CAT_CATEGORY_PROGCMD},
  {"yellow", "yellow", "Display option", "#display=yellow", 0, CAT_CATEGORY_OPTIONS},
  {"|", "|", "Logical or", "#1|2", 0, CAT_CATEGORY_PROGCMD},
  {"~", "~", "Complement", "#~7", 0, CAT_CATEGORY_PROGCMD},
};

constexpr int CAT_COMPLETE_COUNT_EN=sizeof(completeCaten)/sizeof(catalogFunc);
// int CAT_COMPLETE_COUNT=sizeof(completeCat)/sizeof(catalogFunc);

std::string insert_string(int index){
  std::string s;
  const catalogFunc * completeCat=(lang==1)?completeCatfr:completeCaten;
  if (completeCat[index].insert)
    s=completeCat[index].insert;
  else {
    s=completeCat[index].name;
    int pos=s.find('(');
    if (pos>=0 && pos<s.size())
      s=s.substr(0,pos+1);
  }
  return s;//s+' ';
}

int showCatalog(char* insertText,int preselect,int menupos) {
  // returns 0 on failure (user exit) and 1 on success (user chose a option)
#ifdef WITH_UNITS
#ifdef WITH_PLOT
  MenuItem menuitems[CAT_CATEGORY_PLOT+1];
  menuitems[CAT_CATEGORY_PLOT].text = (char*)((lang==1)?"Courbes, points":"Curves, points");
#else
  MenuItem menuitems[CAT_CATEGORY_UNIT+1];
#endif
  menuitems[CAT_CATEGORY_PHYS].text = (char*)((lang==1)?"Constantes physique (frac)":"Physics constants");
  menuitems[CAT_CATEGORY_UNIT].text = (char*)((lang==1)?"Unites physiques (^)":"Units (^)");
#else
  MenuItem menuitems[CAT_CATEGORY_SOLVE+1];
#endif
  menuitems[CAT_CATEGORY_ALL].text = (char*)(lang?"Tout":"All");
  menuitems[CAT_CATEGORY_ALGEBRA].text = (char*)(lang?"Algebre":"Algebra");
  menuitems[CAT_CATEGORY_LINALG].text = (char*)(lang?"Algebre lineaire":"Linear algebra");
  menuitems[CAT_CATEGORY_CALCULUS].text = (char*)(lang?"Analyse":"Calculus");
  menuitems[CAT_CATEGORY_ARIT].text = (char*)"Arithmetic, crypto";
  menuitems[CAT_CATEGORY_COMPLEXNUM].text = (char*)"Complexes";
  menuitems[CAT_CATEGORY_POLYNOMIAL].text = (char*)(lang?"Polynomes":"Polynomials");
  menuitems[CAT_CATEGORY_PROG].text = (char*)(lang?"Programmes":"Programs");
  menuitems[CAT_CATEGORY_PROGCMD].text = (char*)"Programmes cmds";
  menuitems[CAT_CATEGORY_REAL].text = (char*)(lang?"Reels (0)":"Reals (0)");
  menuitems[CAT_CATEGORY_OPTIONS].text = (char*)"Options (X,T..)";
  menuitems[CAT_CATEGORY_MATRIX].text = (char*)"Matrices (matrix)";
  menuitems[CAT_CATEGORY_LIST].text = (char*)(lang?"Listes (stats)":"Lists");
  menuitems[CAT_CATEGORY_TRIG].text = (char*)(lang?"Trigonometrie (trig)":"Trigonometry");
  menuitems[CAT_CATEGORY_SOLVE].text = (char*)(lang?"Resoudre (resol)":"Solve");
  
  Menu menu;
  menu.items=menuitems;
  menu.numitems=sizeof(menuitems)/sizeof(MenuItem);
  menu.scrollout=1;
  menu.title = (char*)(lang?"Liste de commandes":"List of commands");
  menu.height=12;
  //puts("catalog 1");
  while(1) {
    if (preselect)
      menu.selection=preselect;
    else {
      if (menupos>0)
	menu.selection=menupos;
      //drawRectangle(0,18,LCD_WIDTH_PX,LCD_HEIGHT_PX-18,_WHITE);
      int sres = doMenu(&menu);
      if (sres != MENU_RETURN_SELECTION)
	return 0;
    }
    // puts("catalog 3");
    if(doCatalogMenu(insertText, menuitems[menu.selection-1].text, menu.selection-1)) {
      const char * ptr=0;
      if (strcmp("matrix ",insertText)==0 && (ptr=input_matrix(false)) )
	return 0;
      if (strcmp("list ",insertText)==0 && (ptr=input_matrix(true)) )
	return 0;
      return 1;
    }
    if (preselect)
      return 0;
  }
  return 0;
}


// 0 on exit, 1 on success
int doCatalogMenu(char* insertText, const char* title, int category,const char * cmdname) {
  constexpr int CAT_COMPLETE_COUNT=((lang==1)?CAT_COMPLETE_COUNT_FR:CAT_COMPLETE_COUNT_EN);
  const catalogFunc * completeCat=(lang==1)?completeCatfr:completeCaten;
  for (;;){
#ifdef FAKE_GIAC
    int allcmds=CAT_COMPLETE_COUNT;
#else
    int allcmds=giac::builtin_lexer_functions_end()-giac::builtin_lexer_functions_begin(); // CAT_COMPLETE_COUNT;;
    // dbg_printf("catalog #nfunc=%i #sizeof=%i",giac::builtin_lexer_functions_end()-giac::builtin_lexer_functions_begin(),sizeof(MenuItem));
#endif
    bool isall=category==CAT_CATEGORY_ALL; 
    bool isopt=false;//category==CAT_CATEGORY_OPTIONS;
    int nitems = isall? allcmds:(CAT_COMPLETE_COUNT);//CAT_COMPLETE_COUNT;
    MenuItem menuitems[nitems];
    int cur = 0,curmi = 0,i=0,menusel=-1,cmdl=cmdname?strlen(cmdname):0;
    while(cur<nitems) {
      if (!isall && menusel<0 && cmdname && !strncmp(cmdname,completeCat[cur].name,cmdl))
        menusel=curmi;
      menuitems[curmi].type = MENUITEM_NORMAL;
      menuitems[curmi].color = _BLACK;    
      int cat=completeCat[cur].category;
      if ( isall ||
           (cat & 0xff) == category ||
           (cat & 0xff00) == (category<<8) ||
           (cat & 0xff0000) == (category <<16)
           ){
#ifdef FAKE_GIAC
        const char * text=completeCat[cur].name;
#else
        const char * text=isall?(giac::builtin_lexer_functions_begin()+cur)->first:isopt?(giac::lexer_tab_int_values_begin+curmi)->keyword:completeCat[cur].name;
#endif        
        menuitems[curmi].isfolder = isall?allcmds:cur; // little hack: store index of the command in the full list in the isfolder property (unused by the menu system in this case)
        for (;i<CAT_COMPLETE_COUNT;++i){
          const char * catname=completeCat[i].name;
          int tmp=strcmp(catname,text);
          if (tmp>=0){
            size_t st=strlen(text),j=tmp?0:st;
            for (;j<st;++j){
              if (catname[j]!=text[j])
                break;
            }
            if (j==st && (!isalphanum(catname[j]))){
              menuitems[curmi].isfolder = i;
              ++i;
            }
            break;
          }
        }
        menuitems[curmi].text = (char *) text;
        if (menusel<0 && cmdname && !strncmp(cmdname,text,cmdl))
          menusel=curmi;
        curmi++;
      }
      cur++;
    }
    
    Menu menu;
    if (menusel>=0)
      menu.selection=menusel+1;
    menu.items=menuitems;
    menu.numitems=curmi;
    if (isopt){ menu.selection=5; menu.scroll=4; }
    if (curmi>=100)
      lock_alpha(); // SetSetupSetting( (unsigned int)0x14, 0x88);	
    // DisplayStatusArea();
    menu.scrollout=1;
    menu.title = (char *) title;
    menu.type = MENUTYPE_FKEYS;
    menu.height = 12;
    while(1) {
      //drawRectangle(0,18,LCD_WIDTH_PX,LCD_HEIGHT_PX-18,_WHITE);
      Printmini(0,C58,(
                       category==CAT_CATEGORY_ALL?(lang?"   input   |  exemple1  |  exemple2  |           |   aide cmd     ":"   input   |  example1  |  example2  |           |   cmd help    "):(lang?"   input   | exemple1 | exemple2 |    cmds    |  aide cmd    ":"   input   | example1 | example2 |    cmds    |  cmd help   ")
                       ),MINI_REV);
      int sres = doMenu(&menu);
      if ((sres==KEY_CTRL_F4 || sres==KEY_CTRL_F9 || sres==KEY_CTRL_F14) && category!=CAT_CATEGORY_ALL){
        break;
      }
      if(sres == MENU_RETURN_EXIT){
        reset_alpha();
        return sres;
      }
      int index=menuitems[menu.selection-1].isfolder;
      if(sres == KEY_CTRL_F5 || sres==KEY_CTRL_F10 || sres==KEY_CTRL_F15) {
	const char * example=index<allcmds?completeCat[index].example:0;
	const char * example2=index<allcmds?completeCat[index].example2:0;
	textArea text;
	text.editable=false;
	text.clipline=-1;
	text.title = (char*)(lang?"Aide sur la commande":"Help on command");
	text.allowF1=true;
	text.python=true;
	std::vector<textElement> & elem=text.elements;
	elem = std::vector<textElement> (example2?4:3);
	elem[0].s = index<allcmds?completeCat[index].name:menuitems[menu.selection-1].text;
	elem[0].newLine = 0;
	//elem[0].color = COLOR_BLUE;
	elem[1].newLine = 1;
	elem[1].lineSpacing = 1;
	//elem[1].minimini=1;
	std::string autoexample;
	if (index<allcmds)
	  elem[1].s = completeCat[index].desc;
	else {
	  int token=menuitems[menu.selection-1].token;
	  elem[1].s=lang?"Desole, pas d'aide disponible...":"Sorry, no help available";
	  // *logptr(contextptr) << token << endl;
	  if (isopt){
	  }
	  if (isall){
	  }
	}
	std::string ex("F2: ");
	elem[2].newLine = 1;
	elem[2].lineSpacing = 0;
	//elem[2].minimini=1;
	if (example){
	  if (example[0]=='#')
	    ex += example+1;
	  else {
	    ex += insert_string(index);
	    ex += example;
	    ex += ")";
	  }
	  elem[2].s = ex;
	  if (example2){
	    string ex2="F3: ";
	    if (example2[0]=='#')
	      ex2 += example2+1;
	    else {
	      ex2 += insert_string(index);
	      ex2 += example2;
	      ex2 += ")";
	    }
	    elem[3].newLine = 1;
	    // elem[3].lineSpacing = 0;
	    //elem[3].minimini=1;
	    elem[3].s=ex2;
	  }
	}
	else {
	  if (autoexample.size())
	    elem[2].s=ex+autoexample;
	  else
	    elem.pop_back();
	}
	sres=doTextArea(&text);
      }
      if (sres == KEY_CTRL_F2 || sres==KEY_CTRL_F3 ||
	  sres == KEY_CTRL_F7 || sres==KEY_CTRL_F8 ||
	  sres == KEY_CTRL_F12 || sres==KEY_CTRL_F13
	  ) {
	reset_alpha();
	if (index<allcmds && completeCat[index].example){
	  std::string s(insert_string(index));
	  const char * example=0;
	  if (sres==KEY_CTRL_F2 || sres == KEY_CTRL_F7 || sres == KEY_CTRL_F12)
	    example=completeCat[index].example;
	  else
	    example=completeCat[index].example2;
	  if (example){
	    if (example[0]=='#')
	      s=example+1;
	    else {
	      s += example;
	      s += ")";
	    }
	  }
	  strcpy(insertText, s.c_str());
	  return 1;
	}
	if (isopt){
	  int token=menuitems[menu.selection-1].token;
	  *insertText=0;
	  strcat(insertText,menuitems[menu.selection-1].text);
	  return 1;
	}
	sres=KEY_CTRL_F1;
      }
      if(sres == MENU_RETURN_SELECTION || sres == KEY_CTRL_F1 || sres == KEY_CTRL_F6 || sres == KEY_CTRL_F11) {
	reset_alpha();
	strcpy(insertText,index<allcmds?insert_string(index).c_str():menuitems[menu.selection-1].text);
	return 1;
      }
    }
    title="CATALOG";
    category=0;
  } // end endless for
}

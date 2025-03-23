// -*- mode:C++ ; compile-command: "g++-3.4 -I. -I.. -g -c Equation.cc -DHAVE_CONFIG_H -DIN_GIAC -Wall" -*-
/*
 *  Copyright (C) 2005,2014 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "kdisplay.h"
#include "menuGUI.h"
#include "console.h"
#include "textGUI.h"
#include "main.h"
#include "file.h"
#include <sys/lcd.h>

extern giac::context * contextptr;

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC
  
  xcas::tableur * new_tableur(GIAC_CONTEXT){
    xcas::tableur * sheetptr=new xcas::tableur;
#ifdef NUMWORKS
    sheetptr->nrows=14; sheetptr->ncols=4;
#else
    sheetptr->nrows=20; sheetptr->ncols=5;
#endif
    gen g=vecteur(sheetptr->ncols);
    sheetptr->m=makefreematrice(vecteur(sheetptr->nrows,g));
    makespreadsheetmatrice(sheetptr->m,contextptr);
    sheetptr->cur_row=sheetptr->cur_col=sheetptr->disp_row_begin=sheetptr->disp_col_begin=0;
    sheetptr->sel_row_begin=sheetptr->sel_col_begin=-1;
    sheetptr->cmd_pos=sheetptr->cmd_row=sheetptr->cmd_col=-1;
    sheetptr->changed=false;
    sheetptr->recompute=true;
    sheetptr->matrix_fill_cells=true;
    sheetptr->movedown=true;
    sheetptr->filename="session";
    return sheetptr;
  }

  gen current_sheet(const gen & g,GIAC_CONTEXT){
    if (!xcas::sheetptr)
      xcas::sheetptr=new_tableur(contextptr);
    xcas::tableur & t=*xcas::sheetptr;
    if (ckmatrix(g,true)){
      t.m=*g._VECTptr;
      makespreadsheetmatrice(t.m,contextptr);
      t.cur_row=t.cur_col=0;
      t.nrows=t.m.size();
      t.ncols=t.m.front()._VECTptr->size();
      t.sel_row_begin=-1;
      t.cmd_row=t.cmd_pos=-1;
      return 1;
    }
    int r,c;
    if (iscell(g,c,r,contextptr)){
      if (r>=t.nrows||c>=t.ncols)
	return undef;
      gen tmp=t.m[r];
      tmp=tmp[c];
      return tmp[1];
    }
    if (g.type==_VECT && g.subtype==0 && g._VECTptr->empty())
      return gen(extractmatricefromsheet(t.m,false),_SPREAD__VECT);
    gen m(extractmatricefromsheet(t.m,true /* FIXME? */),_MATRIX__VECT);
    if (g.type==_VECT && g._VECTptr->empty())
      return m;
    return m[g];
  }
  static const char _current_sheet_s []="current_sheet";
  static define_unary_function_eval(__current_sheet,&current_sheet,_current_sheet_s);
  define_unary_function_ptr5( at_current_sheet ,alias_at_current_sheet,&__current_sheet,_QUOTE_ARGUMENTS,true);
  
#ifndef NO_NAMESPACE_GIAC
}
#endif // ndef NO_NAMESPACE_GIAC


const int MAX_DISP_RADIUS=2048;

using namespace std;
using namespace giac;

#ifndef NO_NAMESPACE_XCAS
namespace xcas {
#endif // ndef NO_NAMESPACE_XCAS

#ifdef WITH_SHEET
  xcas::tableur * sheetptr=0;
/* **************************
   * SPREADSHEET CODE       *
   ************************** */
#ifdef HP39
const int row_height=15;
const int col_width=45;
#else
const int row_height=20;
const int col_width=60;
#endif

#ifndef TICE
  string printcell(int i,int j){
    string s="";
    s+=char('A'+j);
    s+=giac::print_INT_(i);
    return s;
  }
  string printsel(int r,int c,int R,int C){
    return printcell(r,c)+":"+printcell(R,C);
  }
#else // TICE
  string printcell(int i,int j) {
    char s[sizeof("A-8388608")];
    s[0] = 'A' + j;
    sprintf(&s[1],"%d", i);
    return s;
  }
  string printsel(int r,int c,int R,int C){
    char s[sizeof("A-8388608:A-8388608")];
    sprintf(s, "%c%d:%c%d", 'A' + c, r, 'A' + C, R);
    return s;
  }
#endif // TICE

void change_undo(tableur & t){
  t.undo=t.m;
  t.changed=true;
}

  string print_tableur(const tableur & t,GIAC_CONTEXT){
    string s="spreadsheet[";
    for (int i=0;i<t.nrows;++i){
      printcell_current_row(contextptr)=i;
      s += "[";
      gen g=t.m[i];
      if (g.type!=_VECT) continue;
      vecteur & v=*g._VECTptr;
      for (int j=0;j<t.ncols;++j){
	gen vj=v[j];
	if (vj.type==_VECT && vj._VECTptr->size()==3){
	  vecteur vjv=*vj._VECTptr;
	  vjv[1]=0;
	  vj=gen(vjv,vj.subtype);
	}
	printcell_current_col(contextptr)=j;
	s += vj.print(contextptr);
	if (j==t.ncols-1)
	  s += "]";
	else
	  s += ",";
      }
      if (i==t.nrows-1)
	s += "]";
      else
	s += ",";      
    }
    return s;
  }  
  
  
void save_sheet_to(tableur & t,const char * filename_,GIAC_CONTEXT){
#if 1
  string s=print_tableur(t,contextptr);
#else
  string s=gen(extractmatricefromsheet(t.m,false),_SPREAD__VECT).print(contextptr);
#endif
  string filename(remove_path(giac::remove_extension(filename_)));
#ifdef TICE
  if (filename.size()>7)
    filename=filename.substr(0,7);
  filename += "T.tab";
#else
  filename+=".tab";
#ifdef NSPIRE_NEWLIB
  filename+=".tns";
#endif
#endif
  write_file(filename.c_str(),s.c_str());
}
void save_sheet(tableur & t,GIAC_CONTEXT){
  save_sheet_to(t,t.filename,contextptr);
}

void sheet_status(tableur & t,GIAC_CONTEXT){
  string st;
  st="tabl ";
  if (t.var.type==_IDNT)
    st += t.var.print(contextptr);
  else
    st += "<>";
  st += ' ';
  st += t.filename ;
  st += " R";
  st += giac::print_INT_(t.nrows);
  st += " C";
  st += giac::print_INT_(t.ncols);
  if (t.changed)
    st += " *";
  else
    st += " -";
  if (t.sel_row_begin>=0)
    st += (lang==1)?" esc: annule selection":" esc: cancel selection";
  else {
    if (t.cmd_row>=0)
      st += (lang==1)?" esc: annule ligne cmd":" esc: cancel cmdline";
  }
  statuslinemsg(st.c_str());
}

  void fix_sheet(tableur & t,GIAC_CONTEXT){
    for (int i=0;i<t.nrows;++i){
      vecteur & v = *t.m[i]._VECTptr;
      for (int j=0;j<t.ncols;++j){
	gen & g=v[j];
	if (g.type==_VECT){
	  vecteur & w=*g._VECTptr;
	  if (w[0].type==_SYMB){
	    // cout << "fix " << w[0] << "\n";
	    w[0]=spread_convert(w[0],i,j,contextptr);
	  }
	}
      }
    }
  }
void waitforvblank(){}
  
  bool sheet_display(tableur &t,GIAC_CONTEXT,bool full_redraw){
  int disp_rows=(LCD_HEIGHT_PX-STATUS_AREA_PX)/row_height-3;
  int disp_cols=LCD_WIDTH_PX/(col_width+4)-1;
  if (t.disp_row_begin>t.cur_row)
    t.disp_row_begin=t.cur_row;
  if (t.disp_row_begin<t.cur_row-disp_rows+1)
    t.disp_row_begin=t.cur_row-disp_rows+1;
  if (t.disp_col_begin>t.cur_col)
    t.disp_col_begin=t.cur_col;
  if (t.disp_col_begin<t.cur_col-disp_cols+1)
    t.disp_col_begin=t.cur_col-disp_cols+1;
  int I=giac::giacmin(giac::giacmin(t.nrows,t.m.size()),t.disp_row_begin+disp_rows);
  bool has_sel=t.sel_row_begin>=0 && t.sel_row_begin<t.nrows;
  int sel_r=t.sel_row_begin,sel_R=t.cur_row,sel_c=t.sel_col_begin,sel_C=t.cur_col;
  if (sel_r>sel_R)
    swapint(sel_r,sel_R);
  if (sel_c>sel_C)
    swapint(sel_c,sel_C);
  bool has_cmd=t.cmd_row>=0 && t.cmd_row<t.nrows;
  waitforvblank();
  int sheety=LCD_HEIGHT_PX-STATUS_AREA_PX-2*row_height,xtooltip=0;
  string s;
  if (!full_redraw && !has_sel && has_cmd && t.cmd_row==t.cur_row && t.cmd_col==t.cur_col  && t.cmd_pos>=0 && t.cmd_pos<=t.cmdline.size()){
    // minimal redraw
    drawRectangle(0,sheety+STATUS_AREA_PX,LCD_WIDTH_PX,2*row_height,COLOR_WHITE);
  }
  else {
    drawRectangle(0,STATUS_AREA_PX,LCD_WIDTH_PX,row_height,COLOR_WHITE); // clear column indices row
    if (has_sel)
      s=printsel(sel_r,sel_c,sel_R,sel_C);
    else
      s=printcell(t.cur_row,t.cur_col);
    os_draw_string(2,1,COLOR_BLACK,COLOR_WHITE,s.c_str(),false);
    int y=row_height;
    int x=col_width;
    int J=giac::giacmin(t.ncols,t.disp_col_begin+disp_cols);
    for (int j=t.disp_col_begin;j<J;++j){
      draw_line(x,STATUS_AREA_PX,x,STATUS_AREA_PX+row_height,COLOR_BLACK);
      char colname[3]="A"; 
      if (j>=26){ // if we accept more than 26 cols
        colname[0] += j/26;
        colname[1] = 'A'+(j%26);
        colname[2]=0;
      }
      else
        colname[0] += (j % 26);
      os_draw_string(x+col_width/2-4,2,COLOR_BLACK,COLOR_WHITE,colname);
      x+=col_width+4;
    }
    int waitn=2;
    for (int i=t.disp_row_begin;i<I;++i){
      if ( (i-t.disp_row_begin) % waitn==waitn-1)
        waitforvblank();
      drawRectangle(0,y+STATUS_AREA_PX,LCD_WIDTH_PX,row_height,COLOR_WHITE); // clear current row
      // draw_line(0,y,LCD_WIDTH_PX,y,COLOR_BLACK);
      os_draw_string(4,y+1,COLOR_BLACK,COLOR_WHITE,giac::print_INT_(i).c_str()); // row number
      gen g=t.m[i];
      if (g.type!=_VECT)
        return false;
      vecteur & v=*g._VECTptr;
      int J=giac::giacmin(t.ncols,v.size());
      J=giac::giacmin(J,t.disp_col_begin+disp_cols);
      x=col_width;
      for (int j=t.disp_col_begin;j<J;++j){
        draw_line(x,STATUS_AREA_PX+y,x,STATUS_AREA_PX+y+row_height,COLOR_BLACK);
        gen vj=v[j];
        if (vj.type<_IDNT)
          vj=makevecteur(vj,vj,0);
        if (vj.type==_VECT && vj._VECTptr->size()==3){
          bool iscur=i==t.cur_row && j==t.cur_col;
          string s;
          if (iscur){
            if (!has_cmd)
              t.cmdline=(*vj._VECTptr)[0].print(contextptr);
          }
          bool rev=has_sel?(sel_r<=i && i<=sel_R && sel_c<=j && j<=sel_C):iscur;
          if (rev)
            drawRectangle(x+1,y+STATUS_AREA_PX,col_width+4,row_height,color_gris);
          gen content=(*vj._VECTptr)[1];
          if (taille(content,32)>=32)
            s="...";
          else
            s=content.print(contextptr);
          int dx=os_draw_string(0,0,0,0,s.c_str(),true); // find width
          if (dx<col_width){
#ifdef HP39
            os_draw_string(x+2,y,rev?COLOR_WHITE:COLOR_BLACK,rev?COLOR_BLACK:COLOR_WHITE,s.c_str(),false); // draw
#else
            os_draw_string(x+2,y+1,COLOR_BLACK,rev?color_gris:COLOR_WHITE,s.c_str(),false); // draw
#endif
          }
          else {
            if (iscur && !has_sel && t.cmd_row<0)
              statuslinemsg(s.c_str());
            s=s.substr(0,8)+"...";
#ifdef HP39
            os_draw_string_small(x+2,y,rev?COLOR_WHITE:COLOR_BLACK,rev?COLOR_BLACK:COLOR_WHITE,s.c_str(),false); // draw
#else    
            os_draw_string_small(x+2,y+1,COLOR_BLACK,rev?color_gris:COLOR_WHITE,s.c_str(),false); // draw
#endif
          }
        }
        x+=col_width+4;
      }
      draw_line(0,y+STATUS_AREA_PX,LCD_WIDTH_PX,y+STATUS_AREA_PX,COLOR_BLACK);
      y+=row_height;
    }
    waitforvblank();
    drawRectangle(0,y+STATUS_AREA_PX,LCD_WIDTH_PX,LCD_HEIGHT_PX-y,COLOR_WHITE); // clear cmdline
    draw_line(0,y+STATUS_AREA_PX,LCD_WIDTH_PX,y+STATUS_AREA_PX,COLOR_BLACK);
  } // 
  // commandline
  int p=python_compat(contextptr); python_compat(0,contextptr);
  //int xpe=xcas_python_eval; xcas_python_eval=0;
  s=t.cmdline;
  int dx=os_draw_string(0,0,0,0,s.c_str(),true),xend=2; // find width
  bool small=//t.keytooltip ||
    dx>=LCD_WIDTH_PX-20;
  if (t.cmd_row>=0 && t.cmd_pos>=0 && t.cmd_pos<=s.size()){
#ifdef HP39
    xend=os_draw_string(xend,sheety,COLOR_BLACK,COLOR_WHITE,printcell(t.cmd_row,t.cmd_col).c_str())+5;
#else
    xend=os_draw_string(xend,sheety,COLOR_BLUE,COLOR_WHITE,printcell(t.cmd_row,t.cmd_col).c_str())+5;
#endif
    string s1=s.substr(0,t.cmd_pos);
#if 1
    xtooltip=xend=print_color(xend,sheety,s1.c_str(),COLOR_BLACK,false,small);
#else
    if (small)
      xend=os_draw_string_small(xend,sheety,COLOR_BLACK,COLOR_WHITE,s1.c_str(),false);
    else
      xend=os_draw_string(xend,sheety,COLOR_BLACK,COLOR_WHITE,s1.c_str(),false);
#endif
    drawRectangle(xend+1,STATUS_AREA_PX+sheety+2,2,small?10:13,COLOR_BLACK);
    xend+=4;
    s=s.substr(t.cmd_pos,s.size()-t.cmd_pos);
    if (has_sel){
      s1=printsel(sel_r,sel_c,sel_R,sel_C);
#ifdef HP39
      xend=os_draw_string_small(xend,sheety,COLOR_BLACK,COLOR_WHITE,s1.c_str(),false);
#else
      if (small)
        xend=os_draw_string_small(xend,sheety,COLOR_BLACK,color_gris,s1.c_str(),false);
      else
        xend=os_draw_string(xend,sheety,COLOR_BLACK,color_gris,s1.c_str(),false);        
#endif
    }
    else {
      if (t.cmd_row!=t.cur_row || t.cmd_col!=t.cur_col)
#ifdef HP39
        xend=os_draw_string_small(xend,sheety,COLOR_BLACK,COLOR_WHITE,printcell(t.cur_row,t.cur_col).c_str(),false);
#else
      if (small)
        xend=os_draw_string_small(xend,sheety,COLOR_BLACK,color_gris,printcell(t.cur_row,t.cur_col).c_str(),false);
      else
        xend=os_draw_string(xend,sheety,COLOR_BLACK,color_gris,printcell(t.cur_row,t.cur_col).c_str(),false);
#endif
    }
  } // end cmdline active
  else
    xend=os_draw_string(xend,sheety,COLOR_BLACK,COLOR_WHITE,printcell(t.cur_row,t.cur_col).c_str())+5;    
  int bg=t.cmd_row>=0?COLOR_WHITE:57051;
#if 1
    xend=print_color(xend,sheety,s.c_str(),COLOR_BLACK,false,small);
#else
  if (small)
    xend=os_draw_string_small(xend,sheety,COLOR_BLACK,bg,s.c_str(),false);
  else
    xend=os_draw_string(xend,sheety,COLOR_BLACK,bg,s.c_str(),false);
#endif
  if (t.keytooltip)
    t.keytooltip=tooltip(xtooltip,sheety,t.cmd_pos,t.cmdline.c_str());
  python_compat(p,contextptr); //xcas_python_eval=xpe;
  // fast menus
  string menu("   stat1d   |  stat2d   |  sequence |   edit    |   menu   ");
  bg=65039;// bg=52832;
  drawRectangle(0,213+STATUS_AREA_PX,LCD_WIDTH_PX,9,bg);
  os_draw_string_small(0,213,COLOR_BLACK,bg,menu.c_str());
  return true;
}

void activate_cmdline(tableur & t){
  if (t.cmd_row==-1){
    t.cmd_row=t.cur_row;
    t.cmd_col=t.cur_col;
    t.cmd_pos=t.cmdline.size();
  }
}

bool sheet_eval(tableur & t,GIAC_CONTEXT,bool ckrecompute=true){
  t.changed=true;
  if (!ckrecompute || t.recompute){
    giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
    statuslinemsg(!lang?"cancel: stop computation":"annul: stoppe calcul",COLOR_RED);
    spread_eval(t.m,contextptr);
    giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
  }
  return true;
}

void copy_right(tableur & t,GIAC_CONTEXT){
  int R=t.cur_row,C=t.cur_col,c=t.ncols;
  vecteur v=*t.m[R]._VECTptr;
  gen g=v[C];
  for (int i=C+1;i<c;++i){
    v[i]=freecopy(g);
  }
  t.m[R]=v;
  sheet_eval(t,contextptr,true);
}

void copy_down(tableur & t,GIAC_CONTEXT){
  int R=t.cur_row,C=t.cur_col,r=giac::giacmin(t.nrows,t.m.size());
  gen g=t.m[R][C];
  for (int i=R+1;i<r;++i){
    vecteur v=*t.m[i]._VECTptr;
    v[C]=freecopy(g);
    t.m[i]=v;
  }
  sheet_eval(t,contextptr,true);
}

void paste(tableur & t,const matrice & m,GIAC_CONTEXT){
  int r=t.cur_row,c=t.cur_col,R=t.nrows,C=t.ncols;
  int dr=t.clip.size(),dc=0;
  if (r+dr>R)
    dr=R-r;
  if (dr && ckmatrix(m,true)){
    dc=m.front()._VECTptr->size();
    if (c+dc>C)
      dc=C-c;
    if (dc){
      for (int i=0;i<dr;++i){
	const vecteur & w=*m[i]._VECTptr;
	vecteur v=*t.m[r+i]._VECTptr;
	for (int j=0;j<dc;++j)
	  v[c+j]=w[j];
	t.m[r+i]=v;
      }
    }
  }
  sheet_eval(t,contextptr,true);
}

void paste(tableur & t,GIAC_CONTEXT){
  paste(t,t.clip,contextptr);
}

void sheet_pntv(const vecteur & v,vecteur & res);
void sheet_pnt(const gen & g,vecteur & res){
  if (g.type==_VECT)
    sheet_pntv(*g._VECTptr,res);
  if (g.is_symb_of_sommet(at_pnt))
    res.push_back(g);
}

void sheet_pntv(const vecteur & v,vecteur & res){
  for (int i=0;i<v.size();++i){
    sheet_pnt(v[i],res);
  }
}

void resizesheet(tableur &t){
  int cur_r=t.m.size(),cur_c=t.m.front()._VECTptr->size(),nr=t.nrows,nc=t.ncols;
  if (nr!=cur_r || nc!=cur_c){
    if (do_confirm(((lang==1?"Redimensionner ":"Resize ")+giac::print_INT_(cur_r)+"x"+giac::print_INT_(cur_c)+"->"+giac::print_INT_(nr)+"x"+giac::print_INT_(nc)).c_str())){
      vecteur fill(3,0);
      if (nr<cur_r) // erase rows
	t.m.resize(nr);
      else {
	for (;cur_r<nr;++cur_r){
	  vecteur tmp;
	  for (int j=0;j<nc;++j)
	    tmp.push_back(freecopy(fill));
	  t.m.push_back(tmp);
	}
      }
      for (int i=0;i<nr;++i){
	vecteur & v=*t.m[i]._VECTptr;
	int cur_c=v.size();
	if (nc<cur_c){
	  t.m[i]=vecteur(v.begin(),v.begin()+nc);
	}
	else {
	  for (;cur_c<nc;++cur_c)
	    v.push_back(freecopy(fill));
	}
      }
      t.cur_row=giac::giacmin(t.cur_row,t.nrows);
      t.cur_col=giac::giacmin(t.cur_col,t.ncols);
      t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
    } // end confirmed table resize
    else {
      t.nrows=cur_r;
      t.ncols=cur_c;
    }
  }  
}

void sheet_menu_setup(tableur & t,GIAC_CONTEXT){
  Menu smallmenu;
  smallmenu.numitems=7;
  MenuItem smallmenuitems[smallmenu.numitems];
  smallmenu.items=smallmenuitems;
  smallmenu.height=12;
  smallmenu.scrollbar=1;
  smallmenu.scrollout=1;
  smallmenu.title = (char*)(lang==1?"Configuration tableur":"Sheet config");
  smallmenuitems[3].type = MENUITEM_CHECKBOX;
  smallmenuitems[3].text = (char*)"Reeval";
  smallmenuitems[4].type = MENUITEM_CHECKBOX;
  smallmenuitems[4].text = (char*)(lang==1?"Matrice: remplir cellules":"Matrix: fill cells");
  smallmenuitems[5].type = MENUITEM_CHECKBOX;
  smallmenuitems[5].text = (char*)(lang==1?"Deplacement vers le bas":"Move down");
  smallmenuitems[smallmenu.numitems-1].text = (char*) "Quit";
  while(1) {
    string dig("Digits (in Xcas): ");
    dig += giac::print_INT_(decimal_digits(contextptr));
    smallmenuitems[0].text = (char*)dig.c_str();
    string nrows((lang==1?"Lignes ":"Rows ")+giac::print_INT_(t.nrows));
    smallmenuitems[1].text = (char*)nrows.c_str();
    string ncols((lang==1?"Colonnes ":"Cols ")+giac::print_INT_(t.ncols));
    smallmenuitems[2].text = (char*)ncols.c_str();
    smallmenuitems[3].value = t.recompute;
    smallmenuitems[4].value = t.matrix_fill_cells;
    smallmenuitems[5].value = t.movedown;
    int sres = doMenu(&smallmenu);
    if (sres==MENU_RETURN_EXIT){
      resizesheet(t);
      break;
    }
    if (sres == MENU_RETURN_SELECTION  || sres==KEY_CTRL_EXE) {
      if (smallmenu.selection == 1){
	double d=decimal_digits(contextptr);
	if (inputdouble("Nombre de digits?",d) && d==int(d) && d>0){
	  decimal_digits(d,contextptr);
	}
	continue;
      }
      if (smallmenu.selection == 2){
	double d=t.nrows;
	if (inputdouble((lang==1?"Nombre de lignes?":"Rows?"),d) && d==int(d) && d>0){
	  t.nrows=d;
	}
	continue;
      }
      if (smallmenu.selection == 3){
	double d=t.ncols;
	if (inputdouble((lang==1?"Nombre de colonnes?":"Colonnes?"),d) && d==int(d) && d>0){
	  t.ncols=d;
	}
	continue;
      }
      if (smallmenu.selection == 4){
	t.recompute=!t.recompute;
	continue;
      }
      if (smallmenu.selection==5){
	t.matrix_fill_cells=!t.matrix_fill_cells;
	continue;
      }
      if (smallmenu.selection == 6){
	t.movedown=!t.movedown;
	continue;
      }
      if (smallmenu.selection == smallmenu.numitems){
	change_undo(t);
	resizesheet(t);
	break;
      }	
    }      
  } // end endless while
}

void sheet_graph(tableur &t,GIAC_CONTEXT){
  vecteur v;
  sheet_pnt(t.m,v);
  gen g(v);
  check_do_graph(g,0,2,contextptr);
}

void sheet_cmdline(tableur &t,GIAC_CONTEXT);

int load_sheet(tableur & t,const char * filename,GIAC_CONTEXT){
  const char * s=read_file(filename);
  //dbg_printf("load tableur %s",s);
  if (s){
    gen g(s,contextptr);
    //dbg_printf("load tableur g=%s",g.print().c_str());
    g=eval(g,1,contextptr);
    //dbg_printf("evaled g=%s",g.print().c_str());
    if (ckmatrix(g,true)){
      t.filename=filename;
      t.m=*g._VECTptr;
      t.nrows=t.m.size();
      t.ncols=t.m.front()._VECTptr->size();
      t.cur_col=t.cur_row=0;
      t.sel_row_begin=t.cmd_row=-1;
      fix_sheet(t,contextptr);
      return 1;
    }
  }
  return 0;
}

int sheet_menu_menu(tableur & t,GIAC_CONTEXT){
  t.cmd_row=-1; t.cmd_pos=-1; t.sel_row_begin=-1;
  Menu smallmenu;
  smallmenu.numitems=14;
  MenuItem smallmenuitems[smallmenu.numitems];
  smallmenu.items=smallmenuitems;
  smallmenu.height=12;
  //smallmenu.width=24;
  smallmenu.scrollbar=1;
  smallmenu.scrollout=1;
#ifdef NUMWORKS
  smallmenu.title = (char*)(lang==1?"Back: annule menu tableur":"Back: cancel sheet menu");
#else
  smallmenu.title = (char*)(lang==1?"annul: quitte menu":"cancel: leave menu");
#endif
  smallmenuitems[0].text = (char *)(lang==1?"Sauvegarde tableur":"Save sheet");
  smallmenuitems[1].text = (char *)(lang==1?"Sauvegarder tableur comme":"Save sheet as");
  if (nspire_exam_mode==2) smallmenuitems[1].text=smallmenuitems[0].text = (char*)(lang==1?"Sauvegarde desactivee":"Saving disabled");
  smallmenuitems[2].text = (char*)(lang==1?"Charger":"Load");
  string cell=(lang==1?"Editer cellule ":"Edit cell ")+printcell(t.cur_row,t.cur_col);
  smallmenuitems[3].text = (char*)cell.c_str();
  smallmenuitems[4].text = (char*)(lang==1?"Voir graphique (2nd graph)":"View graph (2nd graph)");
#ifdef NUMWORKS
  smallmenuitems[5].text = (char*)(lang==1?"Copie vers le bas (shift 4)":"Copy down (shift 7)");
  smallmenuitems[6].text = (char*)(lang==1?"Copie vers droite (shift 4)":"Copy right (shift 7)");
#else
  smallmenuitems[5].text = (char*)(lang==1?"Copier vers le bas (2nd format)":"Copy down (2nd format)");
  smallmenuitems[6].text = (char*)(lang==1?"Copier vers la droite (2nd cakc)":"Copy right (2nd calc)");
#endif
  smallmenuitems[7].text = (char*)(lang==1?"Inserer une ligne":"Insert row");
  smallmenuitems[8].text = (char*)(lang==1?"Inserer une colonne":"Insert column");
  smallmenuitems[9].text = (char*)(lang==1?"Effacer ligne courante":"Remove current row");
  smallmenuitems[10].text = (char*)(lang==1?"Effacer colonne courante":"Remove current column");
  smallmenuitems[11].text = (char*)(lang==1?"Remplir le tableau de 0":"Fill sheet with 0");
  smallmenuitems[smallmenu.numitems-2].text = (char*) "Config";
  smallmenuitems[smallmenu.numitems-1].text = (char*) (lang==1?"Quitter tableur":"Leave sheet");
  while(1) {
    int sres = doMenu(&smallmenu);
    if (sres==MENU_RETURN_EXIT)
      return -1;
    if (sres == MENU_RETURN_SELECTION  || sres==KEY_CTRL_EXE) {
      if (smallmenu.selection == 1){
	// save
	save_sheet(t,contextptr);
	return -1;
      }
      if (smallmenu.selection == 2 ){
	// save
	char buf[270];
	if (get_filename(buf,".tab")){
	  t.filename=remove_path(giac::remove_extension(buf));
	  save_sheet(t,contextptr);
	  return -1;
	}
      }
      if (smallmenu.selection== 3 && !exam_mode) {
	char filename[128];
	if (fileBrowser(filename,"tab",(lang==1?"Fichiers tableurs":"Sheet files"))){
	  if (t.changed && do_confirm(lang==1?"Sauvegarder le tableur actuel?":"Save current sheet?"))
	    save_sheet(t,contextptr);
          if (!load_sheet(t,filename,contextptr))
            do_confirm(lang==1?"Erreur de lecture du fichier":"Error reading file");
        }
	return -1;
      } // end load
      if (smallmenu.selection==4){
        if (t.cmd_row<0 && t.sel_row_begin<0){
          char buf[1024];
          strcpy(buf,t.cmdline.substr(0,1024-1).c_str());
          if (textedit(buf)){
            t.cmdline=buf;
            t.cmd_row=t.cur_row; t.cmd_col=t.cur_col;
            sheet_cmdline(t,contextptr);
          }
        }
        return -1;
      }
      if (smallmenu.selection==5){
	sheet_graph(t,contextptr);
	return -1;
      }
      if (smallmenu.selection==6){
	t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	copy_down(t,contextptr);
	return -1;
      }
      if (smallmenu.selection==7){
	t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	copy_right(t,contextptr);
	return -1;
      }
      if (smallmenu.selection==8){
	t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	change_undo(t);
	t.m=matrice_insert(t.m,t.cur_row,t.cur_col,1,0,makevecteur(0,0,2),contextptr);
	t.nrows++;
	return -1;
      }
      if (smallmenu.selection==9){
	t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	change_undo(t);
	t.m=matrice_insert(t.m,t.cur_row,t.cur_col,0,1,makevecteur(0,0,2),contextptr);
	t.ncols++;
	return -1;
      }
      if (smallmenu.selection==10 && t.nrows>=2){
	t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	change_undo(t);
	t.m=matrice_erase(t.m,t.cur_row,t.cur_col,1,0,contextptr);
	--t.nrows;
	return -1;
      }
      if (smallmenu.selection==11 && t.ncols>=2){
	t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	change_undo(t);
	t.m=matrice_erase(t.m,t.cur_row,t.cur_col,0,1,contextptr);
	--t.ncols;
	return -1;
      }
      if (smallmenu.selection==12){
	t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	change_undo(t);
	gen g=vecteur(t.ncols);
	t.m=makefreematrice(vecteur(t.nrows,g));
	makespreadsheetmatrice(t.m,contextptr);
	return -1;
      }
      if (smallmenu.selection == smallmenu.numitems-1){
	sheet_menu_setup(t,contextptr);
	continue;
      }
      if (smallmenu.selection == smallmenu.numitems){
	return 0;
      }
    }
  } // end endless while
  return 1;
}

bool is_empty_cell(const gen & g){
  if (g.type==_VECT) return is_zero(g[0]);
  return is_zero(g);
}

void sheet_cmd(tableur & t,const char * ans){
  string s=ans; 
  if (t.sel_row_begin>=0){
    t.cmdline="";
    s="="+s+"matrix("+giac::print_INT_(giac::absint(t.sel_row_begin-t.cur_row)+1)+","+giac::print_INT_(giac::absint(t.sel_col_begin-t.cur_col)+1)+","+printsel(t.sel_row_begin,t.sel_col_begin,t.cur_row,t.cur_col)+")";
    if (t.cur_row<t.sel_row_begin)
      t.cur_row=t.sel_row_begin;
    t.sel_row_begin=-1;
    if (t.cur_col<t.sel_col_begin)
      t.cur_col=t.sel_col_begin;
    int i,j=t.cur_col;
    // find empty cell in next rows
    for (i=t.cur_row+1;i<t.nrows;++i){
      if (is_empty_cell(t.m[i][t.cur_col]))
	break;
    }
    if (i==t.nrows){
      // find an empty cell in next columns
      for (j=t.cur_col+1;j<t.ncols;++j){
	for (i=0;i<t.nrows;++i){
	  if (is_empty_cell(t.m[i][j]))
	    break;
	}
	if (i<t.nrows)
	  break;
      }
    }
    if (i<t.nrows && j<t.ncols){
      t.cur_row=i;
      t.cur_col=j;
    }
    else {
      do_confirm((lang==1?"Impossible de trouver une cellule libre":"Could not find an empty cell"));
      return;
    }
  }
  activate_cmdline(t);
  insert(t.cmdline,t.cmd_pos,s.c_str());
  t.cmd_pos += s.size();
  t.keytooltip=true;
}

void sheet_cmdline(tableur &t,GIAC_CONTEXT){
  gen g(t.cmdline,contextptr);
  change_undo(t);
  bool doit=true;
  bool tableseq=g.is_symb_of_sommet(at_tableseq);
#ifdef WITH_QUAD
  bool tablefunc=false;
#else
  bool tablefunc=g.is_symb_of_sommet(at_tablefunc);
#endif
  if (tableseq || t.matrix_fill_cells){
    giac::set_abort();
    gen g1=protecteval(g,1,contextptr);
    giac::clear_abort();
    if (g1.type==_VECT){
      doit=false;
      matrice & m=*g1._VECTptr;
      if (!ckmatrix(m)){
	m=vecteur(1,m);
	if (t.movedown)
	  m=mtran(m);
      }
      matrice clip=t.clip;
      makespreadsheetmatrice(m,contextptr);
      t.clip=m;
      paste(t,contextptr);
      t.clip=clip;
      if (tableseq && t.cur_row+4<t.nrows){
	t.cur_row += 4;
	copy_down(t,contextptr);
      }
      if (tablefunc && t.cur_row+3<t.nrows && t.cur_col+1<t.ncols){
	t.cur_row += 3;
	copy_down(t,contextptr);
	t.cur_col++;
	copy_down(t,contextptr);
      }
    }
  }
  if (doit) {
    if (t.cmd_row<t.m.size()){
      gen v=t.m[t.cmd_row];
      if (v.type==_VECT && t.cmd_col>=0 && t.cmd_col<v._VECTptr->size()){
	vecteur w=*v._VECTptr;
	g=spread_convert(g,t.cur_row,t.cur_col,contextptr);
	w[t.cmd_col]=makevecteur(g,g,0);
	t.m[t.cmd_row]=w;
	sheet_eval(t,contextptr,true);
      }
    }
  }
  t.cur_row=t.cmd_row;
  t.cur_col=t.cmd_col;
  t.cmd_row=-1;
  t.cmd_pos=-1;
  if (t.movedown){
    ++t.cur_row;
    if (t.cur_row>=t.nrows){
      t.cur_row=0;
      ++t.cur_col;
      if (t.cur_col>=t.ncols)
	t.cur_col=0;
    }
  }
  else {
    ++t.cur_col;
    if (t.cur_col>=t.ncols){
      t.cur_col=0;
      ++t.cur_row;
      if (t.cur_row>=t.nrows){
	t.cur_row=0;
      }
    }
  }
}

void sheet_help_insert(tableur & t,int exec,GIAC_CONTEXT){
  int back;
  string adds=help_insert(t.cmdline.substr(0,t.cmd_pos).c_str(),back,exec);
  if (back>=t.cmd_pos){
    t.cmdline=t.cmdline.substr(0,t.cmd_pos-back)+t.cmdline.substr(t.cmd_pos,t.cmdline.size()-t.cmd_pos);
    t.cmd_pos-=back;
  }
  if (!adds.empty())
    sheet_cmd(t,adds.c_str());
}

void sheet_clip(tableur & t){
  if (t.sel_row_begin<0){
    t.sel_row_begin=t.cur_row;
    t.sel_col_begin=t.cur_col;
  }
  else {
    int r=t.cur_row,R=t.sel_row_begin,c=t.cur_col,C=t.sel_col_begin;
    if (r>R)
      swapint(r,R);
    if (c>C)
      swapint(c,C);
    t.clip=matrice_extract(t.m,r,c,R-r+1,C-c+1);
    copy_clipboard(gen(extractmatricefromsheet(t.clip,true)).print(contextptr),true);
    t.sel_row_begin=-1;
  }
}
  
int sheet_stat1d(tableur & t){
  sheet_clip(t);
  if (t.sel_row_begin==-1){
    gen g(extractmatricefromsheet(t.clip,true));
    if (ckmatrix(g)){
      vecteur & v=*g._VECTptr;
      int l=v[0]._VECTptr->size();
      if (l!=1 && l!=2){
        statuslinemsg("select 1 or 2 columns!",COLOR_CYAN);
        return -1;
      }        
      string s;
      if (l==1){
        vecteur w;
        aplatir(*g._VECTptr,w,true);
        g=w;
        g=makesequence(_mean(g,contextptr),
                       _stddev(evalf(g,1,contextptr),contextptr),
                       _min(g,contextptr),
                       _quartile1(g,contextptr),
                       _median(g,contextptr),
                       _quartile3(g,contextptr),
                       _max(g,contextptr));
      }
      else {
        g=evalf(g,1,contextptr);
        g=makesequence(_covariance(g,contextptr),
                       _correlation(g,contextptr));
      }
      s=g.print(contextptr);
      print_msg12(l==1?"mean,stddev, min, q1, med, q3, max":"covariance,correlation",s.c_str());
      copy_clipboard(s,true);
      getkey(1);
      return 1;
    }
  }
  statuslinemsg("move to selection end",COLOR_CYAN);
  return 0;
}

giac::gen sheet(GIAC_CONTEXT){
  if (!sheetptr){
    sheetptr=new_tableur(contextptr);
    xcas::load_sheet(*xcas::sheetptr,"sessionT",contextptr);
  }
  tableur & t=*sheetptr;
  sheet_eval(t,contextptr,true);
  t.changed=false;
  bool status_freeze=false;
  t.keytooltip=false;
  bool full_redraw=true;
  for (;;){
    int R=t.cur_row,C=t.cur_col;
    if (t.cmd_row>=0){
      R=t.cmd_row;
      C=t.cmd_col;
    }
    printcell_current_row(contextptr)=R;
    printcell_current_col(contextptr)=C;
    if (!status_freeze)
      sheet_status(t,contextptr);
    sheet_display(t,contextptr,full_redraw);
    int key=getkey(1);
    while (key==KEY_CTRL_SHIFT || key==KEY_CTRL_ALPHA){
      statusflags();
      key=getkey(1);
    }
    full_redraw=false;
    if (key==KEY_SHUTDOWN || key==KEY_CTRL_QUIT){
      save_sheet_to(t,"session.tab",contextptr);
      return key;
    }
    if (t.keytooltip){
      t.keytooltip=false;
      if (key==KEY_CTRL_EXIT)
        continue;
      if (key==KEY_CTRL_RIGHT && t.cmd_pos==t.cmdline.size())
        key=KEY_CTRL_OK;
      if (key==KEY_CTRL_DOWN || key==KEY_CTRL_VARS)
        key=KEY_BOOK;
      if (key==KEY_CTRL_EXE || key==KEY_CTRL_OK || key==KEY_CHAR_ANS){
        sheet_help_insert(t,key,contextptr);
        continue;
      }
    }
    status_freeze=false;
    if (key==KEY_CTRL_SETUP){
      sheet_menu_setup(t,contextptr);
      continue;
    }
#if 1
    if (key==KEY_CHAR_STORE){
      char buf[]="=";
      if (t.cmd_row<0)
        t.cmdline="";
      sheet_cmd(t,buf);
      continue;
    }
    if (key==KEY_CHAR_ANS){
      char buf[]="$";
      if (t.cmd_row<0)
        t.cmdline="";
      sheet_cmd(t,buf);
      continue;
    }
#else
    if (key==KEY_CHAR_STORE && t.cmd_row<0){
      save_sheet(t,contextptr);
      continue;
    }
#endif
    if (key==KEY_CTRL_F5){
      if (sheet_menu_menu(t,contextptr)==0){
        save_sheet_to(t,"session.tab",contextptr);
	return 0;
      }
      full_redraw=true;
    }
    if (key==KEY_CTRL_EXIT){
      if (t.sel_row_begin>=0){
	t.sel_row_begin=-1;
	continue;
      }
      if (t.cmd_row>=0){
	bool b= t.cmd_row==t.cur_row && t.cmd_col==t.cur_col;
	t.cur_row=t.cmd_row;
	t.cur_col=t.cmd_col;
	if (b)
	  t.cmd_row=-1;
	continue;
      }
      if (!t.changed || do_confirm("Quit without saving?"))
	return 0;
    }
    switch (key){
    case KEY_CTRL_STATS:
      if (sheet_stat1d(t)<=0)
        status_freeze=true;
      continue;
    case KEY_CTRL_UNDO:
      std::swap(t.m,t.undo);
      sheet_eval(t,contextptr);
      continue;
    case KEY_CTRL_CLIP:
      sheet_clip(t);
      continue;
    case KEY_CTRL_PASTE:
      paste(t,contextptr);
      status_freeze=true;
      continue;
    case KEY_SELECT_RIGHT:
      if (t.sel_row_begin<0){
	t.sel_row_begin=t.cur_row;
	t.sel_col_begin=t.cur_col;
      }
    case KEY_CTRL_RIGHT:
      if (t.cmd_pos>=0 && t.cmd_row==t.cur_row && t.cmd_col==t.cur_col && t.sel_row_begin==-1){
	++t.cmd_pos;
	if (t.cmd_pos>t.cmdline.size())
	  t.cmd_pos=t.cmdline.size();
      }
      else {
	++t.cur_col;
	if (t.cur_col>=t.ncols)
	  t.cur_col=0;
      }
      continue;
    case KEY_SHIFT_RIGHT:
      if (t.cmd_pos>=0 && t.cmd_row==t.cur_row && t.cmd_col==t.cur_col && t.sel_row_begin==-1){
	t.cmd_pos=t.cmdline.size();
      }
      else 
	t.cur_col=t.ncols-1;
      break;
    case KEY_SELECT_LEFT:
      if (t.sel_row_begin<0){
	t.sel_row_begin=t.cur_row;
	t.sel_col_begin=t.cur_col;
      }
    case KEY_CTRL_LEFT:
      if (t.cmd_pos>=0 && t.cmd_row==t.cur_row && t.cmd_col==t.cur_col && t.sel_row_begin==-1){
	if (t.cmd_pos>0)
	  --t.cmd_pos;
      }
      else {
	--t.cur_col;
	if (t.cur_col<0)
	  t.cur_col=t.ncols-1;
      }
      continue;
    case KEY_SHIFT_LEFT:
      if (t.cmd_pos>=0 && t.cmd_row==t.cur_row && t.cmd_col==t.cur_col && t.sel_row_begin==-1){
	t.cmd_pos=0;
      }
      else {
	t.cur_col=0;
      }
      break;
    case KEY_SELECT_UP:
      if (t.sel_row_begin<0){
	t.sel_row_begin=t.cur_row;
	t.sel_col_begin=t.cur_col;
      }
    case KEY_CTRL_UP:
      --t.cur_row;
      if (t.cur_row<0)
	t.cur_row=t.nrows-1;
      continue;
    case KEY_SELECT_DOWN:
      if (t.sel_row_begin<0){
	t.sel_row_begin=t.cur_row;
	t.sel_col_begin=t.cur_col;
      }
    case KEY_CTRL_DOWN:
      ++t.cur_row;
      if (t.cur_row>=t.nrows)
	t.cur_row=0;
      continue;
    case KEY_CTRL_DEL:
      if (t.cmd_row>=0){
	if (t.cmd_pos>0){
	  t.cmdline.erase(t.cmdline.begin()+t.cmd_pos-1);
	  --t.cmd_pos;
	  t.keytooltip=true;
	}
      }
      else {
	t.cmdline="";
	t.cmd_row=t.cur_row;
	t.cmd_col=t.cur_col;
	t.cmd_pos=0;
      }
      continue;
    case KEY_CTRL_EXE:
#if 1
      if (t.cmd_row<0){
	sheet_eval(t,contextptr);
	continue;
      }
#else
      if (t.cmd_row<0){
	int r=t.sel_row_begin;
	if (r<0)
	  return extractmatricefromsheet(t.m);
	int R=t.cur_row,c=t.sel_col_begin,C=t.cur_col;
	if (r>R)
	  swapint(r,R);
	if (c>C)
	  swapint(c,C);
	return extractmatricefromsheet(matrice_extract(t.m,r,c,R-r+1,C-c+1));
      }
#endif
    case KEY_CTRL_OK:
      if (t.cmd_row>=0){
	string s;
	if (t.sel_row_begin>=0){
	  s=printsel(t.sel_row_begin,t.sel_col_begin,t.cur_row,t.cur_col);
	  t.cur_row=t.cmd_row;
	  t.cur_col=t.cmd_col;
	  t.sel_row_begin=-1;
	}
	if (t.cmd_row!=t.cur_row || t.cmd_col!=t.cur_col){
	  s=printcell(t.cur_row,t.cur_col);
	  t.cur_row=t.cmd_row;
	  t.cur_col=t.cmd_col;
	}
	if (s.empty())
	  sheet_cmdline(t,contextptr);
	else {
	  insert(t.cmdline,t.cmd_pos,s.c_str());
	  t.cmd_pos+=s.size();
	}
      } // if t.cmd_row>=0
      else {
	t.cmd_row=t.cur_row;
	t.cmd_col=t.cur_col;
	t.cmd_pos=t.cmdline.size();
      }
      continue;
    case KEY_CTRL_F7: // view
      {
	string value((*t.m[t.cur_row]._VECTptr)[t.cur_col][1].print(contextptr));
	char buf[1024];
	strcpy(buf,value.substr(0,1024-1).c_str());
	textedit(buf);
      }
      continue;
    case KEY_CTRL_F6: // view graph
      sheet_graph(t,contextptr);
      continue;
    case KEY_CTRL_F8: // copy down
      copy_down(t,contextptr);
      continue;
    case KEY_CTRL_SYMB: {
      char buf[512];
      if (!showCatalog(buf,0,0))
	continue;
      if (t.cmd_row<0)
        t.cmdline="";
      sheet_cmd(t,buf);
      full_redraw=true;
      continue;
    }
#ifndef NUMWORKS
    case KEY_CTRL_F9:
      copy_right(t,contextptr);
      continue;
    case KEY_CTRL_CATALOG:
#endif
    case KEY_BOOK: case '\t':
      {
	if (t.cmd_pos>=0){
	  sheet_help_insert(t,0,contextptr);
          full_redraw=true;
        }
        else {
          activate_cmdline(t);
          t.cmd_pos=t.cmdline.size();
        }
      }
      continue;
    } // end switch
    if ( (key >= KEY_CTRL_F1 && key <= KEY_CTRL_F6) ||
	  (key >= KEY_CTRL_F7 && key <= KEY_CTRL_F14) 
	 ){
#ifdef WITH_QUAD
      const char tmenu[]= "F1 stat1d\nstat1d\nsum(\nmean(\nstddev(\nmedian(\nhistogram(\nbarplot(\nboxwhisker(\nF2 stat2d\nscatterplot(\npolygonscatterplot(\nlinear_regression(\nlinear_regression_plot(\nlogarithmic_regression_plot(\nexponential_regression_plot(\nF3 seq\nrange(\nseq(\ntableseq(\nplotseq(\nrandvector(\nrandmatrix(\nF4 edt\n=\nselect\n$\nedit_cell\nundo\ncopy_down\ncopy_right\ninsert_row\ninsert_col\nF6 graph\nreserved\nF= poly\nproot(\npcoeff(\nquo(\nrem(\ngcd(\negcd(\nresultant(\nGF(\nF: arit\nF9 mod \nirem(\nifactor(\ngcd(\nisprime(\nnextprime(\npowmod(\niegcd(\nF8 list\nmakelist(\nrange(\nseq(\nlen(\nappend(\nranv(\nsort(\napply(\nF; plot\nplot(\nplotseq(\nplotlist(\nplotparam(\nplotpolar(\nplotfield(\nhistogram(\nbarplot(\nF7 real\nexact(\napprox(\nfloor(\nceil(\nround(\nsign(\nmax(\nmin(\nF< prog\n:\n&\n#\nhexprint(\nbinprint(\nf(x):=\ndebug(\npython(\nF> cplx\nabs(\narg(\nre(\nim(\nconj(\ncsolve(\ncfactor(\ncpartfrac(\nF= misc\n!\nrand(\nbinomial(\nnormald(\nexponentiald(\n\\\n % \n\n";
#else
      const char tmenu[]= "F1 stat1d\nstat1d\nsum(\nmean(\nstddev(\nmedian(\nhistogram(\nbarplot(\nboxwhisker(\nF2 stat2d\nscatterplot(\npolygonscatterplot(\nlinear_regression(\nlinear_regression_plot(\nlogarithmic_regression_plot(\nexponential_regression_plot(\nF3 seq\nrange(\nseq(\ntableseq(\nplotseq(\ntablefunc(\nrandvector(\nrandmatrix(\nF4 edt\n=\nselect\n$\nedit_cell\nundo\ncopy_down\ncopy_right\ninsert_row\ninsert_col\nF6 graph\nreserved\nF= poly\nproot(\npcoeff(\nquo(\nrem(\ngcd(\negcd(\nresultant(\nGF(\nF: arit\nF9 mod \nirem(\nifactor(\ngcd(\nisprime(\nnextprime(\npowmod(\niegcd(\nF8 list\nmakelist(\nrange(\nseq(\nlen(\nappend(\nranv(\nsort(\napply(\nF; plot\nplot(\nplotseq(\nplotlist(\nplotparam(\nplotpolar(\nplotfield(\nhistogram(\nbarplot(\nF7 real\nexact(\napprox(\nfloor(\nceil(\nround(\nsign(\nmax(\nmin(\nF< prog\n:\n&\n#\nhexprint(\nbinprint(\nf(x):=\ndebug(\npython(\nF> cplx\nabs(\narg(\nre(\nim(\nconj(\ncsolve(\ncfactor(\ncpartfrac(\nF= misc\n!\nrand(\nbinomial(\nnormald(\nexponentiald(\n\\\n % \n\n";
#endif
      const char * s=console_menu(key,(char *)tmenu,0);
      full_redraw=true;
      if (s && strlen(s)){
	if (strcmp(s,"select")==0){
          sheet_clip(t);
          continue;
        }
	if (strcmp(s,"stat1d")==0){
          if (sheet_stat1d(t)<=0)
            status_freeze=true;
          continue;
        }
	if (strcmp(s,"undo")==0){
	  t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	  std::swap(t.m,t.undo);
	  sheet_eval(t,contextptr);
	  continue;
	}
	if (strcmp(s,"copy_down")==0){
	  t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	  copy_down(t,contextptr);
	  continue;
	}
	if (strcmp(s,"copy_right")==0){
	  t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	  copy_right(t,contextptr);
	  continue;
	}
	if (strcmp(s,"insert_row")==0){
	  t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	  change_undo(t);
	  t.m=matrice_insert(t.m,t.cur_row,t.cur_col,1,0,makevecteur(0,0,2),contextptr);
	  t.nrows++;
	  continue;
	}
	if (strcmp(s,"insert_col")==0){
	  t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	  change_undo(t);
	  t.m=matrice_insert(t.m,t.cur_row,t.cur_col,0,1,makevecteur(0,0,2),contextptr);
	  t.ncols++;
	  continue;
	}
	if (strcmp(s,"erase_row")==0 && t.nrows>=2){
	  t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	  change_undo(t);
	  t.m=matrice_erase(t.m,t.cur_row,t.cur_col,1,0,contextptr);
	  --t.nrows;
	  continue;
	}
	if (strcmp(s,"erase_col")==0 && t.ncols>=2){
	  t.cmd_pos=t.cmd_row=t.sel_row_begin=-1;
	  change_undo(t);
	  t.m=matrice_erase(t.m,t.cur_row,t.cur_col,0,1,contextptr);
	  --t.ncols;
	  continue;
	}
	if (strcmp(s,"edit_cell")==0){
          activate_cmdline(t);
          t.cmd_pos=t.cmdline.size();
	  continue;
	}
	if (t.cmd_row<0)
	  t.cmdline="";
	sheet_cmd(t,s);
      }
      continue;
    }
    if (key==KEY_CHAR_CROCHETS || key==KEY_CHAR_ACCOLADES){
      if (t.cmd_row<0)
	t.cmdline="";
      activate_cmdline(t);
      t.cmdline.insert(t.cmdline.begin()+t.cmd_pos,key==KEY_CHAR_CROCHETS?'[':'{');
      ++t.cmd_pos;
      t.cmdline.insert(t.cmdline.begin()+t.cmd_pos,key==KEY_CHAR_CROCHETS?']':'}');
      continue;
    }
    if (key>=32 && key<128){
      if (t.cmd_row<0)
	t.cmdline="";
      activate_cmdline(t);
      t.cmdline.insert(t.cmdline.begin()+t.cmd_pos,char(key));
      ++t.cmd_pos;
      t.keytooltip=true;
      continue;
    }
    if (const char * ans=keytostring(key,0,false)){
      if (ans && strlen(ans)){
	if (t.cmd_row<0)
	  t.cmdline="";
	sheet_cmd(t,ans);
      }
      continue;
    }
    if (key==KEY_CTRL_AC && t.cmd_row>=0){
      if (t.cmdline=="")
	t.cmd_row=-1;
      t.cmdline="";
      t.cmd_pos=0;
      continue;
    }
    
  }
}

#endif // WITH_SHEET

#ifdef WITH_PLOT
  bool ispnt(const gen & g){
    if (g.is_symb_of_sommet(giac::at_pnt))
      return true;
    if (g.type!=_VECT || g._VECTptr->empty())
      return false;
    return ispnt(g._VECTptr->back());
  }

void displaygraph(const giac::gen & ge){
  // graph display
  //if (aborttimer > 0) { Timer_Stop(aborttimer); Timer_Deinstall(aborttimer);}
  xcas::Graph2d gr(ge);
  gr.show_axes=true;
  // initial setting for x and y
  if (ge.type==_VECT){
    const_iterateur it=ge._VECTptr->begin(),itend=ge._VECTptr->end();
    for (;it!=itend;++it){
      if (it->is_symb_of_sommet(at_equal)){
	const gen & f=it->_SYMBptr->feuille;
	gen & optname = f._VECTptr->front();
	gen & optvalue= f._VECTptr->back();
	if (optname.val==_AXES && optvalue.type==_INT_)
	  gr.show_axes=optvalue.val;
	if (optname.type==_INT_ && optname.subtype == _INT_PLOT && optname.val>=_GL_X && optname.val<=_GL_Z && optvalue.is_symb_of_sommet(at_interval)){
	  //*logptr(contextptr) << optname << " " << optvalue << endl;
	  gen optvf=evalf_double(optvalue._SYMBptr->feuille,1,contextptr);
	  if (optvf.type==_VECT && optvf._VECTptr->size()==2){
	    gen a=optvf._VECTptr->front();
	    gen b=optvf._VECTptr->back();
	    if (a.type==_DOUBLE_ && b.type==_DOUBLE_){
	      switch (optname.val){
	      case _GL_X:
		gr.window_xmin=a._DOUBLE_val;
		gr.window_xmax=b._DOUBLE_val;
		gr.update();
		break;
	      case _GL_Y:
		gr.window_ymin=a._DOUBLE_val;
		gr.window_ymax=b._DOUBLE_val;
		gr.update();
		break;
	      }
	    }
	  }
	}
      }
    }
  }
  gr.init_tracemode();
  if (gr.tracemode & 4)
    gr.orthonormalize();
  // UI
  bool fullredraw=true;
  for (;;){
    giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
    if (fullredraw || (gr.tracemode & 0xe) )
      gr.draw();
    fullredraw=true;
    giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
    int key;
    int keyflag = (unsigned char)Setup_GetEntry(0x14);
    bool alph=keyflag==4||keyflag==0x84||keyflag==8||keyflag==0x88;
    // store cursor position
    unsigned char undercursor[121];
    unsigned char * saveptr=undercursor;
    for (int j=gr.current_j-5;j<=gr.current_j+5;++j){
      unsigned char * lcdramptr=((unsigned char *)lcd_Ram)+(j+STATUS_AREA_PX)*LCD_WIDTH_PX+(gr.current_i-5);
      saveptr[0]=lcdramptr[0];
      saveptr[1]=lcdramptr[1];
      saveptr[2]=lcdramptr[2];
      saveptr[3]=lcdramptr[3];
      saveptr[4]=lcdramptr[4];
      saveptr[5]=lcdramptr[5];
      saveptr[6]=lcdramptr[6];
      saveptr[7]=lcdramptr[7];
      saveptr[8]=lcdramptr[8];
      saveptr[9]=lcdramptr[9];
      saveptr[10]=lcdramptr[10];
      saveptr += 11;
    }
    statuslinemsg("");
    gr.draw_decorations();
    display_time();
    ck_getkey(&key);
    while (key==KEY_CTRL_SHIFT || key==KEY_CTRL_ALPHA){
      statusflags();
      ck_getkey(&key);
    }
    // restore position under cursor
    saveptr=undercursor;
    for (int j=gr.current_j-5;j<=gr.current_j+5;++j){
      unsigned char * lcdramptr=((unsigned char *)lcd_Ram)+(j+STATUS_AREA_PX)*LCD_WIDTH_PX+(gr.current_i-5);
      lcdramptr[0]=saveptr[0];
      lcdramptr[1]=saveptr[1];
      lcdramptr[2]=saveptr[2];
      lcdramptr[3]=saveptr[3];
      lcdramptr[4]=saveptr[4];
      lcdramptr[5]=saveptr[5];
      lcdramptr[6]=saveptr[6];
      lcdramptr[7]=saveptr[7];
      lcdramptr[8]=saveptr[8];
      lcdramptr[9]=saveptr[9];
      lcdramptr[10]=saveptr[10];
      saveptr += 11;
    }    
    if (key==KEY_CTRL_QUIT)
      break;
    if (key==KEY_CTRL_F10)
      gr.table(gr.trace_t,gr.trace_x0,gr.trace_y0,gnuplot_tmin,gnuplot_tmax);
    if (key==KEY_CTRL_F2 || key==KEY_CTRL_F7){
      char menu_xmin[32],menu_xmax[32],menu_ymin[32],menu_ymax[32];
      ustl::string s;
      s="xmin "+print_DOUBLE_(gr.window_xmin,6);
      strcpy(menu_xmin,s.c_str());
      s="xmax "+print_DOUBLE_(gr.window_xmax,6);
      strcpy(menu_xmax,s.c_str());
      s="ymin "+print_DOUBLE_(gr.window_ymin,6);
      strcpy(menu_ymin,s.c_str());
      s="ymax "+print_DOUBLE_(gr.window_ymax,6);
      strcpy(menu_ymax,s.c_str());
      Menu smallmenu;
      smallmenu.numitems=15;
      MenuItem smallmenuitems[smallmenu.numitems];
      smallmenu.items=smallmenuitems;
      smallmenu.height=8;
      //smallmenu.title = "KhiCAS";
      smallmenuitems[0].text = (char *) (lang?"Etude graphe (xtt)":"Curve study (xtt)");
      smallmenuitems[1].text = (char *) menu_xmin;
      smallmenuitems[2].text = (char *) menu_xmax;
      smallmenuitems[3].text = (char *) menu_ymin;
      smallmenuitems[4].text = (char *) menu_ymax;
      smallmenuitems[5].text = (char*) "Orthonormalize /";
      smallmenuitems[6].text = (char*) "Autoscale *";
      smallmenuitems[7].text = (char *) ("Zoom in +");
      smallmenuitems[8].text = (char *) ("Zoom out -");
      smallmenuitems[9].text = (char *) ("Y-Zoom out (-)");
      smallmenuitems[10].text = (char*) ((lang==1)?"Voir axes":"Show axes");
      smallmenuitems[10].type = MENUITEM_CHECKBOX;
      smallmenuitems[10].value = gr.show_axes;
      smallmenuitems[11].text = (char*) ((lang==1)?"Voir tangent (F3)":"Show tangent (F3)");
      smallmenuitems[11].type = MENUITEM_CHECKBOX;
      smallmenuitems[11].value = (gr.tracemode & 2)!=0;
      smallmenuitems[12].text = (char*) ((lang==1)?"Voir normal (F4)":"Show normal (F4)");
      smallmenuitems[12].type = MENUITEM_CHECKBOX;
      smallmenuitems[12].value = (gr.tracemode & 4)!=0;
      smallmenuitems[13].text = (char*) ((lang==1)?"Voir cercle (F5)":"Show circle (F5)");
      smallmenuitems[13].type = MENUITEM_CHECKBOX;
      smallmenuitems[13].value = (gr.tracemode & 8)!=0;
      smallmenuitems[14].text = (char*)(lang?"Quitter":"Quit");
      int sres = doMenu(&smallmenu);
      if(sres == MENU_RETURN_SELECTION) {
	const char * ptr=0;
	ustl::string s1; double d;
	if (smallmenu.selection==1){
	  gr.curve_infos();
	  continue;
	}
	if (smallmenu.selection==2){
	  inputline(menu_xmin,lang?"Nouvelle valeur?":"New value?",s1,false); /* numeric value expected */
	  if (stringtodouble(s1,d)){
	    gr.window_xmin=d;
	    gr.update();
	  }
	}
	if (smallmenu.selection==3){
	  inputline(menu_xmax,lang?"Nouvelle valeur?":"New value?",s1,false); /* numeric value expected */
	  if (stringtodouble(s1,d)){
	    gr.window_xmax=d;
	    gr.update();
	  }
	}
	if (smallmenu.selection==4){
	  inputline(menu_ymin,lang?"Nouvelle valeur?":"New value?",s1,false); /* numeric value expected */
	  if (stringtodouble(s1,d)){
	    gr.window_ymin=d;
	    gr.update();
	  }
	}
	if (smallmenu.selection==5){
	  inputline(menu_ymax,lang?"Nouvelle valeur?":"New value?",s1,false); /* numeric value expected */
	  if (stringtodouble(s1,d)){
	    gr.window_ymax=d;
	    gr.update();
	  }
	}
	if (smallmenu.selection==6)
	  gr.orthonormalize();
	if (smallmenu.selection==7)
	  gr.autoscale();	
	if (smallmenu.selection==8)
	  gr.zoom(0.7);	
	if (smallmenu.selection==9)
	  gr.zoom(1/0.7);	
	if (smallmenu.selection==10)
	  gr.zoomy(1/0.7);
	if (smallmenu.selection==11)
	  gr.show_axes=!gr.show_axes;	
	if (smallmenu.selection==12){
	  if (gr.tracemode & 2)
	    gr.tracemode &= ~2;
	  else
	    gr.tracemode |= 2;
	  gr.tracemode_set();
	}
	if (smallmenu.selection==13){
	  if (gr.tracemode & 4)
	    gr.tracemode &= ~4;
	  else {
	    gr.tracemode |= 4;
	    gr.orthonormalize();
	  }
	  gr.tracemode_set();
	}
	if (smallmenu.selection==14){
	  if (gr.tracemode & 8)
	    gr.tracemode &= ~8;
	  else {
	    gr.tracemode |= 8;
	    gr.orthonormalize();
	  }
	  gr.tracemode_set();
	}
	if (smallmenu.selection==15)
	  break;
      }
    }
    if (key==KEY_CTRL_EXIT || key==KEY_CTRL_AC || key==KEY_CTRL_MENU)
      break;
    if (key==KEY_CTRL_F1){
      gr.tracemode_set(-1); // object info
      continue;
    }
    if (key==KEY_CTRL_F3){
      const char *
        tab[]={
        lang==1?"Zoom in [+]":"Zoom in [+]",  // 0
        lang==1?"Zoom out [-]":"Zoom out [-]",  
        lang==1?"Autoscale [*]":"Autoscale [*]",
        lang==1?"Orthonormalise [/]":"Orthonormalize [/]",
        0};
      const int s=sizeof(tab)/sizeof(char *);
      int choix=select_item(tab,lang==1?"Affichage":"Display",true);
      if (choix<0 || choix>s)
        continue;
      if (choix==0)
        gr.zoom(0.7);
      else if (choix==1)
        gr.zoom(1/0.7);
      else if (choix==2)
        gr.autoscale();
      else if (choix==3)
        gr.orthonormalize();
    }
    if (key==KEY_CTRL_F4){
      const char *
        tab[]={
        lang==1?"Vitesse, pente":"Speed, slope",  // 0
        lang==1?"Vecteur normal":"Normal",  
        lang==1?"Cercle osculateur":"Osculating circle",
        lang==1?"Tout/rien":"All/nothing",
        0};
      const int s=sizeof(tab)/sizeof(char *);
      int choix=select_item(tab,lang==1?"Trace":"Trace",true);
      if (choix<0 || choix>s)
        continue;
      if (choix==0){
        if (gr.tracemode & 2)
          gr.tracemode &= ~2;
        else
          gr.tracemode |= 2;
        gr.tracemode_set();
        continue;
      }
      if (choix==1){
        if (gr.tracemode & 4)
          gr.tracemode &= ~4;
        else
          gr.tracemode |= 4;
        gr.tracemode_set();
        continue;
      }
      if (choix==2){
        if (gr.tracemode & 8)
          gr.tracemode &= ~8;
        else {
          gr.tracemode |= 8;
          gr.orthonormalize();
        }
        gr.tracemode_set();
        continue;
      }
      if (choix==3){
        if (gr.tracemode & 0xe)
          gr.tracemode=1;
        else {
          gr.tracemode=0xf;
          gr.orthonormalize();
        }
        gr.tracemode_set();          
      }
    }
    if (key==KEY_CTRL_F5){
      const char *
        tab[]={
        lang==1?"Axes":"Axes",   // 0
        lang==1?"A droite":"Right", 
        lang==1?"A gauche":"Left",  
        lang==1?"En haut":"Up",
        lang==1?"En bas":"Down",
        0};
      const int s=sizeof(tab)/sizeof(char *);
      int choix=select_item(tab,lang==1?"Trace":"Trace",true);
      if (choix<0 || choix>s)
        continue;
      if (choix==0){
        gr.show_axes=!gr.show_axes;
        continue;
      }
      if (choix==1){
        gr.right((gr.window_xmax-gr.window_xmin)/5);
        continue;
      }
      if (choix==2){
        gr.left((gr.window_xmax-gr.window_xmin)/5);
        continue;
      }
      if (choix==3){
        gr.up((gr.window_ymax-gr.window_ymin)/5);
        continue;
      }
      if (choix==4){
        gr.down((gr.window_ymax-gr.window_ymin)/5);
        continue;
      }
    }
    if (key==KEY_CTRL_XTT || key=='\t'){
      gr.curve_infos();
      continue;
    }
    if (key==KEY_CTRL_UP){
      if (gr.tracemode && !alph){
	--gr.tracemode_n;
	gr.tracemode_set();
        fullredraw=false;
	continue;
      }
      gr.up((gr.window_ymax-gr.window_ymin)/5);
    }
    if (key==KEY_CTRL_PAGEUP) { gr.up((gr.window_ymax-gr.window_ymin)/2); }
    if (key==KEY_CTRL_DOWN) {
      if (gr.tracemode && !alph){
	++gr.tracemode_n;
	gr.tracemode_set();
        fullredraw=false;
	continue;
      }
      gr.down((gr.window_ymax-gr.window_ymin)/5);
    }
    if (key==KEY_CTRL_PAGEDOWN) { gr.down((gr.window_ymax-gr.window_ymin)/2);}
    if (key==KEY_CTRL_LEFT) {
      if (gr.tracemode && !alph){
	if (gr.tracemode_i!=int(gr.tracemode_i))
	  gr.tracemode_i=std::floor(gr.tracemode_i);
	else
	  --gr.tracemode_i;
	gr.tracemode_set();
        fullredraw=false;
	continue;
      }
      gr.left((gr.window_xmax-gr.window_xmin)/5);
    }
    if (key==KEY_CTRL_RIGHT) {
      if (gr.tracemode && !alph){
	if (int(gr.tracemode_i)!=gr.tracemode_i)
	  gr.tracemode_i=std::ceil(gr.tracemode_i);
	else
	  ++gr.tracemode_i;
	gr.tracemode_set();
        fullredraw=false;
	continue;
      }
      gr.right((gr.window_xmax-gr.window_xmin)/5);
    }
    if (key==KEY_CHAR_PLUS) { gr.zoom(0.7);}
    if (key==KEY_CHAR_MINUS){ gr.zoom(1/0.7); }
    if (key==KEY_CHAR_PMINUS){ gr.zoomy(1/0.7); }
    if (key==KEY_CHAR_MULT){ gr.autoscale(); }
    if (key==KEY_CHAR_DIV) { gr.orthonormalize(); }
    if (key==KEY_CTRL_VARS || key==KEY_CTRL_OPTN) {gr.show_axes=!gr.show_axes;}
  }
}  
#endif  

#ifdef WITH_EQW
  unsigned max_prettyprint_equation=512;

  void check_do_graph(giac::gen & ge,const giac::gen & gs,int do_logo_graph_eqw,GIAC_CONTEXT) {
    if (ge.type==giac::_SYMB || (ge.type==giac::_VECT && !ge._VECTptr->empty() && !is_numericv(*ge._VECTptr)) ){
#if 0
      if (islogo(ge)){
        if (do_logo_graph_eqw & 4)
          displaylogo();
        return;
      }
#endif
#ifdef WITH_PLOT
      if (ispnt(ge)){
        if (do_logo_graph_eqw & 2){
          xcas::displaygraph(ge);
        }        
        // aborttimer = Timer_Install(0, check_execution_abort, 100); if (aborttimer > 0) { Timer_Start(aborttimer); }
        return ;
      }
#endif
      if ( do_logo_graph_eqw % 2 ==0)
        return;
      if (taille(ge,xcas::max_prettyprint_equation)>=xcas::max_prettyprint_equation || ge.is_symb_of_sommet(at_program))
        return ; // sizeof(eqwdata)=44
      gen tmp=eqw(ge,false);
      if (!is_undef(tmp) && tmp!=ge){
        //dConsolePutChar(147);
        Console_Output(ge.print(contextptr).c_str());
        Console_NewLine(LINE_TYPE_INPUT, 1);
        ge=tmp;
      }
    }
  }

  bool eqws(char * s,bool eval){
    // s buffer must be at least GEN_PRINT_BUFSIZE char
    gen g,ge;
    int dconsole_save=dconsole_mode;
    int ss=strlen(s);
    for (int i=0;i<ss;++i){
      if (s[i]==char(0x9c))
        s[i]='\n';
    }
    if (ss>=2 && (s[0]=='#' || s[0]=='"' ||
                  (s[0]=='/' && (s[1]=='/' || s[1]=='*'))
                  ))
      return textedit(s);
    dconsole_mode=0;
    if (s[0]==0)
      ge=0;
    else
      ge=gen(s,contextptr);
    if (eval)
      ge=ge.eval(1,contextptr);
    dconsole_mode=dconsole_save;
    if (is_undef(ge))
      return textedit(s);
    if (ge.type==giac::_SYMB || (ge.type==giac::_VECT && !ge._VECTptr->empty() && !is_numericv(*ge._VECTptr)) ){
#if 0
      if (islogo(ge)){
        displaylogo();
        return false;
      }
#endif
#ifdef WITH_PLOT
      if (ispnt(ge)){
        xcas::displaygraph(ge);
        // aborttimer = Timer_Install(0, check_execution_abort, 100); if (aborttimer > 0) { Timer_Start(aborttimer); }
        return false;
      }
#endif
      if (ge.is_symb_of_sommet(at_program))
        return textedit(s);
      if (taille(ge,xcas::max_prettyprint_equation)>=xcas::max_prettyprint_equation)
        return false; // sizeof(eqwdata)=48
    }
    gen tmp=eqw(ge,true);
    if (is_undef(tmp) || tmp==ge || taille(ge,256)>=256)
      return false;
    string S(tmp.print(contextptr));
    if (S.size()>=GEN_PRINT_BUFSIZE)
      return false;
    strcpy(s,S.c_str());
    return true;
  }

  giac::gen eqw(const giac::gen & ge,bool editable){
    bool edited=false;
    freeze=giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
#ifdef CURSOR
    Cursor_SetFlashOff();
#endif
    giac::gen geq(_copy(ge,contextptr));
    // if (ge.type!=giac::_DOUBLE_ && giac::has_evalf(ge,geq,1,contextptr)) geq=giac::symb_equal(ge,geq);
    int line=-1,col=-1,nlines=0,ncols=0,listormat=0;
    xcas::Equation eq(0,0,geq);
    giac::eqwdata eqdata=xcas::Equation_total_size(eq.data);
    if (eqdata.dx>1.5*LCD_WIDTH_PX || eqdata.dy>.7*LCD_HEIGHT_PX){
      if (eqdata.dx>2.25*LCD_WIDTH_PX || eqdata.dy>2*LCD_HEIGHT_PX)
        eq.attr=giac::attributs(12,COLOR_WHITE,COLOR_BLACK);
      else
        eq.attr=giac::attributs(16,COLOR_WHITE,COLOR_BLACK);
      eq.data=0; // clear memory
      eq.data=xcas::Equation_compute_size(geq,eq.attr,LCD_WIDTH_PX,contextptr);
      eqdata=xcas::Equation_total_size(eq.data);
    }
    int dx=(eqdata.dx-LCD_WIDTH_PX)/2,dy=LCD_HEIGHT_PX-2*STATUS_AREA_PX+eqdata.y;
    if (geq.type==_VECT){
      nlines=geq._VECTptr->size();
      if (eqdata.dx>=LCD_WIDTH_PX)
        dx=-20; // line=nlines/2;
      //else
      if (geq.subtype!=_SEQ__VECT){
        line=0;
        listormat=1;
        if (ckmatrix(geq)){
          ncols=geq._VECTptr->front()._VECTptr->size();
          bool big=eqdata.dy>=LCD_HEIGHT_PX-STATUS_AREA_PX;
          if (big)
            dy=eqdata.y+eqdata.dy+32;
          col=0;
          line=giac::giacmin(4,nlines/2);
          listormat=2;
        }
      }
    }
    if (!listormat){
      xcas::Equation_select(eq.data,true);
      xcas::eqw_select_down(eq.data);
    }
    //cout << eq.data << endl;
    int firstrun=2;
    for (;;){
#if 1
      if (firstrun==2){
        statuslinemsg(lang?"EXE: quitte, resultat dans last":"EXE: quit, result stored in last");
        //EnableStatusArea(2);
        //DisplayStatusArea();
        firstrun=1;
      }
      else
        ; // set_xcas_status();
#else
      statuslinemsg("+-: zoom, pad: move, EXIT: quit");
      //EnableStatusArea(2);
      //DisplayStatusArea();
#endif
      gen value;
      if (listormat) // select line l, col c
        xcas::eqw_select(eq.data,line,col,true,value);
#define EQW_TAILLE 54
      if (eqdata.dx>LCD_WIDTH_PX){
        if (dx<-EQW_TAILLE)
          dx=-EQW_TAILLE;
        if (dx>eqdata.dx-LCD_WIDTH_PX+EQW_TAILLE)
          dx=eqdata.dx-LCD_WIDTH_PX+EQW_TAILLE;
      }
      if (eqdata.dy>LCD_HEIGHT_PX-EQW_TAILLE){
        if (dy-eqdata.y<LCD_HEIGHT_PX-EQW_TAILLE)
          dy=eqdata.y+LCD_HEIGHT_PX-EQW_TAILLE;
        if (dy-eqdata.y>eqdata.dy+EQW_TAILLE)
          dy=eqdata.y+eqdata.dy+EQW_TAILLE;
      }
      drawRectangle(0, STATUS_AREA_PX, LCD_WIDTH_PX, LCD_HEIGHT_PX-STATUS_AREA_PX,COLOR_WHITE);
      // Bdisp_AllClr_VRAM();
      int save_clip_ymin=clip_ymin;
      clip_ymin=STATUS_AREA_PX;
      xcas::display(eq,dx,dy);
      // PrintMini(0,58,menu.c_str(),4);
      //draw_menu(2);
      clip_ymin=save_clip_ymin;
      int keyflag = GetSetupSetting( (unsigned int)0x14);
      if (firstrun){ // workaround for e.g. 1+x/2 partly not displayed
        firstrun=0;
        continue;
      }
      int key;
      keyflag = (Char)Setup_GetEntry(0x14);
      bool alph=keyflag==4||keyflag==0x84||keyflag==8||keyflag==0x88;
      Console_FMenu_Init();
      string menu(" "),shiftmenu=menu,alphamenu; int menucolorbg=12345;
      get_current_console_menu(menu,shiftmenu,alphamenu,menucolorbg,2);
      //dbg_printf("keyflag=%i menu=%s %s %s\n",keyflag,menu.c_str(),shiftmenu.c_str(),alphamenu.c_str());
      if (keyflag==1) menu=shiftmenu;
      if (keyflag & 0xc) menu=alphamenu;
      Printmini(0,C58,menu.c_str(),MINI_REV);
      // status, clock, 
      set_xcas_status();
      ck_getkey((int *)&key);
      //cout << key << '\n';
      if (key==KEY_CTRL_SD){
        // FIXME khicas_addins_menu(contextptr);
        continue;
      }
      if (key==KEY_CTRL_INS){
        key=chartab();
        if (key<0)
          continue;
      }
      if (key==KEY_CTRL_F5)
        key=KEY_CTRL_CLIP;
      if (key==KEY_CTRL_UNDO){
        giac::swapgen(eq.undodata,eq.data);
        if (listormat){
          xcas::do_select(eq.data,true,value);
          if (value.type==_EQW){
            gen g=eval(value._EQWptr->g,1,contextptr);
            if (g.type==_VECT){
              const vecteur & v=*g._VECTptr;
              nlines=v.size();
              if (line >= nlines)
                line=nlines-1;
              if (col!=-1 &&v.front().type==_VECT){
                ncols=v.front()._VECTptr->size();
                if (col>=ncols)
                  col=ncols-1;
              }
              xcas::do_select(eq.data,false,value);
              xcas::eqw_select(eq.data,line,col,true,value);
            }
          }
        }
        continue;
      }
      if (key=='=' )
        continue;
      int redo=0;
      if (listormat){
        if (key==KEY_CHAR_COMMA || key==KEY_CTRL_DEL ){
          xcas::do_select(eq.data,true,value);
          if (value.type==_EQW){
            gen g=eval(value._EQWptr->g,1,contextptr);
            if (g.type==_VECT){
              edited=true; eq.undodata=xcas::Equation_copy(eq.data);
              vecteur v=*g._VECTptr;
              if (key==KEY_CHAR_COMMA){
                if (col==-1 || (line>0 && line==nlines-1)){
                  v.insert(v.begin()+line+1,0*v.front());
                  ++line; ++nlines;
                }
                else {
                  v=mtran(v);
                  v.insert(v.begin()+col+1,0*v.front());
                  v=mtran(v);
                  ++col; ++ncols;
                }
              }
              else {
                if (col==-1 || (nlines>=3 && line==nlines-1)){
                  if (nlines>=(col==-1?2:3)){
                    v.erase(v.begin()+line,v.begin()+line+1);
                    if (line) --line;
                    --nlines;
                  }
                }
                else {
                  if (ncols>=2){
                    v=mtran(v);
                    v.erase(v.begin()+col,v.begin()+col+1);
                    v=mtran(v);
                    if (col) --col; --ncols;
                  }
                }
              }
              geq=gen(v,g.subtype);
              key=0; redo=1;
              // continue;
            }
          }
        }
      }
      bool ins=key==KEY_CHAR_STORE  || key==KEY_CHAR_RPAR || key==KEY_CHAR_LPAR || key==KEY_CHAR_COMMA || key==KEY_CTRL_PASTE;
      int xleft,ytop,xright,ybottom,gselpos; gen * gsel=0,*gselparent=0;
      ustl::string varname;
      if (key==KEY_CTRL_CLIP){
        xcas::Equation_adjust_xy(eq.data,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos,0);
        if (gsel==0)
          gsel=&eq.data;
        // cout << "var " << g << " " << eq.data << endl;
        if (xcas::do_select(*gsel,true,value) && value.type==_EQW){
          //cout << g << ":=" << value._EQWptr->g << endl;
          copy_clipboard(value._EQWptr->g.print(contextptr),true);
          continue;
        }
      }
      if (key==KEY_CHAR_STORE){
        int keyflag = GetSetupSetting( (unsigned int)0x14);
        if (keyflag==0)
          handle_f5();
        if (inputline(lang?"Stocker selection dans":"Save selection in",lang?"Nom de variable: ":"Variable name: ",varname,false) && !varname.empty() && isalpha(varname[0])){
          giac::gen g(varname,contextptr);
          giac::gen ge(eval(g,1,contextptr));
          if (g.type!=_IDNT){
            invalid_varname();
            continue;
          }
          if (ge==g || confirm_overwrite()){
            vector<int> goto_sel;
            xcas::Equation_adjust_xy(eq.data,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos,&goto_sel);
            if (gsel==0)
              gsel=&eq.data;
            // cout << "var " << g << " " << eq.data << endl;
            if (xcas::do_select(*gsel,true,value) && value.type==_EQW){
              //cout << g << ":=" << value._EQWptr->g << endl;
              giac::gen gg(value._EQWptr->g);
              if (gg.is_symb_of_sommet(at_makevector))
                gg=giac::eval(gg,1,contextptr);
              giac::sto(gg,g,contextptr);
            }
          }
        }
        continue;
      }
      if (key==KEY_CTRL_DEL){
        vector<int> goto_sel;
        if (xcas::Equation_adjust_xy(eq.data,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos,&goto_sel) && gsel && xcas::do_select(*gsel,true,value) && value.type==_EQW){
          value=value._EQWptr->g;
          if (value.type==_SYMB){
            gen tmp=value._SYMBptr->feuille;
            if (tmp.type!=_VECT || tmp.subtype!=_SEQ__VECT){
              xcas::replace_selection(eq,tmp,gsel,&goto_sel);
              continue;
            }
          }
          if (!goto_sel.empty() && gselparent && gselparent->type==_VECT && !gselparent->_VECTptr->empty()){
            vecteur & v=*gselparent->_VECTptr;
            if (v.back().type==_EQW){
              gen opg=v.back()._EQWptr->g;
              if (opg.type==_FUNC){
                int i=0;
                for (;i<v.size()-1;++i){
                  if (&v[i]==gsel)
                    break;
                }
                if (i<v.size()-1){
                  if (v.size()==5 && (opg==at_integrate || opg==at_sum) && i>=2)
                    v.erase(v.begin()+2,v.begin()+4);
                  else
                    v.erase(v.begin()+i);
                  xcas::do_select(*gselparent,true,value);
                  if (value.type==_EQW){
                    value=value._EQWptr->g;
                    // cout << goto_sel << " " << value << endl; continue;
                    if (v.size()==2 && (opg==at_plus || opg==at_prod || opg==at_pow))
                      value=eval(value,1,contextptr);
                    goto_sel.erase(goto_sel.begin());
                    xcas::replace_selection(eq,value,gselparent,&goto_sel);
                    continue;
                  }
                }
              }
            }
          }
        }
      }
      if (key==KEY_CTRL_F8 || key==KEY_CTRL_F13){
        keyflag=1;
        key=KEY_CTRL_F3;
      }
      if (key==KEY_CTRL_F6){
        key=KEY_CTRL_F4;
        keyflag=1;
      }
      if (key==KEY_CTRL_F3){
        if (keyflag==1){
          xcas::do_select(eq.data,true,value);
          if (value.type==_EQW)
            geq=value._EQWptr->g;
          if (eq.attr.fontsize<=14)
            eq.attr.fontsize=18;
          else {
            eq.attr.fontsize=12;
          }
          redo=1;
        }
        else {
          if (alph){
            xcas::do_select(eq.data,true,value);
            if (value.type==_EQW)
              geq=value._EQWptr->g;
            if (eq.attr.fontsize<=14)
              eq.attr.fontsize=18;
            else
              eq.attr.fontsize=12;
            redo=1;
          }
          else {
            // Edit
            edited=true;
            ins=true;
          }
        }
      }
      if (key==KEY_CHAR_IMGNRY)
        key='i';
      const char keybuf[2]={(key==KEY_CHAR_PMINUS?'-':char(key)),0};
      key=translate_fkey(key);
      const char * adds=(key==KEY_CHAR_PMINUS || key==KEY_CTRL_F3 ||
                         key==KEY_CTRL_F4 ||
                         (key>=KEY_CTRL_F7 && key<=KEY_CTRL_F20) ||
                         (key==char(key) && (giac::isalphanum(key)|| key=='.' ))
                         )?keybuf:keytostring(key,keyflag,false);
      if ( key==KEY_CTRL_F2 || key==KEY_CTRL_F1 
           || (key>=KEY_CTRL_F7 && key<=KEY_CTRL_F20)
           ){
        adds=console_menu(key,1);//alph?"simplify":(keyflag==1?"factor":"partfrac");
        if (!adds) continue;
        // workaround for infinitiy
        if (strlen(adds)>=2 && adds[0]=='o' && adds[1]=='o')
          key=KEY_CTRL_F5;      
      }
      if (key==KEY_CTRL_F4){
        adds=alph?"regroup":(keyflag==1?"evalf":"eval");
      }
      if (key==KEY_CTRL_F5)
        adds="oo";
      if (key==KEY_CHAR_MINUS)
        adds="-";
      if (key==KEY_CHAR_EQUAL)
        adds="=";
      if (key==KEY_CHAR_RECIP)
        adds="inv";
      if (key==KEY_CHAR_SQUARE)
        adds="sq";
      if (key==KEY_CHAR_POWROOT)
        adds="surd";
      if (key==KEY_CHAR_CUBEROOT)
        adds="surd";
      int addssize=adds?strlen(adds):0;
      // cout << addssize << " " << adds << endl;
      if (key==KEY_CTRL_EXE){
        if (xcas::do_select(eq.data,true,value) && value.type==_EQW){
          //cout << "ok " << value._EQWptr->g << endl;
          //DefineStatusMessage((char*)lang?"resultat stocke dans last":"result stored in last", 1, 0, 0);
          //DisplayStatusArea();
          giac::sto(value._EQWptr->g,giac::gen("last",contextptr),contextptr);
          return value._EQWptr->g;
        }
        //cout << "no " << eq.data << endl; if (value.type==_EQW) cout << value._EQWptr->g << endl ;
        return geq;
      }
      if ( key!=KEY_CHAR_MINUS && key!=KEY_CHAR_EQUAL &&
           (ins || key==KEY_CHAR_PI || key==KEY_CTRL_F5 || (addssize==1 && (giac::isalphanum(adds[0])|| adds[0]=='.' || adds[0]=='-') ) )
           ){
        edited=true;
        if (line>=0 && xcas::eqw_select(eq.data,line,col,true,value)){
          ustl::string s;
          if (ins){
            if (key==KEY_CTRL_PASTE)
              s=paste_clipboard();
            else {
              if (value.type==_EQW){
                s=value._EQWptr->g.print(contextptr);
              }
              else
                s=value.print(contextptr);
            }
          }
          else
            s = adds;
          ustl::string msg("Line ");
          msg += giac::print_INT_(line+1);
          msg += " Col ";
          msg += giac::print_INT_(col+1);
          if (inputline(msg.c_str(),0,s,false)==KEY_CTRL_EXE){
            value=gen(s,contextptr);
            if (col<0)
              (*geq._VECTptr)[line]=value;
            else
              (*((*geq._VECTptr)[line]._VECTptr))[col]=value;
            redo=2;
            key=KEY_SHIFT_RIGHT;
          }
          else
            continue;
        }
        else {
          vector<int> goto_sel;
          if (xcas::Equation_adjust_xy(eq.data,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos,&goto_sel) && gsel && xcas::do_select(*gsel,true,value) && value.type==_EQW){
            ustl::string s;
            if (ins){
              if (key==KEY_CTRL_PASTE)
                s=paste_clipboard();
              else {
                s = value._EQWptr->g.print(contextptr);
                if (key==KEY_CHAR_COMMA)
                  s += ',';
              }
            }
            else
              s = adds;
            if (inputline(value._EQWptr->g.print(contextptr).c_str(),0,s,false)==KEY_CTRL_EXE){
              value=gen(s,contextptr);
              //cout << value << " goto " << goto_sel << endl;
              xcas::replace_selection(eq,value,gsel,&goto_sel);
              firstrun=-1; // workaround, force 2 times display
            }
            continue;
          }
        }
      }
      if (redo){
        eq.data=0; // clear memory
        eq.data=xcas::Equation_compute_size(geq,eq.attr,LCD_WIDTH_PX,contextptr);
        eqdata=xcas::Equation_total_size(eq.data);
        if (redo==1){
          dx=(eqdata.dx-LCD_WIDTH_PX)/2;
          dy=LCD_HEIGHT_PX-2*STATUS_AREA_PX+eqdata.y;
          if (listormat) // select line l, col c
            xcas::eqw_select(eq.data,line,col,true,value);
          else {
            xcas::Equation_select(eq.data,true);
            xcas::eqw_select_down(eq.data);
          }
          continue;
        }
      }
      if (key==KEY_CTRL_EXIT || key==KEY_CTRL_AC){
        if (!edited)
          return geq;
        if (confirm(lang?"Vraiment abandonner?":"Really leave",lang?"F1: retour editeur,  F5: confirmer":"F1: back to editor,  F5: confirm")==KEY_CTRL_F5)
          return undef;
      }
      bool doit=eqdata.dx>=LCD_WIDTH_PX;
      int delta=0;
      if (listormat){
        if (key==KEY_CTRL_LEFT  || (!doit && key==KEY_SHIFT_LEFT)){
          if (line>=0 && xcas::eqw_select(eq.data,line,col,false,value)){
            if (col>=0){
              --col;
              if (col<0){
                col=ncols-1;
                if (line>0)
                  --line;
              }
            }
            else {
              if (line>0)
                --line;
            }
            xcas::eqw_select(eq.data,line,col,true,value);
            if (doit) dx -= value._EQWptr->dx;
          }
          continue;
        }
        if (key==KEY_SHIFT_LEFT){
          dx -= 20;
          continue;
        }
        if (key==KEY_CTRL_RIGHT  || (!doit && key==KEY_SHIFT_RIGHT)) {
          if (line>=0 && xcas::eqw_select(eq.data,line,col,false,value)){
            if (doit)
              dx += value._EQWptr->dx;
            if (col>=0){
              ++col;
              if (col==ncols){
                col=0;
                if (line<nlines-1)
                  ++line;
              }
            } else {
              if (line<nlines-1)
                ++line;
            }
            xcas::eqw_select(eq.data,line,col,true,value);
          }
          continue;
        }
        if (key==KEY_SHIFT_RIGHT ){
          dx += 20;
          continue;
        }
        doit=eqdata.dy>=LCD_HEIGHT_PX-3*STATUS_AREA_PX;
        if (key==KEY_CTRL_UP || (!doit && key==KEY_CTRL_PAGEUP)){
          if (line>0 && col>=0 && xcas::eqw_select(eq.data,line,col,false,value)){
            --line;
            xcas::eqw_select(eq.data,line,col,true,value);
            if (doit)
              dy += value._EQWptr->dy+eq.attr.fontsize/2;
          }
          else
            dy += eq.attr.fontsize/2;
          continue;
        }
        if (key==KEY_CTRL_PAGEUP ){
          dy += 20;
          continue;
        }
        if (key==KEY_CTRL_DOWN  || (!doit && key==KEY_CTRL_PAGEDOWN)){
          if (line<nlines-1 && col>=0 && xcas::eqw_select(eq.data,line,col,false,value)){
            if (doit)
              dy -= value._EQWptr->dy+eq.attr.fontsize/2;
            ++line;
            xcas::eqw_select(eq.data,line,col,true,value);
          }
          else
            dy -= eq.attr.fontsize/2;
          continue;
        }
        if ( key==KEY_CTRL_PAGEDOWN ){
          dy -= 20;
          continue;
        }
      }
      else { // else listormat
        if (key==KEY_CTRL_LEFT){
          delta=xcas::eqw_select_leftright(eq,true,alph?2:0);
          // cout << "left " << delta << endl;
          if (doit) dx += (delta?delta:-20);
          continue;
        }
        if (key==KEY_SHIFT_LEFT){
          delta=xcas::eqw_select_leftright(eq,true,1);
          vector<int> goto_sel;
          if (doit) dx += (delta?delta:-20);
          continue;
        }
        if (key==KEY_CTRL_RIGHT){
          delta=xcas::eqw_select_leftright(eq,false,alph?2:0);
          // cout << "right " << delta << endl;
          if (doit)
            dx += (delta?delta:20);
          continue;
        }
        if (key==KEY_SHIFT_RIGHT){
          delta=xcas::eqw_select_leftright(eq,false,1);
          // cout << "right " << delta << endl;
          if (doit)
            dx += (delta?delta:20);
          // dx=eqdata.dx-LCD_WIDTH_PX+20;
          continue;
        }
        doit=eqdata.dy>=LCD_HEIGHT_PX-2*STATUS_AREA_PX;
        if (key==KEY_CTRL_UP){
          delta=xcas::eqw_select_up(eq.data);
          // cout << "up " << delta << endl;
          continue;
        }
        //cout << "up " << eq.data << endl;
        if (key==KEY_CTRL_PAGEUP && doit){
          dy=eqdata.y+eqdata.dy+20;
          continue;
        }
        if (key==KEY_CTRL_DOWN){
          delta=xcas::eqw_select_down(eq.data);
          // cout << "down " << delta << endl;
          continue;
        }
        //cout << "down " << eq.data << endl;
        if ( key==KEY_CTRL_PAGEDOWN && doit){
          dy=eqdata.y+LCD_HEIGHT_PX-STATUS_AREA_PX;
          continue;
        }
      }
      if (adds){
        edited=true;
        if (strcmp(adds,"f(x):=")==0 || strcmp(adds,":=")==0)
          adds="f";
        if (strcmp(adds,"'")==0)
          adds="diff";
        if (strcmp(adds,"^2")==0)
          adds="sq";
        if (strcmp(adds,">")==0)
          adds="simplify";
        if (strcmp(adds,"<")==0)
          adds="factor";
        if (strcmp(adds,"#")==0)
          adds="partfrac";
        string cmd(adds);
        if (cmd.size() && cmd[cmd.size()-1]=='(')
          cmd ='\''+cmd.substr(0,cmd.size()-1)+'\'';
        vector<int> goto_sel;
        if (xcas::Equation_adjust_xy(eq.data,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos,&goto_sel) && gsel){
          gen op;
          int addarg=0;
          if (addssize==1){
            switch (adds[0]){
            case '+':
              addarg=1;
              op=at_plus;
              break;
            case '^':
              addarg=1;
              op=at_pow;
              break;
            case '=':
              addarg=1;
              op=at_equal;
              break;
            case '-':
              addarg=1;
              op=at_binary_minus;
              break;
            case '*':
              addarg=1;
              op=at_prod;
              break;
            case '/':
              addarg=1;
              op=at_division;
              break;
            case '\'':
              addarg=1;
              op=at_diff;
              break;
            }
          }
          if (op==0)
            op=gen(cmd,contextptr);
          if (op.type==_SYMB)
            op=op._SYMBptr->sommet;
          // cout << "keyed " << adds << " " << op << " " << op.type << endl;
          if (op.type==_FUNC){
            edited=true;
            // execute command on selection
            gen tmp,value;
            if (xcas::do_select(*gsel,true,value) && value.type==_EQW){
              if (op==at_integrate || op==at_sum)
                addarg=3;
              if (op==at_limit)
                addarg=2;
              gen args=value._EQWptr->g;
              statuslinemsg(!lang?"cancel: stop computation":"annul: stoppe calcul",COLOR_RED);
              args=giac::eval(args,1,contextptr);
              if (giac::interrupted){
                print_msg12(lang?"Interrompu":"Interrupted",0);
                getkey(0);
                freeze=false;
                continue;
              }
              giac::clear_abort();
              giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
              if (freeze){
                statuslinemsg(lang?"Ecran fige.":"Screen freezed",COLOR_RED);
                getkey(0);
                freeze=false;
              }
              gen vx=x__IDNT_e;
              if (addarg==1)
                args=makesequence(args,0);
              if (addarg==2)
                args=makesequence(args,vx_var(),0);
              if (addarg==3)
                args=makesequence(args,vx_var(),0,1);
              if (op==at_surd)
                args=makesequence(args,key==KEY_CHAR_CUBEROOT?3:4);
              if (op==at_subst)
                args=makesequence(args,giac::symb_equal(vx_var(),0));
              unary_function_ptr immediate_op[]={*at_eval,*at_evalf,*at_evalc,*at_regrouper,*at_simplify,*at_normal,*at_ratnormal,*at_factor,*at_cfactor,*at_partfrac,*at_cpartfrac,*at_expand,*at_canonical_form,*at_exp2trig,*at_trig2exp,*at_sincos,*at_lin,*at_tlin,*at_tcollect,*at_texpand,*at_trigexpand,*at_trigcos,*at_trigsin,*at_trigtan,*at_halftan};
              if (equalposcomp(immediate_op,*op._FUNCptr)){
                //giac::set_abort();
                tmp=(*op._FUNCptr)(args,contextptr);
                //clear_abort();
                //esc_flag=0;
                ctrl_c=false;
                kbd_interrupted=interrupted=false;
              }
              else
                tmp=symbolic(*op._FUNCptr,args);
              //cout << "sel " << value._EQWptr->g << " " << tmp << " " << goto_sel << endl;
              //esc_flag=0;
              ctrl_c=false;
              kbd_interrupted=interrupted=false;
              if (!is_undef(tmp)){
                xcas::replace_selection(eq,tmp,gsel,&goto_sel);
                if (addarg){
                  xcas::eqw_select_down(eq.data);
                  xcas::eqw_select_leftright(eq,false);
                }
                eqdata=xcas::Equation_total_size(eq.data);
                dx=(eqdata.dx-LCD_WIDTH_PX)/2;
                dy=LCD_HEIGHT_PX-2*STATUS_AREA_PX+eqdata.y;
                firstrun=-1; // workaround, force 2 times display
              }
            }
          }
        }
      }
    }
    //*logptr(contextptr) << eq.data << endl;
  }

  // make a free copy of g
  gen Equation_copy(const gen & g){
    if (g.type==_EQW)
      return *g._EQWptr;
    if (g.type!=_VECT)
      return g;
    vecteur & v = *g._VECTptr;
    const_iterateur it=v.begin(),itend=v.end();
    vecteur res;
    res.reserve(itend-it);
    for (;it!=itend;++it)
      res.push_back(Equation_copy(*it));
    return gen(res,g.subtype);
  }

  // matrix/list select
  bool do_select(gen & eql,bool select,gen & value){
    if (eql.type==_VECT && !eql._VECTptr->empty()){
      vecteur & v=*eql._VECTptr;
      size_t s=v.size();
      if (v[s-1].type!=_EQW)
	return false;
      v[s-1]._EQWptr->selected=select;
      gen sommet=v[s-1]._EQWptr->g;
      --s;
      vecteur args(s);
      for (size_t i=0;i<s;++i){
	if (!do_select(v[i],select,args[i]))
	  return false;
	if (args[i].type==_EQW)
	  args[i]=args[i]._EQWptr->g;
      }
      gen va=s==1?args[0]:gen(args,_SEQ__VECT);
      if (sommet.type==_FUNC)
	va=symbolic(*sommet._FUNCptr,va);
      else
	va=sommet(va,context0);
      //cout << "va " << va << endl;
      value=*v[s]._EQWptr;
      value._EQWptr->g=va;
      //cout << "value " << value << endl;
      return true;
    }
    if (eql.type!=_EQW)
      return false;
    eql._EQWptr->selected=select;
    value=eql;
    return true;
  }
  
  bool Equation_box_sizes(const gen & g,int & l,int & h,int & x,int & y,attributs & attr,bool & selected){
    if (g.type==_EQW){
      eqwdata & w=*g._EQWptr;
      x=w.x;
      y=w.y;
      l=w.dx;
      h=w.dy;
      selected=w.selected;
      attr=w.eqw_attributs;
      //cout << g << endl;
      return true;
    }
    else {
      if (g.type!=_VECT || g._VECTptr->empty() ){
	l=0;
	h=0;
	x=0;
	y=0;
	attr=attributs(0,0,0);
	selected=false;
	return true;
      }
      gen & g1=g._VECTptr->back();
      Equation_box_sizes(g1,l,h,x,y,attr,selected);
      return false;
    }
  }

  // return true if g has some selection inside, gsel points to the selection
  bool Equation_adjust_xy(gen & g,int & xleft,int & ytop,int & xright,int & ybottom,gen * & gsel,gen * & gselparent,int &gselpos,std::vector<int> * goto_ptr){
    gsel=0;
    gselparent=0;
    gselpos=0;
    int x,y,w,h;
    attributs f(0,0,0);
    bool selected;
    Equation_box_sizes(g,w,h,x,y,f,selected);
    if ( (g.type==_EQW__VECT) || selected ){ // terminal or selected
      xleft=x;
      ybottom=y;
      if (selected){ // g is selected
	ytop=y+h;
	xright=x+w;
	gsel =  &g;
	//cout << "adjust " << *gsel << endl;
	return true;
      }
      else { // no selection
	xright=x;
	ytop=y;
	return false;
      }
    }
    if (g.type!=_VECT)
      return false;
    // last not selected, recurse
    iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end()-1;
    for (;it!=itend;++it){
      if (Equation_adjust_xy(*it,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos,goto_ptr)){
	if (goto_ptr){
	  goto_ptr->push_back(it-g._VECTptr->begin());
	  //cout << g << ":" << *goto_ptr << endl;
	}
	if (gsel==&*it){
	  // check next siblings
	  
	  gselparent= &g;
	  gselpos=it-g._VECTptr->begin();
	  //cout << "gselparent " << g << endl;
	}
	return true;
      }
    }
    return false;
  }
 
  // select or deselect part of the current eqution
  // This is done *in place*
  void Equation_select(gen & g,bool select){
    if (g.type==_EQW){
      eqwdata & e=*g._EQWptr;
      e.selected=select;
    }
    if (g.type!=_VECT)
      return;
    vecteur & v=*g._VECTptr;
    iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      Equation_select(*it,select);
  }

  // decrease selection (like HP49 eqw Down key)
  int eqw_select_down(gen & g){
    int xleft,ytop,xright,ybottom,gselpos;
    int newxleft,newytop,newxright,newybottom;
    gen * gsel,*gselparent;
    if (Equation_adjust_xy(g,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos)){
      //cout << "select down before " << *gsel << endl;
      if (gsel->type==_VECT && !gsel->_VECTptr->empty()){
	Equation_select(*gsel,false);
	Equation_select(gsel->_VECTptr->front(),true);
	//cout << "select down after " << *gsel << endl;
	Equation_adjust_xy(g,newxleft,newytop,newxright,newybottom,gsel,gselparent,gselpos);
	return newytop-ytop;
      }
    }
    return 0;
  }

  int eqw_select_up(gen & g){
    int xleft,ytop,xright,ybottom,gselpos;
    int newxleft,newytop,newxright,newybottom;
    gen * gsel,*gselparent;
    if (Equation_adjust_xy(g,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos) && gselparent){
      Equation_select(*gselparent,true);
      //cout << "gselparent " << *gselparent << endl;
      Equation_adjust_xy(g,newxleft,newytop,newxright,newybottom,gsel,gselparent,gselpos);
      return newytop-ytop;
    }
    return false;
  }

  // exchange==0 move selection to left or right sibling, ==2 add left or right
  // sibling, ==1 exchange selection with left or right sibling
  int eqw_select_leftright(Equation & eq,bool left,int exchange){
    gen & g=eq.data;
    int xleft,ytop,xright,ybottom,gselpos;
    int newxleft,newytop,newxright,newybottom;
    gen * gsel,*gselparent;
    vector<int> goto_sel;
    if (Equation_adjust_xy(g,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos,&goto_sel) && gselparent && gselparent->type==_VECT){
      vecteur & gselv=*gselparent->_VECTptr;
      int n=gselv.size()-1,gselpos_orig=gselpos;
      if (n<1) return 0;
      if (left) {
	if (gselpos==0)
	  gselpos=n-1;
	else
	  gselpos--;
      }
      else {
	if (gselpos==n-1)
	  gselpos=0;
	else
	  gselpos++;
      }
      if (exchange==1){ // exchange gselpos_orig and gselpos
	swapgen(gselv[gselpos],gselv[gselpos_orig]);
	gsel=&gselv[gselpos_orig];
	gen value;
	if (xcas::do_select(*gsel,true,value) && value.type==_EQW)
	  replace_selection(eq,value._EQWptr->g,gsel,&goto_sel);
      }
      else {
	// increase selection to next sibling possible for + and * only
	if (n>2 && exchange==2 && gselv[n].type==_EQW && (gselv[n]._EQWptr->g==at_plus || gselv[n]._EQWptr->g==at_prod)){
	  gen value1, value2,tmp;
	  if (gselpos_orig<gselpos)
	    swapint(gselpos_orig,gselpos);
	  // now gselpos<gselpos_orig
	  xcas::do_select(gselv[gselpos_orig],true,value1);
	  xcas::do_select(gselv[gselpos],true,value2);
	  if (value1.type==_EQW && value2.type==_EQW){
	    tmp=gselv[n]._EQWptr->g==at_plus?value1._EQWptr->g+value2._EQWptr->g:value1._EQWptr->g*value2._EQWptr->g;
	    gselv.erase(gselv.begin()+gselpos_orig);
	    replace_selection(eq,tmp,&gselv[gselpos],&goto_sel);
	  }
	}
	else {
	  Equation_select(*gselparent,false);
	  gen & tmp=(*gselparent->_VECTptr)[gselpos];
	  Equation_select(tmp,true);
	}
      }
      Equation_adjust_xy(g,newxleft,newytop,newxright,newybottom,gsel,gselparent,gselpos);
      return newxleft-xleft;
    }
    return 0;
  }

  bool eqw_select(const gen & eq,int l,int c,bool select,gen & value){
    value=undef;
    if (l<0 || eq.type!=_VECT || eq._VECTptr->size()<=l)
      return false;
    gen & eql=(*eq._VECTptr)[l];
    if (c<0)
      return do_select(eql,select,value);
    if (eql.type!=_VECT || eql._VECTptr->size()<=c)
      return false;
    gen & eqlc=(*eql._VECTptr)[c];
    return do_select(eqlc,select,value);
  }

  gen Equation_compute_size(const gen & g,const attributs & a,int windowhsize,GIAC_CONTEXT);
  
  // void Bdisp_MMPrint(int x, int y, const char* string, int mode_flags, int xlimit, int P6, int P7, int color, int back_color, int writeflag, int P11); 
  // void PrintCXY(int x, int y, const char *cptr, int mode_flags, int P5, int color, int back_color, int P8, int P9)
  // void PrintMini( int* x, int* y, const char* string, int mode_flags, unsigned int xlimit, int P6, int P7, int color, int back_color, int writeflag, int P11)

  void text_print(int fontsize,const char * s,int x,int y,int c=COLOR_BLACK,int bg=COLOR_WHITE,int mode=0){
    // *logptr(contextptr) << x << " " << y << " " << fontsize << " " << s << endl; return;
    c=(unsigned short) c;
    if (x>LCD_WIDTH_PX) return;
    int ss=strlen(s);
    if (ss==1 && s[0]==0x1e){ // arrow for limit
      if (mode==4)
	c=bg;
      draw_line(x,y-4,x+fontsize/2,y-4,c);
      draw_line(x,y-3,x+fontsize/2,y-3,c);
      draw_line(x+fontsize/2-4,y,x+fontsize/2,y-4,c);
      draw_line(x+fontsize/2-3,y,x+fontsize/2+1,y-4,c);
      draw_line(x+fontsize/2-4,y-7,x+fontsize/2,y-3,c);   
      draw_line(x+fontsize/2-3,y-7,x+fontsize/2+1,y-3,c);   
      return;
    }
    if (ss==2 && strcmp(s,"pi")==0){
      if (mode==4){
	drawRectangle(x,y+2-fontsize,fontsize,fontsize,c);
	c=bg;
      }
      draw_line(x+fontsize/3-1,y+1,x+fontsize/3,y+6-fontsize,c);
      draw_line(x+fontsize/3-2,y+1,x+fontsize/3-1,y+6-fontsize,c);
      draw_line(x+2*fontsize/3,y+1,x+2*fontsize/3,y+6-fontsize,c);
      draw_line(x+2*fontsize/3+1,y+1,x+2*fontsize/3+1,y+6-fontsize,c);
      draw_line(x+2,y+6-fontsize,x+fontsize,y+6-fontsize,c);
      draw_line(x+2,y+5-fontsize,x+fontsize,y+5-fontsize,c);
      return;
    }
    if (fontsize>=16){
      y -= 33; // status area shift
      os_draw_string(x,y,mode?bg:c,mode?c:bg,s,false);
      return;
    }
    y -= 28;
    //PrintCXY(x,y,s,TEXT_MODE_NORMAL,-1,COLOR_BLACK,COLOR_WHITE,1,0);
    os_draw_string_small(x,y,mode?bg:c,mode?c:bg,s,false);
  }

  
  
  int text_width(int fontsize,const char * s){
    if (fontsize>=16)
      return 8*strlen(s);
    else
      return 5*strlen(s);
  }

  int fl_width(const char * s){ // FIXME do a fake print
    return text_width(14,s);
  }

  void fl_arc(int x,int y,int rx,int ry,int theta1_deg,int theta2_deg,int c=COLOR_BLACK){
    rx/=2;
    ry/=2;
    // *logptr(contextptr) << "theta " << theta1_deg << " " << theta2_deg << endl;
    if (ry==rx){
      if (theta2_deg-theta1_deg==360){
	draw_circle(x+rx,y+rx,rx,c,true,true,true,true);
	return;
      }
      if (theta1_deg==0 && theta2_deg==180){
	draw_circle(x+rx,y+rx,rx,c,true,true,false,false);
	return;
      }
      if (theta1_deg==180 && theta2_deg==360){
	draw_circle(x+rx,y+rx,rx,c,false,false,true,true);
	return;
      }
    }
    // *logptr(contextptr) << "draw_arc" << theta1_deg*M_PI/180. << " " << theta2_deg*M_PI/180. << endl;
    draw_arc(x+rx,y+ry,rx,ry,c,theta1_deg*M_PI/180.,theta2_deg*M_PI/180.);
  }

  void fl_pie(int x,int y,int rx,int ry,int theta1_deg,int theta2_deg,int c=COLOR_BLACK,bool segment=false){
    //cout << "fl_pie r=" << rx << " " << ry << " t=" << theta1_deg << " " << theta2_deg << " " << c << endl;
    if (!segment && ry==rx){
      if (theta2_deg-theta1_deg>=360){
	rx/=2;
	draw_filled_circle(x+rx,y+rx,rx,c);
	return;
      }
      if (theta1_deg==-90 && theta2_deg==90){
	rx/=2;
	draw_filled_circle(x+rx,y+rx,rx,c,false,true);
	return;
      }
      if (theta1_deg==90 && theta2_deg==270){
	rx/=2;
	draw_filled_circle(x+rx,y+rx,rx,c,true,false);
	return;
      }
    }
    // approximation by a filled polygon
    // points: (x,y), (x+rx*cos(theta)/2,y+ry*sin(theta)/2) theta=theta1..theta2
    while (theta2_deg<theta1_deg)
      theta2_deg+=360;
    if (theta2_deg-theta1_deg>=360){
      theta1_deg=0;
      theta2_deg=360;
    }
    int N0=theta2_deg-theta1_deg+1;
    // reduce N if rx or ry is small
    double red=double(rx)/LCD_WIDTH_PX*double(ry)/LCD_HEIGHT_PX;
    if (red>1) red=1;
    if (red<0.1) red=0.1;
    int N=red*N0;
    if (N<5)
      N=N0>5?5:N0;
    if (N<2)
      N=2;
    vector< vector<int> > v(segment?N+1:N+2,vector<int>(2));
    x += rx/2;
    y += ry/2;
    int i=0;
    if (!segment){
      v[0][0]=x;
      v[0][1]=y;
      ++i;
    }
    double theta=theta1_deg*M_PI/180;
    double thetastep=(theta2_deg-theta1_deg)*M_PI/(180*(N-1));
    for (;i<v.size()-1;++i){
      v[i][0]=int(x+rx*std::cos(theta)/2+.5);
      v[i][1]=int(y-ry*std::sin(theta)/2+.5); // y is inverted
      theta += thetastep;
    }
    v.back()=v.front();
    draw_filled_polygon(v,0,LCD_WIDTH_PX,24,LCD_HEIGHT_PX,c);
  }

  bool binary_op(const unary_function_ptr & u){
    const unary_function_ptr binary_op_tab_ptr []={*at_plus,*at_prod,*at_pow,*at_and,*at_ou,*at_xor,*at_different,*at_same,*at_equal,*at_unit,*at_compose,*at_composepow,*at_deuxpoints,*at_tilocal,*at_pointprod,*at_pointdivision,*at_pointpow,*at_division,*at_normalmod,*at_minus,*at_intersect,*at_union,*at_interval,*at_inferieur_egal,*at_inferieur_strict,*at_superieur_egal,*at_superieur_strict,*at_equal2,0};
    return equalposcomp(binary_op_tab_ptr,u);
  }
  
  eqwdata Equation_total_size(const gen & g){
    if (g.type==_EQW)
      return *g._EQWptr;
    if (g.type!=_VECT || g._VECTptr->empty())
      return eqwdata(0,0,0,0,attributs(0,0,0),undef);
    return Equation_total_size(g._VECTptr->back());
  }

  // find smallest value of y and height
  void Equation_y_dy(const gen & g,int & y,int & dy){
    y=0; dy=0;
    if (g.type==_EQW){
      y=g._EQWptr->y;
      dy=g._EQWptr->dy;
    }
    if (g.type==_VECT){
      iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	int Y,dY;
	Equation_y_dy(*it,Y,dY);
	// Y, Y+dY and y,y+dy
	int ymax=giac::giacmax(y+dy,Y+dY);
	if (Y<y)
	  y=Y;
	dy=ymax-y;
      }
    }
  }

  void Equation_translate(gen & g,int deltax,int deltay){
    if (g.type==_EQW){
      g._EQWptr->x += deltax;
      g._EQWptr->y += deltay;
      g._EQWptr->baseline += deltay;
      return ;
    }
    if (g.type!=_VECT)
      setsizeerr();
    vecteur & v=*g._VECTptr;
    iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      Equation_translate(*it,deltax,deltay);
  }

  gen Equation_change_attributs(const gen & g,const attributs & newa){
    if (g.type==_EQW){
      gen res(*g._EQWptr);
      res._EQWptr->eqw_attributs = newa;
      return res;
    }
    if (g.type!=_VECT)
      return gensizeerr();
    vecteur v=*g._VECTptr;
    iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      *it=Equation_change_attributs(*it,newa);
    return gen(v,g.subtype);
  }

  vecteur Equation_subsizes(const gen & arg,const attributs & a,int windowhsize,GIAC_CONTEXT){
    vecteur v;
    if ( (arg.type==_VECT) && ( (arg.subtype==_SEQ__VECT) 
				// || (!ckmatrix(arg)) 
				) ){
      const_iterateur it=arg._VECTptr->begin(),itend=arg._VECTptr->end();
      for (;it!=itend;++it)
	v.push_back(Equation_compute_size(*it,a,windowhsize,contextptr));
    }
    else {
      v.push_back(Equation_compute_size(arg,a,windowhsize,contextptr));
    }
    return v;
  }

  // vertical merge with same baseline
  // for vertical merge of hp,yp at top (like ^) add fontsize to yp
  // at bottom (like lower bound of int) substract fontsize from yp
  void Equation_vertical_adjust(int hp,int yp,int & h,int & y){
    int yf=min(y,yp);
    h=max(y+h,yp+hp)-yf;
    y=yf;
  }

  gen Equation_compute_symb_size(const gen & g,const attributs & a,int windowhsize,GIAC_CONTEXT){
    if (g.type!=_SYMB)
      return Equation_compute_size(g,a,windowhsize,contextptr);
    unary_function_ptr & u=g._SYMBptr->sommet;
    gen arg=g._SYMBptr->feuille,rootof_value;
    if (u==at_makevector){
      vecteur v(1,arg);
      if (arg.type==_VECT)
	v=*arg._VECTptr;
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it){
	if ( (it->type==_SYMB) && (it->_SYMBptr->sommet==at_makevector) )
	  *it=_makevector(it->_SYMBptr->feuille,contextptr);
      }
      return Equation_compute_size(v,a,windowhsize,contextptr);
    }
    if (u==at_makesuite){
      if (arg.type==_VECT)
	return Equation_compute_size(gen(*arg._VECTptr,_SEQ__VECT),a,windowhsize,contextptr);
      else
	return Equation_compute_size(arg,a,windowhsize,contextptr);
    }
    if (u==at_sqrt)
      return Equation_compute_size(symb_pow(arg,plus_one_half),a,windowhsize,contextptr);
    if (u==at_division){
      if (arg.type!=_VECT || arg._VECTptr->size()!=2)
	return Equation_compute_size(arg,a,windowhsize,contextptr);
      gen tmp;
      tmp.__FRACptr = new ref_fraction(Tfraction<gen>(arg._VECTptr->front(),arg._VECTptr->back()));
      tmp.type=_FRAC;
      return Equation_compute_size(tmp,a,windowhsize,contextptr);
    }
    if (u==at_prod){
      gen n,d;
      if (rewrite_prod_inv(arg,n,d)){
	if (n.is_symb_of_sommet(at_neg))
	  return Equation_compute_size(symb_neg(Tfraction<gen>(-n,d)),a,windowhsize,contextptr);
	return Equation_compute_size(Tfraction<gen>(n,d),a,windowhsize,contextptr);
      }
    }
    if (u==at_inv){
      if ( (is_integer(arg) && is_positive(-arg,contextptr))
	   || (arg.is_symb_of_sommet(at_neg)))
	return Equation_compute_size(symb_neg(Tfraction<gen>(plus_one,-arg)),a,windowhsize,contextptr);
      return Equation_compute_size(Tfraction<gen>(plus_one,arg),a,windowhsize,contextptr);
    }
    if (u==at_expr && arg.type==_VECT && arg.subtype==_SEQ__VECT && arg._VECTptr->size()==2 && arg._VECTptr->back().type==_INT_){
      gen varg1=Equation_compute_size(arg._VECTptr->front(),a,windowhsize,contextptr);
      eqwdata vv(Equation_total_size(varg1));
      gen varg2=eqwdata(0,0,0,0,a,arg._VECTptr->back());
      vecteur v12(makevecteur(varg1,varg2));
      v12.push_back(eqwdata(vv.dx,vv.dy,0,vv.y,a,at_expr,0));
      return gen(v12,_SEQ__VECT);
    }
    int llp=int(text_width(a.fontsize,("(")))-1;
    int lrp=llp;
    int lc=int(text_width(a.fontsize,(",")));
    string us=u.ptr()->s;
    int ls=int(text_width(a.fontsize,(us.c_str())));
    // if (isalpha(u.ptr()->s[0])) ls += 1;
    if (u==at_abs)
      ls = 2;
    // special cases first int, sigma, /, ^
    // and if printed as printsommetasoperator
    // otherwise print with usual functional notation
    int x=0;
    int h=a.fontsize;
    int y=0;
#if 1
    if ((u==at_integrate) || (u==at_sum) ){ // Int
      int s=1;
      if (arg.type==_VECT)
	s=arg._VECTptr->size();
      else
	arg=vecteur(1,arg);
      // s==1 -> general case
      if ( (s==1) || (s==2) ){ // int f(x) dx and sum f(n) n
	vecteur v(Equation_subsizes(gen(*arg._VECTptr,_SEQ__VECT),a,windowhsize,contextptr));
	eqwdata vv(Equation_total_size(v[0]));
	if (s==1){
	  x=a.fontsize;
	  Equation_translate(v[0],x,0);
	  x += int(text_width(a.fontsize,(" dx")));
	}
	if (s==2){
	  if (u==at_integrate){
	    x=a.fontsize;
	    Equation_translate(v[0],x,0);
	    x += vv.dx+int(text_width(a.fontsize,(" d")));
	    Equation_vertical_adjust(vv.dy,vv.y,h,y);
	    vv=Equation_total_size(v[1]);
	    Equation_translate(v[1],x,0);
	    Equation_vertical_adjust(vv.dy,vv.y,h,y);
	  }
	  else {
	    Equation_vertical_adjust(vv.dy,vv.y,h,y);
	    eqwdata v1=Equation_total_size(v[1]);
	    x=max(a.fontsize,v1.dx)+2*a.fontsize/3; // var name size
	    Equation_translate(v[1],0,-v1.dy-v1.y);
	    Equation_vertical_adjust(v1.dy,-v1.dy,h,y);
	    Equation_translate(v[0],x,0);
	    x += vv.dx; // add function size
	  }
	}
	if (u==at_integrate){
	  x += vv.dx;
	  if (h==a.fontsize)
	    h+=2*a.fontsize/3;
	  if (y==0){
	    y=-2*a.fontsize/3;
	    h+=2*a.fontsize/3;
	  }
	}
	v.push_back(eqwdata(x,h,0,y,a,u,0));
	return gen(v,_SEQ__VECT);
      }
      if (s>=3){ // int _a^b f(x) dx
	vecteur & intarg=*arg._VECTptr;
	gen tmp_l,tmp_u,tmp_f,tmp_x;
	attributs aa(a);
	if (a.fontsize>=10)
	  aa.fontsize -= 2;
	tmp_f=Equation_compute_size(intarg[0],a,windowhsize,contextptr);
	tmp_x=Equation_compute_size(intarg[1],a,windowhsize,contextptr);
	tmp_l=Equation_compute_size(intarg[2],aa,windowhsize,contextptr);
	if (s==4)
	  tmp_u=Equation_compute_size(intarg[3],aa,windowhsize,contextptr);
	x=a.fontsize+(u==at_integrate?-2:+4);
	eqwdata vv(Equation_total_size(tmp_l));
	Equation_translate(tmp_l,x,-vv.y-vv.dy);
	vv=Equation_total_size(tmp_l);
	Equation_vertical_adjust(vv.dy,vv.y,h,y);
	int lx = vv.dx;
	if (s==4){
	  vv=Equation_total_size(tmp_u);
	  Equation_translate(tmp_u,x,a.fontsize-3-vv.y);
	  vv=Equation_total_size(tmp_u);
	  Equation_vertical_adjust(vv.dy,vv.y,h,y);
	}
	x += max(lx,vv.dx);
	Equation_translate(tmp_f,x,0);
	vv=Equation_total_size(tmp_f);
	Equation_vertical_adjust(vv.dy,vv.y,h,y);
	if (u==at_integrate){
	  x += vv.dx+int(text_width(a.fontsize,(" d")));
	  Equation_translate(tmp_x,x,0);
	  vv=Equation_total_size(tmp_x);
	  Equation_vertical_adjust(vv.dy,vv.y,h,y);
	  x += vv.dx;
	}
	else {
	  x += vv.dx;
	  Equation_vertical_adjust(vv.dy,vv.y,h,y);
	  vv=Equation_total_size(tmp_x);
	  x=max(x,vv.dx)+a.fontsize/3;
	  Equation_translate(tmp_x,0,-vv.dy-vv.y);
	  //Equation_translate(tmp_l,0,-1);	  
	  if (s==4) Equation_translate(tmp_u,-2,0);	  
	  Equation_vertical_adjust(vv.dy,-vv.dy,h,y);
	}
	vecteur res(makevecteur(tmp_f,tmp_x,tmp_l));
	if (s==4)
	  res.push_back(tmp_u);
	res.push_back(eqwdata(x,h,0,y,a,u,0));
	return gen(res,_SEQ__VECT);
      }
    }
    if (u==at_limit && arg.type==_VECT){ // limit
      vecteur limarg=*arg._VECTptr;
      int s=limarg.size();
      if (s==2 && limarg[1].is_symb_of_sommet(at_equal)){
	limarg.push_back(limarg[1]._SYMBptr->feuille[1]);
	limarg[1]=limarg[1]._SYMBptr->feuille[0];
	++s;
      }
      if (s>=3){
	gen tmp_l,tmp_f,tmp_x,tmp_dir;
	attributs aa(a);
	if (a.fontsize>=10)
	  aa.fontsize -= 2;
	tmp_f=Equation_compute_size(limarg[0],a,windowhsize,contextptr);
	tmp_x=Equation_compute_size(limarg[1],aa,windowhsize,contextptr);
	tmp_l=Equation_compute_size(limarg[2],aa,windowhsize,contextptr);
	if (s==4)
	  tmp_dir=Equation_compute_size(limarg[3],aa,windowhsize,contextptr);
	eqwdata vf(Equation_total_size(tmp_f));
	eqwdata vx(Equation_total_size(tmp_x));
	eqwdata vl(Equation_total_size(tmp_l));
	eqwdata vdir(Equation_total_size(tmp_dir));
	int sous=max(vx.dy,vl.dy);
	if (s==4)
	  Equation_translate(tmp_f,vx.dx+vl.dx+vdir.dx+a.fontsize+4,0);
	else
	  Equation_translate(tmp_f,vx.dx+vl.dx+a.fontsize+2,0);
	Equation_translate(tmp_x,0,-sous-vl.y);
	Equation_translate(tmp_l,vx.dx+a.fontsize+2,-sous-vl.y);
	if (s==4)
	  Equation_translate(tmp_dir,vx.dx+vl.dx+a.fontsize+4,-sous-vl.y);
	h=vf.dy;
	y=vf.y;
	vl=Equation_total_size(tmp_l);
	Equation_vertical_adjust(vl.dy,vl.y,h,y);
	vecteur res(makevecteur(tmp_f,tmp_x,tmp_l));
	if (s==4){
	  res.push_back(tmp_dir);
	  res.push_back(eqwdata(vf.dx+vx.dx+a.fontsize+4+vl.dx+vdir.dx,h,0,y,a,u,0));
	}
	else
	  res.push_back(eqwdata(vf.dx+vx.dx+a.fontsize+2+vl.dx,h,0,y,a,u,0));
	return gen(res,_SEQ__VECT);
      }
    }
#endif
    if ( (u==at_of || u==at_at) && arg.type==_VECT && arg._VECTptr->size()==2 ){
      // user function, function in 1st arg, arguments in 2nd arg
      gen varg1=Equation_compute_size(arg._VECTptr->front(),a,windowhsize,contextptr);
      eqwdata vv=Equation_total_size(varg1);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      gen arg2=arg._VECTptr->back();
      if (u==at_at && xcas_mode(contextptr)!=0){
	if (arg2.type==_VECT)
	  arg2=gen(addvecteur(*arg2._VECTptr,vecteur(arg2._VECTptr->size(),plus_one)),_SEQ__VECT);
	else
	  arg2=arg2+plus_one; 
      }
      gen varg2=Equation_compute_size(arg2,a,windowhsize,contextptr);
      Equation_translate(varg2,vv.dx+llp,0);
      vv=Equation_total_size(varg2);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      vecteur res(makevecteur(varg1,varg2));
      res.push_back(eqwdata(vv.dx+vv.x+lrp,h,0,y,a,u,0));
      return gen(res,_SEQ__VECT);
    }
    if (u==at_pow){ 
      // first arg not translated
      gen varg=Equation_compute_size(arg._VECTptr->front(),a,windowhsize,contextptr);
      eqwdata vv=Equation_total_size(varg);
      // 1/2 ->sqrt, otherwise as exponent
      if (arg._VECTptr->back()==plus_one_half){
	Equation_translate(varg,a.fontsize,0);
	vecteur res(1,varg);
	res.push_back(eqwdata(vv.dx+a.fontsize,vv.dy+4,vv.x,vv.y,a,at_sqrt,0));
	return gen(res,_SEQ__VECT);
      }
      bool needpar=vv.g.type==_FUNC || vv.g.is_symb_of_sommet(at_pow) || need_parenthesis(vv.g);
      if (needpar)
	x=llp;
      Equation_translate(varg,x,0);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      vecteur res(1,varg);
      // 2nd arg translated 
      if (needpar)
	x+=vv.dx+lrp;
      else
	x+=vv.dx+1;
      int arg1dy=vv.dy,arg1y=vv.y;
      if (a.fontsize>=16){
	attributs aa(a);
	aa.fontsize -= 2;
	varg=Equation_compute_size(arg._VECTptr->back(),aa,windowhsize,contextptr);
      }
      else
	varg=Equation_compute_size(arg._VECTptr->back(),a,windowhsize,contextptr);
      vv=Equation_total_size(varg);
      Equation_translate(varg,x,arg1y+(3*arg1dy)/4-vv.y);
      res.push_back(varg);
      vv=Equation_total_size(varg);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      x += vv.dx;
      res.push_back(eqwdata(x,h,0,y,a,u,0));
      return gen(res,_SEQ__VECT);
    }
    if (u==at_factorial){
      vecteur v;
      gen varg=Equation_compute_size(arg,a,windowhsize,contextptr);
      eqwdata vv=Equation_total_size(varg);
      bool paren=need_parenthesis(vv.g) || vv.g==at_prod || vv.g==at_division || vv.g==at_pow;
      if (paren)
	x+=llp;
      Equation_translate(varg,x,0);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      v.push_back(varg);
      x += vv.dx;
      if (paren)
	x+=lrp;
      varg=eqwdata(x+4,h,0,y,a,u,0);
      v.push_back(varg);
      return gen(v,_SEQ__VECT);
    }
    if (u==at_sto){ // A:=B, *it -> B
      gen varg=Equation_compute_size(arg._VECTptr->back(),a,windowhsize,contextptr);
      eqwdata vv=Equation_total_size(varg);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      Equation_translate(varg,x,0);
      vecteur v(2);
      v[1]=varg;
      x+=vv.dx;
      x+=ls+3;
      // first arg not translated
      varg=Equation_compute_size(arg._VECTptr->front(),a,windowhsize,contextptr);
      vv=Equation_total_size(varg);
      if (need_parenthesis(vv.g))
	x+=llp;
      Equation_translate(varg,x,0);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      v[0]=varg;
      x += vv.dx;
      if (need_parenthesis(vv.g))
	x+=lrp;
      v.push_back(eqwdata(x,h,0,y,a,u,0));
      return gen(v,_SEQ__VECT);
    }
    if (u==at_program && arg._VECTptr->back().type!=_VECT && !arg._VECTptr->back().is_symb_of_sommet(at_local) ){
      gen varg=Equation_compute_size(arg._VECTptr->front(),a,windowhsize,contextptr);
      eqwdata vv=Equation_total_size(varg);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      Equation_translate(varg,x,0);
      vecteur v(2);
      v[0]=varg;
      x+=vv.dx;
      x+=int(text_width(a.fontsize,("->")))+3;
      varg=Equation_compute_size(arg._VECTptr->back(),a,windowhsize,contextptr);
      vv=Equation_total_size(varg);
      Equation_translate(varg,x,0);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      v[1]=varg;
      x += vv.dx;
      v.push_back(eqwdata(x,h,0,y,a,u,0));
      return gen(v,_SEQ__VECT);      
    }
    bool binaryop= (u.ptr()->printsommet==&printsommetasoperator) || binary_op(u);
    if ( u!=at_sto && u.ptr()->printsommet!=NULL && !binaryop ){
      gen tmp=string2gen(g.print(contextptr),false);
      return Equation_compute_size(symbolic(at_expr,makesequence(tmp,xcas_mode(contextptr))),a,windowhsize,contextptr);
    }
    vecteur v;
    if (!binaryop || arg.type!=_VECT)
      v=Equation_subsizes(arg,a,windowhsize,contextptr);
    else
      v=Equation_subsizes(gen(*arg._VECTptr,_SEQ__VECT),a,windowhsize,contextptr);
    iterateur it=v.begin(),itend=v.end();
    if ( it==itend || (itend-it==1) ){ 
      gen gtmp;
      if (it==itend)
	gtmp=Equation_compute_size(gen(vecteur(0),_SEQ__VECT),a,windowhsize,contextptr);
      else
	gtmp=*it;
      // unary op, shift arg position horizontally
      eqwdata vv=Equation_total_size(gtmp);
      bool paren = u!=at_neg || (vv.g!=at_prod && need_parenthesis(vv.g)) ;
      x=ls+(paren?llp:0);
      gen tmp=gtmp; Equation_translate(tmp,x,0);
      x=x+vv.dx+(paren?lrp:0);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      return gen(makevecteur(tmp,eqwdata(x,h,0,y,a,u,0)),_EQW__VECT);
    }
    if (binaryop){ // op (default with par)
      int currenth=h,largeur=0;
      iterateur itprec=v.begin();
      h=0;
      if (u==at_plus){ // op without parenthesis
	if (it->type==_VECT && !it->_VECTptr->empty() && it->_VECTptr->back().type==_EQW && it->_VECTptr->back()._EQWptr->g==at_equal)
	  ;
	else {
	  llp=0;
	  lrp=0;
	}
      }
      for (;;){
	eqwdata vv=Equation_total_size(*it);
	if (need_parenthesis(vv.g))
	  x+=llp;
	if (u==at_plus && it!=v.begin() &&
	    ( 
	     (it->type==_VECT && it->_VECTptr->back().type==_EQW && it->_VECTptr->back()._EQWptr->g==at_neg) 
	     || 
	     ( it->type==_EQW && (is_integer(it->_EQWptr->g) || it->_EQWptr->g.type==_DOUBLE_) && is_strictly_positive(-it->_EQWptr->g,contextptr) ) 
	      ) 
	    )
	  x -= ls;
#if 0 //
	if (x>windowhsize-vv.dx && x>windowhsize/2 && (itend-it)*vv.dx>windowhsize/2){
	  largeur=max(x,largeur);
	  x=0;
	  if (need_parenthesis(vv.g))
	    x+=llp;
	  h+=currenth;
	  Equation_translate(*it,x,0);
	  for (iterateur kt=v.begin();kt!=itprec;++kt)
	    Equation_translate(*kt,0,currenth);
	  if (y){
	    for (iterateur kt=itprec;kt!=it;++kt)
	      Equation_translate(*kt,0,-y);
	  }
	  itprec=it;
	  currenth=vv.dy;
	  y=vv.y;
	}
	else
#endif
	  {
	    Equation_translate(*it,x,0);
	    vv=Equation_total_size(*it);
	    Equation_vertical_adjust(vv.dy,vv.y,currenth,y);
	  }
	x+=vv.dx;
	if (need_parenthesis(vv.g))
	  x+=lrp;
	++it;
	if (it==itend){
	  for (iterateur kt=v.begin();kt!=itprec;++kt)
	    Equation_translate(*kt,0,currenth+y);
	  h+=currenth;
	  v.push_back(eqwdata(max(x,largeur),h,0,y,a,u,0));
	  //cout << v << endl;
	  return gen(v,_SEQ__VECT);
	}
	x += ls+3;
      } 
    }
    // normal printing
    x=ls+llp;
    for (;;){
      eqwdata vv=Equation_total_size(*it);
      Equation_translate(*it,x,0);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      x+=vv.dx;
      ++it;
      if (it==itend){
	x+=lrp;
	v.push_back(eqwdata(x,h,0,y,a,u,0));
	return gen(v,_SEQ__VECT);
      }
      x+=lc;
    }
  }

  // windowhsize is used for g of type HIST__VECT (history) right justify answers
  // Returns either a eqwdata type object (terminal) or a vector 
  // (of subtype _EQW__VECT or _HIST__VECT)
  gen Equation_compute_size(const gen & g,const attributs & a,int windowhsize,GIAC_CONTEXT){
    /*****************
     *   FRACTIONS   *
     *****************/
    if (g.type==_FRAC){
      if (is_integer(g._FRACptr->num) && is_positive(-g._FRACptr->num,contextptr))
	return Equation_compute_size(symb_neg(fraction(-g._FRACptr->num,g._FRACptr->den)),a,windowhsize,contextptr);
      gen v1=Equation_compute_size(g._FRACptr->num,a,windowhsize,contextptr);
      eqwdata vv1=Equation_total_size(v1);
      gen v2=Equation_compute_size(g._FRACptr->den,a,windowhsize,contextptr);
      eqwdata vv2=Equation_total_size(v2);
      // Center the fraction
      int w1=vv1.dx,w2=vv2.dx;
      int w=max(w1,w2)+6;
      vecteur v(3);
      v[0]=v1; Equation_translate(v[0],(w-w1)/2,11-vv1.y);
      v[1]=v2; Equation_translate(v[1],(w-w2)/2,5-vv2.dy-vv2.y);
      v[2]=eqwdata(w,a.fontsize/2+vv1.dy+vv2.dy+1,0,(a.fontsize<=14?4:3)-vv2.dy,a,at_division,0);
      return gen(v,_SEQ__VECT);
    }
    /***************
     *   VECTORS   *
     ***************/
    if ( (g.type==_VECT) && !g._VECTptr->empty() ){
      vecteur v;
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      int x=0,y=0,h=a.fontsize; 
      /***************
       *   MATRICE   *
       ***************/
      bool gmat=ckmatrix(g);
      vector<int> V; int p=0;
      if (!gmat && is_mod_vecteur(*g._VECTptr,V,p) && p!=0){
	gen gm=makemodquoted(unmod(g),p);
	return Equation_compute_size(gm,a,windowhsize,contextptr);
      }
      vector< vector<int> > M; 
      if (gmat && is_mod_matrice(*g._VECTptr,M,p) && p!=0){
	gen gm=makemodquoted(unmod(g),p);
	return Equation_compute_size(gm,a,windowhsize,contextptr);
      }
      if (gmat && g.subtype!=_SEQ__VECT && g.subtype!=_SET__VECT && g.subtype!=_POLY1__VECT && g._VECTptr->front().subtype!=_SEQ__VECT){
	gen mkvect(at_makevector);
	mkvect.subtype=_SEQ__VECT;
	gen mkmat(at_makevector);
	mkmat.subtype=_MATRIX__VECT;
	int nrows,ncols;
	mdims(*g._VECTptr,nrows,ncols);
	if (ncols){
	  vecteur all_sizes;
	  all_sizes.reserve(nrows);
	  vector<int> row_heights(nrows),row_bases(nrows),col_widths(ncols);
	  // vertical gluing
	  for (int i=0;it!=itend;++it,++i){
	    gen tmpg=*it;
	    tmpg.subtype=_SEQ__VECT;
	    vecteur tmp(Equation_subsizes(tmpg,a,max(windowhsize/ncols-a.fontsize,230),contextptr));
	    int h=a.fontsize,y=0;
	    const_iterateur jt=tmp.begin(),jtend=tmp.end();
	    for (int j=0;jt!=jtend;++jt,++j){
	      eqwdata w(Equation_total_size(*jt));
	      Equation_vertical_adjust(w.dy,w.y,h,y);
	      col_widths[j]=max(col_widths[j],w.dx);
	    }
	    if (i)
	      row_heights[i]=row_heights[i-1]+h+a.fontsize/2;
	    else
	      row_heights[i]=h;
	    row_bases[i]=y;
	    all_sizes.push_back(tmp);
	  }
	  // accumulate col widths
	  col_widths.front() +=(3*a.fontsize)/2;
	  vector<int>::iterator iit=col_widths.begin()+1,iitend=col_widths.end();
	  for (;iit!=iitend;++iit)
	    *iit += *(iit-1)+a.fontsize;
	  // translate each cell
	  it=all_sizes.begin();
	  itend=all_sizes.end();
	  int h,y,prev_h=0;
	  for (int i=0;it!=itend;++it,++i){
	    h=row_heights[i];
	    y=row_bases[i];
	    iterateur jt=it->_VECTptr->begin(),jtend=it->_VECTptr->end();
	    for (int j=0;jt!=jtend;++jt,++j){
	      eqwdata w(Equation_total_size(*jt));
	      if (j)
		Equation_translate(*jt,col_widths[j-1]-w.x,-h-y);
	      else
		Equation_translate(*jt,-w.x+a.fontsize/2,-h-y);
	    }
	    it->_VECTptr->push_back(eqwdata(col_widths.back(),h-prev_h,0,-h,a,mkvect,0));
	    prev_h=h;
	  }
	  all_sizes.push_back(eqwdata(col_widths.back(),row_heights.back(),0,-row_heights.back(),a,mkmat,-row_heights.back()/2));
	  gen all_sizesg=all_sizes; Equation_translate(all_sizesg,0,row_heights.back()/2); return all_sizesg;
	}
      } // end matrices
      /*************************
       *   SEQUENCES/VECTORS   *
       *************************/
      // horizontal gluing
      if (g.subtype!=_PRINT__VECT) x += a.fontsize/2;
      int ncols=itend-it;
      //ncols=min(ncols,5);
      for (;it!=itend;++it){
	gen cur_size=Equation_compute_size(*it,a,
					   max(windowhsize/ncols-a.fontsize,
#ifdef IPAQ
					       200
#else
					       480
#endif
					       ),contextptr);
	eqwdata tmp=Equation_total_size(cur_size);
	Equation_translate(cur_size,x-tmp.x,0); v.push_back(cur_size);
	x=x+tmp.dx+((g.subtype==_PRINT__VECT)?2:a.fontsize);
	Equation_vertical_adjust(tmp.dy,tmp.y,h,y);
      }
      gen mkvect(at_makevector);
      if (g.subtype==_SEQ__VECT)
	mkvect=at_makesuite;
      else
	mkvect.subtype=g.subtype;
      v.push_back(eqwdata(x,h,0,y,a,mkvect,0));
      return gen(v,_EQW__VECT);
    } // end sequences
    if (g.type==_MOD){ 
      int x=0;
      int h=a.fontsize;
      int y=0;
      bool py=python_compat(contextptr);
      int modsize=int(text_width(a.fontsize,(py?" mod":"%")))+4;
      bool paren=is_positive(-*g._MODptr,contextptr);
      int llp=int(text_width(a.fontsize,("(")));
      int lrp=int(text_width(a.fontsize,(")")));
      gen varg1=Equation_compute_size(*g._MODptr,a,windowhsize,contextptr);
      if (paren) Equation_translate(varg1,llp,0);
      eqwdata vv=Equation_total_size(varg1);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      gen arg2=*(g._MODptr+1);
      gen varg2=Equation_compute_size(arg2,a,windowhsize,contextptr);
      if (paren)
	Equation_translate(varg2,vv.dx+modsize+lrp,0);
      else
	Equation_translate(varg2,vv.dx+modsize,0);
      vv=Equation_total_size(varg2);
      Equation_vertical_adjust(vv.dy,vv.y,h,y);
      vecteur res(makevecteur(varg1,varg2));
      res.push_back(eqwdata(vv.dx+vv.x,h,0,y,a,at_normalmod,0));
      return gen(res,_SEQ__VECT);
    }
    if (g.type!=_SYMB){
      string s=g.type==_STRNG?*g._STRNGptr:g.print(contextptr);
      //if (g==cst_pi) s=char(129);
      if (s.size()>2000)
	s=s.substr(0,2000)+"...";
      int i=int(text_width(a.fontsize,(s.c_str())));
      gen tmp=eqwdata(i,a.fontsize,0,0,a,g);
      return tmp;
    }
    /**********************
     *  SYMBOLIC HANDLING *
     **********************/
    return Equation_compute_symb_size(g,a,windowhsize,contextptr);
    // return Equation_compute_symb_size(aplatir_fois_plus(g),a,windowhsize,contextptr);
    // aplatir_fois_plus is a problem for Equation_replace_selection
    // because it will modify the structure of the data
  }

  void Equation_draw(const eqwdata & e,int x,int y,int rightx,int lowery,Equation * eq){
    if ( (e.dx+e.x<x) || (e.x>rightx) || (e.y>y) || e.y+e.dy<lowery)
      ; // return; // nothing to draw, out of window
    gen gg=e.g;
    int fontsize=e.eqw_attributs.fontsize;
    int text_color=COLOR_BLACK;
    int background=COLOR_WHITE;
    string s=gg.type==_STRNG?*gg._STRNGptr:gg.print(contextptr);
    if (gg.type==_IDNT && !s.empty() && s[0]=='_')
      s=s.substr(1,s.size()-1);
    // if (gg==cst_pi){      s="p";      s[0]=(unsigned char)129;    }
    if (s.size()>2000)
      s=s.substr(0,2000)+"...";
    // cerr << s.size() << endl;
    text_print(fontsize,s.c_str(),eq->x()+e.x-x,eq->y()+y-e.y,text_color,background,e.selected?4:0);
    return;
  }

  inline void check_fl_rectf(int x,int y,int w,int h,int imin,int jmin,int di,int dj,int delta_i,int delta_j,int c){
    drawRectangle(x+delta_i,y+delta_j,w,h,c);
    //fl_rectf(x+delta_i,y+delta_j,w,h,c);
  }

  void Equation_draw(const gen & g,int x,int y,int rightx,int lowery,Equation * equat){
    int eqx=equat->x(),eqy=equat->y();
    if (g.type==_EQW){ // terminal
      eqwdata & e=*g._EQWptr;
      Equation_draw(e,x,y,rightx,lowery,equat);
    }
    if (g.type!=_VECT)
      return;
    vecteur & v=*g._VECTptr;
    if (v.empty())
      return;
    gen tmp=v.back();
    if (tmp.type!=_EQW){
      cout << "EQW error:" << v << endl;
      return;
    }
    eqwdata & w=*tmp._EQWptr;
    if ( (w.dx+w.x-x<0) || (w.x>rightx) || (w.y>y) || (w.y+w.dy<lowery) )
      ; // return; // nothing to draw, out of window
    /*******************
     * draw the vector *
     *******************/
    // v is the vector, w the master operator eqwdata
    gen oper=w.g; 
    bool selected=w.selected ;
    int fontsize=w.eqw_attributs.fontsize;
    int background=w.eqw_attributs.background;
    int text_color=w.eqw_attributs.text_color;
    int mode=selected?4:0;
    int draw_line_color=selected?background:text_color;
    int x0=w.x;
    int y0=w.y; // lower coordinate of the master vector
    int y1=y0+w.dy; // upper coordinate of the master vector
    if (selected)
      drawRectangle(eqx+w.x-x,eqy+y-w.y-w.dy+1,w.dx,w.dy+1,text_color);
    // draw arguments of v
    const_iterateur it=v.begin(),itend=v.end()-1;
    if (oper==at_expr && v.size()==3){
      Equation_draw(*it,x,y,rightx,lowery,equat);
      return;
    }
    for (;it!=itend;++it)
      Equation_draw(*it,x,y,rightx,lowery,equat);
    if (oper==at_multistring)
      return;
    string s;
    if (oper.type==_FUNC){
      // catch here special cases user function, vect/matr, ^, int, sqrt, etc.
      unary_function_ptr & u=*oper._FUNCptr;
      if (u==at_at){ // draw brackets around 2nd arg
	gen arg2=v[1]; // 2nd arg of at_of, i.e. what's inside the parenth.
	eqwdata varg2=Equation_total_size(arg2);
	x0=varg2.x;
	y0=varg2.y;
	y1=y0+varg2.dy;
	fontsize=varg2.eqw_attributs.fontsize;
	if (x0<rightx)
	  text_print(fontsize,"[",eqx+x0-x-int(text_width(fontsize,("["))),eqy+y-varg2.baseline,text_color,background,mode);
	x0 += varg2.dx ;
	if (x0<rightx)
	  text_print(fontsize,"]",eqx+x0-x,eqy+y-varg2.baseline,text_color,background,mode);
	return;
      }
      if (u==at_of){ // do we need to draw some parenthesis?
	gen arg2=v[1]; // 2nd arg of at_of, i.e. what's inside the parenth.
	if (arg2.type!=_VECT || arg2._VECTptr->back().type !=_EQW || arg2._VECTptr->back()._EQWptr->g!=at_makesuite){ // Yes (if not _EQW it's a sequence with parent)
	  eqwdata varg2=Equation_total_size(arg2);
	  x0=varg2.x;
	  y0=varg2.y;
	  y1=y0+varg2.dy;
	  fontsize=varg2.eqw_attributs.fontsize;
	  int pfontsize=max(fontsize,(fontsize+(varg2.baseline-varg2.y))/2);
	  if (x0<rightx)
	    text_print(pfontsize,"(",eqx+x0-x-int(text_width(fontsize,("("))),eqy+y-varg2.baseline,text_color,background,mode);
	  x0 += varg2.dx ;
	  if (x0<rightx)
	    text_print(pfontsize,")",eqx+x0-x,eqy+y-varg2.baseline,text_color,background,mode);
	}
	return;
      }
      if (u==at_makesuite){
	bool paren=v.size()!=2; // Sequences with 1 arg don't show parenthesis
	int pfontsize=max(fontsize,(fontsize+(w.baseline-w.y))/2);
	if (paren && x0<rightx)
	  text_print(pfontsize,"(",eqx+x0-x-int(text_width(fontsize,("(")))/2,eqy+y-w.baseline,text_color,background,mode);
	x0 += w.dx;
	if (paren && x0<rightx)
	  text_print(pfontsize,")",eqx+x0-x-int(text_width(fontsize,("(")))/2,eqy+y-w.baseline,text_color,background,mode);
	// print commas between args
	it=v.begin(),itend=v.end()-2;
	for (;it!=itend;++it){
	  eqwdata varg2=Equation_total_size(*it);
	  fontsize=varg2.eqw_attributs.fontsize;
	  if (varg2.x+varg2.dx<rightx)
	    text_print(fontsize,",",eqx+varg2.x+varg2.dx-x+1,eqy+y-varg2.baseline,text_color,background,mode);
	}
	return;
      }
      if (u==at_makevector){ // draw [] delimiters for vector/matrices
	if (oper.subtype!=_SEQ__VECT && oper.subtype!=_PRINT__VECT){
	  int decal=1;
	  switch (oper.subtype){
	  case _MATRIX__VECT: decal=2; break;
	  case _SET__VECT: decal=4; break;
	  case _POLY1__VECT: decal=6; break;
	  }
	  if (eqx+x0-x+1>=0){
	    draw_line(eqx+x0-x+1,eqy+y-y0+1,eqx+x0-x+1,eqy+y-y1+1,draw_line_color);
	    draw_line(eqx+x0-x+decal,eqy+y-y0+1,eqx+x0-x+decal,eqy+y-y1+1,draw_line_color);
	    draw_line(eqx+x0-x+1,eqy+y-y0+1,eqx+x0-x+fontsize/4,eqy+y-y0+1,draw_line_color);
	    draw_line(eqx+x0-x+1,eqy+y-y1+1,eqx+x0-x+fontsize/4,eqy+y-y1+1,draw_line_color);
	  }
	  x0 += w.dx ;
	  if (eqx+x0-x-1<LCD_WIDTH_PX){
	    draw_line(eqx+x0-x-1,eqy+y-y0+1,eqx+x0-x-1,eqy+y-y1+1,draw_line_color);
	    draw_line(eqx+x0-x-decal,eqy+y-y0+1,eqx+x0-x-decal,eqy+y-y1+1,draw_line_color);
	    draw_line(eqx+x0-x-1,eqy+y-y0+1,eqx+x0-x-fontsize/4,eqy+y-y0+1,draw_line_color);
	    draw_line(eqx+x0-x-1,eqy+y-y1+1,eqx+x0-x-fontsize/4,eqy+y-y1+1,draw_line_color);
	  }
	} // end if oper.subtype!=SEQ__VECT
	if (oper.subtype!=_MATRIX__VECT && oper.subtype!=_PRINT__VECT){
	  // print commas between args
	  it=v.begin(),itend=v.end()-2;
	  for (;it!=itend;++it){
	    eqwdata varg2=Equation_total_size(*it);
	    fontsize=varg2.eqw_attributs.fontsize;
	    if (varg2.x+varg2.dx<rightx)
	      text_print(fontsize,",",eqx+varg2.x+varg2.dx-x+1,eqy+y-varg2.baseline,text_color,background,mode);
	  }
	}
	return;
      }
      int lpsize=int(text_width(fontsize,("(")));
      int rpsize=int(text_width(fontsize,(")")));
      eqwdata tmp=Equation_total_size(v.front()); // tmp= 1st arg eqwdata
      if (u==at_sto)
	tmp=Equation_total_size(v[1]);
      x0=w.x-x;
      y0=y-w.baseline;
      if (u==at_pow){
	if (!need_parenthesis(tmp.g)&& tmp.g!=at_pow && tmp.g!=at_prod && tmp.g!=at_division)
	  return;
	if (tmp.g==at_pow){
	  fontsize=tmp.eqw_attributs.fontsize+2;
	}
	if (tmp.x-lpsize<rightx)
	  text_print(fontsize,"(",eqx+tmp.x-x-lpsize,eqy+y-tmp.baseline,text_color,background,mode);
	if (tmp.x+tmp.dx<rightx)
	  text_print(fontsize,")",eqx+tmp.x+tmp.dx-x,eqy+y-tmp.baseline,text_color,background,mode);
	return;
      }
      if (u==at_program){
	if (tmp.x+tmp.dx<rightx)
	  text_print(fontsize,"->",eqx+tmp.x+tmp.dx-x,eqy+y-tmp.baseline,text_color,background,mode);
	return;
      }
#if 1
      if (u==at_sum){
	if (x0<rightx){
	  draw_line(eqx+x0,eqy+y0,eqx+x0+(2*fontsize)/3,eqy+y0,draw_line_color);
	  draw_line(eqx+x0,eqy+y0-fontsize,eqx+x0+(2*fontsize)/3,eqy+y0-fontsize,draw_line_color);
	  draw_line(eqx+x0,eqy+y0,eqx+x0+fontsize/2,eqy+y0-fontsize/2,draw_line_color);
	  draw_line(eqx+x0+fontsize/2,eqy+y0-fontsize/2,eqx+x0,eqy+y0-fontsize,draw_line_color);
	  if (v.size()>2){ // draw the =
	    eqwdata ptmp=Equation_total_size(v[1]);
	    if (ptmp.x+ptmp.dx<rightx)
	      text_print(fontsize,"=",eqx+ptmp.x+ptmp.dx-x-2,eqy+y-ptmp.baseline,text_color,background,mode);
	  }
	}
	return;
      }
#endif
      if (u==at_abs){
	y0 =1+y-w.y;
	int h=w.dy;
	if (x0<rightx){
	  draw_line(eqx+x0+2,eqy+y0-1,eqx+x0+2,eqy+y0-h+3,draw_line_color);
	  draw_line(eqx+x0+1,eqy+y0-1,eqx+x0+1,eqy+y0-h+3,draw_line_color);
	  draw_line(eqx+x0+w.dx-1,eqy+y0-1,eqx+x0+w.dx-1,eqy+y0-h+3,draw_line_color);
	  draw_line(eqx+x0+w.dx,eqy+y0-1,eqx+x0+w.dx,eqy+y0-h+3,draw_line_color);
	}
	return;
      }
      if (u==at_sqrt){
	y0 =1+y-w.y;
	int h=w.dy;
	if (x0<rightx){
	  draw_line(eqx+x0+2,eqy+y0-h/2,eqx+x0+fontsize/2,eqy+y0-1,draw_line_color);
	  draw_line(eqx+x0+fontsize/2,eqy+y0-1,eqx+x0+fontsize,eqy+y0-h+3,draw_line_color);
	  draw_line(eqx+x0+fontsize,eqy+y0-h+3,eqx+x0+w.dx-1,eqy+y0-h+3,draw_line_color);
	  ++y0;
	  draw_line(eqx+x0+2,eqy+y0-h/2,eqx+x0+fontsize/2,eqy+y0-1,draw_line_color);
	  draw_line(eqx+x0+fontsize/2,eqy+y0-1,eqx+x0+fontsize,eqy+y0-h+3,draw_line_color);
	  draw_line(eqx+x0+fontsize,eqy+y0-h+3,eqx+x0+w.dx-1,eqy+y0-h+3,draw_line_color);
	}
	return;
      }
      if (u==at_factorial){
	text_print(fontsize,"!",eqx+w.x+w.dx-4-x,eqy+y-w.baseline,text_color,background,mode);
	if (!need_parenthesis(tmp.g)
	    && tmp.g!=at_pow && tmp.g!=at_prod && tmp.g!=at_division
	    )
	  return;
	if (tmp.x-lpsize<rightx)
	  text_print(fontsize,"(",eqx+tmp.x-x-lpsize,eqy+y-tmp.baseline,text_color,background,mode);
	if (tmp.x+tmp.dx<rightx)
	  text_print(fontsize,")",eqx+tmp.x+tmp.dx-x,eqy+y-tmp.baseline,text_color,background,mode);
	return;
      }
#if 1
      if (u==at_integrate){
	x0+=2;
	y0+=fontsize/2;
	if (x0<rightx){
	  fl_arc(eqx+x0,eqy+y0,fontsize/3,fontsize/3,180,360,draw_line_color);
	  draw_line(eqx+x0+fontsize/3,eqy+y0,eqx+x0+fontsize/3,eqy+y0-2*fontsize+4,draw_line_color);
	  fl_arc(eqx+x0+fontsize/3,eqy+y0-2*fontsize+3,fontsize/3,fontsize/3,0,180,draw_line_color);
	}
	if (v.size()!=2){ // if arg has size > 1 draw the d
	  eqwdata ptmp=Equation_total_size(v[1]);
	  if (ptmp.x<rightx)
	    text_print(fontsize," d",eqx+ptmp.x-x-int(text_width(fontsize,(" d"))),eqy+y-ptmp.baseline,text_color,background,mode);
	}
	else {
	  eqwdata ptmp=Equation_total_size(v[0]);
	  if (ptmp.x+ptmp.dx<rightx)
	    text_print(fontsize," dx",eqx+ptmp.x+ptmp.dx-x,eqy+y-ptmp.baseline,text_color,background,mode);
	}
	return;
      }
#endif
      if (u==at_division){
	if (x0<rightx){
	  int yy=eqy+y0-8;
	  draw_line(eqx+x0+2,yy,eqx+x0+w.dx-2,yy,draw_line_color);
	  ++yy;
	  draw_line(eqx+x0+2,yy,eqx+x0+w.dx-2,yy,draw_line_color);
	}
	return;
      }
#if 1
      if (u==at_limit && v.size()>=4){
	if (x0<rightx)
	  text_print(fontsize,"lim",eqx+w.x-x,eqy+y-w.baseline,text_color,background,mode);
	gen arg2=v[1]; // 2nd arg of limit, i.e. the variable
	if (arg2.type==_EQW){ 
	  eqwdata & varg2=*arg2._EQWptr;
	  if (varg2.x+varg2.dx+2<rightx)
	    text_print(fontsize,"\x1e",eqx+varg2.x+varg2.dx+2-x,eqy+y-varg2.y,text_color,background,mode);
	}
	if (v.size()>=5){
	  arg2=v[2]; // 3rd arg of lim, the point, draw a comma after if dir.
	  if (arg2.type==_EQW){ 
	    eqwdata & varg2=*arg2._EQWptr;
	    if (varg2.x+varg2.dx<rightx)
	      text_print(fontsize,",",eqx+varg2.x+varg2.dx-x,eqy+y-varg2.baseline,text_color,background,mode);
	  }
	}
	return;
      } // limit
#endif
      bool parenthesis=true;
      string opstring(",");
      if (u.ptr()->printsommet==&printsommetasoperator || binary_op(u) ){
	if (u==at_normalmod && python_compat(contextptr))
	  opstring=" mod";
	else
	  opstring=u.ptr()->s;
      }
      else {
	if (u==at_sto)
	  opstring=":=";
	parenthesis=false;
      }
      // int yy=y0; // y0 is the lower coordinate of the whole eqwdata
      // int opsize=int(text_width(fontsize,(opstring.c_str())))+3;
      it=v.begin();
      itend=v.end()-1;
      // Reminder: here tmp is the 1st arg eqwdata, w the whole eqwdata
      if ( (itend-it==1) && ( (u==at_neg) 
			      || (u==at_plus) // uncommented for +infinity
			      ) ){ 
	if ( (u==at_neg &&need_parenthesis(tmp.g) && tmp.g!=at_prod)){
	  if (tmp.x-lpsize<rightx)
	    text_print(fontsize,"(",eqx+tmp.x-x-lpsize,eqy+y-tmp.baseline,text_color,background,mode);
	  if (tmp.x+tmp.dx<rightx)
	    text_print(fontsize,")",eqx+tmp.x-x+tmp.dx,eqy+y-tmp.baseline,text_color,background,mode);
	}
	if (w.x<rightx){
	  text_print(fontsize,u.ptr()->s,eqx+w.x-x,eqy+y-w.baseline,text_color,background,mode);
	}
	return;
      }
      // write first open parenthesis
      if (u==at_plus && tmp.g!=at_equal)
	parenthesis=false;
      else {
	if (parenthesis && need_parenthesis(tmp.g)){
	  if (w.x<rightx){
	    int pfontsize=max(fontsize,(fontsize+(tmp.baseline-tmp.y))/2);
	    text_print(pfontsize,"(",eqx+w.x-x,eqy+y-tmp.baseline,text_color,background,mode);
	  }
	}
      }
      for (;;){
	// write close parenthesis at end
	int xx=tmp.dx+tmp.x-x;
	if (parenthesis && need_parenthesis(tmp.g)){
	  if (xx<rightx){
	    int pfontsize=min(max(fontsize,(fontsize+(tmp.baseline-tmp.y))/2),fontsize*2);
	    int deltapary=(2*(pfontsize-fontsize))/3;
	    text_print(pfontsize,")",eqx+xx,eqy+y-tmp.baseline+deltapary,text_color,background,mode);
	  }
	  xx +=rpsize;
	}
	++it;
	if (it==itend){
	  if (u.ptr()->printsommet==&printsommetasoperator || u==at_sto || binary_op(u))
	    return;
	  else
	    break;
	}
	// write operator
	if (u==at_prod){
	  // text_print(fontsize,".",eqx+xx+3,eqy+y-tmp.baseline-fontsize/3);
	  text_print(fontsize,opstring.c_str(),eqx+xx+1,eqy+y-tmp.baseline,text_color,background,mode);
	}
	else {
	  gen tmpgen;
	  if (u==at_plus && ( 
			     (it->type==_VECT && it->_VECTptr->back().type==_EQW && it->_VECTptr->back()._EQWptr->g==at_neg) 
			     || 
			     ( it->type==_EQW && (is_integer(it->_EQWptr->g) || it->_EQWptr->g.type==_DOUBLE_) && is_strictly_positive(-it->_EQWptr->g,contextptr) ) 
			      )
	      )
	    ;
	  else {
	    if (xx+1<rightx)
	      // fl_draw(opstring.c_str(),xx+1,y-tmp.y-tmp.dy/2+fontsize/2);
	      text_print(fontsize,opstring.c_str(),eqx+xx+1,eqy+y-tmp.baseline,text_color,background,mode);
	  }
	}
	// write right parent, update tmp
	tmp=Equation_total_size(*it);
	if (parenthesis && (need_parenthesis(tmp.g)) ){
	  if (tmp.x-lpsize<rightx){
	    int pfontsize=min(max(fontsize,(fontsize+(tmp.baseline-tmp.y))/2),fontsize*2);
	    int deltapary=(2*(pfontsize-fontsize))/3;
	    text_print(pfontsize,"(",eqx+tmp.x-pfontsize*lpsize/fontsize-x,eqy+y-tmp.baseline+deltapary,text_color,background,mode);
	  }
	}
      } // end for (;;)
      if (w.x<rightx){
	s = u.ptr()->s;
	s += '(';
	text_print(fontsize,s.c_str(),eqx+w.x-x,eqy+y-w.baseline,text_color,background,mode);
      }
      if (w.x+w.dx-rpsize<rightx)
	text_print(fontsize,")",eqx+w.x+w.dx-x-rpsize+2,eqy+y-w.baseline,text_color,background,mode);
      return;
    }
    s=oper.print(contextptr);
    if (w.x<rightx){
      text_print(fontsize,s.c_str(),eqx+w.x-x,eqy+y-w.baseline,text_color,background,mode);
    }
  }

  Equation::Equation(int x_, int y_, const gen & g){
    _x=x_;
    _y=y_;
    attr=attributs(18,COLOR_WHITE,COLOR_BLACK);
    if (taille(g,max_prettyprint_equation)<max_prettyprint_equation)
      data=Equation_compute_size(g,attr,LCD_WIDTH_PX,contextptr);
    else
      data=Equation_compute_size(string2gen("Object_too_large",false),attr,LCD_WIDTH_PX,contextptr);
    undodata=Equation_copy(data);
  }

  void replace_selection(Equation & eq,const gen & tmp,gen * gsel,const vector<int> * gotoptr){
    int xleft,ytop,xright,ybottom,gselpos; gen *gselparent;
    vector<int> goto_sel;
    eq.undodata=Equation_copy(eq.data);
    if (gotoptr==0){
      if (xcas::Equation_adjust_xy(eq.data,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos,&goto_sel) && gsel)
	gotoptr=&goto_sel;
      else
	return;
    }
    *gsel=xcas::Equation_compute_size(tmp,eq.attr,LCD_WIDTH_PX,contextptr);
    gen value;
    xcas::do_select(eq.data,true,value);
    if (value.type==_EQW)
      eq.data=xcas::Equation_compute_size(value._EQWptr->g,eq.attr,LCD_WIDTH_PX,contextptr);
    //cout << "new value " << value << " " << eq.data << " " << *gotoptr << endl;
    xcas::Equation_select(eq.data,false);
    gen * gptr=&eq.data;
    for (int i=gotoptr->size()-1;i>=0;--i){
      int pos=(*gotoptr)[i];
      if (gptr->type==_VECT &&gptr->_VECTptr->size()>pos)
	gptr=&(*gptr->_VECTptr)[pos];
    }
    xcas::Equation_select(*gptr,true);
    //cout << "new sel " << *gptr << endl;
  }

  void display(Equation & eq,int x,int y){
    // Equation_draw(eq.data,x,y,LCD_WIDTH_PX,0,&eq);
    int xleft,ytop,xright,ybottom,gselpos; gen * gsel,*gselparent;
    eqwdata eqdata=Equation_total_size(eq.data);
    if ( (eqdata.dx>LCD_WIDTH_PX || eqdata.dy>LCD_HEIGHT_PX-STATUS_AREA_PX) && Equation_adjust_xy(eq.data,xleft,ytop,xright,ybottom,gsel,gselparent,gselpos)){
      if (x<xleft){
	if (x+LCD_WIDTH_PX<xright)
	  x=giac::giacmin(xleft,xright-LCD_WIDTH_PX);
      }
      if (x>=xleft && x+LCD_WIDTH_PX>=xright){
	if (xright-x<LCD_WIDTH_PX)
	  x=giac::giacmax(xright-LCD_WIDTH_PX,0);
      }
#if 0
      cout << "avant " << y << " " << ytop << " " << ybottom << endl;
      if (y<ytop){
	if (y+LCD_HEIGHT_PX<ybottom)
	  y=giac::giacmin(ytop,ybottom-LCD_HEIGHT_PX);
      }
      if (y>=ytop && y+LCD_HEIGHT_PX>=ybottom){
	if (ybottom-y<LCD_HEIGHT_PX)
	  y=giac::giacmax(ybottom-LCD_HEIGHT_PX,0);
      }
      cout << "apres " << y << " " << ytop << " " << ybottom << endl;
#endif
    }
    int save_ymin_clip=clip_ymin;
    clip_ymin=STATUS_AREA_PX;
    Equation_draw(eq.data,x,y,RAND_MAX,0,&eq);
    clip_ymin=save_ymin_clip;
  }

#endif // WITH_EQW

#ifdef WITH_PLOT
  /* ******************* *
   *      GRAPH          *
   * ******************* *
   */

  double find_tick(double dx){
    double d=std::pow(10.0,std::floor(std::log10(absdouble(dx))));
    if (dx<2*d)
      d=d/5;
    else {
      if (dx<5*d)
	d=d/2;
    }
    return d;
  }

  Graph2d::Graph2d(const giac::gen & g_):window_xmin(gnuplot_xmin),window_xmax(gnuplot_xmax),window_ymin(gnuplot_ymin),window_ymax(gnuplot_ymax),g(g_),display_mode(0x45),show_axes(1),show_names(1),labelsize(8) {
    tracemode=0; tracemode_n=0; tracemode_i=0;
    update();
    autoscale();
  }
  
  void Graph2d::zoomx(double d,bool round){
    double x_center=(window_xmin+window_xmax)/2;
    double dx=(window_xmax-window_xmin);
    if (dx==0)
      dx=gnuplot_xmax-gnuplot_xmin;
    dx *= d/2;
    x_tick = find_tick(dx);
    window_xmin = x_center - dx;
    if (round) 
      window_xmin=int( window_xmin/x_tick -1)*x_tick;
    window_xmax = x_center + dx;
    if (round)
      window_xmax=int( window_xmax/x_tick +1)*x_tick;
    update();
  }

  void Graph2d::zoomy(double d,bool round){
    double y_center=(window_ymin+window_ymax)/2;
    double dy=(window_ymax-window_ymin);
    if (dy==0)
      dy=gnuplot_ymax-gnuplot_ymin;
    dy *= d/2;
    y_tick = find_tick(dy);
    window_ymin = y_center - dy;
    if (round)
      window_ymin=int( window_ymin/y_tick -1)*y_tick;
    window_ymax = y_center + dy;
    if (round)
      window_ymax=int( window_ymax/y_tick +1)*y_tick;
    update();
  }

  void Graph2d::zoom(double d){ 
    zoomx(d);
    zoomy(d);
  }

  void Graph2d::autoscale(bool fullview){
    // Find the largest and lowest x/y/z in objects (except lines/plans)
    vector<double> vx,vy,vz;
    int s;
    bool ortho=autoscaleg(g,vx,vy,vz,contextptr);
    autoscaleminmax(vx,window_xmin,window_xmax,fullview);
    zoomx(1.0);
    autoscaleminmax(vy,window_ymin,window_ymax,fullview);
    zoomy(1.0);
    if (window_xmax-window_xmin<1e-20){
      window_xmax=gnuplot_xmax;
      window_xmin=gnuplot_xmin;
    }
    if (window_ymax-window_ymin<1e-20){
      window_ymax=gnuplot_ymax;
      window_ymin=gnuplot_ymin;
    }
    bool do_ortho=ortho;
    if (!do_ortho){
      double w=LCD_WIDTH_PX;
      double h=LCD_HEIGHT_PX-STATUS_AREA_PX;
      double window_w=window_xmax-window_xmin,window_h=window_ymax-window_ymin;
      double tst=h/w*window_w/window_h;
      if (tst>0.7 && tst<1.4)
	do_ortho=true;
    }
    if (do_ortho )
      orthonormalize();
    y_tick=find_tick(window_ymax-window_ymin);
    update();
  }

  void Graph2d::orthonormalize(){ 
    // Center of the directions, orthonormalize
    double w=LCD_WIDTH_PX;
    double h=LCD_HEIGHT_PX-STATUS_AREA_PX;
    double window_w=window_xmax-window_xmin,window_h=window_ymax-window_ymin;
    double window_hsize=h/w*window_w;
    if (window_h > window_hsize*1.01){ // enlarge horizontally
      double window_xcenter=(window_xmin+window_xmax)/2;
      double window_wsize=w/h*window_h;
      window_xmin=window_xcenter-window_wsize/2;
      window_xmax=window_xcenter+window_wsize/2;
    }
    if (window_h < window_hsize*0.99) { // enlarge vertically
      double window_ycenter=(window_ymin+window_ymax)/2;
      window_ymin=window_ycenter-window_hsize/2;
      window_ymax=window_ycenter+window_hsize/2;
    }
    x_tick=find_tick(window_xmax-window_xmin);
    y_tick=find_tick(window_ymax-window_ymin);
    update();
  }

  void Graph2d::update(){
    x_scale=LCD_WIDTH_PX/(window_xmax-window_xmin);    
    y_scale=(LCD_HEIGHT_PX-STATUS_AREA_PX)/(window_ymax-window_ymin);    
  }

  bool Graph2d::findij(const gen & e0,double x_scale,double y_scale,double & i0,double & j0,GIAC_CONTEXT) const {
    if (display_mode&0xc00==0 && e0.type==_CPLX && e0.subtype==3){
      i0=(e0._CPLXptr->_DOUBLE_val-window_xmin)*x_scale;
      j0=(window_ymax-(e0._CPLXptr+1)->_DOUBLE_val)*y_scale;
      return true;
    }
    gen e,f0,f1;
    evalfdouble2reim(e0,e,f0,f1,contextptr);
    if ((f0.type==_DOUBLE_) && (f1.type==_DOUBLE_)){
      if (display_mode & 0x400){
	if (f0._DOUBLE_val<=0)
	  return false;
	f0=std::log10(f0._DOUBLE_val);
      }
      i0=(f0._DOUBLE_val-window_xmin)*x_scale;
      if (display_mode & 0x800){
	if (f1._DOUBLE_val<=0)
	  return false;
	f1=std::log10(f1._DOUBLE_val);
      }
      j0=(window_ymax-f1._DOUBLE_val)*y_scale;
      return true;
    }
    // cerr << "Invalid drawing data" << endl;
    return false;
  }

  std::string printn(const gen & g,int n){
    if (g.type!=_DOUBLE_)
      return g.print();
    return giac::print_DOUBLE_(g._DOUBLE_val,n);
  }

  gen * x0ptr(){
    static gen * ptr=0;
    if (!ptr){
      ptr=new gen;
      *ptr=gen("x0",contextptr);
    }
    return ptr;
  }
  gen * x1ptr(){
    static gen * ptr=0;
    if (!ptr){
      ptr=new gen;
      *ptr=gen("x1",contextptr);
    }
    return ptr;
  }
  gen * x2ptr(){
    static gen * ptr=0;
    if (!ptr){
      ptr=new gen;
      *ptr=gen("x2",contextptr);
    }
    return ptr;
  }
  gen * y0ptr(){
    static gen * ptr=0;
    if (!ptr){
      ptr=new gen;
      *ptr=gen("y0",contextptr);
    }
    return ptr;
  }
  gen * y1ptr(){
    static gen * ptr=0;
    if (!ptr){
      ptr=new gen;
      *ptr=gen("y1",contextptr);
    }
    return ptr;
  }
  gen * y2ptr(){
    static gen * ptr=0;
    if (!ptr){
      ptr=new gen;
      *ptr=gen("y2",contextptr);
    }
    return ptr;
  }
  
  gen * z0ptr(){
    static gen * ptr=0;
    if (!ptr){
      ptr=new gen;
      *ptr=gen("z0",contextptr);
    }
    return ptr;
  }
  
  void Graph2d::table(const gen & t,const gen & x,const gen & y,double tmin,double tstep){
    statuslinemsg("up/down/+/-/left/right");
    double t0=tmin,ts,tc=t0;
    ts=find_tick(tstep);
    t0=int(t0/ts)*ts;
    int ndisp=10,N=6,dy=5;
    for (;;){
      // table of values
      drawRectangle(0,STATUS_AREA_PX,LCD_WIDTH_PX,LCD_HEIGHT_PX-STATUS_AREA_PX,COLOR_WHITE);
      //statuslinemsg("esc: quit, up/down: move");
      // table of values
      if (t==x){
        os_draw_string(0,dy,COLOR_BLACK,COLOR_WHITE,"x");
        os_draw_string(120,dy,COLOR_BLACK,COLOR_WHITE,y.print().c_str());
      }
      else {
        os_draw_string(0,dy,COLOR_BLACK,COLOR_WHITE,"t");
        os_draw_string(107,dy,COLOR_BLACK,COLOR_WHITE,"x");
        os_draw_string(214,dy,COLOR_BLACK,COLOR_WHITE,"y");
      }
      vecteur V;
      for (int i=1;i<=ndisp;++i){
        double tcur=tc+(i-1)*ts;
        vecteur L(1,tcur);
        os_draw_string(0,dy+i*18,COLOR_BLACK,COLOR_WHITE,printn(tcur,N).c_str());
        if (t==x){
          gen cur=subst(y,t,tcur,false,contextptr);
          L.push_back(cur);
          os_draw_string(120,dy+i*18,COLOR_BLACK,COLOR_WHITE,printn(cur,N).c_str());
        }
        else {
          gen cur=subst(x,t,tcur,false,contextptr);
          L.push_back(cur);
          os_draw_string(107,dy+i*18,COLOR_BLACK,COLOR_WHITE,printn(cur,N).c_str());
          cur=subst(y,t,tcur,false,contextptr);
          L.push_back(cur);
          os_draw_string(214,dy+i*18,COLOR_BLACK,COLOR_WHITE,printn(cur,N).c_str());	      
        }
        V.push_back(L);
      }
      int key=getkey(1);
      if (key==KEY_CTRL_EXIT || key==KEY_CTRL_QUIT)
        break;
      if (key==KEY_CTRL_UP)
        tc -= (ndisp/2)*ts;
      if (key==KEY_CTRL_DOWN || key==KEY_CTRL_EXE)
        tc += (ndisp/2)*ts;
      if (key=='+')
        ts /= 2;
      if (key=='-')
        ts *= 2;
      if ( (key==KEY_CTRL_DEL || key==KEY_CTRL_RIGHT) && inputdouble("step",ts))
        ts=fabs(ts);
      if (key==KEY_CTRL_LEFT)
        inputdouble("min",tc);
      if (key==KEY_CTRL_CLIP)
        copy_clipboard(gen(V).print(contextptr),true);
    }
  }

  const int tracemaxdepth=9; // protection against too complex derivatives for curve study
  // protection against too complex derivatives for curve study
  int symb_depth(const gen & g,int curdepth,int maxdepth,bool sum=false){
    if (g.type==_VECT){
      vecteur & v =*g._VECTptr;
      int curmax=0;
      for (int i=0;i<v.size();++i){
        int cur=symb_depth(v[i],curdepth,maxdepth);
        if (cur>maxdepth)
          return curdepth;
        if (sum){
          if (cur>curmax)
            curmax=cur;
        }
        else
          curdepth=cur;
      }
      if (sum)
        curdepth=curmax;
    }
    if (g.type!=_SYMB)
      return curdepth;
    if (curdepth==maxdepth)
      return maxdepth+1;
    return symb_depth(g._SYMBptr->feuille,curdepth+1,maxdepth,g._SYMBptr->sommet==at_plus);
  }

  void Graph2d::tracemode_set(int operation){
    if (plot_instructions.empty())
      plot_instructions=gen2vecteur(g);
    if (is_zero(plot_instructions.back())) // workaround for 0 at end in geometry (?)
      plot_instructions.pop_back();
    gen sol(undef);
    if (operation==1 || operation==8){
      double d=tracemode_mark;
      if (!inputdouble(lang==1?"Valeur du parametre?":"Parameter value",d))
	return;
      if (operation==8)
	tracemode_mark=d;
      sol=d;
    }
    // handle curves with more than one connected component
    vecteur tracemode_v;
    for (int i=0;i<plot_instructions.size();++i){
      gen g=plot_instructions[i];
      if (g.type==_VECT && !g._VECTptr->empty() && g._VECTptr->front().is_symb_of_sommet(at_curve)){
	vecteur & v=*g._VECTptr;
	for (int j=0;j<v.size();++j)
	  tracemode_v.push_back(v[j]);
      }
      else
	tracemode_v.push_back(g);
    }
    gen G;
    if (tracemode_n<0)
      tracemode_n=tracemode_v.size()-1;
    bool retry=tracemode_n>0;
    for (;tracemode_n<tracemode_v.size();++tracemode_n){
      G=tracemode_v[tracemode_n];
      if (G.is_symb_of_sommet(at_pnt))
	break;
    }
    if (tracemode_n>=tracemode_v.size()){
      // retry
      if (retry){
	for (tracemode_n=0;tracemode_n<tracemode_v.size();++tracemode_n){
	  G=tracemode_v[tracemode_n];
	  if (G.is_symb_of_sommet(at_pnt))
	    break;
	}
      }
      if (tracemode_n>=tracemode_v.size()){
	tracemode=0;
	return;
      }
    }
    int p=python_compat(contextptr);
    python_compat(0,contextptr);
    gen G_orig(G);
    G=remove_at_pnt(G);
    tracemode_disp.clear();
    string curve_infos1,curve_infos2;
    gen parameq,x,y,t,tmin,tmax,tstep,x1,x2,y1,y2,z1,z2;
    // extract position at tracemode_i
    if (G.is_symb_of_sommet(at_curve)
        && symb_depth(G._SYMBptr->feuille[0][0],0,tracemaxdepth)<tracemaxdepth
        ){
      gen c=G._SYMBptr->feuille[0];
      parameq=c[0];
      t=c[1];
      if (parameq==trace_parameq && t==trace_t){
        x=trace_x0;
        x1=trace_x1;
        x2=trace_x2;
        y=trace_y0;
        y1=trace_y1;
        y2=trace_y2;
        z1=trace_z1;
        z2=trace_z2;
      }
      else {
        trace_t=t;
        trace_parameq=parameq;
        z1=trace_z1=derive(parameq,t,contextptr);
        z2=trace_z2=derive(z1,t,contextptr);
        // simple expand for i*ln(x)
        bool b=do_lnabs(contextptr);
        do_lnabs(false,contextptr);
        reim(parameq,x,y,contextptr);
        do_lnabs(b,contextptr);
        trace_x0=x;
        trace_x1=x1=derive(x,t,contextptr);
        trace_x2=x2=derive(x1,t,contextptr);
        trace_y0=y;
        trace_y1=y1=derive(y,t,contextptr);
        trace_y2=y2=derive(y1,t,contextptr);
        if (has_i(lop(lvar(parameq),at_exp))){
          // polar plot, keep only z0
          sto(parameq,*z0ptr(),contextptr);
          _purge(*x0ptr(),contextptr);
          _purge(*y0ptr(),contextptr);
          _purge(*x1ptr(),contextptr);
          _purge(*y1ptr(),contextptr);
          _purge(*x2ptr(),contextptr);
          _purge(*y2ptr(),contextptr);
        }
        else {
          _purge(*z0ptr(),contextptr);
          sto(x,*x0ptr(),contextptr);
          sto(x1,*x1ptr(),contextptr);
          sto(x2,*x2ptr(),contextptr);
          sto(y,*y0ptr(),contextptr);
          sto(y1,*y1ptr(),contextptr);
          sto(y2,*y2ptr(),contextptr);
        }
      }
      tmin=c[2];
      tmax=c[3];
      tmin=evalf_double(tmin,1,contextptr);
      tmax=evalf_double(tmax,1,contextptr);
      if (tmin._DOUBLE_val>tracemode_mark)
	tracemode_mark=tmin._DOUBLE_val;
      if (tmax._DOUBLE_val<tracemode_mark)
	tracemode_mark=tmax._DOUBLE_val;
      G=G._SYMBptr->feuille[1];
      if (G.type==_VECT){
	vecteur &Gv=*G._VECTptr;
	tstep=(tmax-tmin)/(Gv.size()-1);
      }
      double eps=1e-6; // epsilon(contextptr)
      double curt=(tmin+tracemode_i*tstep)._DOUBLE_val;
      if (abs(curt-tracemode_mark)<tstep._DOUBLE_val)
	curt=tracemode_mark;
      if (operation==-1){
	gen A,B,C,R; // detect ellipse/hyperbola
	if ( 0 &&
             (( x!=t && c.type==_VECT && c._VECTptr->size()>7
	      //&& centre_rayon(G_orig,C,R,false,contextptr,true)
                )
              ||
              is_quadratic_wrt(parameq,t,A,B,C,contextptr)
              )
             ){
	  if (C.type!=_VECT){ // x+i*y=A*t^2+B*t+C
	    curve_infos1="Parabola";
	    curve_infos2=_equation(G_orig,contextptr).print(contextptr);
	  }
	  else {
	    vecteur V(*C._VECTptr);
	    curve_infos1=V[0].print(contextptr);
	    curve_infos1=curve_infos1.substr(1,curve_infos1.size()-2);
	    curve_infos1+=" O=";
	    curve_infos1+=V[1].print(contextptr);
	    curve_infos1+=", F=";
	    curve_infos1+=V[2].print(contextptr);
	    // curve_infos1=change_subtype(C,_SEQ__VECT).print(contextptr);
	    curve_infos2=change_subtype(R,_SEQ__VECT).print(contextptr);
	  }
	}
	else {
          //dbg_printf("tracemode curve_info\n");
	  if (x==t) curve_infos1="Function "+y.print(contextptr); else curve_infos1="Parametric "+x.print(contextptr)+","+y.print(contextptr);
	  curve_infos2 = t.print(contextptr)+"="+tmin.print(contextptr)+".."+tmax.print(contextptr)+',';
	  curve_infos2 += (x==t?"xstep=":"tstep=")+tstep.print(contextptr);
	}
      }
      if (operation==1)
	curt=sol._DOUBLE_val;
      if (operation==7)
	sol=tracemode_mark=curt;
      if (operation==2){ // root near curt
	sol=newton(y,t,curt,NEWTON_DEFAULT_ITERATION,eps,1e-12,true,tmin._DOUBLE_val,tmax._DOUBLE_val,tmin._DOUBLE_val,tmax._DOUBLE_val,1,contextptr);
	if (sol.type==_DOUBLE_){
	  confirm(lang==1?"Racine en":"Root at",sol.print(contextptr).c_str());
	  sto(sol,gen("Zero",contextptr),contextptr);
	}
      }
      if (operation==4){ // horizontal tangent near curt
	sol=newton(y1,t,curt,NEWTON_DEFAULT_ITERATION,eps,1e-12,true,tmin._DOUBLE_val,tmax._DOUBLE_val,tmin._DOUBLE_val,tmax._DOUBLE_val,1,contextptr);
	if (sol.type==_DOUBLE_){
	  confirm(lang==1?"y'=0, extremum/pt singulier en":"y'=0, extremum/singular pt at",sol.print(contextptr).c_str());
	  sto(sol,gen("Extremum",contextptr),contextptr);
	}
      }
      if (operation==5){ // vertical tangent near curt
	if (x1==1)
	  do_confirm(lang==1?"Outil pour courbes parametriques!":"Tool for parametric curves!");
	else {
	  sol=newton(x1,t,curt,NEWTON_DEFAULT_ITERATION,eps,1e-12,true,tmin._DOUBLE_val,tmax._DOUBLE_val,tmin._DOUBLE_val,tmax._DOUBLE_val,1,contextptr);
	  if (sol.type==_DOUBLE_){
	    confirm("x'=0, vertical or singular",sol.print(contextptr).c_str());
	    sto(sol,gen("Vertical",contextptr),contextptr);
	  }
	}
      }
      if (operation==6){ // inflexion
	sol=newton(x1*y2-x2*y1,t,curt,NEWTON_DEFAULT_ITERATION,eps,1e-12,true,tmin._DOUBLE_val,tmax._DOUBLE_val,tmin._DOUBLE_val,tmax._DOUBLE_val,1,contextptr);
	if (sol.type==_DOUBLE_){
	  confirm("x'*y''-x''*y'=0",sol.print(contextptr).c_str());
	  sto(sol,gen("Inflexion",contextptr),contextptr);
	}
      }
#if 1
      if (operation==3 && x==t){ // intersect this curve with other curves
	for (int j=0;j<tracemode_v.size();++j){
	  if (j==tracemode_n)
	    continue;
	  gen H=remove_at_pnt(tracemode_v[j]),Hx,Hy;
	  if (H.is_symb_of_sommet(at_curve)){
	    H=H._SYMBptr->feuille[0];
	    H=H[0];
	    bool b=do_lnabs(contextptr);
	    do_lnabs(false,contextptr);
	    reim(H,Hx,Hy,contextptr);
	    do_lnabs(b,contextptr);
	    if (Hx==x){
	      //double curt=(tmin+tracemode_i*tstep)._DOUBLE_val,eps=1e-6;
	      gen cursol=newton(Hy-y,t,curt,NEWTON_DEFAULT_ITERATION,eps,1e-12,true,tmin._DOUBLE_val,tmax._DOUBLE_val,tmin._DOUBLE_val,tmax._DOUBLE_val,1,contextptr);
	      if (cursol.type==_DOUBLE_ && 
		  (is_undef(sol) || is_greater(sol-curt,cursol-curt,contextptr)) )
		sol=cursol;
	    }
	  }
	}
	if (sol.type==_DOUBLE_){
	  sto(sol,gen("Intersect",contextptr),contextptr);
	  tracemode_mark=sol._DOUBLE_val;
	}
      } // end intersect
#endif
      //dbg_printf("tracemode push M\n");
      gen M(put_attributs(_point(subst(parameq,t,tracemode_mark,false,contextptr),contextptr),vecteur(1,giac::_POINT_WIDTH_4 | COLOR_BLUE),contextptr));
      tracemode_disp.push_back(M);      
      //dbg_printf("tracemode M pushed\n");
      gen f;
      if (operation==9)
	f=y*derive(x,t,contextptr);
      if (operation==10){
	f=sqrt(pow(x1,2,contextptr)+pow(y1,2,contextptr),contextptr);
      }
      if (operation==9 || operation==10){
	double a=tracemode_mark,b=curt;
	if (a>b)
	  swapdouble(a,b);
	gen res=symbolic( 
#if 0
                         (operation==9 && x==t?at_plotarea:at_integrate),
#else
                         at_integrate,
#endif
			  makesequence(f,symb_equal(t,symb_interval(a,b))));
	if (operation==9)
	  tracemode_disp.push_back(giac::eval(res,1,contextptr));
	string ss=res.print(contextptr);
	if (!tegral(f,t,a,b,1e-6,1<<10,res,false,contextptr))
	  confirm("Numerical Integration Error",ss.c_str());
	else {
	  confirm(ss.c_str(),res.print(contextptr).c_str());
	  sto(res,gen((operation==9?"Area":"Arclength"),contextptr),contextptr);	  
	}
      }
      if (operation>=1 && operation<=8 && sol.type==_DOUBLE_ && !is_zero(tstep)){
	tracemode_i=(sol._DOUBLE_val-tmin._DOUBLE_val)/tstep._DOUBLE_val;
	G=subst(parameq,t,sol._DOUBLE_val,false,contextptr);
      }
    }
    if (G.is_symb_of_sommet(at_cercle)){
      if (operation==-1){
	gen c,r;
	centre_rayon(G,c,r,true,contextptr);
	curve_infos1="Circle radius "+r.print(contextptr);
	curve_infos2="Center "+_coordonnees(c,contextptr).print(contextptr);
      }
      G=G._SYMBptr->feuille[0];
    }
    if (G.type==_VECT){
      vecteur & v=*G._VECTptr;
      if (operation==-1 && curve_infos1.size()==0){
	if (v.size()==2)
	  curve_infos1=_equation(G_orig,contextptr).print(contextptr);
	else if (v.size()==4)
	  curve_infos1="Triangle";
	else curve_infos1="Polygon";
	curve_infos2=G.print(contextptr);
      }
      int i=std::floor(tracemode_i);
      double id=tracemode_i-i;
      if (i>=int(v.size()-1)){
	tracemode_i=i=v.size()-1;
	id=0;
      }
      if (i<0){
	tracemode_i=i=0;
	id=0;
      }
      G=v[i];
      if (!is_zero(tstep) && id>0)
	G=v[i]+id*tstep*(v[i+1]-v[i]);
    }
    G=evalf(G,1,contextptr);
    gen Gx,Gy; reim(G,Gx,Gy,contextptr);
    Gx=evalf_double(Gx,1,contextptr);
    Gy=evalf_double(Gy,1,contextptr);
    if (operation==-1){
      if (curve_infos1.size()==0)
	curve_infos1="Position "+Gx.print(contextptr)+","+Gy.print(contextptr);
      if (G_orig.is_symb_of_sommet(at_pnt)){
	gen f=G_orig._SYMBptr->feuille;
	if (f.type==_VECT && f._VECTptr->size()==3){
	  f=f._VECTptr->back();
	  curve_infos1 = f.print(contextptr)+": "+curve_infos1;
	}
      }
      if (confirm(curve_infos1.c_str(),curve_infos2.c_str())==KEY_CTRL_F1 && tstep!=0){
        table(t,x,y,tmin._DOUBLE_val,tstep._DOUBLE_val*5);
      }
      // confirm(curve_infos1.c_str(),curve_infos2.c_str());
    }
    tracemode_add="";
    if (Gx.type==_DOUBLE_ && Gy.type==_DOUBLE_){
      int prec=t==x?3:2;
      //dbg_printf("tracemode add 1\n");
      tracemode_add += "x="+print_DOUBLE_(Gx._DOUBLE_val,prec)+",y="+print_DOUBLE_(Gy._DOUBLE_val,prec);
      if (tstep!=0){
	gen curt=tmin+tracemode_i*tstep;
	if (curt.type==_DOUBLE_){
	  if (t!=x)
	    tracemode_add += ",t="+print_DOUBLE_(curt._DOUBLE_val,prec);
          if (tracemode){
            // make sure G is the right point, e.g. for plotpolar(sqrt(cos(2x)))
            G=subst(parameq,t,curt,false,contextptr);
          }
	  if (tracemode & 2){
	    //gen G1=derive(parameq,t,contextptr);
	    gen G1t=subst(z1,t,curt,false,contextptr);
	    gen G1x,G1y; reim(G1t,G1x,G1y,contextptr);
            // gen G1x=subst(x1,t,curt,false,contextptr),G1y=subst(y1,t,curt,false,contextptr);
	    gen m=evalf_double(G1y/G1x,1,contextptr);
	    if (m.type==_DOUBLE_)
	      tracemode_add += ",m="+giac::print_DOUBLE_(m._DOUBLE_val,prec);
	    gen T(_vector(makesequence(_point(G,contextptr),_point(G+G1t,contextptr)),contextptr));
	    tracemode_disp.push_back(T);
            if (tracemode & 0xc){
              //gen G2(derive(G1,t,contextptr));
              gen G2t=subst(z2,t,curt,false,contextptr);
              gen G2x,G2y; reim(G2t,G2x,G2y,contextptr);
              // gen G2x=subst(x2,t,curt,false,contextptr),G2y=subst(y2,t,curt,false,contextptr);
              //dbg_printf("tracemode G2x=%s G2y=%s\n",G2x.print().c_str(),G2y.print().c_str());
              //gen det(re(G1t*conj(G2t,contextptr),contextptr)); 
              gen det(G1x*G2y-G2x*G1y);
              gen Tn=abs(G1t,contextptr);
              gen R=evalf_double(Tn*Tn*Tn/det,1,contextptr);
              gen centre=G+R*cst_i*G1t/Tn;
              // gen centre=G+R*(-G1y+cst_i*G1x)/Tn;
              if (tracemode & 4){
                gen N(_vector(makesequence(_point(G,contextptr),_point(centre,contextptr)),contextptr));
                tracemode_disp.push_back(N);
              }
              if (tracemode & 8){
                if (R.type==_DOUBLE_)
                  tracemode_add += ",R="+giac::print_DOUBLE_(R._DOUBLE_val,prec);
                tracemode_disp.push_back(_cercle(makesequence(centre,R),contextptr));
              }
            }
          }
	}
      }
    }
    //dbg_printf("tracemode add end\n");
    double x_scale=LCD_WIDTH_PX/(window_xmax-window_xmin);
    double y_scale=(LCD_HEIGHT_PX-STATUS_AREA_PX)/(window_ymax-window_ymin);
    double i,j;
    findij(G,x_scale,y_scale,i,j,contextptr);
    current_i=int(i+.5);
    current_j=int(j+.5);
    python_compat(p,contextptr);
  }

  void Graph2d::invert_tracemode(){
    if (!tracemode)
      init_tracemode();
    else
      tracemode=0;
  }

  void Graph2d::init_tracemode(){
    tracemode_mark=0.0;
    double w=LCD_WIDTH_PX;
    double h=LCD_HEIGHT_PX-STATUS_AREA_PX;
    double window_w=window_xmax-window_xmin,window_h=window_ymax-window_ymin;
    double r=h/w*window_w/window_h;
    tracemode=1;//(r>0.7 && r<1.4)?7:3;
    tracemode_set();
  }

  void Graph2d::curve_infos(){
    if (!tracemode)
      init_tracemode();
    const char *
      tab[]={
	     lang==1?"Infos objet (F2)":"Object infos (F2)",  // 0
	     lang==1?"Quitte mode etude (xtt)":"Quit study mode (xtt)",
	     lang==1?"Entrer t ou x":"Set t or x", // 1
	     lang==1?"y=0, racine":"y=0, root",
	     "Intersection", // 3
	     "y'=0, extremum",
	     lang==1?"x'=0 (parametriques)":"x'=0 (parametric)", // 5
	     "Inflexion",
	     lang==1?"Marquer la position":"Mark position",
	     lang==1?"Entrer t ou x, marquer":"Set t or x, mark", // 8
	     lang==1?"Aire":"Area",
	     lang==1?"Longueur d'arc":"Arc length", // 10
	     0};
    const int s=sizeof(tab)/sizeof(char *);
    int choix=select_item(tab,lang==1?"Etude courbes":"Curve study",true);
    if (choix<0 || choix>s)
      return;
    if (choix==1)
      tracemode=0;
    else 
      tracemode_set(choix-1);
  }

  inline void swapint(int & i0,int & i1){
    int tmp=i0;
    i0=i1;
    i1=tmp;
  }

  void check_fl_draw(int fontsize,const char * ch,int i0,int j0,int imin,int jmin,int di,int dj,int delta_i,int delta_j,int c){
    /* int n=fl_size();
       if (j0>=jmin-n && j0<=jmin+dj+n) */
    // cerr << i0 << " " << j0 << endl;
    if (strlen(ch)>200)
      text_print(fontsize,"String too long",i0+delta_i,j0+delta_j,c);
    else
      text_print(fontsize,ch,i0+delta_i,j0+delta_j,c);
  }

  inline void check_fl_point(int i0,int j0,int imin,int jmin,int di,int dj,int delta_i,int delta_j,int c){
    /* if (i0>=imin && i0<=imin+di && j0>=jmin && j0<=jmin+dj) */
    os_set_pixel(i0+delta_i,j0+delta_j,c);
  }

  unsigned short motif[8]={0xfff0,0xffff,0xff00,0xf0f0,0xe38e,0xcccc,0xaaaa,0xfc0a};
  inline void fl_line(int x0,int y0,int x1,int y1,int c){
    draw_line(x0,y0,x1,y1,c);//,motif[c%8]);
  }

  inline void fl_polygon(int x0,int y0,int x1,int y1,int x2,int y2,int c){
    draw_line(x0,y0,x1,y1,c);
    draw_line(x1,y1,x2,y2,c);
  }

  inline void check_fl_line(int i0,int j0,int i1,int j1,int imin,int jmin,int di,int dj,int delta_i,int delta_j,int c){
    fl_line(i0+delta_i,j0+delta_j,i1+delta_i,j1+delta_j,c);
  }

  int logplot_points=20;

  void checklog_fl_line(double i0,double j0,double i1,double j1,double deltax,double deltay,bool logx,bool logy,double window_xmin,double x_scale,double window_ymax,double y_scale,int c){
    if (!logx && !logy){
      fl_line(round(i0+deltax),round(j0+deltay),round(i1+deltax),round(j1+deltay),c);
      return;
    }
  }

  void find_dxdy(const string & legendes,int labelpos,int labelsize,int & dx,int & dy){
    int l=text_width(labelsize,legendes.c_str());
    dx=3;
    dy=1;
    switch (labelpos){
    case 1:
      dx=-l-3;
      break;
    case 2:
      dx=-l-3;
      dy=labelsize-2;
      break;
    case 3:
      dy=labelsize-2;
      break;
    }
  }

  void draw_legende(const vecteur & f,int i0,int j0,int labelpos,const Graph2d * iptr,int clip_x,int clip_y,int clip_w,int clip_h,int deltax,int deltay,int c){
    if (f.empty() ||!iptr->show_names )
      return;
    string legendes;
    if (f[0].is_symb_of_sommet(at_curve)){
      gen & f0=f[0]._SYMBptr->feuille;
      if (f0.type==_VECT && !f0._VECTptr->empty()){
	gen & f1 = f0._VECTptr->front();
	if (f1.type==_VECT && f1._VECTptr->size()>4 && (!is_zero((*f1._VECTptr)[4]) || (iptr->show_names & 2)) ){
	  gen legende=f1._VECTptr->front();
	  gen var=(*f1._VECTptr)[1];
	  gen r=re(legende,contextptr),i=im(legende,contextptr),a,b;
	  if (var.type==_IDNT && is_linear_wrt(r,*var._IDNTptr,a,b,contextptr)){
	    i=subst(i,var,(var-b)/a,false,contextptr);
	    legendes=i.print(contextptr);
	  }
	  else
	    legendes=r.print(contextptr)+","+i.print(contextptr);
	  if (legendes.size()>18){
	    if (legendes.size()>30)
	      legendes="";
	    else
	      legendes=legendes.substr(0,16)+"...";
	  }
	}
      }
    }
    if (f.size()>2)
      legendes=gen2string(f[2])+(legendes.empty()?"":":")+legendes;
    if (legendes.empty())
      return;
    int fontsize=iptr->labelsize;
    int dx=3,dy=1;
    find_dxdy(legendes,labelpos,fontsize,dx,dy);
    check_fl_draw(fontsize,legendes.c_str(),i0+dx,j0+dy,clip_x,clip_y,clip_w,clip_h,deltax,deltay,c);
  }

  void petite_fleche(double i1,double j1,double dx,double dy,int deltax,int deltay,int width,int c){
    double dxy=std::sqrt(dx*dx+dy*dy);
    if (dxy){
      dxy/=max(2,min(5,int(dxy/10)))+width;
      dx/=dxy;
      dy/=dxy;
      double dxp=-dy,dyp=dx; // perpendicular
      dx*=std::sqrt(3.0);
      dy*=sqrt(3.0);
      fl_polygon(round(i1)+deltax,round(j1)+deltay,round(i1+dx+dxp)+deltax,round(j1+dy+dyp)+deltay,round(i1+dx-dxp)+deltax,round(j1+dy-dyp)+deltay,c);
    }
  }

  void fltk_point(int deltax,int deltay,int i0,int j0,int epaisseur_point,int type_point,int c){
    switch (type_point){
    case 1: // losange
      fl_line(deltax+i0-epaisseur_point,deltay+j0,deltax+i0,deltay+j0-epaisseur_point,c);
      fl_line(deltax+i0,deltay+j0-epaisseur_point,deltax+i0+epaisseur_point,deltay+j0,c);
      fl_line(deltax+i0-epaisseur_point,deltay+j0,deltax+i0,deltay+j0+epaisseur_point,c);
      fl_line(deltax+i0,deltay+j0+epaisseur_point,deltax+i0+epaisseur_point,deltay+j0,c);
      break;
    case 2: // croix verticale
      fl_line(deltax+i0,deltay+j0-epaisseur_point,deltax+i0,deltay+j0+epaisseur_point,c);
      fl_line(deltax+i0-epaisseur_point,deltay+j0,deltax+i0+epaisseur_point,deltay+j0,c);
      break;
    case 3: // carre
      fl_line(deltax+i0-epaisseur_point,deltay+j0-epaisseur_point,deltax+i0-epaisseur_point,deltay+j0+epaisseur_point,c);
      fl_line(deltax+i0+epaisseur_point,deltay+j0-epaisseur_point,deltax+i0+epaisseur_point,deltay+j0+epaisseur_point,c);
      fl_line(deltax+i0-epaisseur_point,deltay+j0-epaisseur_point,deltax+i0+epaisseur_point,deltay+j0-epaisseur_point,c);
      fl_line(deltax+i0-epaisseur_point,deltay+j0+epaisseur_point,deltax+i0+epaisseur_point,deltay+j0+epaisseur_point,c);
      break;
    case 5: // triangle
      fl_line(deltax+i0-epaisseur_point,deltay+j0,deltax+i0,deltay+j0-epaisseur_point,c);
      fl_line(deltax+i0,deltay+j0-epaisseur_point,deltax+i0+epaisseur_point,deltay+j0,c);
      fl_line(deltax+i0-epaisseur_point,deltay+j0,deltax+i0+epaisseur_point,deltay+j0,c);
      break;
    case 7: // point
      if (epaisseur_point>2)
	fl_arc(deltax+i0-(epaisseur_point-1),deltay+j0-(epaisseur_point-1),2*(epaisseur_point-1),2*(epaisseur_point-1),0,360,c);
      else
	fl_line(deltax+i0,deltay+j0,deltax+i0+1,deltay+j0,c);
      break;
    case 6: // etoile
      fl_line(deltax+i0-epaisseur_point,deltay+j0,deltax+i0+epaisseur_point,deltay+j0,c);
      // no break to add the following lines
    case 0: // 0 croix diagonale
      fl_line(deltax+i0-epaisseur_point,deltay+j0-epaisseur_point,deltax+i0+epaisseur_point,deltay+j0+epaisseur_point,c);
      fl_line(deltax+i0-epaisseur_point,deltay+j0+epaisseur_point,deltax+i0+epaisseur_point,deltay+j0-epaisseur_point,c);
      break;
    default: // 4 nothing drawn
      break;
    }
  }

  int horiz_or_vert(const_iterateur jt,GIAC_CONTEXT){
    gen tmp(*(jt+1)-*jt),r,i;
    reim(tmp,r,i,contextptr);
    if (is_zero(r,contextptr)) return 1;
    if (is_zero(i,contextptr)) return 2;
    return 0;
  }

  void fltk_draw(Graph2d & Mon_image,const gen & g,double x_scale,double y_scale,int clip_x,int clip_y,int clip_w,int clip_h){
    int deltax=0,deltay=STATUS_AREA_PX,fontsize=Mon_image.labelsize;
    if (g.type==_VECT){
      const vecteur & v=*g._VECTptr;
      //dbg_printf("fltk_draw vectsize=%i\n",v.size());
      const_iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it)
	fltk_draw(Mon_image,*it,x_scale,y_scale,clip_x,clip_y,clip_w,clip_h);
    }
    if (g.type!=_SYMB)
      return;
    unary_function_ptr s=g._SYMBptr->sommet;
    if (g._SYMBptr->feuille.type!=_VECT)
      return;
    vecteur f=*g._SYMBptr->feuille._VECTptr;
    int mxw=LCD_WIDTH_PX,myw=LCD_HEIGHT_PX-STATUS_AREA_PX;
    double i0,j0,i0save,j0save,i1,j1;
    int fs=f.size();
    if ((fs==4) && (s==at_parameter)){
      return ;
    }
    string the_legend;
    vecteur style(get_style(f,the_legend));
    int styles=style.size();
    // color
    int ensemble_attributs = style.front().val;
    bool hidden_name = false;
    if (style.front().type==_ZINT){
      ensemble_attributs = mpz_get_si(*style.front()._ZINTptr);
      hidden_name=true;
    }
    else
      hidden_name=ensemble_attributs<0;
    int width           =(ensemble_attributs & 0x00070000) >> 16; // 3 bits
    int epaisseur_point =(ensemble_attributs & 0x00380000) >> 19; // 3 bits
    int type_line       =(ensemble_attributs & 0x01c00000) >> 22; // 3 bits
    if (type_line>4)
      type_line=(type_line-4)<<8;
    int type_point      =(ensemble_attributs & 0x0e000000) >> 25; // 3 bits
    int labelpos        =(ensemble_attributs & 0x30000000) >> 28; // 2 bits
    bool fill_polygon   =(ensemble_attributs & 0x40000000) >> 30;
    int couleur         =(ensemble_attributs & 0x0007ffff);
    epaisseur_point += 2;
    if (s==at_pnt){ 
      // f[0]=complex pnt or vector of complex pnts or symbolic
      // f[1] -> style 
      // f[2] optional=label
      gen point=f[0];
      if (point.type==_VECT && point.subtype==_POINT__VECT)
	return;
      if ( (f[0].type==_SYMB) && (f[0]._SYMBptr->sommet==at_curve) && (f[0]._SYMBptr->feuille.type==_VECT) && (f[0]._SYMBptr->feuille._VECTptr->size()) ){
	// Mon_image.show_mouse_on_object=false;
	point=f[0]._SYMBptr->feuille._VECTptr->back();
	if (type_line>=4 && point.type==_VECT && point._VECTptr->size()>2){
	  vecteur v=*point._VECTptr;
	  int vs=v.size()/2; // 3 -> 1
	  if (Mon_image.findij(v[vs],x_scale,y_scale,i0,j0,contextptr) && Mon_image.findij(v[vs+1],x_scale,y_scale,i1,j1,contextptr)){
	    bool logx=Mon_image.display_mode & 0x400,logy=Mon_image.display_mode & 0x800;
	    checklog_fl_line(i0,j0,i1,j1,deltax,deltay,logx,logy,Mon_image.window_xmin,x_scale,Mon_image.window_ymax,y_scale,couleur);
	    double dx=i0-i1,dy=j0-j1;
	    petite_fleche(i1,j1,dx,dy,deltax,deltay,width+3,couleur);
	  }
	}
      }
      if (is_undef(point))
	return;
      // fl_line_style(type_line,width+1,0); 
      if (point.type==_SYMB) {
	if (point._SYMBptr->sommet==at_cercle){
	  vecteur v=*point._SYMBptr->feuille._VECTptr;
	  gen diametre=remove_at_pnt(v[0]);
	  gen e1=diametre._VECTptr->front().evalf_double(1,contextptr),e2=diametre._VECTptr->back().evalf_double(1,contextptr);
	  gen centre=rdiv(e1+e2,2.0,contextptr);
	  gen e12=e2-e1;
	  double ex=evalf_double(re(e12,contextptr),1,contextptr)._DOUBLE_val,ey=evalf_double(im(e12,contextptr),1,contextptr)._DOUBLE_val;
	  if (!Mon_image.findij(centre,x_scale,y_scale,i0,j0,contextptr))
	    return;
	  gen diam=std::sqrt(ex*ex+ey*ey);
	  gen angle=std::atan2(ey,ex);
	  gen a1=v[1].evalf_double(1,contextptr),a2=v[2].evalf_double(1,contextptr);
	  bool full=v[1]==0 && v[2]==cst_two_pi;
	  if ( (diam.type==_DOUBLE_) && (a1.type==_DOUBLE_) && (a2.type==_DOUBLE_) ){
	    i1=diam._DOUBLE_val*x_scale/2.0;
	    j1=diam._DOUBLE_val*y_scale/2.0;
	    double a1d=a1._DOUBLE_val,a2d=a2._DOUBLE_val,angled=angle._DOUBLE_val;
	    bool changer_sens=a1d>a2d;
	    if (changer_sens){
	      double tmp=a1d;
	      a1d=a2d;
	      a2d=tmp;
	    }
	    double anglei=(angled+a1d),anglef=(angled+a2d),anglem=(anglei+anglef)/2;
	    if (fill_polygon)
	      fl_pie(deltax+round(i0-i1),deltay+round(j0-j1),round(2*i1),round(2*j1),full?0:anglei*180/M_PI+.5,full?360:anglef*180/M_PI+.5,couleur,false);
	    else {
	      fl_arc(deltax+round(i0-i1),deltay+round(j0-j1),round(2*i1),round(2*j1),full?0:anglei*180/M_PI+.5,full?360:anglef*180/M_PI+.5,couleur);
	      if (v.size()>=4){ // if cercle has the optionnal 5th arg
		if (v[3]==2)
		  petite_fleche(i0+i1*std::cos(anglem),j0-j1*std::sin(anglem),-i1*std::sin(anglem),-j1*std::cos(anglem),deltax,deltay,width,couleur);
		else {
		  if (changer_sens)
		    petite_fleche(i0+i1*std::cos(anglei),j0-j1*std::sin(anglei),-i1*std::sin(anglei),-j1*std::cos(anglei),deltax,deltay,width,couleur);
		  else
		    petite_fleche(i0+i1*std::cos(anglef),j0-j1*std::sin(anglef),i1*std::sin(anglef),j1*std::cos(anglef),deltax,deltay,width,couleur);
		}
	      }
	    }
	    // Label a few degrees from the start angle, 
	    // FIXME should use labelpos
	    double anglel=angled+a1d+0.3;
	    if (v.size()>=4 && v[3]==2)
	      anglel=angled+(0.45*a1d+0.55*a2d);
	    i0=i0+i1*std::cos(anglel); 
	    j0=j0-j1*std::sin(anglel);
	    if (!hidden_name)
	      draw_legende(f,round(i0),round(j0),labelpos,&Mon_image,clip_x,clip_y,clip_w,clip_h,0,0,couleur);
	    return;
	  }
	} // end circle
#if 0
	if (point._SYMBptr->sommet==at_legende){
	  gen & f=point._SYMBptr->feuille;
	  if (f.type==_VECT && f._VECTptr->size()==3){
	    vecteur & fv=*f._VECTptr;
	    if (fv[0].type==_VECT && fv[0]._VECTptr->size()>=2 && fv[1].type==_STRNG && fv[2].type==_INT_){
	      vecteur & fvv=*fv[0]._VECTptr;
	      if (fvv[0].type==_INT_ && fvv[1].type==_INT_){
		int dx=0,dy=0;
		string legendes(*fv[1]._STRNGptr);
		find_dxdy(legendes,labelpos,fontsize,dx,dy);
		text_print(fontsize,legendes.c_str(),deltax+fvv[0].val+dx,deltay+fvv[1].val+dy,fv[2].val);
	      }
	    }
	  }
	}
#endif
      } // end point.type==_SYMB
      if (point.type!=_VECT || (point.type==_VECT && (point.subtype==_GROUP__VECT || point.subtype==_VECTOR__VECT) && point._VECTptr->size()==2 && is_zero(point._VECTptr->back()-point._VECTptr->front())) ){ // single point
	if (!Mon_image.findij((point.type==_VECT?point._VECTptr->front():point),x_scale,y_scale,i0,j0,contextptr))
	  return;
	if (i0>0 && i0<mxw && j0>0 && j0<myw)
	  fltk_point(deltax,deltay,round(i0),round(j0),epaisseur_point,type_point,couleur);
	if (!hidden_name)
	  draw_legende(f,round(i0),round(j0),labelpos,&Mon_image,clip_x,clip_y,clip_w,clip_h,0,0,couleur);
	return;
      }
      // path
      const_iterateur jt=point._VECTptr->begin(),jtend=point._VECTptr->end();
      if (jt==jtend)
	return;
      bool logx=Mon_image.display_mode & 0x400,logy=Mon_image.display_mode & 0x800;
      if (jt->type==_VECT)
	return;
      if ( (type_point || epaisseur_point>2) && type_line==0 && width==0){
	for (;jt!=jtend;++jt){
	  if (!Mon_image.findij(*jt,x_scale,y_scale,i0,j0,contextptr))
	    return;
	  if (i0>0 && i0<mxw && j0>0 && j0<myw)
	    fltk_point(deltax,deltay,round(i0),round(j0),epaisseur_point,type_point,couleur);
	}
	if (!hidden_name)
	  draw_legende(f,round(i0),round(j0),labelpos,&Mon_image,clip_x,clip_y,clip_w,clip_h,0,0,couleur);
	return;
      }
      // initial point
      if (!Mon_image.findij(*jt,x_scale,y_scale,i0,j0,contextptr))
	return;
      i0save=i0;
      j0save=j0;
      if (fill_polygon){
	if (jtend-jt==5 && *(jt+4)==*jt){
	  // check rectangle parallel to axes -> draw_rectangle (filled)
	  int cote1=horiz_or_vert(jt,contextptr);
	  if (cote1 && horiz_or_vert(jt+1,contextptr)==3-cote1 && horiz_or_vert(jt+2,contextptr)==cote1 && horiz_or_vert(jt+3,contextptr)==3-cote1){
	    if (!Mon_image.findij(*(jt+2),x_scale,y_scale,i0,j0,contextptr))
	      return;
	    int x,y,w,h;
	    if (i0<i0save){
	      x=i0;
	      w=i0save-i0;
	    }
	    else {
	      x=i0save;
	      w=i0-i0save;
	    }
	    if (j0<j0save){
	      y=j0;
	      h=j0save-j0;
	    }
	    else {
	      y=j0save;
	      h=j0-j0save;
	    }
	    draw_rectangle(deltax+x,deltay+y,w,h,couleur);
	    return;
	  }
	} // end rectangle check
	bool closed=*jt==*(jtend-1);
	vector< vector<int> > vi(jtend-jt+(closed?0:1),vector<int>(2));
	for (int pos=0;jt!=jtend;++pos,++jt){
	  if (!Mon_image.findij(*jt,x_scale,y_scale,i0,j0,contextptr))
	    return;
	  vi[pos][0]=i0+deltax;
	  vi[pos][1]=j0+deltay;
	}
	if (!closed)
	  vi.back()=vi.front();
	draw_filled_polygon(vi,0,LCD_WIDTH_PX,0,LCD_HEIGHT_PX,couleur);
	return;
      }
      ++jt;
      if (jt==jtend){
	if (i0>0 && i0<mxw && j0>0 && j0<myw)
	  check_fl_point(deltax+round(i0),deltay+round(j0),clip_x,clip_y,clip_w,clip_h,0,0,couleur);
	if (!hidden_name)
	  draw_legende(f,round(i0),round(j0),labelpos,&Mon_image,clip_x,clip_y,clip_w,clip_h,0,0,couleur);
	return;
      }
      bool seghalfline=( point.subtype==_LINE__VECT || point.subtype==_HALFLINE__VECT ) && (point._VECTptr->size()==2);
      int I0=i0+.5+deltax,J0=j0+.5+deltay,I1,J1;
      // rest of the path
      for (;;){
	if (!Mon_image.findij(*jt,x_scale,y_scale,i1,j1,contextptr))
	  return;
	if (!seghalfline){
          if (!logx && !logy && point.subtype!=_VECTOR__VECT){
            I1=i1+.5+deltax,J1=j1+.5+deltay;
            fl_line(I0,J0,I1,J1,couleur);
            I0=I1; J0=J1; 
          }
          else {
            checklog_fl_line(i0,j0,i1,j1,deltax,deltay,logx,logy,Mon_image.window_xmin,x_scale,Mon_image.window_ymax,y_scale,couleur);
            if (point.subtype==_VECTOR__VECT){
              double dx=i0-i1,dy=j0-j1;
              petite_fleche(i1,j1,dx,dy,deltax,deltay,width,couleur);
            }
          }
	}
	++jt;
	if (jt==jtend){ // label of line at midpoint
	  if (point.subtype==_LINE__VECT){
	    i0=(6*i1-i0)/5-8;
	    j0=(6*j1-j0)/5-8;
	  }
	  else {
	    i0=(i0+i1)/2-8;
	    j0=(j0+j1)/2;
	  }
	  break;
	}
	i0=i1;
	j0=j1;
      }
      // check for a segment/halfline/line
      if ( seghalfline){
	double deltai=i1-i0save,adeltai=absdouble(deltai);
	double deltaj=j1-j0save,adeltaj=absdouble(deltaj);
	if (point.subtype==_LINE__VECT){
	  if (deltai==0)
	    checklog_fl_line(i1,0,i1,clip_h,deltax,deltay,logx,logy,Mon_image.window_xmin,x_scale,Mon_image.window_ymax,y_scale,couleur);
	  else {
	    if (deltaj==0)
	      checklog_fl_line(0,j1,clip_w,j1,deltax,deltay,Mon_image.display_mode & 0x400,Mon_image.display_mode & 0x800,Mon_image.window_xmin,x_scale,Mon_image.window_ymax,y_scale,couleur);
	    else {
	      // Find the intersections with the 4 rectangle segments
	      // Horizontal x=0 or w =i1+t*deltai: y=j1+t*deltaj
	      vector< complex<double> > pts;
	      double y0=j1-i1/deltai*deltaj,tol=clip_h*1e-6;
	      if (y0>=-tol && y0<=clip_h+tol)
		pts.push_back(complex<double>(0.0,y0));
	      double yw=j1+(clip_w-i1)/deltai*deltaj;
	      if (yw>=-tol && yw<=clip_h+tol)
		pts.push_back(complex<double>(clip_w,yw));
	      // Vertical y=0 or h=j1+t*deltaj, x=i1+t*deltai
	      double x0=i1-j1/deltaj*deltai;
	      tol=clip_w*1e-6;
	      if (x0>=-tol && x0<=clip_w+tol)
		pts.push_back(complex<double>(x0,0.0));
	      double xh=i1+(clip_h-j1)/deltaj*deltai;
	      if (xh>=-tol && xh<=clip_w+tol)
		pts.push_back(complex<double>(xh,clip_h));
	      if (pts.size()>=2)
		checklog_fl_line(pts[0].real(),pts[0].imag(),pts[1].real(),pts[1].imag(),deltax,deltay,Mon_image.display_mode & 0x400,Mon_image.display_mode & 0x800,Mon_image.window_xmin,x_scale,Mon_image.window_ymax,y_scale,couleur);
	    } // end else adeltai==0 , adeltaj==0
	  } // end else adeltai==0
	} // end LINE_VECT
	else {
	  double N=1;
	  if (adeltai){
	    N=clip_w/adeltai+1;
	    if (adeltaj)
	      N=max(N,clip_h/adeltaj+1);
	  }
	  else {
	    if (adeltaj)
	      N=clip_h/adeltaj+1;
	  }
	  N *= 2; // increase N since rounding might introduce too small clipping
	  while (fabs(N*deltai)>10000)
	    N /= 2;
	  while (fabs(N*deltaj)>10000)
	    N /= 2;
	  checklog_fl_line(i0save,j0save,i1+N*deltai,j1+N*deltaj,deltax,deltay,Mon_image.display_mode & 0x400,Mon_image.display_mode & 0x800,Mon_image.window_xmin,x_scale,Mon_image.window_ymax,y_scale,couleur);
	}
      } // end seghalfline
      if ( (point.subtype==_GROUP__VECT) && (point._VECTptr->size()==2))
	; // no legend for segment
      else {
	if (!hidden_name)
	  draw_legende(f,round(i0),round(j0),labelpos,&Mon_image,clip_x,clip_y,clip_w,clip_h,0,0,couleur);
      }
    } // end pnt subcase
  }


  void Graph2d::draw_decorations(){
    if (!tracemode) return;
    statuslinemsg(tracemode_add.c_str(),COLOR_CYAN);//os_draw_string_small(1,-16,COLOR_CYAN,COLOR_BLACK,tracemode_add.c_str());
    if (!tracemode_disp.empty())
      fltk_draw(*this,tracemode_disp,x_scale,y_scale,0,0,LCD_WIDTH_PX,LCD_HEIGHT_PX);
    int taille=5;
    int j=current_j+STATUS_AREA_PX;
    fl_line(current_i-taille,j,current_i+taille,j,COLOR_BLACK);
    fl_line(current_i,j-taille,current_i,j+taille,COLOR_BLACK);
  }

  // return a vector of values with simple decimal representation
  // between xmin/xmax or including xmin/xmax (if bounds is true)
  vecteur ticks(double xmin,double xmax,bool bounds){
    if (xmax<xmin)
      swapdouble(xmin,xmax);
    double dx=xmax-xmin;
    vecteur res;
    if (dx==0)
      return res;
    double d=std::pow(10.0,std::floor(std::log10(dx)));
    if (dx<2*d)
      d=d/5;
    else {
      if (dx<5*d)
	d=d/2;
    }
    double x1=std::floor(xmin/d)*d;
    double x2=(bounds?std::ceil(xmax/d):std::floor(xmax/d))*d;
    for (double x=x1+(bounds?0:d);x<=x2;x+=d){
      if (absdouble(x-int(x+.5))<1e-6*d)
	res.push_back(int(x+.5));
      else
	res.push_back(x);
    }
    return res;
  }

  void Graph2d::draw(){
    if (window_xmin>=window_xmax) autoscale();
    if (window_ymin>=window_ymax) autoscale();
    int save_clip_ymin=clip_ymin;
    clip_ymin=STATUS_AREA_PX;
    int horizontal_pixels=LCD_WIDTH_PX,vertical_pixels=LCD_HEIGHT_PX-STATUS_AREA_PX,deltax=0,deltay=STATUS_AREA_PX,clip_x=0,clip_y=0,clip_w=horizontal_pixels,clip_h=vertical_pixels;
    drawRectangle(0,STATUS_AREA_PX,LCD_WIDTH_PX,LCD_HEIGHT_PX-STATUS_AREA_PX,COLOR_WHITE);//Bdisp_AllClr_VRAM();
    // Draw axis
    double I0,J0;
    findij(zero,x_scale,y_scale,I0,J0,contextptr); // origin
    int i_0=round(I0),j_0=round(J0);
    vecteur affx,affy; int affxs,affys;
    if (show_axes){ 
      int taille,delta;
      char ch[256];
      // X
      affx=ticks(window_xmin,window_xmax,true);
      affxs=affx.size();
      for (int i=0;i<affxs;++i){
	double d=evalf_double(affx[i],1,contextptr)._DOUBLE_val;
	if (fabs(d)<1e-6) strcpy(ch,"0"); else sprintfdouble(ch,"",d);
	delta=int((d-window_xmin)*x_scale);//int(horizontal_pixels*(d-window_xmin)/(window_xmax-window_xmin));
	taille=strlen(ch)*4;
	fl_line(delta,vertical_pixels+STATUS_AREA_PX-2,delta,vertical_pixels+STATUS_AREA_PX-1,COLOR_GREEN);
	if (delta>=taille/2 && delta<=horizontal_pixels){
	  text_print(6,ch,delta-taille/2,vertical_pixels+STATUS_AREA_PX-2,COLOR_GREEN);
	}
      }
      // Y
      affy=ticks(window_ymin,window_ymax,true);
      affys=affy.size();
      taille=3;
      for (int j=0;j<affys;++j){
	double d=evalf_double(affy[j],1,contextptr)._DOUBLE_val;
	if (fabs(d)<1e-6) strcpy(ch,"0"); else sprintfdouble(ch,"",d);
	delta=int((window_ymax-d)*y_scale);//int(vertical_pixels*(window_ymax-d)/(window_ymax-window_ymin));
	if (delta>=taille && delta<=vertical_pixels-taille){
	  fl_line(horizontal_pixels-2,STATUS_AREA_PX+delta,horizontal_pixels-1,STATUS_AREA_PX+delta,COLOR_RED);
	  text_print(6,ch,horizontal_pixels-strlen(ch)*5-2,STATUS_AREA_PX+delta+taille,COLOR_RED);
	}
      }
    }
    if (show_axes &&  (window_ymax>=0) && (window_ymin<=0)){ // X-axis
      char ch[256];
      check_fl_line(deltax,deltay+j_0,deltax+horizontal_pixels,deltay+j_0,clip_x,clip_y,clip_w,clip_h,0,0,COLOR_GREEN); 
      check_fl_line(deltax+i_0,deltay+j_0,deltax+i_0+int(x_scale),deltay+j_0,clip_x,clip_y,clip_w,clip_h,0,0,COLOR_CYAN);
      for (int i=0;i<affxs;++i){
	double d=evalf_double(affx[i],1,contextptr)._DOUBLE_val;
	sprint_double(ch,d);
	int delta=int((d-window_xmin)*x_scale);//int(horizontal_pixels*(d-window_xmin)/(window_xmax-window_xmin));
	int taille=strlen(ch)*9;
	fl_line(delta,deltay+j_0,delta,deltay+j_0-2,COLOR_BLACK);
      }
      // check_fl_draw(labelsize,"x",deltax+horizontal_pixels-40,deltay+j_0-4,clip_x,clip_y,clip_w,clip_h,0,0,COLOR_GREEN);
    }
    if ( show_axes && (window_xmax>=0) && (window_xmin<=0) ) {// Y-axis
      char ch[256];
      check_fl_line(deltax+i_0,deltay,deltax+i_0,deltay+vertical_pixels,clip_x,clip_y,clip_w,clip_h,0,0,COLOR_RED);
      check_fl_line(deltax+i_0,deltay+j_0,deltax+i_0,deltay+j_0-int(y_scale),clip_x,clip_y,clip_w,clip_h,0,0,COLOR_CYAN);
      int taille=3;
      for (int j=0;j<affys;++j){
	double d=evalf_double(affy[j],1,contextptr)._DOUBLE_val;
	sprint_double(ch,d);
	int delta=int((window_ymax-d)*y_scale);//int(vertical_pixels*(window_ymax-d)/(window_ymax-window_ymin));
	if (delta>=taille && delta<=vertical_pixels-taille){
	  fl_line(deltax+i_0,STATUS_AREA_PX+delta,deltax+i_0+2,STATUS_AREA_PX+delta,COLOR_BLACK);
	}
      }
      //check_fl_draw(labelsize,"y",deltax+i_0+2,deltay+labelsize,clip_x,clip_y,clip_w,clip_h,0,0,COLOR_RED);
    }
#if 0 // if ticks are enabled, don't forget to set freeze to false
    // Ticks
    if (show_axes && (horizontal_pixels)/(x_scale*x_tick) < 40 && vertical_pixels/(y_tick*y_scale) <40  ){
      if (x_tick>0 && y_tick>0 ){
	double nticks=(horizontal_pixels-I0)/(x_scale*x_tick);
	double mticks=(vertical_pixels-J0)/(y_tick*y_scale);
	int count=0;
	for (int ii=int(-I0/(x_tick*x_scale));ii<=nticks;++ii){
	  int iii=int(I0+ii*x_scale*x_tick+.5);
	  for (int jj=int(-J0/(y_tick*y_scale));jj<=mticks && count<1600;++jj,++count){
	    int jjj=int(J0+jj*y_scale*y_tick+.5);
	    check_fl_point(deltax+iii,deltay+jjj,clip_x,clip_y,clip_w,clip_h,0,0,COLOR_BLACK);
	  }
	}
      }
    }
#endif
    
    // draw
    fltk_draw(*this,g,x_scale,y_scale,clip_x,clip_y,clip_w,clip_h);
    clip_ymin=save_clip_ymin;
    //draw_decorations(); // done in displaygraph in order to have fast trace mode
  }
  
  void Graph2d::left(double d){ 
    window_xmin -= d;
    window_xmax -= d;
  }

  void Graph2d::right(double d){ 
    window_xmin += d;
    window_xmax += d;
  }

  void Graph2d::up(double d){ 
    window_ymin += d;
    window_ymax += d;
  }

  void Graph2d::down(double d){ 
    window_ymin -= d;
    window_ymax -= d;
  }

#endif // WITH_PLOT

  
#ifndef NO_NAMESPACE_XCAS
} // namespace xcas
#endif // ndef NO_NAMESPACE_XCAS

// mathinput_edit.cc - modal 2D editor (MathPrint style) for a giac input line, built on FlowCE's
// mathinput.cc (layout, caret moves, Backspace, selection in the expression tree) and
// mathedit.cc (structure keys). The buffer stays plain giac text: the screen shows it in 2D,
// the bottom line shows the text itself.
// Two modes, as the equation editor (MODE switches between them):
//  caret: keys insert at the caret; UP at the top level (no 2D construct above) selects the
//    subexpression left of the caret, as MODE.
//  selection: UP / DOWN select the parent / the first child, LEFT / RIGHT the siblings (DOWN on
//    a leaf: the caret at its end); a command (factor, simplify...) runs on the selection and
//    its result replaces it, another function wraps it (sin(sel)); a binary operator takes the
//    selection as its first argument, the caret in the second one; a char replaces it; DEL
//    deletes it.
// integrate( sum( limit( diff( surd( series( (F1, F2, MATH: the catalog) insert their template
// (series with polynom: no remainder term, polynom can be erased):
// the integral sign, sigma, lim... with boxes for the bounds (left empty: dropped).
// Matrices ([[a,b],[c,d]] is drawn as a grid; matrix( from the matrix menu inserts a 2x2 one):
// in a cell, , adds a column after it, shift ENTER a row after it (DOWN on the last row too), DEL at the start of a
// cell deletes its column or its row if they are empty; empty cells and list elements get 0
// when the line is accepted.
// shift INS: a menu of the history (and the clipboard), the level chosen goes at the caret or
// replaces the selection.
// No static data beyond a few bytes (KhiCAS's start needs the heap that follows the BSS): the
// buffers are on the stack or on the heap while the editor is open.
// F5: eval, the selection (or the whole expression) replaced by its value.
// EXE: accept; ANNUL (and shift UNDO): back to the state before the last change (nothing to
// undo: ANNUL cancels); shift QUIT: cancel; shift CLEAR: empty the line.
#include <string.h>
#include <stdlib.h>
#include "calc.h"
#include "console.h"
#include "menuGUI.h"
#include "main.h"
#include "mathedit.h"
#include "mathinput.h" // last: it maps std to ustl on the calculator

namespace xcas {
  void text_print(int fontsize,const char * s,int x,int y,int c,int bg,int mode); // kdisplay.cc
}
extern const mi_metrics mi_device_metrics; // mathinput_glue.cc
void mi_draw(const mi_layout & L,const char * s,int bx,int by,int x0,int y0,int x1,int y1,bool caret,int selbg);
bool mi_eval_text(const char * s,char * res,int cap); // mathinput_eval.cc

// edit area (2D), and the line with the giac text, between the status line and the F-key menu.
// Screen coordinates: the text functions (os_draw_string*, Printmini) add STATUS_AREA_PX to y,
// the menu line (Printmini at C58) is at C58+STATUS_AREA_PX on the screen.
static const int MIE_X0=0,MIE_X1=LCD_WIDTH_PX,MIE_Y0=STATUS_AREA_PX+2,MIE_Y1=C58,MIE_TXT=C58+3,MIE_BOT=C58+STATUS_AREA_PX;

// inserts t at *p, the caret ends back chars before the end of t; false if it does not fit
static bool mie_insert(char * s,int cap,int * p,const char * t,int back){
  const int l=strlen(s),n=strlen(t);
  if (l+n>=cap)
    return false;
  memmove(s+*p+n,s+*p,l-*p+1);
  memcpy(s+*p,t,n);
  *p+=n-back;
  return true;
}

// replaces s[a,b) by t; false if it does not fit
static bool mie_replace(char * s,int cap,int a,int b,const char * t){
  const int l=strlen(s),n=strlen(t);
  if (l-(b-a)+n>=cap)
    return false;
  memmove(s+a+n,s+b,l-b+1);
  memcpy(s+a,t,n);
  return true;
}

// the commands drawn as 2D templates (mathinput.cc): what follows their first argument, the
// other slots as boxes (bounds left empty are dropped, mie_clean); car: where the caret goes
// in rest, the first slot drawn: the lower bound, the point of a limit, the index of a root
// (-1: on the selection, the whole is selected; typed, the caret in the first argument)
static const struct { const char * name, * rest; short car; } mie_tpl[]={
  {"integrate",",x,,)",3},{"int",",x,,)",3},{"sum",",k,,)",3},{"limit",",x,)",3},{"diff",",x)",-1},{"surd",",)",1},{"series",",x,,,polynom)",3},{0,0,0}};
static int mie_tpl_find(const char * name){
  for (int i=0;mie_tpl[i].name;++i){
    if (!strcmp(mie_tpl[i].name,name))
      return i;
  }
  return -1;
}

// empty bounds dropped: integrate(f,x,,) -> integrate(f,x), sum(f,k,,) -> sum(f,k) (,,) is never
// valid giac)
// series(f,x,a,n,polynom) with a or n left empty: 0 and 5 (giac's defaults), the boxes drawn in
// place of the point and the order cannot be dropped as the bounds are (polynom follows them)
static void mie_series_fill(char * s,int cap){
  for (char * q=s;(q=strstr(q,"series("));q+=7){
    if (q>s && ((q[-1]>='a' && q[-1]<='z') || (q[-1]>='A' && q[-1]<='Z') || q[-1]=='_'))
      continue;
    int d=0,na=0,i=q+7-s;
    int st[5]={i,0,0,0,0},en[5]={0,0,0,0,0};
    for (;s[i];++i){
      const char c=s[i];
      if (c=='(' || c=='[' || c=='{')
        ++d;
      else if (c==')' || c==']' || c=='}'){
        if (!d){
          if (na<5)
            en[na]=i;
          break;
        }
        --d;
      }
      else if (c==',' && !d){
        if (na<5)
          en[na]=i;
        ++na;
        if (na<5)
          st[na]=i+1;
      }
    }
    if (!s[i] || na!=4) // not series(f,x,a,n,polynom): left as typed
      continue;
    for (int k=3;k>=2;--k){ // from the right: the positions on the left stay valid
      int a=st[k],b=en[k];
      while (a<b && s[a]==' ')
        ++a;
      const int l=strlen(s);
      if (a==b && l+1<cap){
        memmove(s+a+1,s+a,l-a+1);
        s[a]=k==2?'0':'5';
      }
    }
  }
}

static void mie_clean(char * s,int cap){
  mie_series_fill(s,cap);
  for (char * q;(q=strstr(s,",,)"));)
    memmove(q,q+2,strlen(q+2)+1);
  // an empty element of a list or a matrix cell gets 0: [[1,],[,2]] -> [[1,0],[0,2]] ([] stays)
  char st[32];
  int d=0;
  for (int i=0;s[i];++i){
    const char c=s[i];
    if (c=='"'){ // a string as is
      for (++i;s[i] && s[i]!='"';++i){
        if (s[i]=='\\' && s[i+1])
          ++i;
      }
      if (!s[i])
        break;
      continue;
    }
    if (c=='(' || c=='[' || c=='{'){
      if (d<32)
        st[d]=c;
      ++d;
    }
    if ((c==')' || c==']' || c=='}') && d>0)
      --d;
    if (d>0 && d<=32 && st[d-1]=='[' && (c=='[' || c==',')){
      int j=i+1;
      while (s[j]==' ')
        ++j;
      const int l=strlen(s);
      if ((s[j]==',' || (s[j]==']' && c==',')) && l+1<cap){
        memmove(s+i+2,s+i+1,l-i);
        s[i+1]='0';
      }
    }
  }
}

// the text of a key typed in the 2D editor: ^ -> ^(), sin( -> sin(), integrate( -> its
// template, the caret in the first slot
static void mie_insert_key_text(char * s,int cap,int * p,const char * t){
  const int l=strlen(t);
  if (l==0 || l>=60){
    mie_insert(s,cap,p,t,0);
    return;
  }
  char buf[64];
  strcpy(buf,t);
  if (!strcmp(t,"matrix(")){ // a 2x2 matrix, the caret in its first cell (, adds a column,
    mie_insert(s,cap,p,"[[,],[,]]",7);         // DOWN on the last row a row)
    return;
  }
  if (l>1 && l<20 && t[l-1]=='('){
    char name[20];
    memcpy(name,t,l-1);
    name[l-1]=0;
    const int i=mie_tpl_find(name);
    if (i>=0){
      strcat(buf,mie_tpl[i].rest); // the caret in the first slot drawn (the lower bound, as a TI)
      const int car=mie_tpl[i].car<0?0:mie_tpl[i].car;
      mie_insert(s,cap,p,buf,strlen(mie_tpl[i].rest)-car);
      return;
    }
  }
  if (!strcmp(t,"^")){
    strcat(buf,"()");
    mie_insert(s,cap,p,buf,1);
    return;
  }
  if (t[l-1]=='('){
    strcat(buf,")");
    mie_insert(s,cap,p,buf,1);
    return;
  }
  mie_insert(s,cap,p,t,0);
}

// Backspace at *p, as FlowCE's console: a glyph, an empty template as a whole, or out of a slot
static void mie_backspace(char * s,int * p){
  int depth=0;
  for (int i=0;i<*p-1;++i)
    depth+=s[i]=='('?1:s[i]==')'?-1:0;
  if (*p>0 && s[*p-1]==')' && depth>0){ // sin(x)| -> sin(x|): into the group
    --*p;
    return;
  }
  if (me_backspace(s,p)) // at the start of a slot: unwraps a call, a fraction, an exponent
    return;
  const int len=strlen(s);
  int from,to,q=*p;
  const bool name=q>=2 && ((s[q-2]>='a' && s[q-2]<='z') || (s[q-2]>='A' && s[q-2]<='Z') || (s[q-2]>='0' && s[q-2]<='9'));
  if (q>=2 && q<len && (s[q-2]=='/' || s[q-2]=='^') && s[q-1]=='(' && s[q]==')'){
    from=q-2; // x/(|) or x^(|): the fraction or power goes, x stays
    to=q+1;
    q=from;
  }
  else if (q>=1 && s[q-1]=='/'){ // a fraction bar typed with the / key: deleted like a char
    from=q-1;
    to=q;
    q=from;
  }
  else if (q>=1 && q<len && s[q-1]=='(' && s[q]==')' && !name){ // an empty () group
    from=q-1;
    to=q+1;
    q=from;
  }
  else
    q=mi_backspace(s,len,q,from,to,mi_device_metrics);
  if (to>from)
    memmove(s+from,s+to,len-to+1);
  *p=q;
}

// ---------------------------------------------------------------- matrices
// the matrix around the caret (mi_matrix_at): 1.1 KB on the heap while the editor is open (the
// BSS must not grow: KhiCAS's start uses the heap right after it almost to the last byte)
static mi_matrix * mie_Mp;
#define mie_M (*mie_Mp)

// edits of s at increasing positions: del chars at pos replaced by ins
struct mie_edit { short pos,del; const char * ins; };
// applies the n edits; returns the position the original position q goes to (after an insertion
// at q; inside a deletion: its start), -1 if it does not fit
static int mie_apply(char * s,int cap,const mie_edit * e,int n,int q){
  int l=strlen(s),grow=0,nq=q;
  for (int i=0;i<n;++i){
    grow+=strlen(e[i].ins)-e[i].del;
    if (e[i].pos+e[i].del<=q)
      nq+=strlen(e[i].ins)-e[i].del;
    else if (e[i].pos<q)
      nq-=q-e[i].pos;
  }
  if (l+grow>=cap)
    return -1;
  for (int i=n-1;i>=0;--i){
    const int k=strlen(e[i].ins);
    memmove(s+e[i].pos+k,s+e[i].pos+e[i].del,l-e[i].pos-e[i].del+1);
    memcpy(s+e[i].pos,e[i].ins,k);
    l+=k-e[i].del;
  }
  return nq;
}
static bool mie_blank(const char * s,int a,int b){
  for (int i=a;i<b;++i){
    if (s[i]!=' ')
      return false;
  }
  return true;
}
// the caret p in a cell of a matrix, at the cell's level (not in a group inside the cell)
static bool mie_in_cell(const char * s,int p){
  if (!mie_Mp || !mi_matrix_at(s,strlen(s),p,mi_device_metrics,mie_M))
    return false;
  const mi_matrix & M=mie_M;
  const int k=M.r*M.nc+M.c;
  if (p<M.ca[k] || p>M.cb[k])
    return false;
  int d=0;
  for (int i=M.ca[k];i<p;++i)
    d+=(s[i]=='(' || s[i]=='[' || s[i]=='{')?1:(s[i]==')' || s[i]==']' || s[i]=='}')?-1:0;
  return d==0;
}
// a column after column c (an empty cell in each row); the caret in the new cell of row r
static int mie_addcol(char * s,int cap,int c,int r){
  const mi_matrix & M=mie_M;
  mie_edit e[MI_MAXMC];
  for (int i=0;i<M.nr;++i){
    e[i].pos=M.cb[i*M.nc+c];
    e[i].del=0;
    e[i].ins=",";
  }
  return mie_apply(s,cap,e,M.nr,M.cb[r*M.nc+c]);
}
static int mie_addrow(char * s,int cap,int r){ // a row of empty cells after row r, the caret in its first cell
  const mi_matrix & M=mie_M;
  char buf[2*MI_MAXMC+4];
  int k=0;
  buf[k++]=',';
  buf[k++]='[';
  for (int j=1;j<M.nc;++j)
    buf[k++]=',';
  buf[k++]=']';
  buf[k]=0;
  mie_edit e={M.rb[r],0,buf};
  const int q=mie_apply(s,cap,&e,1,M.rb[r]);
  return q<0?-1:q-k+2;
}
// Backspace at the start of cell (r,c): its column goes if it is empty (and not the only one),
// else its row if it is empty, else the whole matrix if it has one empty cell; returns the caret,
// -1: none of them (the caret goes left)
static int mie_delcell(char * s,int cap){
  const mi_matrix & M=mie_M;
  const int r=M.r,c=M.c,nc=M.nc;
  bool colempty=true,rowempty=true;
  for (int i=0;i<M.nr;++i)
    colempty=colempty && mie_blank(s,M.ca[i*nc+c],M.cb[i*nc+c]);
  for (int j=0;j<nc;++j)
    rowempty=rowempty && mie_blank(s,M.ca[r*nc+j],M.cb[r*nc+j]);
  mie_edit e[MI_MAXMC];
  if (colempty && nc>1){
    for (int i=0;i<M.nr;++i){
      const int k=i*nc+c;
      e[i].pos=c?M.cb[k-1]:M.ca[k];      // ,cell after the previous cell, or cell, before the next
      e[i].del=c?M.cb[k]-M.cb[k-1]:M.ca[k+1]-M.ca[k];
      e[i].ins="";
    }
    return mie_apply(s,cap,e,M.nr,c?M.cb[r*nc+c-1]:M.ca[r*nc]);
  }
  if (rowempty && M.nr>1){
    e[0].pos=r?M.rb[r-1]:M.ra[0]; // ,[row] after the previous row, or [row], before the next
    e[0].del=r?M.rb[r]-M.rb[r-1]:M.ra[1]-M.ra[0];
    e[0].ins="";
    const int q=mie_apply(s,cap,e,1,r?M.cb[r*nc-1]:M.ra[0]);
    return r || q<0?q:q+1;
  }
  if (M.nr==1 && nc==1 && rowempty){
    mie_edit d={M.a,short(M.b-M.a),""};
    return mie_apply(s,cap,&d,1,M.a);
  }
  return -1;
}

// ---------------------------------------------------------------- history
// shift INS: a menu of the clipboard and the console's history (newest last, preselected);
// the text chosen, 0 if none
static const char * mie_history(){
  const char * clip=paste_clipboard();
  const int nh=Last_Line>0?Last_Line:0,n=nh+(clip && clip[0]?1:0);
  if (!n)
    return 0;
  MenuItem * items=new MenuItem[n]; // on the heap
  if (!items)
    return 0;
  int k=0;
  if (clip && clip[0])
    items[k++].text=(char *)clip;
  for (int i=0;i<nh;++i)
    items[k++].text=Line[i].str?(char *)Line[i].str:(char *)"";
  Menu menu;
  menu.items=items;
  menu.numitems=n;
  menu.height=12;
  menu.scrollbar=1;
  menu.scrollout=1;
  menu.title=(char *)(lang?"Historique":"History");
  menu.selection=n;
  menu.scroll=n>12?n-12:0;
  const char * res=0;
  if (doMenu(&menu)==MENU_RETURN_SELECTION && menu.selection>=1 && menu.selection<=n)
    res=items[menu.selection-1].text;
  delete [] items;
  return res;
}

// ---------------------------------------------------------------- selection
// precedence level of the whole text t (mi_select's levels: 1 , ... 4 + - 5 * / 7 ^ 9 primary)
static int mie_level(const char * t){
  const int l=strlen(t);
  int a=0,b=0,lv=9;
  if (!mi_select(t,l,&a,&b,-1,mi_device_metrics,&lv))
    return 9;
  for (int k=0;k<64 && !(a==0 && b==l);++k){
    const int a0=a,b0=b;
    mi_select(t,l,&a,&b,2,mi_device_metrics,&lv);
    if (a==a0 && b==b0)
      break;
  }
  return a==0 && b==l?lv:1;
}

// whether a text of level elv put in place of the selection [a,b) of s needs parentheses:
// not if the selection already sits between ( and ) (a group, a slot, an argument), if its
// parent is a list, a call or a template (level 9), or for the same associative operator
// (a+b replaced by a+(c+d): a+c+d); an opposite after an operator always does (a-(-b))
static bool mie_needpar(const char * s,int a,int b,int elv,int elop){
  const int l=strlen(s);
  if (a>0 && s[a-1]=='(' && b<l && s[b]==')')
    return false;
  int i=a-1;
  while (i>=0 && s[i]==' ')
    --i;
  if (elop=='-' && elv==6 && i>=0 && strchr("+-*/^",s[i])) // a--b: a-(-b)
    return true;
  int pa=a,pb=b,plv=9;
  mi_select(s,l,&pa,&pb,2,mi_device_metrics,&plv);
  if (pa==a && pb==b) // the whole line
    return false;
  if (plv>7 || elv>plv)
    return false;
  if (elv==plv && (elop=='+' || elop=='*' || elop==',')){ // associative: not after - or / or ^
    if (i<pa || s[i]==elop)
      return false;
  }
  return true;
}

// the selection s[a,b) as an operand of level lv: in parentheses if its level slv is lower
static void mie_operand(const char * s,int a,int b,int slv,int lv,char * out){
  const bool par=slv<lv;
  int n=0;
  if (par)
    out[n++]='(';
  memcpy(out+n,s+a,b-a);
  n+=b-a;
  if (par)
    out[n++]=')';
  out[n]=0;
}

// replaces the selection [*a,*b) by e (level elv, top operator elop), in parentheses if needed;
// caret: the position in e to put the caret at (-1: e selected); returns the new caret (-1:
// the selection is [*a,*b), -2: does not fit)
static int mie_put(char * s,int cap,int * a,int * b,const char * e,int elv,int elop,int caret){
  const int n=strlen(e);
  char buf[GEN_PRINT_BUFSIZE];
  if (n+3>=int(sizeof(buf)))
    return -2;
  const bool par=mie_needpar(s,*a,*b,elv,elop);
  int k=0;
  if (par)
    buf[k++]='(';
  memcpy(buf+k,e,n);
  k+=n;
  if (par)
    buf[k++]=')';
  buf[k]=0;
  if (!mie_replace(s,cap,*a,*b,buf))
    return -2;
  if (caret>=0)
    return *a+(par?1:0)+caret;
  *b=*a+k;
  return -1;
}

// the commands run on the selection (the equation editor's immediate commands); another
// function wraps it
static bool mie_immediate(const char * name){
  static const char * const tab[]={"eval","evalf","evalc","regroup","simplify","normal","ratnormal","factor","cfactor","partfrac","cpartfrac","expand","canonical_form","exp2trig","trig2exp","sincos","lin","tlin","tcollect","texpand","trigexpand","trigcos","trigsin","trigtan","halftan",0};
  for (int i=0;tab[i];++i){
    if (!strcmp(tab[i],name))
      return true;
  }
  return false;
}

// a key on the selection [*a,*b) (level slv); returns the new caret (caret mode) or -1 (the
// selection, maybe changed, stays in [*a,*b)), -3: not handled
static int mie_sel_key(char * s,int cap,int * a,int * b,int slv,int key,const char * t){
  char e[GEN_PRINT_BUFSIZE],op[GEN_PRINT_BUFSIZE];
  const int sl=*b-*a;
  if (sl+8>=int(sizeof(e)))
    return -3;
  // binary operators: the selection is the first argument, the caret in the second one
  int bop=0,blv=0;
  if (key==KEY_CHAR_PLUS || key==KEY_CHAR_MINUS){ bop=key; blv=4; }
  else if (key==KEY_CHAR_MULT){ bop='*'; blv=5; }
  else if (key==KEY_CHAR_EQUAL || key=='<' || key=='>'){ bop=key; blv=3; }
  else if (key==KEY_CHAR_COMMA){ bop=','; blv=1; }
  if (bop){
    mie_operand(s,*a,*b,slv,blv,op);
    strcpy(e,op);
    const int l=strlen(e);
    e[l]=bop;
    e[l+1]=0;
    int r=mie_put(s,cap,a,b,e,blv,bop,l+1);
    return r>=0?r:-1;
  }
  if (key==KEY_CHAR_DIV || key==KEY_EQW_TEMPLATE || key==KEY_EQW_TEMPLATE2){ // a fraction: the selection over the caret
    mie_operand(s,*a,*b,slv,5,op);
    strcpy(e,op);
    strcat(e,"/()");
    int r=mie_put(s,cap,a,b,e,5,'/',strlen(e)-1);
    return r>=0?r:-1;
  }
  if (key==KEY_CHAR_POW || key==KEY_CHAR_SQUARE || key==KEY_CHAR_RECIP){ // the selection as the base
    mie_operand(s,*a,*b,slv,9,op);
    strcpy(e,op);
    strcat(e,key==KEY_CHAR_POW?"^()":key==KEY_CHAR_SQUARE?"^2":"^(-1)");
    int r=mie_put(s,cap,a,b,e,7,'^',key==KEY_CHAR_POW?strlen(e)-1:strlen(e));
    return r>=0?r:-1;
  }
  if (key==KEY_CHAR_PMINUS){ // the opposite, selected
    mie_operand(s,*a,*b,slv,6,op);
    strcpy(e,"-");
    strcat(e,op);
    mie_put(s,cap,a,b,e,6,'-',-1);
    return -1;
  }
  if (key==KEY_CHAR_LPAR){ // in parentheses, selected
    strcpy(e,"(");
    strncat(e,s+*a,sl);
    strcat(e,")");
    mie_put(s,cap,a,b,e,9,0,-1);
    return -1;
  }
  if (!t || !t[0])
    return -3;
  const int tl=strlen(t);
  if (tl>1 && tl<40 && t[tl-1]=='('){ // a function: run or wrap
    char name[40];
    memcpy(name,t,tl-1);
    name[tl-1]=0;
    memcpy(op,s+*a,sl);
    op[sl]=0;
    if (mie_immediate(name)){
      strcpy(e,name);
      strcat(e,"(");
      strcat(e,op);
      strcat(e,")");
      char res[GEN_PRINT_BUFSIZE];
      mie_clean(e,sizeof(e));
      if (!mi_eval_text(e,res,sizeof(res)))
        return -1; // error: the selection stays
      mie_put(s,cap,a,b,res,mie_level(res),0,-1);
      return -1;
    }
    // a template with the selection in its first slot: ∫ sel dx, lim, Σ (the caret in the
    // first empty slot), or name(sel) selected
    int car=-1;
    strcpy(e,t);
    strcat(e,op);
    const int i=mie_tpl_find(name);
    if (i>=0){
      if (mie_tpl[i].car>=0)
        car=strlen(e)+mie_tpl[i].car;
      strcat(e,mie_tpl[i].rest);
    }
    else
      strcat(e,")");
    int r=mie_put(s,cap,a,b,e,9,0,car);
    return r>=0?r:-1;
  }
  // a text (a digit, a name, a constant): replaces the selection
  if (!mie_replace(s,cap,*a,*b,t))
    return -1;
  return *a+tl;
}

// draws the 2D layout, scrolled so that the caret (or the selection) is visible, and the giac
// text below
static void mie_draw(const char * s,int p,int sa,int sb,int & scrollx,int & scrolly){
  mi_layout L;
  const int len=strlen(s);
  const bool sel=sa<sb;
  mi_build(s,len,sel?-1:p,mi_device_metrics,L,sel?sa:-1,sel?sb:-1);
  drawRectangle(0,STATUS_AREA_PX,LCD_WIDTH_PX,MIE_BOT-STATUS_AREA_PX,COLOR_WHITE);
  const int w=MIE_X1-MIE_X0-8,h=MIE_Y1-MIE_Y0-4,th=L.asc+L.desc;
  // what is kept visible (layout coordinates, from its left / top): the caret and 16 px around
  // it, or the whole selection (its beginning when it is too large)
  int x0=L.cx-16,x1=L.cx+18,y0=L.cy+L.asc-4,y1=L.cy+L.asc+L.ch+4;
  if (sel && L.sw>0){
    x0=L.sx-4;
    x1=L.sx+L.sw+4;
    y0=L.sy+L.asc-2;
    y1=L.sy+L.asc+L.sh+2;
  }
  if (x1-x0>w)
    x1=x0+w;
  if (y1-y0>h)
    y1=y0+h;
  // horizontal: centered when it fits, else scrolled as little as possible
  if (L.width<=w)
    scrollx=-(w-L.width)/2;
  else {
    if (x1-scrollx>w)
      scrollx=x1-w;
    if (x0-scrollx<0)
      scrollx=x0;
    if (scrollx>L.width-w)
      scrollx=L.width-w;
    if (scrollx<0)
      scrollx=0;
  }
  // vertical: the same
  int top;
  if (th<=h){
    top=MIE_Y0+2+(h-th)/2;
    scrolly=0;
  }
  else {
    if (y1-scrolly>h)
      scrolly=y1-h;
    if (y0-scrolly<0)
      scrolly=y0;
    if (scrolly>th-h)
      scrolly=th-h;
    if (scrolly<0)
      scrolly=0;
    top=MIE_Y0+2-scrolly;
  }
  mi_draw(L,s,MIE_X0+4-scrollx,top+L.asc,MIE_X0,MIE_Y0,MIE_X1,MIE_Y1,!sel,sel?COLOR_SELECTED:-1);
  draw_line(0,MIE_Y1,LCD_WIDTH_PX-1,MIE_Y1,COLOR_BLACK);
  // the giac text (small font, 5 px per char), a window around the caret (the selection
  // underlined)
  const int nc=(LCD_WIDTH_PX-4)/5;
  int start=0;
  if (len>nc){
    start=(sel?sa:p)-nc/2;
    if (start>len-nc)
      start=len-nc;
    if (start<0)
      start=0;
  }
  char buf[80];
  int n=len-start;
  if (n>nc)
    n=nc;
  memcpy(buf,s+start,n);
  buf[n]=0;
  xcas::text_print(12,buf,2,MIE_TXT+10,COLOR_BLACK,COLOR_WHITE,0);
  if (sel){
    int x0=sa-start,x1=sb-start;
    if (x0<0)
      x0=0;
    if (x1>n)
      x1=n;
    if (x1>x0)
      drawRectangle(2+5*x0,MIE_TXT+12,5*(x1-x0),2,COLOR_BLUE);
  }
  else
    drawRectangle(2+5*(p-start),MIE_TXT,1,12,COLOR_BLUE);
}

// undo: the states before the last changes (text, caret, selection)
struct mie_state { char * s; short p,sa,sb; };
enum { MIE_NUNDO=10 };
static mie_state mie_undo[MIE_NUNDO];
static int mie_nundo;
static void mie_state_free(mie_state & u){
  free(u.s);
  u.s=0;
}
static void mie_undo_clear(){
  for (int i=0;i<mie_nundo;++i)
    mie_state_free(mie_undo[i]);
  mie_nundo=0;
}
static void mie_undo_push(mie_state & u){ // takes u (the oldest state goes when it is full)
  if (mie_nundo==MIE_NUNDO){
    mie_state_free(mie_undo[0]);
    memmove(mie_undo,mie_undo+1,(MIE_NUNDO-1)*sizeof(mie_state));
    --mie_nundo;
  }
  mie_undo[mie_nundo++]=u;
  u.s=0;
}

// edits s (0-terminated, capacity cap) in 2D; true if accepted (EXE), false if cancelled
// (shift QUIT, or ANNUL with nothing to undo)
static bool mie_run(char * s,int cap,bool sel){
  int p=strlen(s),sa=0,sb=0,slv=9,scrollx=0,scrolly=0,oldkeyflag=-1; // sa<sb: a selection
  mie_state pend={0,0,0,0}; // the state before the last key, kept if the key changed the text
  mie_undo_clear();
  if (sel && p){ // the whole expression selected, as in the equation editor
    sa=0;
    sb=p;
    if (!mi_select(s,p,&sa,&sb,-2,mi_device_metrics,&slv))
      sa=sb=0;
  }
  for (;;){
    if (pend.s){
      if (strcmp(pend.s,s))
        mie_undo_push(pend);
      else
        mie_state_free(pend);
    }
    int keyflag=(unsigned char)Setup_GetEntry(0x14);
    if (oldkeyflag!=keyflag)
      console_disp_status(keyflag,6); // the console's menu line, F5: eval
    oldkeyflag=keyflag;
    mie_draw(s,p,sa,sb,scrollx,scrolly);
    int key;
    ck_getkey(&key);
    if (key==KEY_CTRL_SHIFT || key==KEY_CTRL_ALPHA)
      continue;
    if (key==KEY_CHAR_MAT) // the matrix menu, as in the console (matrix(: a 2x2 matrix)
      key=KEY_CTRL_F10;
    oldkeyflag=-1; // the menu line is drawn again after each key (a menu may have covered it)
    if (key==KEY_CTRL_EXE){
      mie_clean(s,cap);
      mie_undo_clear();
      return true;
    }
    if (key==KEY_CTRL_QUIT){
      mie_undo_clear();
      return false;
    }
    if (key==KEY_CTRL_EXIT || key==KEY_CTRL_UNDO){ // back to the state before the last change
      if (mie_nundo){
        mie_state & u=mie_undo[--mie_nundo];
        strcpy(s,u.s);
        p=u.p;
        sa=u.sa;
        sb=u.sb;
        mie_state_free(u);
        if (sa<sb)
          mi_select(s,strlen(s),&sa,&sb,-2,mi_device_metrics,&slv);
        continue;
      }
      if (key==KEY_CTRL_UNDO)
        continue;
      return false; // nothing to undo: the editor is left (MODE leaves the selection)
    }
    if (key==KEY_CTRL_SETUP){ // MODE: caret <-> selection (the subexpression left of the caret)
      if (sa<sb){
        p=sb;
        sa=sb=0;
      }
      else {
        sa=sb=p;
        if (!mi_select(s,strlen(s),&sa,&sb,-1,mi_device_metrics,&slv))
          sa=sb=0;
      }
      continue;
    }
    pend.s=(char *)malloc(strlen(s)+1);
    if (pend.s){
      strcpy(pend.s,s);
      pend.p=p;
      pend.sa=sa;
      pend.sb=sb;
    }
    char catbuf[512]; // MATH: the catalog of all the commands
    if (key==KEY_CTRL_SYMB && !showCatalog(catbuf,0,0))
      continue;
    if (key==KEY_CTRL_AC){
      if (!s[0])
        continue;
      s[0]=0;
      p=sa=sb=0;
      continue;
    }
    if (sa<sb){ // ---- selection mode
      if (key==KEY_CTRL_LEFT || key==KEY_CTRL_RIGHT || key==KEY_CTRL_UP || key==KEY_CTRL_DOWN){
        const int dir=key==KEY_CTRL_LEFT?0:key==KEY_CTRL_RIGHT?1:key==KEY_CTRL_UP?2:3;
        const int a0=sa,b0=sb;
        mi_select(s,strlen(s),&sa,&sb,dir,mi_device_metrics,&slv);
        if (dir==3 && sa==a0 && sb==b0){ // a leaf: the caret at its end
          p=sb;
          sa=sb=0;
        }
        continue;
      }
      if (key==KEY_CTRL_DEL){
        mie_replace(s,cap,sa,sb,"");
        p=sa;
        sa=sb=0;
        continue;
      }
      if (key==KEY_CTRL_CLIP){
        char buf[GEN_PRINT_BUFSIZE];
        const int n=sb-sa<int(sizeof(buf))?sb-sa:int(sizeof(buf))-1;
        memcpy(buf,s+sa,n);
        buf[n]=0;
        copy_clipboard(buf);
        continue;
      }
      const char * t=0;
      char c[2]={0,0};
      if (key>=' ' && key<=126 && key!=KEY_CHAR_PLUS && key!=KEY_CHAR_MINUS && key!=KEY_CHAR_MULT && key!=KEY_CHAR_DIV && key!=KEY_CHAR_EQUAL && key!='<' && key!='>' && key!=KEY_CHAR_COMMA){
        c[0]=key;
        t=c;
      }
      else if (key==KEY_CTRL_PASTE){ // shift INS: a level of the history in place of the selection
        if (const char * h=mie_history()){
          char buf[GEN_PRINT_BUFSIZE];
          if (strlen(h)<sizeof(buf)){
            strcpy(buf,h);
            mie_put(s,cap,&sa,&sb,buf,mie_level(buf),0,-1);
            mi_select(s,strlen(s),&sa,&sb,-2,mi_device_metrics,&slv);
          }
        }
        continue;
      }
      else if (key==KEY_CTRL_SYMB)
        t=catbuf;
      else if (key==KEY_CTRL_F5) // eval: the selection replaced by its value
        t="eval(";
      else if ((key>=KEY_CTRL_F1 && key<=KEY_CTRL_F6) || (key>=KEY_CTRL_F7 && key<=KEY_CTRL_F20))
        t=key==KEY_CTRL_F4?0:console_menu(key,0);
      else if (key!=KEY_CHAR_POW && key!=KEY_CHAR_SQUARE && key!=KEY_CHAR_RECIP && key!=KEY_EQW_TEMPLATE && key!=KEY_EQW_TEMPLATE2 && key!=KEY_CHAR_PMINUS)
        t=keytostring(key,keyflag,false);
      if ((key==KEY_CHAR_COMMA || key==KEY_CHAR_CR) && mie_in_cell(s,sa) && mie_M.ca[mie_M.r*mie_M.nc+mie_M.c]==sa && mie_M.cb[mie_M.r*mie_M.nc+mie_M.c]==sb){
        const int np=key==KEY_CHAR_CR?mie_addrow(s,cap,mie_M.r):mie_addcol(s,cap,mie_M.c,mie_M.r); // a cell selected: a column or a row after it
        if (np>=0){
          p=np;
          sa=sb=0;
        }
        continue;
      }
      const int r=mie_sel_key(s,cap,&sa,&sb,slv,key,t);
      if (r>=0){
        p=r;
        sa=sb=0;
      }
      else if (r==-1) // the selection may have changed: its level again
        mi_select(s,strlen(s),&sa,&sb,-2,mi_device_metrics,&slv);
      continue;
    }
    // ---- caret mode
    if (key==KEY_CTRL_F5){ // eval: the whole expression replaced by its value, selected
      const int l=strlen(s);
      sa=0;
      sb=l;
      if (l && mi_select(s,l,&sa,&sb,-2,mi_device_metrics,&slv)){
        mie_sel_key(s,cap,&sa,&sb,slv,key,"eval(");
        mi_select(s,strlen(s),&sa,&sb,-2,mi_device_metrics,&slv);
      }
      else
        sa=sb=0;
      continue;
    }
    if (key==KEY_CTRL_LEFT || key==KEY_CTRL_RIGHT || key==KEY_CTRL_UP || key==KEY_CTRL_DOWN){
      const int dir=key==KEY_CTRL_LEFT?0:key==KEY_CTRL_RIGHT?1:key==KEY_CTRL_UP?2:3;
      const int q=mi_move(s,strlen(s),p,dir,mi_device_metrics);
      if (dir==3 && mie_in_cell(s,p) && mie_M.r==mie_M.nr-1 && (q<0 || q<mie_M.a || q>=mie_M.b)){
        // down out of the last row of a matrix: a new row (not after an empty one)
        bool empty=true;
        for (int j=0;j<mie_M.nc;++j){
          const int k=mie_M.r*mie_M.nc+j;
          empty=empty && mie_blank(s,mie_M.ca[k],mie_M.cb[k]);
        }
        if (!empty){
          const int np=mie_addrow(s,cap,mie_M.nr-1);
          if (np>=0)
            p=np;
          continue;
        }
      }
      if (q>=0)
        p=q;
      else if (dir==2){ // up at the top level: select the subexpression left of the caret
        sa=sb=p;
        if (!mi_select(s,strlen(s),&sa,&sb,-1,mi_device_metrics,&slv))
          sa=sb=0;
      }
      continue;
    }
    if (key==KEY_SHIFT_LEFT){
      p=0;
      continue;
    }
    if (key==KEY_SHIFT_RIGHT){
      p=strlen(s);
      continue;
    }
    if (key==KEY_CTRL_DEL){
      if (mie_in_cell(s,p) && p==mie_M.ca[mie_M.r*mie_M.nc+mie_M.c]){ // at the start of a cell
        const int np=mie_delcell(s,cap);
        p=np>=0?np:mi_move(s,strlen(s),p,0,mi_device_metrics);
        continue;
      }
      mie_backspace(s,&p);
      continue;
    }
    if (key==KEY_CHAR_COMMA && mie_in_cell(s,p)){ // in a matrix: a column after this one
      const int np=mie_addcol(s,cap,mie_M.c,mie_M.r);
      if (np>=0)
        p=np;
      continue;
    }
    if (key==KEY_EQW_TEMPLATE || key==KEY_EQW_TEMPLATE2){ // the fraction key: an empty fraction
      const mi_template & t=mi_get_template(MI_T_FRAC);
      mie_insert(s,cap,&p,t.text,strlen(t.text)-t.caret);
      continue;
    }
    if (key==KEY_CTRL_CLIP){
      copy_clipboard(s);
      continue;
    }
    if (key==KEY_CTRL_PASTE){ // shift INS: a level of the history (or the clipboard) at the caret
      if (const char * h=mie_history()){
        const int hl=strlen(h);
        if (s[0] && hl+2<GEN_PRINT_BUFSIZE && mie_level(h)<9){
          char buf[GEN_PRINT_BUFSIZE];
          buf[0]='(';
          strcpy(buf+1,h);
          buf[hl+1]=')';
          buf[hl+2]=0;
          mie_insert(s,cap,&p,buf,0);
        }
        else
          mie_insert(s,cap,&p,h,0);
      }
      continue;
    }
    if (key==KEY_CHAR_CR && mie_in_cell(s,p)){ // shift ENTER in a matrix: a row after this one
      const int np=mie_addrow(s,cap,mie_M.r);
      if (np>=0)
        p=np;
      continue;
    }
    // structure keys (mathedit.cc): / makes a fraction of the term left of the caret, ( [ {
    // wrap the rest of the slot, ) steps out of a group, , = < > leave a denominator or an
    // exponent first, x² and x⁻¹ insert ^(2) and ^(-1)
    int mk=0;
    if (key==KEY_CHAR_DIV || key==KEY_CHAR_LPAR || key==KEY_CHAR_RPAR || key==KEY_CHAR_COMMA || key==KEY_CHAR_EQUAL || key=='<' || key=='>' || key==KEY_CHAR_PLUS || key==KEY_CHAR_MINUS || key==KEY_CHAR_LBRCKT || key==KEY_CHAR_RBRCKT || key==KEY_CHAR_LBRACE || key==KEY_CHAR_RBRACE)
      mk=key;
    else if (key==KEY_CHAR_SQUARE)
      mk=ME_SQUARE;
    else if (key==KEY_CHAR_RECIP)
      mk=ME_RECIP;
    if (mk && me_key(s,cap,&p,mk))
      continue;
    if (key>=' ' && key<=126){
      char t[2]={(char)key,0};
      mie_insert(s,cap,&p,t,0);
      continue;
    }
    const char * t=0;
    if (key==KEY_CTRL_F4){
      static char c[2];
      const int k=chartab();
      c[0]=k>0?k:0;
      c[1]=0;
      t=c;
    }
    else if (key==KEY_CTRL_SYMB)
      t=catbuf;
    else if ((key>=KEY_CTRL_F1 && key<=KEY_CTRL_F6) || (key>=KEY_CTRL_F7 && key<=KEY_CTRL_F20))
      t=console_menu(key,0);
    else
      t=keytostring(key,keyflag,false);
    if (t && t[0])
      mie_insert_key_text(s,cap,&p,t);
  }
}

// sel: starts in selection mode, the whole expression selected (else the caret at the end)
bool mi_edit(char * s,int cap,bool sel){
  mie_Mp=(mi_matrix *)malloc(sizeof(mi_matrix)); // 0: no matrix structure keys
  const bool r=mie_run(s,cap,sel);
  free(mie_Mp);
  mie_Mp=0;
  return r;
}

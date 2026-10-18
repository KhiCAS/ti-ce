// mathinput_eval.cc - giac side of the 2D editor (mathinput_edit.cc): a command run on the
// selection, its result printed back as giac text.
#include <cstring>
#include <cstdlib>
#include <giac/giacPCH.h>
#include "main.h"

extern giac::context * contextptr;
void do_eval(giac::gen & g); // main.cc: eval with the ON key to stop it

// evaluates the giac text s and prints the result in res (capacity cap); false on a parse
// error, an error, undef or a result too long
bool mi_eval_text(const char * s,char * res,int cap){
  giac::gen g(std::string(s),contextptr);
  if (giac::is_undef(g))
    return false;
  do_eval(g);
  if (giac::is_undef(g) || (g.type==giac::_STRNG && g.subtype==-1))
    return false;
  std::string r=g.print(contextptr);
  if (int(r.size())>=cap)
    return false;
  strcpy(res,r.c_str());
  return true;
}

namespace xcas {
  bool ispnt(const giac::gen & g); // kdisplay.cc
  void displaygraph(const giac::gen & ge,char ** thumb);
}
bool mi_edit(char * s,int cap,bool sel=false); // mathinput_edit.cc
bool textedit(char * s); // main.cc

// the equation editor's entry point (xcas::eqws) with the 2D editor: edits s in place (at least
// GEN_PRINT_BUFSIZE bytes); eval: the value of s is edited (a graph is displayed instead). Text,
// strings, comments and programs go to the text editor. True if s was changed (EXE).
bool mi_eqws(char * s,bool eval){
  int ss=strlen(s);
  for (int i=0;i<ss;++i){
    if (s[i]==char(0x9c))
      s[i]='\n';
  }
  if (ss>=2 && (s[0]=='#' || s[0]=='"' || (s[0]=='/' && (s[1]=='/' || s[1]=='*'))))
    return textedit(s);
  if (eval && s[0]){
    giac::gen ge(std::string(s),contextptr);
    if (giac::is_undef(ge))
      return textedit(s);
    ge=giac::eval(ge,1,contextptr);
#ifdef WITH_PLOT
    if (xcas::ispnt(ge)){
      xcas::displaygraph(ge,0);
      return false;
    }
#endif
    if (ge.is_symb_of_sommet(giac::at_program))
      return textedit(s);
    std::string r=ge.print(contextptr);
    if (int(r.size())>=GEN_PRINT_BUFSIZE)
      return false;
    strcpy(s,r.c_str());
  }
  if (int(strlen(s))>=GEN_PRINT_BUFSIZE)
    return textedit(s);
  return mi_edit(s,GEN_PRINT_BUFSIZE,true); // selected, as the equation editor
}

// a matrix or a list edited in 2D (input_matrix in main.cc), or a result (check_do_graph in
// kdisplay.cc, sel: selected, as the equation editor showed it): ge printed, edited, parsed
// back; false if cancelled
bool mi_eqw_matrix(giac::gen & ge,bool sel){
  std::string r=ge.print(contextptr);
  if (int(r.size())>=GEN_PRINT_BUFSIZE)
    return false;
  char * buf=(char *)malloc(GEN_PRINT_BUFSIZE); // on the heap
  if (!buf)
    return false;
  strcpy(buf,r.c_str());
  const bool ok=mi_edit(buf,GEN_PRINT_BUFSIZE,sel);
  if (ok)
    ge=giac::gen(std::string(buf),contextptr);
  free(buf);
  return ok;
}

// the console is in Python mode: its lines are not drawn in 2D (mathinput_glue.cc)
bool mi_python_mode(){
  return giac::python_compat(contextptr)!=0;
}

#include <stdlib.h>
#include "main.h"
#include <vector>
#include <inttypes.h>
extern "C"
size_t ti_Read(void *data, size_t size, size_t count, uint8_t handle){ return 0; }
extern "C"
int ti_GetC(uint8_t handle){ return 0;}
extern "C"
char * micropy_init(int stack_size,int heap_size){
  char * ptr=(char *)malloc(4);
  return ptr;
}
extern "C"
int micropy_eval(const char * line){
  return 0;
}
extern "C"
void  mp_deinit(){
}
extern "C"
void mp_stack_ctrl_init(){
  int a;
  dbg_printf("stack ptr %x\n",(unsigned)&a);
}

extern "C" int mp_token(const char * line){ return 1; }

extern "C" const char * const * mp_vars(){
  return nullptr;
}

int get_free_memory(){ return 2048; }

ustl::vector<logo_turtle> & turtle_stack(){
  static ustl::vector<logo_turtle> * ptr=new ustl::vector<logo_turtle>;
  return *ptr;
}

void displaylogo(){}
int execution_in_progress_py,ctrl_c_py,parser_errorline;
logo_turtle & turtle(){
  static logo_turtle l;
  return l;
}

extern "C" int ctrl_c_interrupted(int exception);
int ctrl_c_interrupted(int exception){ return exception;}

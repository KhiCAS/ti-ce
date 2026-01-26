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
  dbg_printf("stack ptr %p\n",&a);
}

extern "C" int mp_token(const char * line){ return 1; }

extern "C" const char * const * mp_vars(){
  return nullptr;
}

int get_free_memory(){ return 2048; }

std::vector<logo_turtle> & turtle_stack(){
  static std::vector<logo_turtle> * ptr = new std::vector<logo_turtle>;
  return *ptr;
}

void displaylogo(){}

extern int execution_in_progress_py;
int execution_in_progress_py;

// this variable is volatile in main.cc, so thats why it is also volatile here
extern volatile int ctrl_c_py;
volatile int ctrl_c_py;

int parser_errorline;

logo_turtle & turtle(){
  static logo_turtle l;
  return l;
}

extern "C" int ctrl_c_interrupted(int exception);
int ctrl_c_interrupted(int exception){ return exception;}

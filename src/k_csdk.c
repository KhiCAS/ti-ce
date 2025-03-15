// implementation of the minimal C SDK for KhiCAS
int (*shutdown)()=0;

short shutdown_state=0;
short exam_mode=0,nspire_exam_mode=0;
unsigned exam_start=0; // RTC start
int exam_duration=0;
// <0: indicative duration, ==0 time displayed during exam, >0 end exam_mode after
const int exam_bg1=0x4321,exam_bg2=0x1234;
int exam_bg(){
  return exam_mode?(exam_duration>0?exam_bg1:exam_bg2):0x50719;
}

void SetQuitHandler( void (*f)(void)){}

// TI83/84
#ifdef TICE
  // TICE: the toolchain strtol (from zilog) is bugged, switching to
  // https://stackoverflow.com/questions/7457163/what-is-the-implementation-of-strtol
#include <errno.h>
#define LONG_MAX ((long)(~0UL>>1))
#define LONG_MIN (~LONG_MAX)

#if 0
long my_strtol(const char * nptr, char ** endptr, int base) {
  const char *p = nptr, *endp;
  int is_neg = 0, overflow = 0;
  /* Need unsigned so (-LONG_MIN) can fit in these: */
  unsigned long n = 0UL, cutoff;
  int cutlim;
  if (base < 0 || base == 1 || base > 36) {
#ifdef EINVAL /* errno value defined by POSIX */
    errno = EINVAL;
#endif
    return 0L;
  }
  endp = nptr;
  while (*p==' ')
    p++;
  if (*p == '+') {
    p++;
  } else if (*p == '-') {
    is_neg = 1, p++;
  }
  if (*p == '0') {
    p++;
    /* For strtol(" 0xZ", &endptr, 16), endptr should point to 'x';
     * pointing to ' ' or '0' is non-compliant.
     * (Many implementations do this wrong.) */
    endp = p;
    if (base == 16 && (*p == 'X' || *p == 'x')) {
      p++;
    } else if (base == 2 && (*p == 'B' || *p == 'b')) {
      /* C23 standard supports "0B" and "0b" prefixes. */
      p++;
    } else if (base == 0) {
      if (*p == 'X' || *p == 'x') {
        base = 16, p++;
      } else if (*p == 'B' || *p == 'b') {
        base = 2, p++;
      } else {
        base = 8;
      }
    }
  } else if (base == 0) {
    base = 10;
  }
  cutoff = (is_neg) ? -(LONG_MIN / base) : LONG_MAX / base;
  cutlim = (is_neg) ? -(LONG_MIN % base) : LONG_MAX % base;
  while (1) {
    int c;
    if (*p >= 'A')
      c = ((*p - 'A') & (~('a' ^ 'A'))) + 10;
    else if (*p <= '9')
      c = *p - '0';
    else
      break;
    if (c < 0 || c >= base) break;
    endp = ++p;
    if (overflow) {
      /* endptr should go forward and point to the non-digit character
       * (of the given base); required by ANSI standard. */
      if (endptr) continue;
      break;
    }
    if (n > cutoff || (n == cutoff && c > cutlim)) {
      overflow = 1; continue;
    }
    n = n * base + c;
  }
  if (endptr) *endptr = (char *)endp;
  if (overflow) {
    errno = ERANGE; return ((is_neg) ? LONG_MIN : LONG_MAX);
  }
  return (long)((is_neg) ? -n : n);
}
#endif

int clip_ymin=0;
const int STATUS_AREA_PX=18;
// debug: dbg_printf() Add #include <debug.h> to a source file, and use make debug instead of make to build a debug program. You may need to run make clean beforehand in order to ensure all source files are rebuilt.
// ASM syscalls: https://wikiti.brandonw.net/index.php?title=Category:84PCE:Syscalls:By_Name
// doc: https://ce-programming.github.io/toolchain/index.html
// Makefile options https://ce-programming.github.io/toolchain/static/makefile-options.html
// memory layout: https://ce-programming.github.io/toolchain/static/faq.html
// parameters are in CEdev/meta (and app_tools if present)
// makefile.mk:
// BSSHEAP_LOW ?= D052C6
// BSSHEAP_HIGH ?= D13FD8
// STACK_HIGH ?= D1A87E
// INIT_LOC ?= D1A87F
// Can we set STACK_HIGH to another value? I think the global area stack+data could be "reversed", I mean stack top at 0xD2A87F and data(+code+ro_data for RAM programs) at a new position: INIT_LOC=D1987E (maybe +1 or +2)

// TI stack 4K D1A87Eh: Top of the SPL stack.
// change stack pointer (if STACK_HIGH change does not work)
// requires assembly code (https://0x04.net/~mwk/doc/z80/eZ80.pdf),
// save stack pointer
// LD (Mmn), SP
// set HL to the new stack address (top of the area-3)
// LD SP,HL
// call main
// restore stack pointer
// LD SP,(Mmn)
// 1023 bytes: uint8_t[1023] os_RamCode (do not use if flash write occurs)
// 0xD052C6: 60989 bytes used for bss+heap (temp buffers in TI OS)
// 0xD1A881: Start of UserMem. 64K for code, data, ro data
// size_t os_MemChk(void **free) size and position of free ram area
// Or we could create a VarApp in RAM with no real data inside and use this area for temporary storage.
// 0xD40000: Start of VRAM. 320x240x2 bytes = 153600 bytes.
// half may be used in 8 bits palette mode (graphx)
// half=0x12C00
// ;safeRAM Locations
// pixelShadow		equ 0D031F6h ; 8400 bytes
// pixelShadow2		equ 0D052C6h ; 8400 bytes
// cmdPixelShadow		equ 0D07396h ; 8400 bytes
// plotSScreen		equ 0D09466h ; 21945 bytes	; Set GraphDraw Flag to redraw graph if used
//saveSScreen		equ 0D0EA1Fh ; 21945 bytes	; Set GraphDraw Flag to redraw graph if used
//cursorImage		equ 0E30800h ; 1020 bytes
#include "k_csdk.h"
#include <ti/getkey.h>
//#include <keypadc.h>
#include <ti/getcsc.h>
#include <ti/screen.h>
#include <ti/flags.h>
#include <ti/vars.h>
#include <sys/power.h>
#include <sys/rtc.h> // boot_GetTime(uint8_t *seconds, uint8_t *minutes, uint8_t *hours), boot_SetTime(uint8_t seconds, uint8_t minutes, uint8_t hours)
#include <sys/timers.h>
#include <sys/lcd.h>
#ifdef STANDALONE
// #include "graphic.h"
#else
#include <graphx.h>
#include <fileioc.h>
#endif
#include <string.h>
#include <debug.h>
#define FILENAME_MAXRECORDS 32
#define FILENAME_MAXSIZE 9
#define FILE_MAXSIZE 16384
char os_filenames[FILENAME_MAXRECORDS][FILENAME_MAXSIZE];

inline int sdk_rgb(int a,int b,int c){
  return (((a*32)/256)<<11) | (((b*64)/256)<<5) | ((c*32)/256);
}

void sdk_init(){
  gfx_Begin();
#ifndef STANDALONE
  unsigned short * addr=gfx_palette;
  dbg_printf("KhiCAS SDK Init palette=%x\n",addr);
  for (int r=0;r<4;r++){
    for (int g=0;g<8;g++){
      for (int b=0;b<4;b++){
        int R=r*255/3,G=g*255/7,B=b*255/3;
        addr[(r<<5)|(g<<2)|b]=sdk_rgb(R,G,B);//gfx_RGBTo1555(R,G,B);
        // dbg_printf("palette %i %i %i %i\n",(r<<5)|(g<<2)|b,R,G,B);
      }
    }
  }
#endif
  // 128-254 arc-en-ciel? 255 should remain white
}

void sdk_end(){
#ifndef WITH_QUAD
  dbg_printf("SDK End\n");
#endif
  gfx_End();
}

void clear_screen(void){
#ifdef STANDALONE
  vGL_setArea(0,0,VIR_LCD_PIX_W,VIR_LCD_PIX_H,GL_WHITE);
#else
  gfx_FillScreen(255); // gfx_ZeroScreen(void);
#endif
}

int alpha=0,alphalock=0,prevalpha=0,shift=0;
int handle_f5(){
  if (alphalock)
    alphalock=3-alphalock;
  else
    alphalock=2;
  alpha=1;
  return alpha;
}
void dbgprint(int i){
  char buf[16]={0};
  buf[0]='0'+i/100;
  buf[1]='0'+(i % 100)/10;
  buf[2]='0'+(i % 10);
  os_draw_string(20,60,SDK_WHITE,SDK_BLACK,buf,false);
}

#define kb_On (*(volatile uint8_t*)0xF00020 & 1)
#define kb_EnableOnLatch() ((*(volatile uint8_t*)0xF0002C) |= 1)
#define kb_DisableOnLatch() ((*(volatile uint8_t*)0xF0002C) &= ~1)
#define kb_ClearOnLatch() ((*(volatile uint8_t*)0xF00028) = 1)

int getkey(int allow_suspend){
  sync_screen();
  display_time();//statusline(0);
  for (;;){
    int i=0,j=0,joff=10000;
    for (;!i;j++){
#if 1
      if (kb_On || j==joff
          //&& allow_suspend
          ){
#ifndef WITH_QUAD
        dbg_printf("on pressed\n");
#endif
        if (shift || j==joff){
          boot_TurnOff();
          shift=false;
        }
        else if (kb_On){
          boot_TurnOn();
          lcd_Control = 0b100100100111; // 8bpp like graphx
          j=0;
          display_time();
        }
        kb_ClearOnLatch();
        os_wait_1ms(100);
        continue;
      }
#endif
      if (j<joff){
        if ((j&0x3ff)==0x3ff)
          display_time();
        i=os_GetCSC();
      }
      os_wait_1ms(10);      
    }
    // dbgprint(i);
    int decal=(alpha>>1)<<5; // 0 or 32 for upper or lowercase
    int Alpha=alpha,Shift=shift;
    shift=0; prevalpha=alpha;
    if (!alphalock)
      alpha=0;
    switch (i){
    case sk_Fx:
      return Alpha?KEY_CTRL_F11:Shift?KEY_CTRL_F6:KEY_CTRL_F1;
    case sk_Fenetre:
      return Alpha?KEY_CTRL_F12:Shift?KEY_CTRL_F7:KEY_CTRL_F2;
    case sk_Zoom:
      return Alpha?KEY_CTRL_F13:Shift?KEY_CTRL_F8:KEY_CTRL_F3;      
    case sk_Trace:
      return Alpha?KEY_CTRL_F14:Shift?KEY_CTRL_F9:KEY_CTRL_F4;      
    case sk_Graph:
      return Alpha?KEY_CTRL_F15:Shift?KEY_CTRL_F10:KEY_CTRL_F5;      
    case sk_Mode:
      return Shift?KEY_CTRL_QUIT:KEY_CTRL_SETUP;
    case sk_Del:
      return Shift?KEY_CTRL_PASTE:KEY_CTRL_DEL;
    case sk_GraphVar:
#ifdef FRANCAIS
      return Shift?KEY_CTRL_UNDO:KEY_CTRL_XTT;
#else
      return Alpha?KEY_CTRL_UNDO:(Shift?KEY_EQW_TEMPLATE:KEY_CTRL_XTT);
#endif
    case sk_Stat:
      return Shift?KEY_CHAR_LIST:KEY_CTRL_STATS;
    case sk_Right:
      return Shift?KEY_SHIFT_RIGHT:KEY_CTRL_RIGHT;
    case sk_Left:
      return Shift?KEY_SHIFT_LEFT:KEY_CTRL_LEFT;
    case sk_Up:
      return Shift?KEY_CTRL_PAGEUP:KEY_CTRL_UP;
    case sk_Down:
      return Shift?KEY_CTRL_PAGEDOWN:KEY_CTRL_DOWN;
    case sk_Enter:
      return Shift?KEY_CHAR_CR:KEY_CTRL_EXE;    
    case sk_Alpha:
      if (alphalock){
        alpha=alphalock=0;
      }
      else {
        if (Shift)
          alphalock=alpha=2;
        else {
          alpha=2;
          if (prevalpha)
            alphalock=alpha=prevalpha;            
        }
      }
      //statusline(0);
      return KEY_CTRL_ALPHA; // continue;
    case sk_2nd:
      if (alphalock)
        alpha=3-alpha; // maj <> min
      else
        shift=!Shift;
      //statusline(0);
      return KEY_CTRL_SHIFT; // continue;
    case sk_Math:
      return Alpha?KEY_CHAR_A+decal:KEY_CTRL_SYMB;
    case sk_Matrice:
      return Alpha?KEY_CHAR_B+decal:KEY_CHAR_MAT;
    case sk_Prgm:
      return Alpha?KEY_CHAR_C+decal:(Shift?KEY_CTRL_F15:KEY_CTRL_PRGM);
    case sk_Vars:
      return KEY_CTRL_VARS;
    case sk_Annul:
      return Shift?KEY_CTRL_AC:KEY_CTRL_EXIT;
    case sk_TglExact:
      //dbg_printf("tab\n");
      return Alpha?KEY_CHAR_D+decal:'\t';
#ifdef FRANCAIS
    case sk_Trig:
      return Alpha?KEY_CHAR_E+decal:(Shift?KEY_CHAR_PI:KEY_CHAR_SIN);
    case sk_Resol:
      return Alpha?KEY_CHAR_F+decal:(Shift?KEY_CTRL_APPS:KEY_CTRL_SOLVE);
    case sk_Frac:
      return Alpha?KEY_CHAR_G+decal:KEY_EQW_TEMPLATE;
    case sk_Power:
      return Alpha?KEY_CHAR_H+decal:KEY_CHAR_POW;
#else
    case sk_Sin:
      return Alpha?KEY_CHAR_E+decal:(Shift?KEY_CHAR_ASIN:KEY_CHAR_SIN);
    case sk_Cos:
      return Alpha?KEY_CHAR_F+decal:(Shift?KEY_CHAR_ACOS:KEY_CHAR_COS);
    case sk_Tan:
      return Alpha?KEY_CHAR_G+decal:(Shift?KEY_CHAR_ATAN:KEY_CHAR_TAN);
    case sk_Power:
      return Alpha?KEY_CHAR_H+decal:(Shift?KEY_CHAR_PI:KEY_CHAR_POW);
#endif
    case sk_Square:
      return Alpha?KEY_CHAR_I+decal:Shift?KEY_CHAR_ROOT:KEY_CHAR_SQUARE;
    case sk_Comma:
      return Alpha?KEY_CHAR_J+decal:Shift?KEY_CHAR_E:KEY_CHAR_COMMA;      
    case sk_LParen:
      return Alpha?KEY_CHAR_K+decal:Shift?KEY_CHAR_LBRACE:KEY_CHAR_LPAR;      
    case sk_RParen:
      return Alpha?KEY_CHAR_L+decal:Shift?KEY_CHAR_RBRACE:KEY_CHAR_RPAR;      
    case sk_Div:
      return Alpha?KEY_CHAR_M+decal:Shift?KEY_CHAR_E+32:KEY_CHAR_DIV;
    case sk_Log:
      return Alpha?KEY_CHAR_N+decal:Shift?KEY_CHAR_EXPN10:KEY_CHAR_LOG;
    case sk_7:
      return Alpha?KEY_CHAR_O+decal:Shift?KEY_LIST7:KEY_CHAR_7;
    case sk_8:
      return Alpha?KEY_CHAR_P+decal:Shift?KEY_LIST8:KEY_CHAR_8;
    case sk_9:
      return Alpha?KEY_CHAR_Q+decal:Shift?KEY_LIST9:KEY_CHAR_9;
    case sk_Mul:
      return Alpha?KEY_CHAR_R+decal:Shift?KEY_CHAR_LBRCKT:KEY_CHAR_MULT;
    case sk_Ln:
      return Alpha?KEY_CHAR_S+decal:Shift?KEY_CHAR_EXPN:KEY_CHAR_LN;
    case sk_4:
      return Alpha?KEY_CHAR_T+decal:Shift?KEY_LIST4:KEY_CHAR_4;
    case sk_5:
      return Alpha?KEY_CHAR_U+decal:Shift?KEY_LIST5:KEY_CHAR_5;
    case sk_6:
      return Alpha?KEY_CHAR_V+decal:Shift?KEY_LIST6:KEY_CHAR_6;
    case sk_Sub:
      return Alpha?KEY_CHAR_W+decal:Shift?KEY_CHAR_RBRCKT:KEY_CHAR_MINUS;
    case sk_Store:
      return Alpha?KEY_CHAR_X+decal:Shift?KEY_SHIFT_ANS:KEY_CHAR_STORE;
    case sk_1:
      return Alpha?KEY_CHAR_Y+decal:Shift?KEY_LIST1:KEY_CHAR_1;
    case sk_2:
      return Alpha?KEY_CHAR_Z+decal:Shift?KEY_LIST2:KEY_CHAR_2;
    case sk_3:
      return Alpha?KEY_CHAR_THETA:Shift?KEY_LIST3:KEY_CHAR_3;
    case sk_Add:
      return Alpha?'"':KEY_CHAR_PLUS;
    case sk_0:
      return Alpha?KEY_CHAR_SPACE:Shift?KEY_CTRL_CATALOG:KEY_CHAR_0;
    case sk_DecPnt:
      return Alpha?':':Shift?KEY_CHAR_I+32:'.';
    case sk_Chs:
      return Alpha?'?':Shift?KEY_CHAR_ANS:KEY_CHAR_PMINUS;
    default:
      return i;
    }
  }
}
void GetKey(int * key){
  *key=getkey(0);
}
int iskeydown(int key){
  return kb_Data[1] == key;
}

// if (kb_On) ...
void enable_back_interrupt(){
  // kb_EnableOnLatch();
}
void disable_back_interrupt(){
  // kb_DisableOnLatch();
}
int isalphaactive(){
  return alpha;
}
int alphawasactive(int * key){
  return prevalpha;
}
void lock_alpha(){
  alpha=2; alphalock=1;
  shift=0;
  statusflags();
}
void reset_kbd(){
  shift=alpha=alphalock=0;
  statusflags();
}
int GetSetupSetting(int k){
  if (k!=0x14) return -1;
  if (!alpha) return shift?1:0;
  if (!alphalock) return alpha==2?8:4;
  return alpha==2?0x88:0x84;
}

void os_wait_1ms(int ms){
  msleep(ms); // delay(ms)?
}
double millis(){
  return rtc_Days*86400.0+rtc_Hours*3600.+rtc_Minutes*60.+rtc_Seconds;
}
int os_set_angle_unit(int mode){
  if (mode) os_ResetFlag(TRIG,DEGREES); else os_SetFlag(TRIG,DEGREES);
  return true;
}

int os_get_angle_unit(){
  int i=os_TestFlag(TRIG,DEGREES);
  return i?0:1;
}

int file_exists(const char * filename){
#ifdef STANDALONE
  int archived=0;
  var_t * ptr_=os_GetAppVarData(filename,&archived);
#ifndef WITH_QUAD
  dbg_printf("file exists name=%s ptr=%x archived=%i\n",filename,ptr_,archived);
#endif
  if (!ptr_)
    return 0;
  return archived?2:1;
#else
  int h=ti_Open(filename, "r");
  if (!h)
    return false;
  ti_Close(h);
  return true;
#endif
}

void warn_archived(const char * filename){
  char buf[256];
  strcpy(buf,filename);
  strcat(buf,": unarchive variable!");
  os_fill_rect(0,115,LCD_WIDTH_PX,25,COLOR_BLACK);
  os_draw_string_medium(20,102,COLOR_RED,COLOR_WHITE,buf,0);
  getkey(1);
}

int erase_file(const char * filename){
  int i=file_exists(filename);
#ifndef WITH_QUAD
  dbg_printf("erase file=%s i=%i\n",filename,i);
#endif
  if (!i)
    return 0;
#ifdef STANDALONE
  if (i==2){
    warn_archived(filename);
    // appvar in archive should be unarchived first
    return 0;
  }
  os_DelAppVar(filename);
  return 1;
#else
  ti_Delete(filename);
  return true;
#endif
}

const char * read_file(const char * filename){
  const char * ext=0;
  int l=strlen(filename);
  char var[9]={0};
  strncpy(var,filename,8);
  for (--l;l>0;--l){
    if (filename[l]=='.'){
      ext=filename+l+1;
      if (l<9)
        var[l]=0;
      break;
    }
  }
#ifdef STANDALONE
  int archived;
  //dbg_printf("read_file var=%s\n",var);
  var_t * ptr_=os_GetAppVarData(var,&archived);
  if (!ptr_)
    return 0;
  char * ptrc=ptr_->data;
  int s=ptr_->size;
  //dbg_printf("read_file name=%s size=%i %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x\n",var,s,ptrc[0],ptrc[1],ptrc[2],ptrc[3],ptrc[4],ptrc[5],ptrc[6],ptrc[7],ptrc[8],ptrc[9],ptrc[10],ptrc[11],ptrc[12],ptrc[13],ptrc[14],ptrc[15],ptrc[16],ptrc[17],ptrc[18],ptrc[19],ptrc[20],ptrc[21],ptrc[22],ptrc[23]);
  if (s>7){
    //unsigned short u;
    //ti_Read(&u,1,2,h);
    char subtype[8]={0};
    strncpy(subtype,ptrc,4); ptrc+=4;
    if (strncmp(subtype,"PYCD",4)==0 || strncmp(subtype,"PYSC",4)==0 || strncmp(subtype,"XCAS",4)==0 || strncmp(subtype,"TABL",4)==0){
      unsigned char dx;
      dx=*ptrc; ++ptrc;
      //dbg_printf("read subtype=%s dx=%i ptrc=%s\n",subtype,(int)dx,ptrc);
      if (dx>0){
        // skip desktop filename
        char buf[256]={0};
        strncpy(buf,ptrc,dx+1); ptrc+=dx+1;
        s -= 5+dx;
        //dbg_printf("tiread subtype=%s filename=%s dx=%i effsize=%i\n",subtype,buf,dx,s);
      }
      else
        s -= 4;
    }
  }
  //dbg_printf("strlen=%i s=%i ptrc[s-1]=%x\n",strlen(ptrc),s,ptrc[s-1]);
  if (ptrc[s-1]==0)
    return ptrc; // if it ends with a 0, we are done
  // we must insure a final 0
#if 0 // lcd_Ram can not be used anymore
  if (s>=LCD_WIDTH_PX*LCD_HEIGHT_PX/2)
    return 0; // Unable to read
  // Copy in upper part of lcd ram
  char * ptr=((char *) lcd_Ram)+LCD_WIDTH_PX*LCD_HEIGHT_PX*3/2; // pointer in vram buffer, s should not exceed 320*240/2
  memcpy(ptr,ptrc,s);
  ptr[s]=0;
  return ptr;
#endif
  // without lcd, using os_MemChk
  char * ptr; int S=os_MemChk((void **)&ptr);
  if (s>=S)
    return 0;
  memcpy(ptr,ptrc,s);
  ptr[s]=0;
  //dbg_printf("data=%x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x\n",ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],ptr[8],ptr[9],ptr[10],ptr[11],ptr[12],ptr[13],ptr[14],ptr[15]);
  return ptr;
#else // STANDALONE
  // old code using fileio
  int h=ti_Open(var, "r");
  if (!h)
    return 0;
  int s=ti_GetSize(h);
  if (s>7){
    //unsigned short u;
    //ti_Read(&u,1,2,h);
    char subtype[8]={0};
    ti_Read(subtype,1,4,h);
    if (strncmp(subtype,"PYCD",4)==0 || strncmp(subtype,"PYSC",4)==0 || strncmp(subtype,"XCAS",4)==0 || strncmp(subtype,"TABL",4)==0){
      unsigned char dx;
      ti_Read(&dx,1,1,h);
      if (dx!=0){
        // skip desktop filename
        char buf[256]={0};
        ti_Read(buf,1,dx-1,h);
        s -= 4+dx;
        //dbg_printf("tiread subtype=%s filename=%s dx=%i effsize=%i\n",subtype,buf,dx,s);
      }
      else
        s -= 4;
    }
    else
      ti_Seek(0,SEEK_SET,h);
  }
  char * ptr=0;
  // Code requiring a copy
  // if it starts with 
  // char * ptr=(char *) gfx_vram+LCD_WIDTH_PX*LCD_HEIGHT_PX; // pointer in vram buffer
  int S=os_MemChk((void **)&ptr);
  if (s>=S)
    return 0;
  S=ti_Read(ptr,1,s,h);
  ptr[S]=0;
  ti_Close(h);
  //dbg_printf("data=%x %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x\n",ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],ptr[8],ptr[9],ptr[10],ptr[11],ptr[12],ptr[13],ptr[14],ptr[15]);
  return ptr;
#endif // fileio
}
int write_file(const char * filename,const char * s,int len){
  if (!len)
    len=strlen(s)+1;
  //dbg_printf("write_file file=%s len=%i\n",filename,len);
  // find extension
  const char * ext=0;
  int l=strlen(filename);
  char var[32]={0};
  strncpy(var,filename,8);
  for (--l;l>0;--l){
    if (filename[l]=='.'){
      ext=filename+l+1;
      if (l<9)
        var[l]=0;
      break;
    }
  }
  //dbg_printf("write_file var=%s fname=%s ext=%s\n",var,filename,ext);
#ifdef STANDALONE
  // first erase filename
  int i=file_exists(var);
  //dbg_printf("write_file exist=%i\n",i);
  if (i==2){
    warn_archived(filename);
    // appvar in archive should be unarchived first
    return 0; 
  }
  if (i==1)
    os_DelAppVar(var);
  int effsize=len;
  bool ispy=false,isxw=false,istab=false,with_filename=false;
  if (ext){
    ispy=strncmp(ext,"py",2)==0;
    isxw=strncmp(ext,"xw",2)==0;
    istab=strncmp(ext,"tab",3)==0;
    effsize += 5; // 4 + 1 byte for size of filename
    if (with_filename)
      effsize += strlen(filename)+1;
  }
  //dbg_printf("write_file ext=%s\n",ext);
  var_t * ptr_=os_CreateAppVar(var, effsize);
  if (!ptr_){
#ifndef WITH_QUAD    
    dbg_printf("Unable to write appvar %s\n",var);
#endif
    return false;
  }
  //dbg_printf("write_file var=%s file=%s size=%i effsize=%i\n",var,filename,len,effsize);
  char * ptrc=ptr_->data;
  if (ispy || isxw || istab){
    strncpy(ptrc,istab?"TABL":(isxw?"XCAS":"PYCD"),4); ptrc+=4;
    if (with_filename){
      *ptrc=strlen(filename)+1;
      ++ptrc;
      strncpy(ptrc,filename,strlen(filename)); ptrc+=strlen(filename);
    }
    else {
      *ptrc=0;
      ++ptrc;
    }
  }
  memcpy(ptrc,s,len);
  //dbg_printf("strlen=%i len=%i ptrc[len-1]=%x\n",strlen(s),len,ptrc[len-1]);
#else
  //dbg_printf("tiwrite %s\n",filename);
  int h=ti_Open(var,"w");
  if (!h) return false;
  if (ext){
    bool ispy=strncmp(ext,"py",2)==0;
    bool isxw=strncmp(ext,"xw",2)==0;
    if (ispy || isxw || istab){
      const char * subtype=istab?"TABL":(isxw?"XCAS":"PYCD");
      //dbg_printf("type %s\n",subtype);
      ti_Write(subtype,strlen(subtype),1,h);
      unsigned char dx=strlen(filename)+1;
      ti_Write(&dx,1,1,h);
      ti_Write(filename,dx-1,1,h);
    }
  }
  //dbg_printf("len=%i %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x \n",len,s[0]&0xff,s[1]&0xff,s[2]&0xff,s[3]&0xff,s[4]&0xff,s[5]&0xff,s[6]&0xff,s[7]&0xff,s[8]&0xff,s[9]&0xff,s[10]&0xff,s[11]&0xff,s[12]&0xff,s[13]&0xff,s[14]&0xff,s[15]&0xff);
  int Len=ti_Write(s,1,len,h);
  ti_Close(h);
  return Len==len;
#endif
}

inline bool isalpha(char c){
  return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

int os_file_browser(const char ** filenames,int maxrecords,const char * extension,int storage){
  if (extension && extension[0]=='*' && extension[1]=='.')
    extension+=2;
  //dbg_printf("os_file_browser max=%i extension=%s\n",maxrecords,extension);
  if (maxrecords>FILENAME_MAXRECORDS)
    maxrecords=FILENAME_MAXRECORDS;
  void * ptr=os_GetSymTablePtr();
  int cur=0;
  for (int count=0;cur<maxrecords && ptr;count++){
    uint24_t type, l,j;
    char s[16]={0};
    char * dataptr=0;
    char * ext=0;
    uint8_t * ptr2 = (uint8_t*)ptr - 1;
    ptr=os_NextSymEntry(ptr, &type, &l, s,&dataptr);
    //    if (isalpha(s[0])) dbg_printf("os_file_browser name=%s l=%i type=%i dataptr=%x [count=%i cur=%i ptr=%x ptr2=%x]\n",s,l,type,dataptr,count,cur,ptr,ptr2);
    if (l>=FILENAME_MAXSIZE || !dataptr || !isalpha(s[0]) || type!=21
        | *ptr2
        )
      continue;
    s[l]=0;
    var_t * ptr_=os_GetAppVarData(s,0);
    dataptr=ptr_->data-2;
    //dbg_printf("filebrowser varname=%s type=%i data=%x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x\n",s,type,dataptr[0]&0xff,dataptr[1]&0xff,dataptr[2]&0xff,dataptr[3]&0xff,dataptr[4]&0xff,dataptr[5]&0xff,dataptr[6]&0xff,dataptr[7]&0xff,dataptr[8]&0xff,dataptr[9]&0xff,dataptr[10]&0xff,dataptr[11]&0xff,dataptr[12]&0xff,dataptr[13]&0xff,dataptr[14]&0xff,dataptr[15]&0xff);
    // if type==21 dataptr[1]*256+dataptr[0]==size, then data
    // xcas session begins with 4 bytes size, on the 83 should be 00 00 xx xx
    if (type==21 && dataptr[2]==0 && dataptr[3]==0)
      ext="xw";
    // python app, starts with 2 bytes size, "PYCD" or "PYSC"
    // the script ifself begins at data.begin() + 6 + scriptOffset
    // where scriptOffset = dataptr[6] + 1
    if (!ext){
      if (strncmp(&dataptr[2],"PYCD",4)==0 || strncmp(&dataptr[2],"PYSC",4)==0)
        ext="py";
      else if (strncmp(&dataptr[2],"XCAS",4)==0)
        ext="xw";
      else if (strncmp(&dataptr[2],"TABL",4)==0)
        ext="tab";
      else { // extension from filename _xw or _py or _...
        //dbg_printf("os_file_browser2 %i %i %x\n",type,l,dataptr);
        //dbg_printf("filename2 %i %s\n",count,s);
        for (j=l-1;j>0;--j){
          if (s[j]=='_'){
            ext=s+j+1;
            break;
          }
        }
      }
    }
    //dbg_printf("filebrowser ext=%s [arg extension=%s]\n",ext,extension);
    if (ext && strcmp(ext,extension)==0){
#ifndef TICE
      if (exam_mode &&
          (strcmp(s,"session")!=0
           )
          )
        continue;
#endif
      strncpy(os_filenames[cur],s,FILENAME_MAXSIZE);
      filenames[cur]=os_filenames[cur];
      //dbg_printf("extension match %i %s %s\n",cur,s,filenames[cur]);
      ++cur;
    }
  }
  //dbg_printf("filebrowser end %i\n",cur);
  return cur;
}
// gfx_Begin, gfx_SetDrawBuffer(); gfx_End
// GFX_LCD_WIDTH, HEIGHT, gfx_vbuffer=LCD RAM buffer 76800 bytes
// gfx_vram Total of 153600 bytes in size = 320x240x2
// gfx_SetDrawBuffer()gfx_SetDrawScreen()
// uint8_t gfx_SetColor(uint8_t index)
// gfx_SetPixel(uint24_t x, uint8_t y)
// uint8_t gfx_GetPixel(uint24_t x, uint8_t y)
// gfx_FillRectangle(int x, int y, int width, int height)
// gfx_FillRectangle_NoClip(uint24_t x, uint8_t y, uint24_t width, uint8_t height)
// gfx_Wait(void)
// gfx_PrintStringXY(const char *string, int x, int y)
//gfx_SetTextFGColor(uint8_t color)
// gfx_SetTextScale(uint8_t width_scale, uint8_t height_scale)
// gfx_SetTextConfig
void sync_screen(){
  //gfx_Wait();
  // gfx_BlitBuffer(); // shoud be done if gfx_SetDrawBuffer() is active;
}
int c_rgb565to888(int c){
  c &= 0xffff;
  int r=(c>>11)&0x1f,g=(c>>5)&0x3f,b=c&0x1f;
  return (r<<19)|(g<<10)|(b<<3);
}

int convertcolor(int c){
  // convert 16 bits to default palette
  c &= 0xffff;
  int r=(c>>11)&0x1f,g=(c>>5)&0x3f,b=c&0x1f;
  int R = ((r>>3)<<5) | ((g>>3)<<2) | (b>>3);
  //dbg_printf("convertcolor rgb565=%x r=%i/32 g=%i/64 b=%i/32 to rgb232=%i palette[]=%x\n",c,r,g,b,R,lcd_Palette[R]);
  return R;
}

int convertbackcolor(int c){
  c &= 0x7f;
  int r=(c>>5)&0x3,g=(c>>2)&0x7,b=c&0x3;
  r <<= 3; g <<= 3; b <<= 3;
  int R = (r<<11) | (g<<5) | b;
  //dbg_printf("convert %i r=%i g=%i b=%i to %i\n",c,r,g,b,R);
  return R;
  
}

#ifdef STANDALONE
  void os_set_pixel(int x,int y,int c){
    vGL_set_pixel(x,y,convertcolor(c));
  }
#else
void setcolor(int c){
  gfx_SetColor(convertcolor(c));
  //gfx_SetTextTransparentColor(0);
}
void os_set_pixel(int x,int y,int c){
  setcolor(c);
  gfx_SetPixel(x,y);
}
#endif

void os_fill_rect(int x,int y,int w,int h,int c){
#ifdef STANDALONE
  if (x<0){
    w+=x;
    x=0;
  }
  if (y<0){
    h+=y;
    y=0;
  }
  vGL_setArea(x,y,x+w,y+h,convertcolor(c));
#else
  setcolor(c);
  gfx_FillRectangle(x,y,w,h);
#endif
}
int os_get_pixel(int x,int y){
#ifdef STANDALONE
  return convertbackcolor(vGL_get_pixel(x,y));
#else
  return convertbackcolor(gfx_GetPixel(x,y));
#endif
}

#ifdef STANDALONE
int os_draw_string(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  if (!fake)
    vGL_putString(x,y,s,convertcolor(c),convertcolor(bg),16);
  return x+8*strlen(s);
}

int os_draw_string_medium(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  if (!fake)
    vGL_putString(x,y,s,convertcolor(c),convertcolor(bg),14);
  return x+7*strlen(s);
}

int os_draw_string_small(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  if (!fake)
    vGL_putString(x,y,s,convertcolor(c),convertcolor(bg),8);
  return x+5*strlen(s);
}

#else
void printstringxdxy(const char * s,int x,int dx,int y){
  if (x+dx>LCD_WIDTH_PX){
    char buf[64];
    int pos=sizeof(buf)-1;
    strncpy(buf,s,pos);
    buf[pos]=0;
    for (;pos>=1 && x+gfx_GetStringWidth(buf)>LCD_WIDTH_PX;--pos)
      buf[pos]=0;
    //dbg_printf("buf printstringxdxy %s x=%i dx=%i y=%i\n",buf,x,dx,y);
    gfx_PrintStringXY(buf,x,y);
  }
  else {
    //dbg_printf("s printstringxdxy %s x=%i dx=%i y=%i\n",s,x,dx,y);
    gfx_PrintStringXY(s,x,y);
  }
}

static inline int minint(int a,int b){
  return a<b?a:b;
}

// FIXME? use gfx_SetTransparentColor with a value != FG and BG instead of fill rectangle
int os_draw_string_small(int x,int y,int c,int bg,const char * s,int fake){
  //dbg_printf("small at %i,%i c=%i bg=%i %s\n",x,y,c,bg,s);
  y+=STATUS_AREA_PX;
  gfx_SetTextScale(1,1);
  int dx=gfx_GetStringWidth(s);
  if (x<LCD_WIDTH_PX && !fake){
    setcolor(bg);
    gfx_FillRectangle(x,y,minint(dx,LCD_WIDTH_PX-x),8);
    int c_=gfx_SetTextFGColor(convertcolor(c));
    int bg_=gfx_SetTextBGColor(convertcolor(bg));
    printstringxdxy(s,x,dx,y);
    gfx_SetTextFGColor(c_);
    gfx_SetTextBGColor(bg_);
  }
  return x+dx; 
}

int os_draw_string_medium(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  gfx_SetTextScale(1,2);
  //gfx_SetFontHeight(12);
  int dx=gfx_GetStringWidth(s);
  if (x<LCD_WIDTH_PX && !fake){
    setcolor(bg);
    int effdx=minint(dx,LCD_WIDTH_PX-x);
    //dbg_printf("draw_string_medium rect x=%i y=%i effdx=%i\n",x,y,effdx);
    gfx_FillRectangle(x,y,effdx,16);
    int c_=gfx_SetTextFGColor(convertcolor(c));
    int bg_=gfx_SetTextBGColor(convertcolor(bg));
    printstringxdxy(s,x,dx,y);
    gfx_SetTextFGColor(c_);
    gfx_SetTextBGColor(bg_);
  }
  return x+dx; 
}
int os_draw_string(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  gfx_SetTextScale(2,2);
  int dx=gfx_GetStringWidth(s);
  if (x<LCD_WIDTH_PX && !fake){
    setcolor(bg);
    gfx_FillRectangle(x,y,minint(dx,LCD_WIDTH_PX-x),16);
    int c_=gfx_SetTextFGColor(convertcolor(c));
    int bg_=gfx_SetTextBGColor(convertcolor(bg));
    printstringxdxy(s,x,dx,y);
    gfx_SetTextFGColor(c_);
    gfx_SetTextBGColor(bg_);
  }
  return x+dx; 
}
#endif

const int statuscolor=2016;
void statuslinemsg(const char * msg,int warncolor){
  os_fill_rect(0,0,154,16,SDK_BLACK);
  int l=strlen(msg);
  if (l<=22)
    os_draw_string_medium(0,-STATUS_AREA_PX,warncolor?warncolor:statuscolor,SDK_BLACK,msg,false);
  else {
    char buf[64]={0};
    strncpy(buf,msg,35);
    buf[36]=buf[35]='.';
    os_draw_string_small(0,3-STATUS_AREA_PX,warncolor?warncolor:statuscolor,SDK_BLACK,buf,false);
  }
  os_fill_rect(0,16,154,2,COLOR_WHITE);
}

void set_time(int h,int m){
  int s=rtc_Seconds,d=rtc_Days,month,year;
  boot_GetDate(&d,&month,&year);
  //dbg_printf("set_time s=%i m=%i h=%i d=%i\n",s,m,h,d);
  boot_SetTime(s,m,h);
  boot_SetDate(d,month,year);
  //rtc_Set(s,m,h,d);
}

void get_time(int *h,int *m){
  *h=rtc_Hours;
  *m=rtc_Minutes;
}

void display_time(){
  int h=rtc_Hours,m=rtc_Minutes;
  char msg[10];
  msg[0]=' ';
  msg[1]='0'+(h/10);
  msg[2]='0'+(h%10);
  msg[3]= 'h';
  msg[4]= ('0'+(m/10));
  msg[5]= ('0'+(m%10));
  msg[6]=0;
  //msg[6]= 'm';
  //msg[7] = ('0'+(s/10));
  //msg[8] = ('0'+(s%10));
  //msg[9]=0;
  os_draw_string_medium(260,-STATUS_AREA_PX,statuscolor,SDK_BLACK,msg,false);
}

void display_flags(){
  const char *msg=0;
  if (alpha==2){
    msg=alphalock?"alock ":"alpha ";
  }
  else if (alpha==1){
    msg=alphalock?"ALOCK ":"ALPHA ";
  }
  else {
    if (shift)
      msg=" 2nd  ";
    else
      msg="      ";
  }
  os_draw_string_medium(220,-STATUS_AREA_PX,statuscolor,SDK_BLACK,msg,false);
}  

void statusflags(){
  os_fill_rect(150,0,LCD_WIDTH_PX-150,16,SDK_BLACK);
  display_flags();
  os_draw_string_medium(150,-STATUS_AREA_PX,statuscolor,SDK_BLACK,os_get_angle_unit()?" rad CAS  ":" deg CAS  ",false);
  display_time();
  int x0=302,w=LCD_WIDTH_PX-x0;
  os_fill_rect(x0,0,6,STATUS_AREA_PX-2,SDK_BLACK);
  x0+=3; w-=6;
  os_fill_rect(x0+w-2,0,5,STATUS_AREA_PX-2,SDK_BLACK);
  os_fill_rect(x0,0,w,2,COLOR_RED);
  os_fill_rect(x0,STATUS_AREA_PX-5,w,2,COLOR_RED);
  os_fill_rect(x0,0,2,STATUS_AREA_PX-4,COLOR_RED);
  os_fill_rect(x0+w,0,2,STATUS_AREA_PX-4,COLOR_RED);
  uint8_t i=boot_GetBatteryStatus(); int color=statuscolor;
  //i=0; // emu check colors
  switch (i){
  case 4:
    color=COLOR_GREEN;
    break;
  case 3:
    color=COLOR_CYAN;
    break;
  case 2:
    color=COLOR_YELLOW;
    break;
  case 1:
    color=COLOR_RED;
    break;
  case 0:
    color=COLOR_MAGENTA;
    break;
  }
  os_fill_rect(x0+4,1+(4-i)*3,w-6,i?i*3:1,color);
  os_fill_rect(0,16,LCD_WIDTH_PX,STATUS_AREA_PX-16,COLOR_WHITE);
  // dbg_printf("Battery level=%i color=%i\n",i,color);
  //os_draw_string_medium(302,-STATUS_AREA_PX,color,SDK_BLACK,msg,false);
}
void statusline(int mode){
  statusflags();
  if (mode==0)
    return;
  sync_screen();
}
#endif

#ifdef NSPIRE_NEWLIB
// NB changes for the nspire cx ii
// on_key_pressed() should be modified (returns always true)
// https://hackspire.org/index.php?title=Memory-mapped_I/O_ports_on_CX_II#90140000_-_Power_management
// cx ii power management 0x90140000,
// cx 900B0018 (R/W), 900B0020 (?)
// cx ii 90140050 (R/W): Disable bus access to peripherals. Reads will just return the last word read from anywhere in the address range, and writes will be ignored.
// cx 900F0020 (R/W): LCD contrast/backlight. Valid range for contrast: 0x11a to 0x1ce; normal value is 0x174. However, it can range from 0x100 (backlight off) to about 0x1d0 (about max brightness).
// -> cx ii The OS controls the LCD backlight by writing to 90130018.
#include <libndls.h>
#include "os.h" // Ndless/ndless-sdk/include/os.h
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <ngc.h>
#include "k_defs.h"


int c_rgb565to888(int c){
  c &= 0xffff;
  int r=(c>>11)&0x1f,g=(c>>5)&0x3f,b=c&0x1f;
  return (r<<19)|(g<<10)|(b<<3);
}

const int nspire_statusarea=18;
int nspireemu=false;

int waitforvblank(){
}

int back_key_pressed(){
  return isKeyPressed(KEY_NSPIRE_DEL);
}
// next 3 functions may be void if not inside a window class hierarchy
void os_show_graph(){} // show graph inside Python shell (Numworks), not used
void os_hide_graph(){} // hide graph, not used anymore
void os_redraw(){} // force redraw of window class hierarchy

int os_set_angle_unit(int mode){
  return false;
}
int os_get_angle_unit(){
  return 0;
}

double millis(){
  unsigned NSPIRE_RTC_ADDR=0x90090000;
  unsigned t1= * (volatile unsigned *) NSPIRE_RTC_ADDR;
  return 1000.0*t1;
}


void get_hms(int *h,int *m,int *s){
  unsigned NSPIRE_RTC_ADDR=0x90090000;
  unsigned t1= * (volatile unsigned *) NSPIRE_RTC_ADDR;
  if (exam_mode){
    unsigned t=t1-exam_start;
    if (exam_duration>0 && t>exam_duration){
      ;//set_exam_mode(0);
    }
    else {
      if (exam_duration>0)
	t1=exam_duration-t;
      else {
	if (exam_duration<0 && t<-exam_duration)
	  t1=-exam_duration-t;
      }	
    }
  }
  unsigned d=t1/86400;
  *s=t1%86400;
  *h=*s/3600;
  *m=(*s-3600* *h)/60;
  *s%=60;
}

void get_time(int *h,int *m){
  int s;
  get_hms(h,m,s);
}

void set_time(int h,int m){
  // FIXME
}

#ifndef is_cx2
#define is_cx2 false
#endif

double loopsleep(int ms){
  double n=ms*(is_cx2?3000:1000),j=0.0;
  for (double i=0;i<n;++i){
    j+=i;
  }
  return j;
}

void ck_msleep(int ms){
  //msleep(ms);
  loopsleep(ms);
}

void os_wait_1ms(int ms){
  ck_msleep(ms);
}

int file_exists(const char * filename){
  if (access(filename,R_OK))
    return false;
  return true;
}

int erase_file(const char * filename){
  return remove(filename)==0;
}

char nspire_filebuf[NSPIRE_FILEBUFFER];

const char * read_file(const char * filename){
  if (exam_mode &&
      (strcmp(filename,"session.xw")!=0 &&
       strcmp(filename,"session.xw.tns")!=0 &&
       strcmp(filename,"session.py")!=0 &&
       strcmp(filename,"session.py.tns")!=0 
       )
      )
    return 0;
  FILE * f = fopen(filename,"r");
  if (!f) return 0;
  fseek(f,0L,SEEK_END);
  unsigned s = ftell(f);
  fseek(f,0L,SEEK_SET);
  if (s>NSPIRE_FILEBUFFER-1){
    fclose(f);    
    return 0;
  }
  for (int i=0;i<s;++i){
    if (feof(f))
      break;
    nspire_filebuf[i]=fgetc(f);
  }
  nspire_filebuf[s]=0;
  fclose(f);
  return nspire_filebuf;
}

int write_file(const char * filename,const char * s,int len){
  if (exam_mode &&
      (strcmp(filename,"session.xw")!=0 &&
       strcmp(filename,"session.xw.tns")!=0 &&
       strcmp(filename,"session.py")!=0 &&
       strcmp(filename,"session.py.tns")!=0 
       )
      )
    return false;
  FILE * f=fopen(filename,"w");
  if (!f) return false;
  if (!len) len=strlen(s);
  for (int i=0;i<len;++i)
    fputc(s[i],f);
  fclose(f);
  return true;
}

#define FILENAME_MAXRECORDS 64
#define FILENAME_MAXSIZE 16
char os_filenames[FILENAME_MAXRECORDS][FILENAME_MAXSIZE];
int c_trialpha(const void *p1,const void * p2){
  int i=strcmp(* (char * const *) p1, * (char * const *) p2);
  return i;
}
int os_file_browser(const char ** filenames,int maxrecords,const char * extension,int storage){ // storage is ignored on nspire
  DIR *dp;
  struct dirent *ep;
  if (maxrecords>FILENAME_MAXRECORDS-1)
    maxrecords=FILENAME_MAXRECORDS-1;
  dp = opendir (".");
  if (dp == NULL){
    filenames[0]=0;
    return 0;
  }
  int cur=0;
  while ( (ep = readdir (dp)) && cur<maxrecords){
    const char * s_=ep->d_name,*ext=0;
    int l=strlen(s_),j;
    char s[l+1];
    strcpy(s,s_);
    for (j=l-1;j>0;--j){
      if (s[j]=='.'){
	ext=s+j+1;
	break;
      }
    }
    if (ext && strcmp(ext,"tns")==0){
      s[j]=0;
      for (;j>0;--j){
	if (s[j]=='.'){
	  ext=s+j+1;
	  break;
	}
      }
    }
    if (ext && strcmp(ext,extension)==0){
      if (exam_mode &&
	  (strcmp(s_,"session.xw")!=0 &&
	   strcmp(s_,"session.xw.tns")!=0 &&
	   strcmp(s_,"session.py")!=0 &&
	   strcmp(s_,"session.py.tns")!=0 
	   )
	  )
	continue;
      strncpy(os_filenames[cur],s_,FILENAME_MAXSIZE);
      filenames[cur]=os_filenames[cur];
      ++cur;
    }
  }
  closedir (dp);
  filenames[cur]=NULL;
#if 0
  qsort(filenames,cur,sizeof(char *),c_trialpha);
#else
  // qsort would be faster for large n, but here n<FILENAME_MAXRECORDS
  for (;;){
    int finished=true;
    for (int i=1;i<cur;++i){
      if (strcmp(filenames[i-1],filenames[i])>0){
	finished=false;
	const char * tmp=filenames[i-1];
	filenames[i-1]=filenames[i];
	filenames[i]=tmp;
      }
    }
    if (finished)
      break;
  }
#endif
  return cur;
}

Gc nspire_gc=0;

void reset_gc(){
  if (nspire_gc){
    gui_gc_finish(nspire_gc);
    //gui_gc_free(nspire_gc);
  }
  nspire_gc=0;
}

Gc * get_gc(){
  if (!nspire_gc){
    nspire_gc=gui_gc_global_GC();
    gui_gc_setRegion(nspire_gc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gui_gc_begin(nspire_gc);
  }
  return &nspire_gc;
}

void os_set_pixel(int x,int y,int c){
  get_gc();
  gui_gc_setColor(nspire_gc,c_rgb565to888(c));
  gui_gc_drawRect(nspire_gc,x,y+nspire_statusarea,0,0);
}

void os_fill_rect(int x,int y,int w,int h,int c){
  get_gc();
  gui_gc_setColor(nspire_gc,c_rgb565to888(c));
  gui_gc_fillRect(nspire_gc,x,y+nspire_statusarea,w,h);
}

int os_get_pixel(int x,int y){
  if (x<0 || x>=SCREEN_WIDTH || y<0 || y>=SCREEN_HEIGHT)
    return -1;
#if 1
  get_gc();
  char ** off_buff = ((((char *****)nspire_gc)[9])[0])[0x8];
  int res = *(unsigned short *) (off_buff[y+nspire_statusarea] + 2*x);
  return res;
#else
  unsigned short * addr=*(unsigned short **) 0xC0000010;
  int r=addr[(y+nspire_statusarea)*SCREEN_WIDTH+x];
  return r;
#endif
}

int nspire_draw_string(int x,int y,int c,int bg,int f,const char * s,int fake){
  // void ascii2utf16(void *buf, const char *str, int max_size): converts the UTF-8 string str to the UTF-16 string buf of size max_size.
  int l=strlen(s);
  char utf16[2*l+2];
  ascii2utf16(utf16,s,l);
  utf16[2*l]=0;
  utf16[2*l+1]=0;
  get_gc();
  gui_gc_setFont(nspire_gc,f);
  int dx=gui_gc_getStringWidth(nspire_gc, f, utf16, 0, l) ;
  if (fake)
    return x+dx;
  int dy=17;
  if (f==Regular9)
    dy=13;
  if (f==Regular11)
    dy=16;
  gui_gc_setColor(nspire_gc,c_rgb565to888(bg));
  gui_gc_fillRect(nspire_gc,x,y,dx,dy);
  gui_gc_setColor(nspire_gc,c_rgb565to888(c));
  //gui_gc_setPen(nspire_gc, GC_PS_MEDIUM, GC_PM_SMOOTH);
  gui_gc_drawString(nspire_gc, utf16, x, y-1, GC_SM_NORMAL | GC_SM_TOP); // normal mode
  return x+dx;
}

int os_draw_string(int x,int y,int c,int bg,const char * s,int fake){
  get_gc();
  gui_gc_clipRect(nspire_gc,0,nspire_statusarea,SCREEN_WIDTH,SCREEN_HEIGHT-nspire_statusarea,0);
  int i=nspire_draw_string(x,y+nspire_statusarea,c,bg,Regular12,s,fake);
  gui_gc_clipRect(nspire_gc,0,0,SCREEN_WIDTH,SCREEN_HEIGHT,GC_CRO_RESET);
  return i;
}
int os_draw_string_small(int x,int y,int c,int bg,const char * s,int fake){
  get_gc();
  gui_gc_clipRect(nspire_gc,0,nspire_statusarea,SCREEN_WIDTH,SCREEN_HEIGHT-nspire_statusarea,GC_CRO_SET);
  int i=nspire_draw_string(x,y+nspire_statusarea,c,bg,Regular9,s,fake);
  gui_gc_clipRect(nspire_gc,0,0,SCREEN_WIDTH,SCREEN_HEIGHT,GC_CRO_RESET);
  return i;
}

int os_draw_string_medium(int x,int y,int c,int bg,const char * s,int fake){
  get_gc();
  gui_gc_clipRect(nspire_gc,0,nspire_statusarea,SCREEN_WIDTH,SCREEN_HEIGHT-nspire_statusarea,GC_CRO_SET);
  int i=nspire_draw_string(x,y+nspire_statusarea,c,bg,Regular11,s,fake);
  gui_gc_clipRect(nspire_gc,0,0,SCREEN_WIDTH,SCREEN_HEIGHT,GC_CRO_RESET);
  return i;
}

void statuslinemsg(const char * msg){
  get_gc();
  int bg=exam_bg();
  gui_gc_setColor(nspire_gc,c_rgb565to888(bg));
  gui_gc_fillRect(nspire_gc,0,0,SCREEN_WIDTH,nspire_statusarea);
  nspire_draw_string(0,0,exam_mode?0xffff:0,bg,Regular9,msg,false);
  if (nspireemu)
    nspire_draw_string(190,0,exam_mode?0xffff:0,bg,Regular9," emu ",false);
  else
    nspire_draw_string(190,0,exam_mode?0xffff:0,bg,Regular9," CAS ",false);    
}

void display_time(){
  int h,m,s;
  get_hms(&h,&m,&s);
  char msg[10];
  msg[0]=' ';
  msg[1]='0'+(h/10);
  msg[2]='0'+(h%10);
  msg[3]= 'h';
  msg[4]= ('0'+(m/10));
  msg[5]= ('0'+(m%10));
  msg[6]=0;
  //msg[6]= 'm';
  //msg[7] = ('0'+(s/10));
  //msg[8] = ('0'+(s%10));
  //msg[9]=0;
  int bg=exam_bg();
  gui_gc_setColor(nspire_gc,c_rgb565to888(bg));
  gui_gc_fillRect(nspire_gc,270,0,SCREEN_WIDTH-270,nspire_statusarea);
  nspire_draw_string(270,0,exam_mode?0xffff:0,bg,Regular9,msg,false);
}

void sync_screen(){
  get_gc();
  //gui_gc_finish(nspire_gc);
  gui_gc_blit_to_screen(nspire_gc);
  ck_msleep(10);
  //nspire_gc=0;
  // gui_gc_begin(nspire_gc);
}

// Nspire peripheral reset :
// https://github.com/nDroidProject/nDroid-bootloader/blob/master/kernel.c
// https://hackspire.org/index.php?title=Memory-mapped_I/O_ports_on_CX#CC000000_-_SHA-256_hash_generator
// hardware ports
// https://hackspire.org/index.php?title=Memory-mapped_I/O_ports_on_CX

int nspire_shift=false;
int nspire_ctrl=false;
int nspire_select=false;
void statusflags(){
  char *msg=0;
  if (nspire_ctrl){
    if (nspire_shift)
      msg="shift ctrl";
    else
      msg="        ctrl";
  }
  else {
    if (nspire_shift)
      msg="shift";
    else
      msg="";
  }
  int bg=exam_bg();
  gui_gc_setColor(nspire_gc,c_rgb565to888(bg));
  gui_gc_fillRect(nspire_gc,210,0,SCREEN_WIDTH-210,nspire_statusarea);
  nspire_draw_string(224,0,exam_mode?0xffff:0,bg,Regular9,msg,false);
  if (nspireemu)
    nspire_draw_string(190,0,exam_mode?0xffff:0,bg,Regular9," emu ",false);
  else
    nspire_draw_string(190,0,0xf800,bg,Regular9," CAS ",false);    
}
void statusline(int mode){
  statusflags();
  display_time();
  if (mode==0)
    return;
  sync_screen();
}


#define SHIFTCTRL(x, y, z) (nspire_ctrl ? (z) : nspire_shift ? (y) : (x))
#define SHIFT(x, y) SHIFTCTRL(x, y, x)
#define CTRL(x, y) SHIFTCTRL(x, x, y)
#define NORMAL(x) SHIFTCTRL(x, x, x)

int ascii_get(int* adaptive_cursor_state){
  if (isKeyPressed(KEY_NSPIRE_CTRL)){
    nspire_ctrl=!nspire_ctrl;
    statusline(0);
    sync_screen();
    return -2;
  }
  if (isKeyPressed(KEY_NSPIRE_SHIFT)){
    nspire_shift=!nspire_shift;
    statusline(0);
    sync_screen();
    return -1;
  }
  *adaptive_cursor_state = SHIFTCTRL(0, 1, 4);
  if (isKeyPressed(KEY_NSPIRE_LEFT)|| isKeyPressed(KEY_NSPIRE_LEFTUP) || isKeyPressed(KEY_NSPIRE_DOWNLEFT))		return SHIFTCTRL(KEY_CTRL_LEFT,KEY_SHIFT_LEFT,KEY_LEFT_CTRL);
  if (isKeyPressed(KEY_NSPIRE_RIGHT)|| isKeyPressed(KEY_NSPIRE_UPRIGHT) || isKeyPressed(KEY_NSPIRE_RIGHTDOWN))		return SHIFTCTRL(KEY_CTRL_RIGHT,KEY_SHIFT_RIGHT,KEY_RIGHT_CTRL);
  if (isKeyPressed(KEY_NSPIRE_UP))		return SHIFTCTRL(KEY_CTRL_UP,KEY_CTRL_PAGEUP,KEY_UP_CTRL);
  if (isKeyPressed(KEY_NSPIRE_DOWN))		return SHIFTCTRL(KEY_CTRL_DOWN,KEY_CTRL_PAGEDOWN,KEY_DOWN_CTRL);
	
  if (isKeyPressed(KEY_NSPIRE_ESC)) return KEY_CTRL_EXIT ;
  if (isKeyPressed(KEY_NSPIRE_HOME)) return KEY_CTRL_MENU ;
  if (isKeyPressed(KEY_NSPIRE_MENU)) return KEY_CTRL_CATALOG ;
  
  // Characters
  if (isKeyPressed(KEY_NSPIRE_A)) return SHIFTCTRL('a','A',KEY_CTRL_A);
  if (isKeyPressed(KEY_NSPIRE_B)) return SHIFTCTRL('b','B',KEY_BOOK);
  if (isKeyPressed(KEY_NSPIRE_C)) return SHIFTCTRL('c','C',KEY_CTRL_CLIP);
  if (isKeyPressed(KEY_NSPIRE_D)) return SHIFTCTRL('d','D',KEY_CTRL_D);
  if (isKeyPressed(KEY_NSPIRE_E)) return SHIFTCTRL('e','E',KEY_CTRL_F10);
  if (isKeyPressed(KEY_NSPIRE_F)) return SHIFTCTRL('f','F',KEY_CTRL_F11);
  if (isKeyPressed(KEY_NSPIRE_G)) return SHIFTCTRL('g','G',KEY_CTRL_F12);
  if (isKeyPressed(KEY_NSPIRE_H)) return SHIFTCTRL('h','H',KEY_CTRL_F13);
  if (isKeyPressed(KEY_NSPIRE_I)) return SHIFTCTRL('i','I',KEY_CTRL_F14);
  if (isKeyPressed(KEY_NSPIRE_J)) return SHIFTCTRL('j','J',KEY_CTRL_F15);
  if (isKeyPressed(KEY_NSPIRE_K)) return SHIFTCTRL('k','K',KEY_CTRL_AC);
  if (isKeyPressed(KEY_NSPIRE_L)) return SHIFTCTRL('l','L',KEY_CTRL_F14);
  if (isKeyPressed(KEY_NSPIRE_M)) return SHIFTCTRL('m','M',KEY_CTRL_CATALOG);
  if (isKeyPressed(KEY_NSPIRE_N)) return SHIFTCTRL('n','N',KEY_CTRL_N);
  if (isKeyPressed(KEY_NSPIRE_O)) return SHIFTCTRL('o','O',KEY_SHIFT_OPTN);
  if (isKeyPressed(KEY_NSPIRE_P)) return SHIFTCTRL('p','P',KEY_CTRL_PRGM);
  if (isKeyPressed(KEY_NSPIRE_Q)) return SHIFT('q','Q');
  if (isKeyPressed(KEY_NSPIRE_R)) return SHIFTCTRL('r','R',KEY_CTRL_R);
  if (isKeyPressed(KEY_NSPIRE_S)) return SHIFTCTRL('s','S',KEY_CTRL_S);
  if (isKeyPressed(KEY_NSPIRE_T)) return SHIFTCTRL('t','T',KEY_CTRL_T);
  if (isKeyPressed(KEY_NSPIRE_U)) return SHIFTCTRL('u','U',KEY_CTRL_F13);
  if (isKeyPressed(KEY_NSPIRE_V)) return SHIFTCTRL('v','V',KEY_CTRL_PASTE);
  if (isKeyPressed(KEY_NSPIRE_W)) return SHIFT('w','W');
  if (isKeyPressed(KEY_NSPIRE_X)) return SHIFTCTRL('x','X',KEY_CTRL_CUT);
  if (isKeyPressed(KEY_NSPIRE_Y)) return SHIFT('y','Y');
  if (isKeyPressed(KEY_NSPIRE_Z)) return SHIFTCTRL('z','Z',KEY_CTRL_UNDO);

  // Numbers
  if (nspireemu){ // for firebird, redefine ctrl
    if (isKeyPressed(KEY_NSPIRE_0)) return SHIFTCTRL('0',KEY_CTRL_F10,')');
    if (isKeyPressed(KEY_NSPIRE_1)) return SHIFTCTRL('1',KEY_CTRL_F1,'!');
    if (isKeyPressed(KEY_NSPIRE_2)) return SHIFTCTRL('2',KEY_CTRL_F2,'@');
    if (isKeyPressed(KEY_NSPIRE_3)) return SHIFTCTRL('3',KEY_CTRL_F3,'#');
    if (isKeyPressed(KEY_NSPIRE_4)) return SHIFTCTRL('4',KEY_CTRL_F4,'$');
    if (isKeyPressed(KEY_NSPIRE_5)) return SHIFTCTRL('5',KEY_CTRL_F5,'%');
    if (isKeyPressed(KEY_NSPIRE_6)) return SHIFTCTRL('6',KEY_CTRL_F6,'^');
    if (isKeyPressed(KEY_NSPIRE_7)) return SHIFTCTRL('7',KEY_CTRL_F7,'&');
    if (isKeyPressed(KEY_NSPIRE_8)) return SHIFTCTRL('8',KEY_CTRL_F8,'*');
    if (isKeyPressed(KEY_NSPIRE_9)) return SHIFTCTRL('9',KEY_CTRL_F9,'(');
  }
  else {
    if (isKeyPressed(KEY_NSPIRE_0)) return SHIFTCTRL('0',KEY_CTRL_F10,KEY_CTRL_F10);
    if (isKeyPressed(KEY_NSPIRE_1)) return SHIFTCTRL('1',KEY_CTRL_F1,KEY_CTRL_F1);
    if (isKeyPressed(KEY_NSPIRE_2)) return SHIFTCTRL('2',KEY_CTRL_F2,KEY_CTRL_F2);
    if (isKeyPressed(KEY_NSPIRE_3)) return SHIFTCTRL('3',KEY_CTRL_F3,KEY_CTRL_F3);
    if (isKeyPressed(KEY_NSPIRE_4)) return SHIFTCTRL('4',KEY_CTRL_F4,KEY_CTRL_F4);
    if (isKeyPressed(KEY_NSPIRE_5)) return SHIFTCTRL('5',KEY_CTRL_F5,KEY_CTRL_F5);
    if (isKeyPressed(KEY_NSPIRE_6)) return SHIFTCTRL('6',KEY_CTRL_F6,KEY_CTRL_F6);
    if (isKeyPressed(KEY_NSPIRE_7)) return SHIFTCTRL('7',KEY_CTRL_F7,KEY_CTRL_F7);
    if (isKeyPressed(KEY_NSPIRE_8)) return SHIFTCTRL('8',KEY_CTRL_F8,KEY_CTRL_F8);
    if (isKeyPressed(KEY_NSPIRE_9)) return SHIFTCTRL('9',KEY_CTRL_F9,KEY_CTRL_F9);
  }
  
  // Symbols
  if (isKeyPressed(KEY_NSPIRE_FRAC)) return SHIFTCTRL(KEY_EQW_TEMPLATE,KEY_AFFECT,KEY_AFFECT);
  if (isKeyPressed(KEY_NSPIRE_SQU)) return CTRL(KEY_CHAR_SQUARE,KEY_CHAR_ROOT);
  if (isKeyPressed(KEY_NSPIRE_TENX)) return CTRL(KEY_CHAR_EXPN10,KEY_CHAR_LOG);
  if (isKeyPressed(KEY_NSPIRE_eEXP)) return CTRL(KEY_CHAR_EXPN,KEY_CHAR_LN);
  if (isKeyPressed(KEY_NSPIRE_COMMA))		return SHIFTCTRL(',',';',':');
  if (isKeyPressed(KEY_NSPIRE_PERIOD)) 	return SHIFTCTRL('.',KEY_CTRL_F11,':');
  if (isKeyPressed(KEY_NSPIRE_COLON))		return NORMAL(':');
  if (isKeyPressed(KEY_NSPIRE_LP))			return SHIFTCTRL('(',KEY_CTRL_F13,KEY_CHAR_CROCHETS);
  if (isKeyPressed(KEY_NSPIRE_RP))			return SHIFTCTRL(')',KEY_CTRL_F14,KEY_CHAR_ACCOLADES);
  if (isKeyPressed(KEY_NSPIRE_SPACE))		return SHIFTCTRL(' ','_','_');
  if (isKeyPressed(KEY_NSPIRE_DIVIDE))
    return SHIFTCTRL('/','%','\\');
  if (isKeyPressed(KEY_NSPIRE_MULTIPLY))	return SHIFTCTRL('*','\'','\"');
  if (isKeyPressed(KEY_NSPIRE_MINUS))		return SHIFTCTRL('-','_', '<');
  if (isKeyPressed(KEY_NSPIRE_NEGATIVE))	return SHIFTCTRL('-',KEY_CTRL_F12,KEY_CHAR_ANS);
  if (isKeyPressed(KEY_NSPIRE_PLUS))		return SHIFTCTRL('+', KEY_CHAR_NORMAL,'>');
  if (isKeyPressed(KEY_NSPIRE_EQU))		return SHIFTCTRL('=', '|',KEY_CHAR_STORE);
  if (isKeyPressed(KEY_NSPIRE_LTHAN))		return NORMAL('<');
  if (isKeyPressed(KEY_NSPIRE_GTHAN))		return NORMAL('>');
  if (isKeyPressed(KEY_NSPIRE_QUOTE))		return NORMAL('\"');
  if (isKeyPressed(KEY_NSPIRE_APOSTROPHE))	return NORMAL('\'');
  if (isKeyPressed(KEY_NSPIRE_QUES))		return SHIFTCTRL('?','|','!');
  if (isKeyPressed(KEY_NSPIRE_QUESEXCL))	return SHIFTCTRL('?','|','!');
  if (isKeyPressed(KEY_NSPIRE_BAR))		return NORMAL('|');
  if (isKeyPressed(KEY_NSPIRE_EXP))		return SHIFT('^',KEY_CHAR_RECIP);
  if (isKeyPressed(KEY_NSPIRE_EE))		return SHIFTCTRL('&','%', '@');
  if (isKeyPressed(KEY_NSPIRE_PI)) return KEY_CHAR_PI;
  if (isKeyPressed(KEY_NSPIRE_FLAG)) return SHIFTCTRL(';',':',KEY_CHAR_IMGNRY);
  if (isKeyPressed(KEY_NSPIRE_ENTER))		return SHIFTCTRL(KEY_CTRL_OK,'~',KEY_CTRL_EXE);
  if (isKeyPressed(KEY_NSPIRE_TRIG))		return SHIFTCTRL(KEY_CHAR_SIN,KEY_CHAR_COS,KEY_CHAR_TAN);
  
  // Special chars
  if (isKeyPressed(KEY_NSPIRE_SCRATCHPAD)) return SHIFTCTRL(KEY_CTRL_SETUP,KEY_LOAD,KEY_SAVE);
  if (isKeyPressed(KEY_NSPIRE_VAR)) return SHIFTCTRL(KEY_CTRL_VARS,KEY_CHAR_FACTOR,KEY_CHAR_STORE);
  if (isKeyPressed(KEY_NSPIRE_DOC))		return SHIFT(KEY_CTRL_MENU,KEY_CTRL_SD);
  if (isKeyPressed(KEY_NSPIRE_CAT))		return KEY_BOOK;
  if (isKeyPressed(KEY_NSPIRE_DEL))		return SHIFTCTRL(KEY_CTRL_DEL,KEY_CTRL_DEL,KEY_CTRL_AC);
  if (isKeyPressed(KEY_NSPIRE_RET))		return KEY_CTRL_EXE;
  if (isKeyPressed(KEY_NSPIRE_TAB))		return '\t';
  
  return 0;
}

int handle_f5(){}
int iskeydown(int key){
  t_key t=KEY_NSPIRE_SPACE;
  switch (key){
  case 0:
    t=KEY_NSPIRE_LEFT;
    break;
  case 1:
    t=KEY_NSPIRE_UP;
    break;
  case 2:
    t=KEY_NSPIRE_DOWN;
    break;
  case 3:
    t=KEY_NSPIRE_RIGHT;
    break;
  case 4:
    t=KEY_NSPIRE_ENTER;
    break;
  case 5:
    t=KEY_NSPIRE_ESC;
    break;
  case 6:
    t=KEY_NSPIRE_HOME;
    break;
  case 7:
    t=KEY_NSPIRE_MENU;
    break;
  case 12:
    t=KEY_NSPIRE_SHIFT;
    break;
  case 13:
    t=KEY_NSPIRE_CTRL;
    break;
  case 14:
    t=KEY_NSPIRE_SCRATCHPAD;
    break;
  case 15:
    t=KEY_NSPIRE_VAR;
    break;
  case 16:
    t=KEY_NSPIRE_DOC;
    break;
  case 17:
    t=KEY_NSPIRE_DEL;
    break;
  case 18:
    t=KEY_NSPIRE_eEXP;
    break;
  case 19:
    t=KEY_NSPIRE_EQU;
    break;
  case 20:
    t=KEY_NSPIRE_TENX;
    break;
  case 21:
    t=KEY_NSPIRE_I;
    break;
  case 22:
    t=KEY_NSPIRE_COMMA;
    break;
  case 23:
    t=KEY_NSPIRE_EXP;
    break;
  case 24:
    t=KEY_NSPIRE_TRIG;
    break;
  case 25:
    t=KEY_NSPIRE_C;
    break;
  case 26:
    t=KEY_NSPIRE_T;
    break;
  case 27:
    t=KEY_NSPIRE_PI;
    break;
  case 28:
    t=KEY_NSPIRE_S;
    break;
  case 29:
    t=KEY_NSPIRE_SQU;
    break;
  case 30:
    t=KEY_NSPIRE_7;
    break;
  case 31:
    t=KEY_NSPIRE_8;
    break;
  case 32:
    t=KEY_NSPIRE_9;
    break;
  case 33:
    t=KEY_NSPIRE_LP;
    break;
  case 34:
    t=KEY_NSPIRE_RP;
    break;
  case 36:
    t=KEY_NSPIRE_4;
    break;
  case 37:
    t=KEY_NSPIRE_5;
    break;
  case 38:
    t=KEY_NSPIRE_6;
    break;
  case 39:
    t=KEY_NSPIRE_MULTIPLY;
    break;
  case 40:
    t=KEY_NSPIRE_DIVIDE;
    break;
  case 42:
    t=KEY_NSPIRE_1;
    break;
  case 43:
    t=KEY_NSPIRE_2;
    break;
  case 44:
    t=KEY_NSPIRE_3;
    break;
  case 45:
    t=KEY_NSPIRE_PLUS;
    break;
  case 46:
    t=KEY_NSPIRE_MINUS;
    break;
  case 48:
    t=KEY_NSPIRE_0;
    break;
  case 49:
    t=KEY_NSPIRE_PERIOD;
    break;
  case 50:
    t=KEY_NSPIRE_EE;
    break;
  case 51:
    t=KEY_NSPIRE_NEGATIVE;
    break;
  case 52:
    t=KEY_NSPIRE_RET;
    break;
  }
  return isKeyPressed(t);
}


// ? see also ndless-sdk/thirdparty/nspire-io/arch-nspire/nspire.c nio_ascii_get
int getkey(int allow_suspend){
  sync_screen();
  if (shutdown_state)
    return KEY_SHUTDOWN;
  int lastkey=-1;
  unsigned NSPIRE_RTC_ADDR=0x90090000;
  static unsigned lastt=0;
  for (;;){
    unsigned t1= * (volatile unsigned *) NSPIRE_RTC_ADDR;
    if (lastt==0)
      lastt=t1;
    if (t1-lastt>10){
      display_time();
      sync_screen();
    }
    int autosuspend=(t1-lastt>=100);
    if (nspire_exam_mode!=2 &&
	is_cx2 && nspire_ctrl && on_key_pressed()){
      os_fill_rect(50,90,200,40,0x1234);
      nspire_draw_string(60,120,0,0xffff,Regular12,"Quit KhiCAS to shutdown",false);
      nspire_ctrl=false;
      statusline(1);
      continue;
    }
    if ( (nspire_exam_mode==2 || !is_cx2) &&
	allow_suspend && (autosuspend || (nspire_ctrl && on_key_pressed()))){
      nspire_ctrl=nspire_shift=false;
      while (!autosuspend && on_key_pressed())
	loopsleep(10);
      // somewhat OFF by setting LCD to 0
      unsigned NSPIRE_CONTRAST_ADDR=is_cx2?0x90130014:0x900f0020;
      unsigned oldval=*(volatile unsigned *)NSPIRE_CONTRAST_ADDR,oldval2;
      if (is_cx2){
	oldval2=*(volatile unsigned *) (NSPIRE_CONTRAST_ADDR+4);
	*(volatile unsigned *) (NSPIRE_CONTRAST_ADDR+4)=0xffff;
      }
      *(volatile unsigned *)NSPIRE_CONTRAST_ADDR=is_cx2?0xffff:0x100;
      static volatile uint32_t *lcd_controller = (volatile uint32_t*) 0xC0000000;
      lcd_controller[6] &= ~(0b1 << 11);
      loopsleep(20);
      lcd_controller[6] &= ~ 0b1;
      unsigned offtime=* (volatile unsigned *) NSPIRE_RTC_ADDR;
      for (int n=0;!on_key_pressed();++n){
	loopsleep(100);
	idle();
	if (!exam_mode && nspire_exam_mode!=2 && shutdown
	    // && n&0xff==0
	    ){
	  unsigned curtime=* (volatile unsigned *) NSPIRE_RTC_ADDR;
	  if (curtime-offtime>7200){
	    shutdown_state=1;
	    // after 2 hours, leave KhiCAS
	    // that way the OS will really shutdown the calc
	    lcd_controller[6] |= 0b1;
	    loopsleep(20);
	    lcd_controller[6]|= 0b1 << 11;
	    if (is_cx2)
	      *(volatile unsigned *)(NSPIRE_CONTRAST_ADDR+4)=oldval2;
	    *(volatile unsigned *)NSPIRE_CONTRAST_ADDR=oldval;
	    statuslinemsg("Press ON to disable KhiCAS auto shutdown");
	    //os_fill_rect(0,0,320,222,0xffff);
	    sync_screen();
	    int m=0,mmax=150;
	    for (;m<mmax;++m){
	      if (on_key_pressed()){
		break;
	      }
	      loopsleep(100);
	      idle();
	    }
	    if (m==mmax){
	      if (shutdown())
		return KEY_SHUTDOWN;
	    }
	    else {
	      shutdown_state=0;
	      break;
	    }
	  }
	}
      }
      if (nspire_exam_mode==2){
	os_fill_rect(20,30,280,150,0x1234);
	os_fill_rect(25,45,270,121,0xffff);
	nspire_draw_string(25,60,0,0xffff,Regular12,"Mode examen de KhiCAS, avec CAS.",false);
	nspire_draw_string(25,77,0,0xffff,Regular12,"Mode conforme à la règlementation",false);
	nspire_draw_string(25,94,0,0xffff,Regular12,"du bac en France (les fichiers non",false);
	nspire_draw_string(25,111,0,0xffff,Regular12,"autorisés ont été effacés et les",false);
	nspire_draw_string(25,128,0,0xffff,Regular12,"sauvegardes sont desactivées).",false);
	nspire_draw_string(25,150,0,0xffff,Regular12,"Quitter KhiCAS (ou appuyer sur reset)",false);
	nspire_draw_string(25,167,0,0xffff,Regular12,"relancera le clignotement des leds.",false);
      }
      lcd_controller[6] |= 0b1;
      loopsleep(20);
      lcd_controller[6]|= 0b1 << 11;
      if (is_cx2)
	*(volatile unsigned *)(NSPIRE_CONTRAST_ADDR+4)=oldval2;
      *(volatile unsigned *)NSPIRE_CONTRAST_ADDR=oldval;
      statusline(0);
      sync_screen();
      lastt=* (volatile unsigned *) NSPIRE_RTC_ADDR;
      continue;
    }
    if (!any_key_pressed()){
      if (nspireemu)
	ck_msleep(50); // 100?
      else // real calculator
	ck_msleep(1);
      continue;
    }
    lastt=t1;
    int cursor_state=0;
    int i=0;
    if (isKeyPressed(KEY_NSPIRE_SHIFT)){
      while (i==0 && isKeyPressed(KEY_NSPIRE_SHIFT)){
	if (isKeyPressed(KEY_NSPIRE_LEFT)|| isKeyPressed(KEY_NSPIRE_LEFTUP) || isKeyPressed(KEY_NSPIRE_DOWNLEFT))
	  i=KEY_SELECT_LEFT;
	if (isKeyPressed(KEY_NSPIRE_RIGHT)|| isKeyPressed(KEY_NSPIRE_UPRIGHT) || isKeyPressed(KEY_NSPIRE_RIGHTDOWN))
	  i=KEY_SELECT_RIGHT;
	if (isKeyPressed(KEY_NSPIRE_UP))
	  i=KEY_SELECT_UP;
	if (isKeyPressed(KEY_NSPIRE_DOWN))
	  i=KEY_SELECT_DOWN;
      }
      if (i!=0){
	nspire_select=true;
      }
      if (i==0){
	nspire_shift=!nspire_shift;
	statusline(0);
	sync_screen();
	i=-1;
      }
    }
    else {
      if (nspire_select){
	nspire_select=nspire_shift=false;
	statusline(0);
	sync_screen();
	continue;
      }
      i=ascii_get(&cursor_state);
    }
    if (i<0){
      wait_no_key_pressed();
      continue;
    }
    if (i==KEY_CTRL_N){
      nspireemu=!nspireemu;
      nspire_ctrl=nspire_shift=false;
      statusline(0);
      sync_screen();
      continue;
    }
    if ( (i>=KEY_CTRL_LEFT && i<=KEY_CTRL_RIGHT) ||
	 (i>=KEY_UP_CTRL && i<=KEY_RIGHT_CTRL) ||
	 (i>=KEY_SELECT_LEFT && i<=KEY_SELECT_RIGHT) ||
	 i==KEY_CTRL_DEL){
      int delay=(lastkey==i)?5:60,j;
      for (j=0;j<delay && any_key_pressed();++j){
	if (nspireemu)
	  ck_msleep(14);
	else
	  ck_msleep(1);
      }
      if (any_key_pressed())
	lastkey=i;
      else 
	lastkey=-1;
    }
    else {
      wait_no_key_pressed();
      lastkey=-1;
    }
    if (nspire_ctrl || nspire_shift){
      nspire_ctrl=nspire_shift=false;
      statusline(0);
      sync_screen();
    }
    return i;
  }
  // void send_key_event(struct s_ns_event* eventbuf, unsigned short keycode_asciicode, INT is_key_up, INT unknown): since r721. Simulate a key event
}

// void idle(void)
// void msleep(unsigned ms)
// cfg_register_fileext(const char *ext, const char *prgm): (since v3.1 r797) associate for Ndless the file extension ext (without leading '.') to the program name prgm. Does nothing if the extension is already registered.

void GetKey(int * key){
  *key=getkey(true);
}

int alphawasactive(int * key){
  if (*key==KEY_DOWN_CTRL){
    *key=KEY_CTRL_DOWN;
    return true;
  }
  if (*key==KEY_UP_CTRL){
    *key=KEY_CTRL_UP;
    return true;
  }
  if (*key==KEY_LEFT_CTRL){
    *key=KEY_CTRL_LEFT;
    return true;
  }
  if (*key==KEY_RIGHT_CTRL){
    *key=KEY_CTRL_RIGHT;
    return true;
  }
  return false;
}

int isalphaactive(){
  return false;//nspire_ctrl;
}

void lock_alpha(){
  //nspire_ctrl=true;
}

int GetSetupSetting(int k){
  if (k!=0x14) return -1;
  if (!isalphaactive()) return 0;
  return 4;
}

void reset_kbd(){
  nspire_ctrl=nspire_shift=false;
}

int on_key_enabled=true;

void enable_back_interrupt(){
  on_key_enabled=true;
}

void disable_back_interrupt(){
  on_key_enabled=false;
}
#else // NSPIRE_NEWLIB

void set_exam_mode(int i){
  exam_mode=i;
}
#endif // NSPIRE_NEWLIB


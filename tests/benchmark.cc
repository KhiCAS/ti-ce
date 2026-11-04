// Optional on-calculator CAS timing harness. Absent from production builds.
#ifdef KHICAS_BENCHMARK
#include <giac/giacPCH.h>
#include <giac/input_parser.h>
#include <stdio.h>
#include <stdlib.h>
#undef clock // Giac's TICE configuration replaces its internal clock with zero.
#include <time.h>

volatile unsigned char khicas_benchmark_mode=1;

int khicas_run_benchmarks(giac::context *ctx){
  static const char * const expressions[]={
    "normal((x+1)^12)",
    "factor(x^12-1)",
    "normal((x^8-1)/(x^4-1))",
    "diff(exp(x)*sin(x)/(1+x^2),x)",
    "solve(x^4-5*x^2+4=0,x)",
    "integrate(x^8-3*x^3+2,x)",
    "integrate(1/(x^4+1),x)",
    "integrate(x^3*exp(x),x)",
    "integrate(sin(x)^4,x)",
    "integrate(1/(1+x^2),x,0,1)",
    "evalf(integrate(exp(-x^2),x,0,1))",
    "inv([[2,1,0,0],[1,3,1,0],[0,1,4,1],[0,0,1,5]])"
  };
  giac::angle_radian(true,ctx);
  sprintf((char *)0xFB0000,"KHICAS_BENCH_BEGIN|%lu|%u|32\n",
          (unsigned long)CLOCKS_PER_SEC,
          unsigned(sizeof(expressions)/sizeof(expressions[0])));
  for (unsigned i=0;i<sizeof(expressions)/sizeof(expressions[0]);++i){
    // First iteration warms parser/function state; three measured repetitions.
    for (unsigned repeat=0;repeat<4;++repeat){
      srand(12345);
      giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
      const clock_t start=clock();
      giac::gen input(expressions[i],ctx);
      const clock_t parsed=clock();
      giac::set_abort();
      giac::gen result=giac::eval(input,giac::eval_level(ctx),ctx);
      const clock_t evaluated=clock();
      giac::clear_abort();
      auto text=result.print(ctx);
      const clock_t printed=clock();
      for (unsigned j=0;j<text.size();++j)
        if (text[j]=='\n' || text[j]=='\r') text[j]=' ';
      sprintf((char *)0xFB0000,"KHICAS_BENCH|%u|%u|%lu|%lu|%lu|%s|%s\n",
              i,repeat,(unsigned long)(parsed-start),
              (unsigned long)(evaluated-parsed),(unsigned long)(printed-evaluated),
              expressions[i],text.c_str());
    }
  }
  // Independent exact-zero identities exercise factoring and integration.
  for (unsigned i=0;i<32;++i){
    const unsigned n=2+i%16;
    char expression[160];
    if (i<16)
      sprintf(expression,"normal(factor(x^4-%u*x^2+%u)-(x^4-%u*x^2+%u))",n+1,n,n+1,n);
    else
      sprintf(expression,"normal(diff(integrate(x^%u,x),x)-x^%u)",n,n);
    giac::gen input(expression,ctx);
    const auto text=giac::eval(input,giac::eval_level(ctx),ctx).print(ctx);
    sprintf((char *)0xFB0000,"KHICAS_CHECK|%u|%s\n",i,text.c_str());
  }
  sprintf((char *)0xFB0000,"KHICAS_BENCH_DONE\n");
  return 0;
}
#endif

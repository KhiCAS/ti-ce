// -*- mode:C++ ; compile-command: "g++-3.4 -I.. -I../include -g -c maple.cc  -DIN_GIAC -DHAVE_CONFIG_H" -*-
#include "giacPCH.h"
/*
 *  Copyright (C) 2000,2014 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
using namespace std;
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <stdio.h>
#if !defined HAVE_NO_SYS_TIMES_H && defined HAVE_SYS_TIME_H
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#endif
#include <sys/rtc.h>
// #include <sys/resource.h>
// #include <unistd.h>
#include "sym2poly.h"
#include "usual.h"
#include "moyal.h"
#include "solve.h"
#include "intg.h"
#include "prog.h"
#include "misc.h"
#include "maple.h"
#include "plot.h"
#include "ifactor.h"
#include "subst.h"
#include "input_parser.h"
#include "sym2poly.h"
#include "modpoly.h"
#include "lin.h"
#include "derive.h"
#include "ti89.h"
#include "giacintl.h"
#ifdef HAVE_LIBGSL
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_fft_real.h>
#endif
#ifndef HAVE_PNG_H
#undef HAVE_LIBPNG
#endif
#ifdef HAVE_LIBPNG
#include <png.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef FXCG
#include "syscalls.h"
#else
#include "k_csdk.h"
#endif

#if 0 // def GIAC_HAS_STO_38
  TMillisecs PrimeGetNow();
#endif

#if defined(EMCC) && !defined(PNACL)
extern "C" double emcctime();
// definition of emcctime should be added in emscripten directory src/library.js
// search for _time definition, and return only Date.now() for _emcctime
// otherwise time() will not work
#endif

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  gen _zip(const gen & g,const context * contextptr){
#ifdef XLIGHT
    return undef;
#endif
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    int s=-1;
    if (g.type!=_VECT || (s=g._VECTptr->size())<2)
      return symbolic(at_zip,g);
    vecteur & v=*g._VECTptr;
    gen & f=v[0];
    if (s==2){
      if (f.type!=_VECT || v[1].type!=_VECT || f._VECTptr->size()!=v[1]._VECTptr->size())
	return gendimerr(contextptr);
      return _tran(g,contextptr);
    }
    if (v[1].type!=_VECT || v[2].type!=_VECT)
      return f(makesequence(v[1],v[2]),contextptr);
    vecteur & w1=*v[1]._VECTptr;
    vecteur & w2=*v[2]._VECTptr;
    int s1=int(w1.size()),s2=int(w2.size());
    vecteur res;
    int ss=giacmin(s1,s2),i=0;
    res.reserve(ss);
    for (;i<ss;++i)
      res.push_back(_zip(makesequence(f,w1[i],w2[i]),contextptr));
    if (s==3)
      return res;
    gen & ff=v[3];
    for (;i<s1;++i)
      res.push_back(_zip(makesequence(f,w1[i],ff),contextptr));
    for (;i<s2;++i)
      res.push_back(_zip(makesequence(f,ff,w2[i]),contextptr));
    return res;
  }
  static const char _zip_s []="zip";
  static define_unary_function_eval (__zip,&_zip,_zip_s);
  define_unary_function_ptr5( at_zip ,alias_at_zip,&__zip,0,true);


  gen _ratnormal(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return ratnormal(g,contextptr);
  }
  static const char _ratnormal_s []="ratnormal";
  static define_unary_function_eval (__ratnormal,&_ratnormal,_ratnormal_s);
  define_unary_function_ptr5( at_ratnormal ,alias_at_ratnormal,&__ratnormal,0,true);

  gen _about(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT)
      return apply(g,contextptr,_about);
    if (g.type==_IDNT)
      return g._IDNTptr->eval(1,g,contextptr);
    return g;
  }
  static const char _about_s []="about";
  static define_unary_function_eval (__about,&_about,_about_s);
  define_unary_function_ptr5( at_about ,alias_at_about,&__about,0,true);

  gen _inverse(const gen & a_orig,GIAC_CONTEXT){
    if ( a_orig.type==_STRNG && a_orig.subtype==-1) return  a_orig;
    matrice a;
    bool convert_internal,minor_det,keep_pivot;
    int algorithm,last_col;
    if (!read_reduction_options(a_orig,a,convert_internal,algorithm,minor_det,keep_pivot,last_col))
      return inv(a_orig,contextptr);
    if (keep_pivot)
      return gensizeerr(gettext("Option keep_pivot not applicable"));
    if (minor_det){ // not really minors, use Leverrier algorithm
      vecteur b;
      vecteur p(mpcar(a,b,true,contextptr));
      gen res=b.back()/p.back();
      // if (a.size()%2==0)
	res=-res;
      return res;
    }
    matrice res;
    if (!minv(a,res,convert_internal,algorithm,contextptr))
      return gendimerr(contextptr);
    return res;
  }
  static const char _inverse_s []="inverse";
  static define_unary_function_eval (__inverse,&_inverse,_inverse_s);
  define_unary_function_ptr5( at_inverse ,alias_at_inverse,&__inverse,0,true);

  static const char _igcd_s []="igcd";
  static define_unary_function_eval (__igcd,&_gcd,_igcd_s);
  define_unary_function_ptr5( at_igcd ,alias_at_igcd,&__igcd,0,true);

  static const char _indets_s []="indets";
  static define_unary_function_eval (__indets,&_lname,_indets_s);
  define_unary_function_ptr5( at_indets ,alias_at_indets,&__indets,0,true);

  gen _revlist(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type==_VECT){
      vecteur v=*a._VECTptr;
      vreverse(v.begin(),v.end());
      return gen(v,a.subtype);
    }
    if (a.type==_STRNG){
      string s=*a._STRNGptr;
      int l=int(s.size());
      for (int i=0;i<l/2;++i){
	char c=s[i];
	s[i]=s[l-1-i];
	s[l-1-i]=c;
      }
      return string2gen(s,false);
    }
    return a;
  }
  static const char _revlist_s []="revlist";
  static define_unary_function_eval (__revlist,&_revlist,_revlist_s);
  define_unary_function_ptr5( at_revlist ,alias_at_revlist,&__revlist,0,true);

  static const char _reverse_s []="reverse";
  static define_unary_function_eval (__reverse,&_revlist,_reverse_s);
  define_unary_function_ptr5( at_reverse ,alias_at_reverse,&__reverse,0,true);

  gen _restart(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    //init_context((context *) ((void *) contextptr));
    gen res= _rm_all_vars(args,contextptr);
    *logptr(contextptr) << "//=== restarted ===" << "\n";
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->empty())
      _srand(_time(gen(vecteur(0),_SEQ__VECT),contextptr),contextptr);
    return res;
  }
  static const char _restart_s []="restart";
  static define_unary_function_eval (__restart,&_restart,_restart_s);
  define_unary_function_ptr5( at_restart ,alias_at_restart,&__restart,0,T_RETURN);

  
  gen _time(const gen & a,GIAC_CONTEXT){
#if 1
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type==_VECT && a.subtype==_SEQ__VECT){
      if (a._VECTptr->size()==2 && a._VECTptr->front().type==_INT_ && a._VECTptr->back().type==_INT_){
        int h=a._VECTptr->front().val;
        h=h%24;
        if (h<0)
          h+=24;
        int m=a._VECTptr->back().val;
        m=m%60;
        if (m<0)
          m+=60;
        set_time(h,m); // does not work.
        return 1;
      }
#ifdef FXCG
      return RTC_GetTicks();
#else
      return millis();
#endif
    }
    double delta;
    int ntimes=1,i=0;
    int level=eval_level(contextptr);
    unsigned t1= rtc_Minutes*60+rtc_Seconds; // RTC_GetTicks(); 
    // CERR << t1 << "\n";
    for (unsigned i=1;i<=20;++i){
      eval(a,level,contextptr);
      unsigned t2= rtc_Minutes*60+rtc_Seconds; // RTC_GetTicks();
      if (t2>=t1+10)
	return double(t2-t1)/double(i);
    }
    return 0.0;
#else
    return undef;
#endif    
  }
  static const char _time_s []="time";
  static define_unary_function_eval_quoted (__time,&_time,_time_s);
  define_unary_function_ptr5( at_time ,alias_at_time,&__time,_QUOTE_ARGUMENTS,true);

  static const char _powermod_s []="powermod";
  static define_unary_function_eval (__powermod,&_powmod,_powermod_s);
  define_unary_function_ptr5( at_powermod ,alias_at_powermod,&__powermod,0,true);

  static const char _length_s []="length";
  static define_unary_function_eval (__length,&_size,_length_s);
  define_unary_function_ptr5( at_length ,alias_at_length,&__length,0,true);

  static const char _len_s []="len";
  static define_unary_function_eval (__len,&_size,_len_s);
  define_unary_function_ptr5( at_len ,alias_at_len,&__len,0,true);

  static const char _nops_s []="nops";
  static define_unary_function_eval (__nops,&_size,_nops_s);
  define_unary_function_ptr5( at_nops ,alias_at_nops,&__nops,0,true);

  gen _cat(const gen & a_orig,GIAC_CONTEXT){
    if ( a_orig.type==_STRNG && a_orig.subtype==-1) return  a_orig;
    vecteur v(gen2vecteur(a_orig));
    string s;
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (it->type==_STRNG)
	s = s + *it->_STRNGptr;
      else
	s = s+it->print(contextptr);
    }
    return string2gen(s,false);
  }
  static const char _cat_s []="cat";
  static define_unary_function_eval (__cat,&_cat,_cat_s);
  define_unary_function_ptr5( at_cat ,alias_at_cat,&__cat,0,true);

  gen _copy(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT){
      vecteur v(*g._VECTptr);
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it)
	*it=_copy(*it,contextptr);
      return gen(v,g.subtype);
    }
    if (g.type==_MAP)
      return gen(*g._MAPptr);
    return g;
  }
  static const char _copy_s []="copy";
  static define_unary_function_eval (___copy,&_copy,_copy_s);
  define_unary_function_ptr5( at_copy ,alias_at_copy,&___copy,0,true);

  gen _row(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr();
    int shift = array_start(contextptr); //xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38;
    gen indice=g._VECTptr->back();
    if (indice.is_interval() && indice._SYMBptr->feuille.type==_VECT)
      indice=symbolic(at_interval,indice._SYMBptr->feuille-gen(shift)*vecteur(indice._SYMBptr->feuille._VECTptr->size(),1));
    else
      indice -= shift;
    gen res=g._VECTptr->front().operator_at(indice,contextptr);
    if (ckmatrix(res))
      return gen(*res._VECTptr,_SEQ__VECT);
    else
      return res;
  }
  static const char _row_s []="row";
  static define_unary_function_eval (__row,&_row,_row_s);
  define_unary_function_ptr5( at_row ,alias_at_row,&__row,0,true);

  gen _col(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr();
    int shift = array_start(contextptr); //xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38;
    gen indice=g._VECTptr->back();
    if (indice.is_interval() && indice._SYMBptr->feuille.type==_VECT)
      indice=symbolic(at_interval,indice._SYMBptr->feuille-gen(shift)*vecteur(indice._SYMBptr->feuille._VECTptr->size(),1));
    else
      indice -= shift;
    gen res=_tran(g._VECTptr->front(),contextptr)[indice];
    if (ckmatrix(res))
      return gen(*res._VECTptr,_SEQ__VECT);
    else
      return res;
  }
  static const char _col_s []="col";
  static define_unary_function_eval (__col,&_col,_col_s);
  define_unary_function_ptr5( at_col ,alias_at_col,&__col,0,true);

  static gen count(const gen & f,const gen & v,const context * contextptr,const gen & param){
    if (v.type!=_VECT)
      return f(v,contextptr);
    const_iterateur it=v._VECTptr->begin(),itend=v._VECTptr->end();
    if (param==at_row){
      vecteur res;
      for (;it!=itend;++it){
	res.push_back(count(f,*it,contextptr,0));
      }
      return res;
    }
    if (param==at_col){
      if (!ckmatrix(v))
	return gentypeerr();
      return count(f,mtran(*v._VECTptr),contextptr,at_row);
    }
    gen res;
    for (;it!=itend;++it){
      res=res+count(f,*it,contextptr,0);
    }
    return res;
  }
  gen _count(const gen & args,const context * contextptr){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()<2))
      return gensizeerr(contextptr);
    if (args.subtype!=_SEQ__VECT) {
      if (!is_integer_vecteur(*args._VECTptr))
	return gensizeerr(contextptr);
      // count elements in list of integers
      vector<int> x=vecteur_2_vector_int(*args._VECTptr);
      int m=giacmin(x),M=giacmax(x),s=int(x.size());
      if (M-m<3*s){
	vector<int> eff(M-m+1);
	effectif(x,eff,m);
	matrice res;
	for (int i=m;i<=M;++i){
	  int e=eff[i-m];
	  if (e>0)
	    res.push_back(makevecteur(i,e));
	}
	return res;
      }
      sort(x.begin(),x.end());
      int old=M,eff=0,cur;
      matrice res;
      for (int pos=0;pos<s;++pos){
	cur=x[pos];
	if (cur==old){
	  ++eff;
	  continue;
	}
	if (eff)
	  res.push_back(makevecteur(old,eff));
	old=cur;
	eff=1;
      }
      if (eff)
	res.push_back(makevecteur(old,eff));
      return res;
    }
    gen v((*args._VECTptr)[1]);
    gen f(args._VECTptr->front());
    if (f.type==_STRNG && v.type==_STRNG){
      // (Python-like) count occurences of v in f
      int count=0,pos=-1,s=f._STRNGptr->size();
      for (;pos<s;++count){
	pos=f._STRNGptr->find(*v._STRNGptr,pos+1);
	if (pos<0 || pos>=s)
	  break;
      }
      return count;
    }
    gen param;
    if (args._VECTptr->size()>2)
      param=(*args._VECTptr)[2];
    else {
      if (v.type!=_VECT) 
	return _count_eq(makesequence(v,f),contextptr);
    }
    return count(f,v,contextptr,param);
  }
  static const char _count_s []="count";
  static define_unary_function_eval (___count,&_count,_count_s);
  define_unary_function_ptr5( at_count ,alias_at_count,&___count,0,true);

  static longlong count_eq0(const gen & f,const gen & v,GIAC_CONTEXT){
    if (v.type!=_VECT)
      return v==f;
    const_iterateur it=v._VECTptr->begin(),itend=v._VECTptr->end();
    longlong res=0;
    for (;it!=itend;++it){
      if (it->type!=_VECT){
	if (*it==f)
	  ++res;
      }
      else
	res += count_eq0(f,*it,contextptr);
    }
    return res;
  }

  static gen count_eq1(const gen & f,const gen & v,GIAC_CONTEXT){
    if (v.type!=_VECT)
      return is_strictly_greater(v,f,contextptr);
    const_iterateur it=v._VECTptr->begin(),itend=v._VECTptr->end();
    gen res;
    for (;it!=itend;++it){
      res=res+count_eq1(f,*it,contextptr);
    }
    return res;
  }

  static gen count_eq2(const gen & f,const gen & v,GIAC_CONTEXT){
    if (v.type!=_VECT)
      return is_strictly_greater(f,v,contextptr);
    const_iterateur it=v._VECTptr->begin(),itend=v._VECTptr->end();
    gen res;
    for (;it!=itend;++it){
      res=res+count_eq2(f,*it,contextptr);
    }
    return res;
  }

  static gen count_eq(const gen & f,const gen & v,GIAC_CONTEXT,int type,const gen & rowcol){
    if (rowcol==at_row || rowcol==at_col){
      if (!ckmatrix(v))
	return gentypeerr();
      if (rowcol==at_row){
	const_iterateur it=v._VECTptr->begin(),itend=v._VECTptr->end();
	vecteur res;
	for (;it!=itend;++it){
	  res.push_back(count_eq(f,*it,contextptr,type,0));
	}
	return res;
      }
      if (rowcol==at_col)
	return count_eq(f,mtran(*v._VECTptr),contextptr,type,at_row);
    }
    if (type==0)
      return count_eq0(f,v,contextptr);
    if (type==1)
      return count_eq1(f,v,contextptr);
    if (type==2)
      return count_eq2(f,v,contextptr);
    return gentypeerr();
#if 0
    if (v.type!=_VECT){
      switch (type){
      case 0:
	return v==f;
      case 1:
	return is_strictly_greater(v,f,contextptr);
      case 2:
	return is_strictly_greater(f,v,contextptr);
      default:
	return gentypeerr();
      }
    }
    const_iterateur it=v._VECTptr->begin(),itend=v._VECTptr->end();
    gen res;
    for (;it!=itend;++it){
      res=res+count_eq(f,*it,contextptr,type,0);
    }
    return res;
#endif
  }

  gen _count_eq(const gen & args,const context * contextptr){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( args.type!=_VECT || args.subtype!=_SEQ__VECT || args._VECTptr->size()<2)
      return gensizeerr(contextptr);
    gen v((*args._VECTptr)[1]);
    gen f(args._VECTptr->front());
    gen param;
    if (args._VECTptr->size()>2)
      param=(*args._VECTptr)[2];
    return count_eq(f,v,contextptr,0,param);
  }
  static const char _count_eq_s []="count_eq";
  static define_unary_function_eval (___count_eq,&_count_eq,_count_eq_s);
  define_unary_function_ptr5( at_count_eq ,alias_at_count_eq,&___count_eq,0,true);

  gen _count_sup(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( args.type!=_VECT || args.subtype!=_SEQ__VECT || args._VECTptr->size()<2)
      return gensizeerr(contextptr);
    gen v((*args._VECTptr)[1]);
    gen f(args._VECTptr->front());
    gen param;
    if (args._VECTptr->size()>2)
      param=(*args._VECTptr)[2];
    return count_eq(f,v,contextptr,1,param);
  }
  static const char _count_sup_s []="count_sup";
  static define_unary_function_eval (___count_sup,&_count_sup,_count_sup_s);
  define_unary_function_ptr5( at_count_sup ,alias_at_count_sup,&___count_sup,0,true);

  gen _count_inf(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( args.type!=_VECT || args.subtype!=_SEQ__VECT || args._VECTptr->size()<2)
      return gensizeerr(contextptr);
    gen v((*args._VECTptr)[1]);
    gen f(args._VECTptr->front());
    gen param;
    if (args._VECTptr->size()>2)
      param=(*args._VECTptr)[2];
    return count_eq(f,v,contextptr,2,param);
  }
  static const char _count_inf_s []="count_inf";
  static define_unary_function_eval (___count_inf,&_count_inf,_count_inf_s);
  define_unary_function_ptr5( at_count_inf ,alias_at_count_inf,&___count_inf,0,true);

  gen _trunc(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (is_equal(args))
      return apply_to_equal(args,_trunc,contextptr);
#ifdef WITH_UNITS
    if (args.is_symb_of_sommet(at_unit))
      return apply_unit(args,_trunc,contextptr);
#endif
    if (args.type==_VECT) {
      if (args.subtype==_SEQ__VECT && args._VECTptr->size()==2 && args._VECTptr->back().type==_INT_){
#ifdef BCD
	if (args._VECTptr->front().type==_FLOAT_)
	  return ftrunc(args._VECTptr->front()._FLOAT_val,args._VECTptr->back().val);
#endif
	gen b=args._VECTptr->back();
	if (b.val<0){
	  gen gf=_floor(log10(abs(args._VECTptr->front(),contextptr),contextptr),contextptr); 
	  if (gf.type!=_INT_)
	    return gensizeerr(contextptr);
	  b=-1-b-gf;
	}
#ifdef _SOFTMATH_H
	double d=std::giac_gnuwince_pow(10.0,double(b.val));
#else
	double d=std::pow(10.0,double(b.val));
#endif
	return _floor(d*args._VECTptr->front(),contextptr)/d;
      }
      if (args.subtype==_SEQ__VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_INT_){
	if (args._VECTptr->front().val)
	  return zero;
	return _trunc(args._VECTptr->back(),contextptr);
      }
      return apply(args,contextptr,_trunc);
    }
    if (is_strictly_positive(-args,contextptr))
      return -_floor(-args,contextptr);
    return _floor(args,contextptr);
  }
  static const char _trunc_s []="trunc";
  static define_unary_function_eval (___trunc,&_trunc,_trunc_s);
  define_unary_function_ptr5( at_trunc ,alias_at_trunc,&___trunc,0,true);

  static const char _div_s []="div";
  static define_unary_function_eval4 (__div,&_iquo,_div_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr5( at_div ,alias_at_div,&__div,0,T_MOD);

  gen _evalc(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT)
      return apply(g,_evalc,contextptr);
    gen tmp(_exp2pow(_lin(recursive_normal(g,contextptr),contextptr),contextptr));
    vecteur l(lop(tmp,at_arg));
    if (!l.empty()){
      vecteur lp=*apply(l,gen_feuille)._VECTptr;
      lp=*apply(lp,contextptr,arg_CPLX)._VECTptr;
      tmp=subst(tmp,l,lp,false,contextptr);
    }
    tmp=recursive_normal(tmp,contextptr);
    vecteur vtmp(lvar(tmp));
    if (vtmp.size()==1 && vtmp[0].is_exp()){
      tmp=ratnormal(_halftan(_exp2trig(tmp,contextptr),contextptr),contextptr);
    }
    gen r,i;
    reim(tmp,r,i,contextptr);
    gen tmp2=_lin(inv(tmp,contextptr),contextptr);
    gen re2,im2;
    reim(tmp2,re2,im2,contextptr);
    if (lvar(makevecteur(re2,im2)).size()<lvar(makevecteur(r,i)).size())
      reim(inv(ratnormal(re2,contextptr)+cst_i*ratnormal(im2,contextptr),contextptr),r,i,contextptr);
    // tmp=simplify(i,contextptr);
    if (is_zero(i))
      return r;
    if (is_zero(r))
      return cst_i*i;
    return symb_plus(gen(makesequence(r,cst_i*i)));
  }
  static const char _evalc_s []="evalc";
  static define_unary_function_eval (__evalc,&_evalc,_evalc_s);
  define_unary_function_ptr5( at_evalc ,alias_at_evalc,&__evalc,0,true);

#ifndef S_IRUSR
#define S_IRUSR 00400
#endif
#ifndef S_IWUSR
#define S_IWUSR 00200
#endif

#ifdef TICE
  gen _fopen(const gen & g,GIAC_CONTEXT){
    return 0;
  }
  gen _fprint(const gen & g,GIAC_CONTEXT){
    return 0;
  }
  gen _close(const gen & g0,GIAC_CONTEXT){
    return 0;
  }
#else  
  // open a file, returns a FILE *
  gen _fopen(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen filename(g);
    string mode="w+";
    if (g.type==_VECT && g.subtype==_SEQ__VECT && g._VECTptr->size()==2 && g._VECTptr->back().type==_STRNG){
      filename=g._VECTptr->front();
      mode=*g._VECTptr->back()._STRNGptr;
    }
    if (filename.type!=_STRNG || mode.size()>2)
      return gensizeerr();
    FILE * f=fopen(filename._STRNGptr->c_str(),mode.c_str());
    return gen((void *) f,_FILE_POINTER);
  }

  // fprint first arg=FD, rest is printed
  gen _fprint(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()<1)
      return gensizeerr(gettext("1st arg=open result, then other args"));
    vecteur & v=*g._VECTptr;
    int s=int(v.size());
    FILE * f=0;
#if !defined(BESTA_OS) && !defined(NSPIRE_NEWLIB) && !defined(FXCG) && !defined TICE
    if (v[0].type==_INT_ && v[0].subtype==_INT_FD)
      f= fdopen(v[0].val,"a");
#endif    
    if (v[0].type==_POINTER_ && v[0].subtype==_FILE_POINTER)
      f=(FILE *) v[0]._POINTER_val;
    if (f){
      if (s>1 && v[1]==gen("Unquoted",contextptr)){
	for (int i=2;i<s;++i)
	  fprintf(f,"%s",v[i].type==_STRNG?v[i]._STRNGptr->c_str():unquote(v[i].print(contextptr)).c_str());
      }
      else {
	for (int i=1;i<s;++i)
	  fprintf(f,"%s",v[i].print(contextptr).c_str());
      }
      // fclose(f);
      return plus_one;
    }
    return zero;
  }

  gen _close(const gen & g0,GIAC_CONTEXT){
    gen g=eval(g0,1,contextptr);
    if ( g.type==_STRNG && g.subtype==-1) return  g;
#if !defined(VISUALC) && !defined(BESTA_OS) && !defined(__MINGW_H) && !defined(NSPIRE) && !defined(FXCG) && !defined TICE
    if (g.type==_INT_ && g.subtype==_INT_FD){
      purgenoassume(g0,contextptr);
      close(g.val);
      return plus_one;
    }
#endif
    if (g.type==_POINTER_){
      purgenoassume(g0,contextptr);
      fclose((FILE *)g._POINTER_val);
      return plus_one;
    }
    return zero;
  }
#endif
  static const char _fopen_s []="fopen";
  static define_unary_function_eval (__fopen,&_fopen,_fopen_s);
  define_unary_function_ptr5( at_fopen ,alias_at_fopen,&__fopen,0,true);

  static const char _fprint_s []="fprint";
  static define_unary_function_eval (__fprint,&_fprint,_fprint_s);
  define_unary_function_ptr5( at_fprint ,alias_at_fprint,&__fprint,0,true);

  static const char _fclose_s []="fclose";
  static define_unary_function_eval (__fclose,&_close,_fclose_s);
  define_unary_function_ptr5( at_fclose ,alias_at_fclose,&__fclose,_QUOTE_ARGUMENTS,true);

  gen _blockmatrix(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()!=3)
      return gentypeerr();
    vecteur & v = *g._VECTptr;
    if (v[0].type!=_INT_ || v[1].type!=_INT_ || v[2].type!=_VECT)
      return gensizeerr();
    int n=v[0].val,m=v[1].val;
    vecteur l=*v[2]._VECTptr;
    int s=int(l.size());
    if (n<=0 || m<=0 || n*m!=s)
      return gendimerr();
    for (int k=0;k<s;++k){
      if (!ckmatrix(l[k])){
	vecteur vtmp(vecteur(1,l[k]));
	if (ckmatrix(vtmp))
	  l[k]=mtran(vtmp);
	if (!ckmatrix(l[k]))
	  return gentypeerr();
      }
    }
    matrice res;
    int pos=0;
    unsigned final=0;
    for (int i=0;i<n;++i){
      // glue m matrices from l vertically, these matrices must have same #rows
      pos=i*m;
      int nrows=int(l[pos]._VECTptr->size());
      for (int j=1;j<m;++j){
	if (l[pos+j]._VECTptr->size()!=unsigned(nrows))
	  return gendimerr();
      }
      for (int k=0;k<nrows;++k){
	// glue row k of l[pos+j] for j=0..m
	vecteur tmp;
	for (int j=0;j<m;++j){
	  gen cur_row = (*l[pos+j]._VECTptr)[k];
	  const_iterateur it=cur_row._VECTptr->begin(),itend=cur_row._VECTptr->end();
	  for (;it!=itend;++it)
	    tmp.push_back(*it);
	}
	if (final && tmp.size()!=final)
	  return gendimerr();
	else
	  final=unsigned(tmp.size());
	res.push_back(tmp);
      }
    }
    return res;
  }
  static const char _blockmatrix_s []="blockmatrix";
  static define_unary_function_eval (__blockmatrix,&_blockmatrix,_blockmatrix_s);
  define_unary_function_ptr5( at_blockmatrix ,alias_at_blockmatrix,&__blockmatrix,0,true);

  static gen delrowscols(const gen & g,bool isrow,GIAC_CONTEXT){
    gen gm,interval,f,fa,fb;
    if (g.type!=_VECT || g._VECTptr->size()!=2 )
      return gentypeerr();
    gm=g._VECTptr->front();
    if (gm.is_symb_of_sommet(at_ans))
      gm=eval(gm,1,contextptr);
    if (gm.type==_IDNT){
      return sto(delrowscols(eval(g,eval_level(contextptr),contextptr),isrow,contextptr),gm,contextptr);
    }
    interval=g._VECTptr->back();
    // if (interval.type==_IDNT || interval.type==_SYMB)
      interval=eval(interval,1,contextptr);
    if (!interval.is_interval())
      interval=symb_interval(interval,interval);
    if (!ckmatrix(gm) || !interval.is_interval() || (f=interval._SYMBptr->feuille).type!=_VECT || f._VECTptr->size()!=2 || !is_integral(fa=f._VECTptr->front()) || !is_integral(fb=f._VECTptr->back()) )
      return gentypeerr();
    int shift = array_start(contextptr); //xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38;
    int a=fa.val-shift,b=fb.val-shift,s;
    matrice m=*gm._VECTptr;
    if (!isrow)
      m=mtran(m);
    s=int(m.size());
    if (a>=s || b>=s || a<0 || b<0 || a>b)
      return gendimerr();
    m.erase(m.begin()+a,m.begin()+b+1);
    if (!isrow)
      m=mtran(m);
    return m;
  }

  gen _delrows(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return delrowscols(g,true,contextptr);
  }
  static const char _delrows_s []="delrows";
  static define_unary_function_eval (__delrows,&_delrows,_delrows_s);
  define_unary_function_ptr5( at_delrows ,alias_at_delrows,&__delrows,0,true);

  gen _delcols(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return delrowscols(g,false,contextptr);
  }
  static const char _delcols_s []="delcols";
  static define_unary_function_eval (__delcols,&_delcols,_delcols_s);
  define_unary_function_ptr5( at_delcols ,alias_at_delcols,&__delcols,0,true);

  gen _JordanBlock(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    int s;
    if (g.type!=_VECT || g._VECTptr->size()!=2 || g._VECTptr->back().type!=_INT_ )
      return gentypeerr();
    s=g._VECTptr->back().val;
    if (s<=0 || double(s)*s>LIST_SIZE_LIMIT)
      return gendimerr();
    --s;
    gen x=g._VECTptr->front();
    matrice m;
    m.reserve(s+1);
    for (int i=0;i<=s;++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (ctrl_c || interrupted)
	return gensizeerr(gettext("Stopped by user interruption."));
      vecteur v(s+1);
      v[i]=x;
      if (i<s)
	v[i+1]=plus_one;
      m.push_back(v);
    }
    return m;
  }
  static const char _JordanBlock_s []="JordanBlock";
  static define_unary_function_eval (__JordanBlock,&_JordanBlock,_JordanBlock_s);
  define_unary_function_ptr5( at_JordanBlock ,alias_at_JordanBlock,&__JordanBlock,0,true);

  gen _companion(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur v;
    if (g.type!=_VECT)
      return _companion(makesequence(g,vx_var()),contextptr); // gentypeerr();
    if (g.subtype==_SEQ__VECT && g._VECTptr->size()==2){
      gen P=g._VECTptr->front();
      gen x=g._VECTptr->back();
      gen Px=_e2r(makevecteur(P,x),contextptr);
      if (Px.type==_FRAC)
	Px=inv(Px._FRACptr->den,contextptr)*Px._FRACptr->num;
      if (Px.type!=_VECT)
	return gensizeerr();
      v=*Px._VECTptr;
    }
    else
      v=*g._VECTptr;
    return companion(v);
  }
  static const char _companion_s []="companion";
  static define_unary_function_eval (__companion,&_companion,_companion_s);
  define_unary_function_ptr5( at_companion ,alias_at_companion,&__companion,0,true);

  gen _border(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()!=2 || !ckmatrix(g._VECTptr->front()) || g._VECTptr->back().type!=_VECT)
      return gensizeerr();
    matrice m=*g._VECTptr->front()._VECTptr,v=*g._VECTptr->back()._VECTptr;
    if (m.size()!=v.size())
      return gendimerr();
    m=mtran(m);
    if (ckmatrix(v))
      m=mergevecteur(m,mtran(v));
    else
      m.push_back(v);
    return mtran(m);
  }
  static const char _border_s []="border";
  static define_unary_function_eval (__border,&_border,_border_s);
  define_unary_function_ptr5( at_border ,alias_at_border,&__border,0,true);

  // pade(f(x),x,n,p) or pade(f(x),x,N(x),p)
  // Find P/Q such that P/Q=f(x) mod N(x) [1st case N(x)=x^n], deg(P)<p
  // Assume deg(f)<n
  gen _pade(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()!=4)
      return gensizeerr();
    vecteur v =*g._VECTptr;
    if (v[1].type!=_IDNT || v[3].type!=_INT_)
      return gensizeerr();
    int n,p=v[3].val,ns;
    gen f=v[0],x=v[1],N=v[2];
    if (v[2].type==_INT_){
      n=v[2].val+1;
      N=pow(v[1],n);
      ns=n;
    }
    else {
      n=p;
      ns=_degree(makevecteur(N,x),contextptr).val;
    }
    if (p<=0 || n<=0)
      return gensizeerr();
    f=_series(makevecteur(f,x,zero,ns-1),contextptr);
    vecteur l1(lop(f,at_order_size));
    vecteur l2(l1.size(),zero);
    f=subst(f,l1,l2,false,contextptr);
    // Convert f and N to poly1
    vecteur l(1,x);
    lvar(f,l);
    lvar(N,l);
    int ls=int(l.size());
    gen ff(sym2r(f,l,contextptr));
    gen fn,fd;
    fxnd(ff,fn,fd);
    gen Nf(sym2r(N,l,contextptr));
    gen Nn,Nd;
    fxnd(Nf,Nn,Nd); // Note nd has no meaning
    vecteur fp;
    if (fn.type==_POLY)
      fp=polynome2poly1(*fn._POLYptr,1);
    else
      return gensizeerr();
    vecteur Np;
    if (Nn.type==_POLY)
      Np=polynome2poly1(*Nn._POLYptr,1);
    else
      return gensizeerr();
    n=int(Np.size())-1;
    // Check that fp is a poly of degree less than n
    if (n<1 || p>n || signed(fp.size())>n)
      return gendimerr();
    vecteur a,b;
    if (!egcd_pade(Np,fp,p,a,b,0))
      *logptr(contextptr) << gettext("Solution may be wrong since a and b are not prime together: ")+gen(a).print(contextptr)+","+gen(b).print(contextptr) << "\n";
    gen res=poly12polynome(a,1,ls);
    res=res/(fd*gen(poly12polynome(b,1,ls)));
    res=r2sym(res,l,contextptr);
    return res;
  }
  static const char _pade_s []="pade";
  static define_unary_function_eval (__pade,&_pade,_pade_s);
  define_unary_function_ptr5( at_pade ,alias_at_pade,&__pade,0,true);

  static const char _interp_s []="interp";
  static define_unary_function_eval (__interp,&_lagrange,_interp_s);
  define_unary_function_ptr5( at_interp ,alias_at_interp,&__interp,0,true);

  static gen lhsrhs(const gen & g,int i){
    if (!is_equal(g) && !g.is_interval())
      return gensizeerr();
    gen & f=g._SYMBptr->feuille;
    if (f.type!=_VECT || f._VECTptr->size()!=2)
      return gensizeerr();
    return (*f._VECTptr)[i];
  }
  gen _lhs(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return lhsrhs(g,0);
  }
  static const char _lhs_s []="lhs";
  static define_unary_function_eval (__lhs,&_lhs,_lhs_s);
  define_unary_function_ptr5( at_lhs ,alias_at_lhs,&__lhs,0,true);

  gen _rhs(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return lhsrhs(g,1);
  }
  static const char _rhs_s []="rhs";
  static define_unary_function_eval (__rhs,&_rhs,_rhs_s);
  define_unary_function_ptr5( at_rhs ,alias_at_rhs,&__rhs,0,true);

  // Given [v_0 ... v_(2n-1)] (begin of the recurrence sequence) 
  // return [b_n...b_0] such that b_n*v_{n+k}+...+b_0*v_k=0
  // Example [1,-1,3,3] -> [1,-3,-6]
  // -> the recurrence relation is v_{n+2}=3v_{n+1}+6v_n
  gen _reverse_rsolve(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT)
      return gensizeerr();
    vecteur v=reverse_rsolve(*g._VECTptr);
    return v/v.front();
  }
  static const char _reverse_rsolve_s []="reverse_rsolve";
  static define_unary_function_eval (__reverse_rsolve,&_reverse_rsolve,_reverse_rsolve_s);
  define_unary_function_ptr5( at_reverse_rsolve ,alias_at_reverse_rsolve,&__reverse_rsolve,0,true);

  // Approx fft or exact if args=poly1,omega,n
  gen fft(const gen & g_orig,int direct,GIAC_CONTEXT){
    if (g_orig.type==_VECT && g_orig.subtype==_SEQ__VECT && g_orig._VECTptr->size()==3 && g_orig._VECTptr->front().type==_VECT){
      vecteur & v =*g_orig._VECTptr->front()._VECTptr;
      int n=int(v.size());
      if (n<2)
	return gendimerr();
      gen omega=(*g_orig._VECTptr)[1];
      gen modulo=(*g_orig._VECTptr)[2];
      if (direct==1)
	omega=invmod(omega,modulo);
      vecteur w(n),res;
      iterateur it=w.begin(),itend=w.end();
      if (omega.type==_INT_ && modulo.type==_INT_){
	int Omega=omega.val,m=modulo.val;
	longlong omegan=1;
	for (;it!=itend;++it,omegan=(Omega*omegan) %m){
	  *it=int(omegan);
	}
      }
      else {
	gen omegan=1;
	for (;it!=itend;++it,omegan=smod(omega*omegan,modulo)){
	  *it=omegan;
	}
      }
      environment * env = new environment;
      env->moduloon=true;
      env->modulo=modulo;
      fft(v,w,res,env);
      delete env;
      if (direct==1)
	return res;
      else
	return smod(invmod(n,modulo)*res,modulo);
    }
    gen g=g_orig;
    if (g.type!=_VECT)
      return gensizeerr();
    int n=int(g._VECTptr->size());
    if (n<2)
      return gendimerr();
    g=evalf_double(g_orig,1,contextptr);
    if (debug_infolevel)
      CERR << CLOCK()*1e-6 << " fft start" << "\n";
    vecteur v =*g._VECTptr;
#ifdef HAVE_LIBGSL
    if (direct && is_zero(im(v,contextptr))){
      double * data=new double[n];
      for (int i=0;i<n;++i){
	if (v[i].type!=_DOUBLE_){
	  delete [] data;
	  return gensizeerr(contextptr);
	}
	data[i]=v[i]._DOUBLE_val;
      }
      if (n==(1<<(sizeinbase2(n)-1))){
	gsl_fft_real_radix2_transform(data,1,n);
	v[0]=data[0];
	v[n/2]=data[n/2];
	for (int i=1;i<n/2;++i){
	  v[i]=gen(data[i],data[n-i]);
	  v[n-i]=conj(v[i],contextptr);
	}
      }
      else {
	gsl_fft_real_wavetable * wavetable = gsl_fft_real_wavetable_alloc (n);
	gsl_fft_real_workspace * workspace = gsl_fft_real_workspace_alloc (n);
	gsl_fft_real_transform (data, 1, n,wavetable,workspace);
	gsl_fft_real_wavetable_free (wavetable);
	gsl_fft_real_workspace_free (workspace);
	v[0]=data[0];
	int n2;
	if (n%2==0){
	  v[n/2]=data[n-1];
	  n2=n/2;
	}
	else 
	  n2=n/2+1;
	for (int i=1;i<n2;++i){
	  v[i]=gen(data[2*i-1],data[2*i]);
	  v[n-i]=conj(v[i],contextptr);
	}
      }
      delete [] data;
      if (debug_infolevel)
	CERR << CLOCK()*1e-6 << " fft end" << "\n";
      return v;
    }
    // Could be improved by keeping the wavetable
    double * data=new double[2*n];
    gen gr,gi;
    for (int i=0;i<n;++i){
      reim(v[i],gr,gi,contextptr);
      gi=evalf_double(gi,1,contextptr);
      if (gr.type!=_DOUBLE_ || gi.type!=_DOUBLE_){
	delete [] data;
	return gensizeerr(contextptr);
      }
      data[2*i]=gr._DOUBLE_val;
      data[2*i+1]=gi._DOUBLE_val;
    }
    if (n==(1<<(sizeinbase2(n)-1))){
      if (direct)
	gsl_fft_complex_radix2_forward(data,1,n);
      else
	gsl_fft_complex_radix2_backward(data,1,n);
    }
    else {
      gsl_fft_complex_wavetable * wavetable = gsl_fft_complex_wavetable_alloc (n);
      gsl_fft_complex_workspace * workspace=gsl_fft_complex_workspace_alloc (n);
      if (direct)
	gsl_fft_complex_forward (data, 1, n,wavetable,workspace);
      else
	gsl_fft_complex_backward (data, 1, n,wavetable,workspace);
      gsl_fft_complex_wavetable_free (wavetable);
      gsl_fft_complex_workspace_free (workspace);
    }
    if (direct){
      for (int i=0;i<n;++i){
	v[i]=gen(data[2*i],data[2*i+1]);
      }
    }
    else {
      for (int i=0;i<n;++i){
	v[i]=gen(data[2*i],data[2*i+1])/n;
      }
    }
    delete [] data;
    if (debug_infolevel)
      CERR << CLOCK()*1e-6 << " fft end" << "\n";
    return v;
#endif
    /* 
       unsigned m=gen(n).bindigits()-1;
       if (n!=1<<m)
       return gensizeerr(gettext("Size is not a power of 2 ")+print_INT_(n));
    */
    vecteur res;
    double theta;
    vecteur w(n);
    iterateur it=w.begin(),itend=w.end();
    for (int i=0;it!=itend;++it,++i){
      theta = (2*M_PI*i)/n;
      if (direct==1)
	*it = gen(std::cos(theta),-std::sin(theta));
      else
	*it = gen(std::cos(theta),std::sin(theta));
    }
    fft(v,w,res,0);
    if (direct==1)
      return res;
    else
      return gen(res)/n;
  }

  gen _fft(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return fft(g,1,contextptr);
  }
  static const char _fft_s []="fft";
  static define_unary_function_eval (__fft,&_fft,_fft_s);
  define_unary_function_ptr5( at_fft ,alias_at_fft,&__fft,0,true);

  gen _ifft(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return fft(g,0,contextptr);
  }
  static const char _ifft_s []="ifft";
  static define_unary_function_eval (__ifft,&_ifft,_ifft_s);
  define_unary_function_ptr5( at_ifft ,alias_at_ifft,&__ifft,0,true);

  gen _even(const gen & g_,GIAC_CONTEXT){
    gen g(g_);
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (!is_integral(g)) return gentypeerr(contextptr);
    return is_zero(smod(g,2));
  }
  static const char _even_s []="even";
  static define_unary_function_eval (__even,&_even,_even_s);
  define_unary_function_ptr5( at_even ,alias_at_even,&__even,0,true);

  gen _odd(const gen & g_,GIAC_CONTEXT){
    gen g(g_);
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (!is_integral(g)) return gentypeerr(contextptr);
    return !is_zero(smod(g,2));
  }
  static const char _odd_s []="odd";
  static define_unary_function_eval (__odd,&_odd,_odd_s);
  define_unary_function_ptr5( at_odd ,alias_at_odd,&__odd,0,true);


  gen linear_apply(const gen & e,const gen & x,const gen & l,gen & remains, GIAC_CONTEXT, gen (* f)(const gen &,const gen &,const gen &,gen &,const context *)){
    if (is_constant_wrt(e,x,contextptr) || (e==x) )
      return f(e,x,l,remains,contextptr);
    // e must be of type _SYMB
    if (e.type!=_SYMB) return gensizeerr(gettext("in linear_apply"));
    unary_function_ptr u(e._SYMBptr->sommet);
    gen arg(e._SYMBptr->feuille);
    gen res;
    if (u==at_neg){
      res=-linear_apply(arg,x,l,remains,contextptr,f);
      remains=-remains;
      return res;
    } // end at_neg
    if (u==at_plus){
      if (arg.type!=_VECT)
	return linear_apply(arg,x,l,remains,contextptr,f);
      const_iterateur it=arg._VECTptr->begin(),itend=arg._VECTptr->end();
      for (gen tmp;it!=itend;++it){
	res = res + linear_apply(*it,x,l,tmp,contextptr,f);
	remains =remains + tmp;
      }
      return res;
    } // end at_plus
    if (u==at_prod){
      if (arg.type!=_VECT)
	return linear_apply(arg,x,l,remains,contextptr,f);
      // find all constant terms in the product
      vecteur non_constant;
      gen prod_constant;
      decompose_prod(*arg._VECTptr,x,non_constant,prod_constant,true,contextptr);
      if (non_constant.empty()) return gensizeerr(gettext("in linear_apply 2")); // otherwise the product would be constant
      if (non_constant.size()==1)
	res = linear_apply(non_constant.front(),x,l,remains,contextptr,f);
      else
	res = f(symb_prod(non_constant),x,l,remains,contextptr);
      remains = prod_constant * remains;
      return prod_constant * res;
    } // end at_prod
    return f(e,x,l,remains,contextptr);
  }

  // discrete product primitive of a polynomial P
  gen product(const polynome & P,const vecteur & v,const gen & n,gen & remains,GIAC_CONTEXT){
    // factor P, for a linear factor a*n+b, multiply res by a^n*gamma(n+b/a)
    // for other factors multiply remains by the factor
    polynome Pcont;
    factorization f;
    gen divan=1,res,extra_div=1;
    if (!factor(P,Pcont,f,/* is_primitive*/false,/* with_sqrt*/true,/* complex */true,divan,extra_div) || extra_div!=1){
      remains=r2e(P,v,contextptr);
      return 1;
    }
    res = pow(divan,-n,contextptr);
    factorization::const_iterator it=f.begin(),itend=f.end();
    for (;it!=itend;++it){
      gen tmp=r2e(it->fact,v,contextptr);
      if (it->fact.lexsorted_degree()!=1){
	remains = remains * pow(tmp,it->mult);
      }
      else {
	gen a=derive(tmp,n,contextptr);
	if (is_undef(a))
	  return a;
	gen b=normal(tmp-a*n,contextptr);
	res  = res * pow(a,it->mult*n,contextptr) * pow(symbolic(at_factorial,n+b/a-1),it->mult,contextptr);
      }
    }
    return res*pow(r2e(Pcont,v,contextptr),n,contextptr);
  }

  // product(P,n=a..b) where the first variable in v is n
  gen product(const polynome & P,const vecteur & v,const gen & n,const gen & a,const gen & b,GIAC_CONTEXT){
    gen remains(1),res=product(P,v,n,remains,contextptr);
    res=subst(res,n,b+1,false,contextptr)/subst(res,n,a,false,contextptr);
    if (is_one(remains))
      return res;
    else
      return res*symbolic(at_product,makesequence(remains,n,a,b));
  }

  // solve vnext*sol[n+1]+v*sol[n]+vcst=0
  // example: u_{n+1}=u_n/n+1 -> n*u_{n+1}-u_n-n=0 
  // vnext=[1,0], v=[1], vcst=[-1,0] -> n solution
  static bool rsolve_particular(const modpoly & vnext,const modpoly & v,const modpoly & vcst,modpoly & sol,GIAC_CONTEXT){
    // find majoration of degree for solution
    int vnextdeg=int(vnext.size())-1,vdeg=int(v.size())-1,vcstdeg=int(vcst.size())-1,soldeg;
    soldeg=vcstdeg-giacmax(vnextdeg,vdeg);
    if (vnextdeg==vdeg && is_zero(vnext.front()+v.front()))
      soldeg++;
    if (soldeg<0)
      return false;
    modpoly vars(soldeg+1);
    for (int i=0;i<=soldeg;++i){
      vars[i]=identificateur("rsolve_x"+print_INT_(i));
    }
    modpoly equations=operator_plus(operator_times(vnext,taylor(vars,1,0),0),
				    operator_times(v,vars,0),0);
    equations=operator_plus(equations,vcst,0);
    sol=linsolve(equations,vars,contextptr);
    return !sol.empty() && !is_undef(sol);
  }

  // solve a recurrence relation x_{n+1}=l0*x_n+e
  static gen rsolve(const gen & e,const gen & n,const gen & l0,gen & remains,GIAC_CONTEXT){
    if (is_zero(e)){
      remains=0;
      return e;
    }
    vecteur v;
    polynome P,Q,R;
    if (!is_hypergeometric(e,*n._IDNTptr,v,P,Q,R,contextptr)){
      *logptr(contextptr) << gettext("Cst part must be hypergeometric") << "\n";
      remains=e;
      return 0;
    }
    if (Q.lexsorted_degree() || R.lexsorted_degree()){
      *logptr(contextptr) << gettext("Cst part must be of type a^n*P(n)") << "\n";
      remains=e;
      return 0;
    }
    gen a=r2e(Q,v,contextptr)/r2e(R,v,contextptr);
    gen P0=r2e(P,v,contextptr);
    if (is_zero(P0))
      return 0;
    for (int i=0;;i++){
      gen tmp=normal(subst(e/P0,n,i,false,contextptr),contextptr);
      if (!is_undef(tmp)){
	P0=tmp*P0/pow(a,i);
	break;
      }
    }
    v.clear();
    v.push_back(n);
    lvar(P0,v);
    lvar(l0,v);
    lvar(a,v);
    vecteur v1(v.begin()+1,v.end());
    gen l=e2r(l0,v1,contextptr);
    P0=e2r(P0,v,contextptr);
    gen P0n,P0d;
    fxnd(P0,P0n,P0d);
    if (P0n.type!=_POLY)
      P=polynome(P0n,int(v.size()));
    else
      P=*P0n._POLYptr;
    a=e2r(a,v1,contextptr);
    remains=0;
    // solve u(n+1)=l*u(n)+a^n*P(n)
    if (a==l){ // let v(n)=u(n)/l^n, then v(n+1)=v(n)+P(n)/l
      return pow(l0,n,contextptr)*sum(r2e(P,v,contextptr),n,remains,contextptr)/r2e(l,v1,contextptr)/r2e(P0d,v,contextptr);
    }
    // search u(n)=a^n*Q(n) with Q a polynomial of same degree than P
    // we have u(n+1)=a^(n+1)*Q(n+1)=l*a^n*Q(n)+a^n*P(n)
    // hence a*Q(n+1)-l*Q(n)=P(n)
    vecteur p=polynome2poly1(P,1);
    int pdeg=int(p.size())-1;
    vecteur q(pdeg+1);
    vreverse(p.begin(),p.end());
    for (int j=pdeg;j>=0;j--){
      gen tmp;
      for (int k=j+1;k<=pdeg;++k){
	tmp = tmp + comb(k,j)*q[k];
      }
      q[j]=(p[j]-a*tmp)/(a-l);
    }
    vreverse(q.begin(),q.end());
    gen res=r2e(q,v1,contextptr);
    if (res.type!=_VECT)
      return gentypeerr();
    res=symb_horner(*res._VECTptr,n);
    res=res*pow(r2e(a,v1,contextptr),n,contextptr)/r2e(P0d,v,contextptr);
    return res;
  }

  /*
  void gen2fracpoly1(const gen & g,vecteur & num,vecteur & den){
    if (g.type==_FRAC){
      num=gen2vecteur(g._FRACptr->num);
      den=gen2vecteur(g._FRACptr->den);	  
    }
    else {
      num=gen2vecteur(g);
      den=vecteur(1,1);
    }
  }
  */

  gen symb_seqsolve(const gen & g){
    return symbolic(at_seqsolve,g);
  }
  // solveseq(expression,[var,value])
  // var is a vector of dim the number of terms in the recurrence
  gen _seqsolve(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen f,x=vx_var(),uzero=zero,n;
    int dim=1,udim=1;
    if (args.type==_VECT){
      vecteur & v=*args._VECTptr;
      int s=int(v.size());
      if (!s)
	return gentoofewargs("");
      f=v[0];
      if (s>1)
	x=v[1];
      if (s>2)
	uzero=eval(v[2],eval_level(contextptr),contextptr);
      if (x.type==_VECT){
	dim=int(x._VECTptr->size());
	if (uzero.type==_VECT)
	  udim=int(uzero._VECTptr->size());
      }
    }
    else
      f=args;
    x=gen2vecteur(x);
    vecteur fv=gen2vecteur(f);
    fv=quote_eval(fv,*x._VECTptr,contextptr);
    bool fv1=false;
    if (fv.size()==1 && fv.front().type==_VECT){
      fv1=true;
      vecteur tmp=*fv.front()._VECTptr;
      fv=tmp;
    }
    if (dim==udim+1){
      n=x._VECTptr->back();
      if (n.type!=_IDNT){
	identificateur nn(" rsolve_n");
	x._VECTptr->back()=nn;
	f=quotesubst(f,n,nn,contextptr);
	gen res=_seqsolve(makesequence(f,x,uzero),contextptr);
	return quotesubst(res,nn,n,contextptr);
      }
      x=vecteur(x._VECTptr->begin(),x._VECTptr->end()-1);
      --dim;
    }
    else {
      if (dim!=udim)
	return gendimerr();
      n=gen(identificateur("rsolve_n"));
    }
    int fs=int(fv.size());
    if (dim<fs) return gensizeerr(contextptr);
    int add=dim-fs;
    if (f.type!=_VECT && !fv1){
      f=vecteur(x._VECTptr->end()-add,x._VECTptr->end());
      f._VECTptr->push_back(fv[0]);
    }
    else {
      for (int i=0;i<add;++i)
	fv.push_back(x[i]);
      f=fv;
    }
    // check linear recurrence + admissible cst term wrt n
    gen difff=derive(f,x,contextptr);
    if (is_undef(difff) || difff.type!=_VECT)
      return difff;
    matrice m=mtran(*difff._VECTptr);
    if (!is_zero(derive(m,x,contextptr))){
      if (f._VECTptr->size()==1 && x._VECTptr->size()==1 && is_zero(derive(f,n,contextptr))){
	// homographic?
	gen tmp=ratnormal(f._VECTptr->front(),contextptr),var(x._VECTptr->front());
	gen tmpn=_getNum(tmp,contextptr),tmpd=_getDenom(tmp,contextptr),a,b,c,d;
	if (is_linear_wrt(tmpn,var,a,b,contextptr) && is_linear_wrt(tmpd,var,c,d,contextptr)&& !is_zero(c)){
	  // u_{n+1}=(a*u_n+b)/(c*u_n+d)
	  // find fixed points
	  gen fixed=solve(a*n+b-(c*n+d)*n,n,1,contextptr);
	  if (fixed.type==_VECT && fixed._VECTptr->size()==2){
	    gen r1=fixed._VECTptr->front(),r2=fixed._VECTptr->back();
	    // (u_n-r1)/(u_n-r2) is geometric, compute ratio
	    gen un1=(a*n+b)/(c*n+d);
	    gen r=normal((un1-r1)*(n-r2)/((un1-r2)*(n-r1)),contextptr);
	    if (is_zero(derive(r,n,contextptr))){
	      un1=pow(r,n,contextptr)*(var-r1)/(var-r2);
	      return (r1-un1*r2)/(1-un1);
	    }
	  }
	  if (fixed.type==_VECT && fixed._VECTptr->size()==1){
	    gen r1=fixed._VECTptr->front();
	    // 1/(u_n-r1) is arithmetic
	    gen un1=(a*n+b)/(c*n+d);
	    gen r=normal(inv(un1-r1,contextptr)-inv(n-r1,contextptr),contextptr);
	    if (is_zero(derive(r,n,contextptr))){
	      un1=r*n+inv(var-r1,contextptr);
	      return inv(un1,contextptr)+r1;
	    }
	  }
	}
      }
      return symb_seqsolve(args);
    }
    vecteur cst=*normal(subvecteur(*f._VECTptr,multmatvecteur(m,*x._VECTptr)),contextptr)._VECTptr;
    if (m.size()==1 && cst.size()==1){
      gen l(m[0][0]),c(cst[0]),remains;
      gen u0=gen2vecteur(uzero)[0];
      if (n.type!=_IDNT)
	return gentypeerr();
      if (!is_zero(derive(l,n,contextptr))){
	vecteur v;
	polynome P,Q,R;
	if (!is_hypergeometric(l,*n._IDNTptr,v,P,Q,R,contextptr)){
	  *logptr(contextptr) << gettext("Cst part must be hypergeometric") << "\n";
	  return symb_seqsolve(args);
	}
	// l(n+1)/l(n) = P(n+1)/P(n)*Q(n)/R(n+1)
	std::vector< monomial<gen> >::const_iterator it=Q.coord.begin();
	polynome q0=Tnextcoeff<gen>(it,Q.coord.end()).untrunc1();
	it=R.coord.begin();
	polynome r0=Tnextcoeff<gen>(it,R.coord.end()).untrunc1();
	if (!is_zero(r0*Q-q0*R)){
	  *logptr(contextptr) << gettext("Unable to handle coeff of homogeneous part") << "\n";
	  return symb_seqsolve(args);
	}
	// -> l(n)=l(0)*P(n)/P(0) -> product(l(n))
	gen pn=r2e(P,v,contextptr)/r2e(Q,v,contextptr);
	gen q0r0=r2e(q0,v,contextptr)/r2e(r0,v,contextptr);
	gen res=pow(subst(normal(l/pn,contextptr),n,0,false,contextptr),n,contextptr)*simplify(product(P,v,n,0,n-1,contextptr)/product(Q,v,n,0,n-1,contextptr),contextptr)*pow(q0r0,n*(n-1)/2,contextptr);
	// then we might search for a polynomial particular solution to e
	if (is_zero(c))
	  return u0/subst(res,n,0,false,contextptr)*res;
	if (!is_one(q0r0))
	  return gensizeerr("Unable to find particular solution, general solution is "+res.print(contextptr));
	// u_{n+1}=l*u_{n}+c
	gen tmp=l*x[0]+c,tmpnum,tmpden;
	vecteur tmpv(1,n);
	lvar(tmp,tmpv);
	tmp=e2r(tmp,tmpv,contextptr);
	fxnd(tmp,tmpnum,tmpden);
	tmpnum=r2e(tmpnum,tmpv,contextptr);
	tmpden=r2e(tmpden,tmpv,contextptr);
	tmpnum=_e2r(makesequence(tmpnum,n),contextptr);
	tmpden=_e2r(makesequence(tmpden,n),contextptr);
	if (is_zero(derive(tmpnum,n,contextptr)) && 
	    is_zero(derive(tmpnum,n,contextptr)) &&
	    tmpnum.type==_VECT && tmpden.type==_VECT){
	  vecteur tmpn,tmpd,ln,cn;
	  tmpn=*tmpnum._VECTptr;
	  tmpd=*tmpden._VECTptr;
	  gen difftmpn=derive(tmpn,x[0],contextptr);
	  if (is_undef(difftmpn) || difftmpn.type!=_VECT)
	    return gensizeerr(contextptr);
	  ln=trim(*difftmpn._VECTptr,0);
	  cn=trim(subst(tmpn,x[0],0,false,contextptr),0);
	  if (rsolve_particular(-tmpd,ln,cn,tmpn,contextptr)){
	    gen sol=_r2e(makesequence(tmpn,n),contextptr);
	    // general solution is sol+C*res, sol(0)+C*res(0)=u0
	    gen C=subst((u0-sol)/res,n,0,false,contextptr);
	    return sol+C*res;
	  }
	}
	*logptr(contextptr) << gettext("Unable to find a particular solution for inhomogeneous part") << "\n";
	return symb_seqsolve(args);
      }
      gen d=linear_apply(c,n,l,remains,contextptr,rsolve);
      if (!is_zero(remains))
	*logptr(contextptr) << gettext("Unable to solve recurrence") << "\n";
      // d is a particular solution of u(n+1)=l*u(n)+c(n)
      // add a general solution d(n)+C*l^n
      // such that at n=0 we get u0 -> C+d(0)=u0
      gen C=normal(u0-quotesubst(d,n,0,contextptr),contextptr);
      return d+C*pow(l,n,contextptr);
    }
    // u(n+1)=M*u(n)+cst
    // Let M=PDP^-1, v=P^-1*u, v satisfies v(n+1)=Dv(n)+P^-1*cst
    // solve for v then for u
    if (!is_zero(derive(m,n,contextptr)))
      return symb_seqsolve(args);
    if (has_num_coeff(m))
      m=*evalf(m,1,contextptr)._VECTptr;
    matrice P,Pinv,D;
    bool b=complex_mode(contextptr);
    complex_mode(true,contextptr);
    egv(m,P,D,contextptr,true,false,false);
    complex_mode(b,contextptr);
    Pinv=minv(P,contextptr);
    if (is_undef(Pinv))
      return Pinv;
    // if (fs==1) uzero=_revlist(uzero);
    vecteur vzero=multmatvecteur(Pinv,*uzero._VECTptr);
    vecteur vcst=multmatvecteur(Pinv,cst);
    int taille=int(m.size());
    vecteur res(taille);
    for (int i=taille-1;i>=0;--i){
      // find cst coefficient
      gen c=vcst[i],l=D[i][i];
      for (int j=i+1;j<taille;j++){
	c += D[i][j]*res[j];
      }
      c=normal(c,contextptr);
      gen remains,d=linear_apply(c,n,l,remains,contextptr,rsolve);
      if (!is_zero(remains))
	*logptr(contextptr) << gettext("Unable to solve recurrence") << "\n";
      gen C=normal(vzero[i]-quotesubst(d,n,0,contextptr),contextptr);
      if (is_zero(l))
	res[i]=d+C*symbolic(at_same,makesequence(n,0));
      else
	res[i]=d+C*pow(l,n,contextptr);
    }
    // u=P*v
    res=multmatvecteur(P,res);
    if (fs==1)
      //return normal(subst(res[0],n,n-add,false,contextptr),contextptr);// normal(res[0]); 
      return ratnormal(res[0],contextptr);
    else
      return ratnormal(res,contextptr);
  }
  static const char _seqsolve_s []="seqsolve";
  static define_unary_function_eval_quoted (__seqsolve,&_seqsolve,_seqsolve_s);
  define_unary_function_ptr5( at_seqsolve ,alias_at_seqsolve,&__seqsolve,_QUOTE_ARGUMENTS,true);

  static vecteur rsolve_initcond(const vecteur & initcond,const gen & un,const gen & u,const gen & n,const vecteur & uinit,GIAC_CONTEXT){
    if (initcond.empty())
      return makevecteur(un);
    gen initc=subst(initcond,u,_unapply(makesequence(un,n),contextptr),false,contextptr);
    initc=initc.eval(1,contextptr);
    if (initc.type!=_VECT) 
      return vecteur(1,gensizeerr());
    vecteur valv=gsolve(*initc._VECTptr,uinit,/* complex mode */ true,0,contextptr);
    if (is_undef(valv))
      return valv;
    vecteur resv(valv.size());
    for (unsigned int i=0;i<valv.size();++i){
      resv[i]=normal(quotesubst(un,uinit,valv[i],contextptr),contextptr);
    }
    return resv;
  }

  // example f : u(n+1)=2*u(n)+n, initcond [u(0)=1]
  static gen crsolve(const gen & f0,const gen &u,const gen & n,vecteur & initcond0,GIAC_CONTEXT){
    if (f0.type==_VECT){
      if (u.type!=_VECT || f0._VECTptr->size()!=u._VECTptr->size())
	return gendimerr();
    }
    else {
      if (u.type==_VECT)
	return gendimerr();
    }
    vecteur uv(gen2vecteur(u));
    int uvs=int(uv.size());
    vecteur initcond;
    aplatir(*apply(initcond0,equal2diff)._VECTptr,initcond);
    gen f=apply(f0,equal2diff);
    if (n.type!=_IDNT){
      identificateur N(" rsolve_N");
      gen F=quotesubst(f,n,N,contextptr);
      gen tmp=crsolve(F,u,N,initcond0,contextptr);
      return quotesubst(tmp,N,n,contextptr);
    }
    vecteur vof0(lop(f,at_of));
    // keep only those with u
    vecteur vofa,vofb,vopos,vofun;
    bool all_one=true,all_bzero=true;
    for (const_iterateur it=vof0.begin();it!=vof0.end();++it){
      gen & itf=it->_SYMBptr->feuille;
      int pos;
      if (itf.type==_VECT && itf._VECTptr->size()==2 && (pos=equalposcomp(uv,itf._VECTptr->front())) ){
	gen tmp=it->_SYMBptr->feuille._VECTptr->back(),a,b;
	if (is_linear_wrt(tmp,n,a,b,contextptr) && !is_zero(a)){
	  if (!is_one(a))
	    all_one=false;
	  if (!is_zero(b))
	    all_bzero=false;
	  vofa.push_back(a);
	  vofb.push_back(b);
	  vopos.push_back(pos-1);
	  vofun.push_back(*it);
	}
	else
	  return gensizeerr(gettext("Unable to handle this kind of recurrence"));
      }
    }
    if (vofa.empty())
      return undef;
    if (all_one){
      gen bmin=vofb.front(),bmax=vofb.front();
      int nmax=0;
      for (const_iterateur it=vofb.begin();it!=vofb.end();++it){
	const gen & tmp = *it;
	if (is_strictly_greater(tmp,bmax,contextptr)){
	  nmax=int(it-vofb.begin());
	  bmax=tmp;
	}
	if (is_strictly_greater(bmin,tmp,contextptr))
	  bmin=tmp;
      }
      if (bmin.type!=_INT_ || bmax.type!=_INT_)
	return gensizeerr(gettext("Bad indexes in recurrence"));
      int m=bmin.val,M=bmax.val;
      if (uvs>1){
	if (M!=m+1)
	  return gendimerr(gettext("Multi-recurrences implemented only for 1 step"));
	// generate identifiers x0,...,x_{uvs-1} and y0,...,y_{uvs-1}
	vecteur idx,idX;
	for (int i=0;i<uvs;++i){
	  idx.push_back(gen(identificateur("rsolve_x"+print_INT_(i))));
	  idX.push_back(gen(identificateur("rsolve_X"+print_INT_(i))));
	}
	vecteur vofid;
	for (const_iterateur it=vofb.begin();it!=vofb.end();++it){
	  if (it->type!=_INT_)
	    return gensizeerr();
	  bool isx=it->val==m;
	  int pos=vopos[it-vofb.begin()].val;
	  vofid.push_back(isx?idx[pos]:idX[pos]);
	}
	gen F=quotesubst(f,vofun,vofid,contextptr);
	if (F.type!=_VECT)
	  return gensizeerr();
	// solves for idX in terms of idx
	vecteur Fv=gsolve(*F._VECTptr,idX,1/* complex */,0/* approx mode=no*/,contextptr),res;
	if (is_undef(Fv))
	  return Fv;
	for (const_iterateur it=Fv.begin();it!=Fv.end();++it){
	  vecteur idxn(idx);
	  idxn.push_back(n);
	  gen vn=_seqsolve(makevecteur(*it,idxn,idx),contextptr);
	  gen un=quotesubst(vn,n,n+1-M,contextptr);
	  if (un.type!=_VECT) 
	    return gensizeerr("Unable to solve this recurrence");
	  if (int(un._VECTptr->size())!=uvs)
	    return gendimerr(contextptr);
	  if (initcond.empty())
	    return vecteur(1,un);
	  // solve initial conditions
	  vecteur vout;
	  for (int i=0;i<uvs;++i){
	    vout.push_back(_unapply(makesequence(un[i],n),contextptr));
	  }	  
	  gen initc=subst(initcond,uv,vout,false,contextptr);
	  initc=initc.eval(1,contextptr);
	  if (initc.type!=_VECT) 
	    return gensizeerr();
	  vecteur valv=gsolve(*initc._VECTptr,idx,/* complex mode */ true,/* approx=no */ 0,contextptr);
	  if (is_undef(valv))
	    return valv;
	  for (unsigned int i=0;i<valv.size();++i){
	    res.push_back(normal(quotesubst(un,idx,valv[i],contextptr),contextptr));
	  }
	}
	return res;
      }
      // generate identifiers xm,...,xM
      vecteur ids;
      for (int i=m;i<=M;++i){
	ids.push_back(gen(identificateur("rsolve_x"+print_INT_(i-m))));
      }
      // replace vofun
      vecteur vofid;
      for (const_iterateur it=vofb.begin();it!=vofb.end();++it){
	if (it->type!=_INT_)
	  return gensizeerr();
	vofid.push_back(ids[it->val-m]);
      }
      gen F=quotesubst(f,vofun,vofid,contextptr),a,b;
      if (!is_linear_wrt(F,ids.back(),a,b,contextptr))
	return gensizeerr();
      F=-b/a; // xM in terms of xm to xM-1
      ids.back()=n;
      vecteur uinit(ids);
      uinit.pop_back();
      // let v_n=u_{n+m} -> u_n=v_{n-m}
      // old code
      // gen vn=_seqsolve(makevecteur(F,ids,uinit),contextptr);
      // gen un=quotesubst(vn,n,n-m,contextptr);
      // new code that solves rsolve(u(n)=(n)*u(n-1),u(n),u(1)=3);
      gen Fm=quotesubst(F,n,n-m,contextptr);
      gen un=_seqsolve(makevecteur(Fm,ids,uinit),contextptr);
      // end new code
      return rsolve_initcond(initcond,un,u,n,uinit,contextptr);
    }
    // divide and conquier?
    if (uvs==1 && vofa.size()==2 && all_bzero){
      gen idy=identificateur("rsolve_y"),x(identificateur("rsolve_x"));
      vecteur ids(makevecteur(x,idy));
      // replace vofun
      vecteur vofid;
      gen b;
      if (is_one(vofa[0])){
	vofid=ids;
	b=vofa[1];
      }
      if (is_one(vofa[1])){
	vofid=makevecteur(ids[1],ids[0]);
	b=vofa[0];
      }
      if (vofid.empty() || ck_is_greater(1,b,contextptr))
	return gensizeerr();
      vofun.push_back(n);
      vofid.push_back(pow(b,n,contextptr));
      gen F=quotesubst(f,vofun,vofid,contextptr);
      // F(x,y,n)=0 where y=t(b*n) x=t(n)
      // auxiliary sequence u(n)=t(b^n), verifies F(u(n+1),u(n),b^n)=0
      gen A,B;
      if (is_linear_wrt(F,idy,A,B,contextptr)){
	F=-B/A; // u(n+1) in terms of u(n)=x
	gen vn=_seqsolve(makevecteur(F,makevecteur(x,n),x),contextptr);
	gen un=quotesubst(vn,n,ln(n,contextptr)/ln(b,contextptr),contextptr);
	// solve initial cond
	return rsolve_initcond(initcond,un,u,n,makevecteur(x),contextptr);
      }
    }
    return gensizeerr(gettext("Not yet implemented"));
  }
  static gen rsolve(const gen & f0,const gen &u,const gen & n,vecteur & initcond0,int st,GIAC_CONTEXT){
    bool b=complex_mode(contextptr);
    complex_mode(true,contextptr);
    gen res=crsolve(f0,u,n,initcond0,contextptr);
    if (!b){
      complex_mode(b,contextptr);
      // FIXME take real part for res
    }
    return res;
  }
  // example args=u(n+1)=2*u(n)+n,u(n),u(0)=1
  gen _rsolve(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    vecteur varg=gen2vecteur(args);
    if (debug_infolevel>20)
      varg.dbgprint();
    int s=int(varg.size());
    if (!s)
      return gendimerr();
    gen f,u,n;
    vecteur initcond;
    if (s>1){
      gen un=varg[1];
      if (un.is_symb_of_sommet(at_of)){
	gen & unf=un._SYMBptr->feuille;
	if (unf.type==_VECT && unf._VECTptr->size()==2){
	  u=unf._VECTptr->front();
	  n=unf._VECTptr->back();
	}
      }
      if (un.type==_VECT){
	vecteur & unv=*un._VECTptr;
	vecteur uv;
	for (const_iterateur it=unv.begin();it!=unv.end();++it){
	  const gen & itg=*it;
	  if (itg.is_symb_of_sommet(at_of)){
	    gen & unf=itg._SYMBptr->feuille;
	    if (unf.type==_VECT && unf._VECTptr->size()==2){
	      uv.push_back(unf._VECTptr->front());
	      if (is_zero(n))
		n=unf._VECTptr->back();
	      else
		if (n!=unf._VECTptr->back())
		  return gentypeerr();
	    }
	  }
	}
	u=uv;
      }
      if (is_zero(n))
	return gentypeerr();
    }
    else {
      u=gen(identificateur("rsolve_u"));
      n=gen(identificateur("rsolve_n"));
    }
    vecteur quoted=gen2vecteur(u);
    quoted.push_back(n);
    varg=quote_eval(varg,quoted,contextptr);
    f=varg[0];
    if (s>2){
      initcond=vecteur(varg.begin()+2,varg.end());
      if (initcond.size()==1 && initcond.front().type==_VECT){
	// use a temporary vector,
	// otherwise the source is destroyed when copying the first element of the vector
	// another way to do that could be to have a gen with a copy of initcond.front
	vecteur tmp=*initcond.front()._VECTptr;
	initcond=tmp;
      }
    }
    else {
      if (f.type==_VECT && !f._VECTptr->empty()){
	if (u.type!=_VECT || f._VECTptr->size()!=u._VECTptr->size()){
	  initcond=*f._VECTptr;
	  f=initcond.front();
	  initcond.erase(initcond.begin());
	}
      }
    }
    int st=step_infolevel(contextptr);
    step_infolevel(0,contextptr);
    gen res=rsolve(f,u,n,initcond,st,contextptr);
    step_infolevel(st,contextptr);
    return res;
  }
  static const char _rsolve_s []="rsolve";
  static define_unary_function_eval_quoted (__rsolve,&_rsolve,_rsolve_s);
  define_unary_function_ptr5( at_rsolve ,alias_at_rsolve,&__rsolve,_QUOTE_ARGUMENTS,true);


  gen _makemod(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gentypeerr();
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (is_zero(b))
      return unmod(a);
    if (!is_integer(a) || !is_integer(b))
      return gentypeerr();
    return makemod(a,b);
  }
  static const char _makemod_s []="makemod";
  static define_unary_function_eval (__makemod,&_makemod,_makemod_s);
  define_unary_function_ptr5( at_makemod ,alias_at_makemod,&__makemod,0,true);

  gen _hexprint(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type == _INT_)
      return string2gen(hexa_print_INT_(g.val), false);  
    if (g.type == _ZINT)
      return string2gen(hexa_print_ZINT(*g._ZINTptr), false);
    return gentypeerr();
  }
  static const char _hexprint_s []="hex";
  static define_unary_function_eval (__hexprint,&_hexprint,_hexprint_s);
  define_unary_function_ptr5( at_hexprint ,alias_at_hexprint,&__hexprint,0,true);
  
  gen _octprint(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type == _INT_)
      return string2gen(octal_print_INT_(g.val), false);	
    if (g.type == _ZINT)
      return string2gen(octal_print_ZINT(*g._ZINTptr), false);
    return gentypeerr();
  }
  static const char _octprint_s []="oct";
  static define_unary_function_eval (__octprint,&_octprint,_octprint_s);
  define_unary_function_ptr5( at_octprint ,alias_at_octprint,&__octprint,0,true);
  
  gen _binprint(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type == _INT_)
      return string2gen(binary_print_INT_(g.val), false);
    if (g.type == _ZINT)
      return string2gen(binary_print_ZINT(*g._ZINTptr), false);
    return gentypeerr();
  }
  static const char _binprint_s[]="bin";
  static define_unary_function_eval(__binprint,&_binprint,_binprint_s);
  define_unary_function_ptr5(at_binprint,alias_at_binprint,&__binprint,0,true);
  // const unary_function_ptr at_binprint (&__binprint,0,true);
  


#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC

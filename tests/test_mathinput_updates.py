"""Native checks for imported editor completion, cleanup and clipping behavior."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MathInputUpdateTests(unittest.TestCase):
    def run_cpp(self, code):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'test.cc'
            binary = Path(directory) / 'test'
            source.write_text('#include <cassert>\n#include <cstring>\n#include <string>\n#include <vector>\n#include <cctype>\n' + code)
            subprocess.run([os.environ.get('HOST_CXX', 'c++'), '-std=c++14',
                            '-fsanitize=address,undefined', '-g', str(source),
                            '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_empty_cells_and_quoted_cleanup(self):
        source = (ROOT / 'src/mathinput_edit.cc').read_text()
        helpers = source[source.index('// Closing quote,'):source.index('// the text of a key typed')]
        self.run_cpp(helpers + r'''
int main() {
  struct Case { const char *input, *expected; } cases[] = {
    {"[[,],[,]]", "[[0,0],[0,0]]"},
    {"[,2,]", "[0,2,0]"},
    {"integrate(x,x,,)", "integrate(x,x)"},
    {"series(sin(x),x,,,polynom)", "series(sin(x),x,0,5,polynom)"},
    {"\"series(x,x,,,polynom),,)\"", "\"series(x,x,,,polynom),,)\""},
    {"f(\"a\\\",,)\",x,,)", "f(\"a\\\",,)\",x)"},
    {"[[\"[,],)\",],[,2]]", "[[\"[,],)\",0],[0,2]]"},
    {"\"unfinished series(x,x,,,polynom)", "\"unfinished series(x,x,,,polynom)"}
  };
  for (auto c : cases) {
    char text[256]; strcpy(text,c.input);
    mie_clean(text,sizeof(text)); assert(!strcmp(text,c.expected));
  }
  char exact[]="[[,],[,]]";
  mie_clean(exact,sizeof(exact)); assert(strlen(exact)<sizeof(exact));
}
''')

    def test_bars_clip_and_diagonals_stay_inside(self):
        source = (ROOT / 'src/mathinput_glue.cc').read_text()
        helpers = source[source.index('static int mi_x0'):source.index('// draws layout')]
        self.run_cpp(r'''
struct Line { int xa,ya,xb,yb,color; };
std::vector<Line> lines;
void draw_line(int a,int b,int c,int d,int e) { lines.push_back({a,b,c,d,e}); }
''' + helpers + r'''
int main() {
  mi_x0=10; mi_x1=20; mi_y0=30; mi_y1=40; mi_ink=7;
  mi_line(0,35,25,35); mi_line(25,36,0,36);
  mi_line(15,0,15,45); mi_line(16,45,16,0);
  mi_line(0,29,25,29); mi_line(20,0,20,45);
  mi_line(0,35,15,36); mi_line(10,30,19,39);
  assert(lines.size()==5);
  assert(lines[0].xa==10 && lines[0].xb==19 && lines[0].ya==35);
  assert(lines[1].xa==10 && lines[1].xb==19 && lines[1].ya==36);
  assert(lines[2].ya==30 && lines[2].yb==39 && lines[2].xa==15);
  assert(lines[3].ya==30 && lines[3].yb==39 && lines[3].xa==16);
  for (auto l : lines) {
    assert(l.xa>=10 && l.xa<20 && l.xb>=10 && l.xb<20);
    assert(l.ya>=30 && l.ya<40 && l.yb>=30 && l.yb<40 && l.color==7);
  }
}
''')

    def test_completion_replaces_name_or_call_without_overflow(self):
        source = (ROOT / 'src/mathinput_edit.cc').read_text()
        insert = source[source.index('static bool mie_insert('):source.index('// Closing quote,')]
        start = source.index('static void mie_insert_key_text(')
        insertion = source[start:source.index('\n}', start)+2]
        a = source.index("    if (key=='\\t'){")
        z = source.index('    char catbuf[512];',a)
        block = source[a:z]
        # Execute the same branch in a one-iteration key loop.
        prefix = r'''
static std::string chosen;
static int erase_count;
static std::string query;
bool is_alphanum(char c) { return std::isalnum((unsigned char)c); }
std::string help_insert(const char *s,int &back,bool) {
  query=s; back=erase_count; return chosen;
}
'''
        self.run_cpp(prefix + insert + insertion + r'''
void complete(char *s,int cap,int &p) {
  int key='\t',sa=0,sb=0; bool full=false;
  for (int once=0;once<1;++once) {
''' + block + r'''
  }
}
int main() {
  char s[128]="1+fac"; int p=5;
  chosen="factor("; erase_count=3; complete(s,sizeof(s),p);
  assert(query=="1+fac"); assert(!strcmp(s,"1+factor()")); assert(p==9);
  strcpy(s,"1+factor(x^2-1)+2"); p=9;
  chosen="factor(x^4+4)"; erase_count=7; complete(s,sizeof(s),p);
  assert(query=="1+factor("); assert(!strcmp(s,"1+factor(x^4+4)+2"));
  assert(p==15);
  strcpy(s,"factor(x)"); p=7; chosen=""; erase_count=0;
  complete(s,sizeof(s),p); assert(!strcmp(s,"factor(x)") && p==7);
  char tiny[5]="fac"; p=3; chosen="factor("; erase_count=3;
  complete(tiny,sizeof(tiny),p); assert(!strcmp(tiny,"fac") && p==3);
}
''')


if __name__ == '__main__':
    unittest.main()

#ifdef KHICAS_BENCHMARK
#include <giac/giacPCH.h>
#include <giac/input_parser.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#undef clock
#include <time.h>

volatile unsigned char khicas_benchmark_mode = 1;
extern int dconsole_mode;
extern "C" volatile char oom_hit;
extern "C" {
extern const char khicas_suite_selector[] __attribute__((used)) = "KHICAS_SUITE_SELECT=000";
}
struct Workload
{
    const char* expr;
    unsigned char kind;
    const char* aux;
};
static const Workload workloads[] = {
#include "cases.inc"
};

static void quiet_steps(unsigned, const std::string&, const giac::vecteur&, const giac::context*)
{}

static double ab(double x) { return x < 0 ? -x : x; }

static bool number(const giac::gen& g, double& v, giac::context* ctx)
{
    const giac::gen a = giac::evalf(g, 1, ctx);
    if (a.type == giac::_DOUBLE_)
    {
        v = a._DOUBLE_val;
        return v == v;
    }
    if (a.type == giac::_INT_)
    {
        v = a.val;
        return true;
    }
    return false;
}

static bool equivalent(const giac::gen& a, const giac::gen& b, bool approximate, giac::context* ctx)
{
    if (a.type == giac::_VECT && b.type == giac::_VECT)
    {
        if (a._VECTptr->size() != b._VECTptr->size())return false;
        for (unsigned i = 0; i < a._VECTptr->size(); ++i)if (!equivalent(
            (*a._VECTptr)[i], (*b._VECTptr)[i], approximate, ctx))return false;
        return true;
    }
    if (approximate)
    {
        double u, v;
        return number(a, u, ctx) && number(b, v, ctx) && ab(u - v) <= 0.003 * (1 + ab(v));
    }
    return giac::is_zero(giac::normal(a - b, ctx));
}

static giac::gen strip_order(const giac::gen& g)
{
    if (g.is_symb_of_sommet(giac::at_order_size))return 0;
    if (g.type == giac::_SYMB)return giac::symbolic(g._SYMBptr->sommet, strip_order(g._SYMBptr->feuille));
    if (g.type == giac::_VECT)
    {
        giac::vecteur v = *g._VECTptr;
        for (unsigned i = 0; i < v.size(); ++i)v[i] = strip_order(v[i]);
        return giac::gen(v, g.subtype);
    }
    return g;
}

static unsigned complex_points(const giac::gen& g)
{
    if (g.type == giac::_CPLX)return 1;
    if (g.type == giac::_SYMB)return complex_points(g._SYMBptr->feuille);
    unsigned n = 0;
    if (g.type == giac::_VECT)for (unsigned i = 0; i < g._VECTptr->size(); ++i)n += complex_points((*g._VECTptr)[i]);
    return n;
}

static bool eigencheck(const giac::gen& r, const giac::gen& m, bool vectors, giac::context* ctx)
{
    if (r.type != giac::_VECT || r._VECTptr->size() != 4)return false;
    double a[4][4];
    for (unsigned i = 0; i < 4; ++i)for (unsigned j = 0; j < 4; ++j)if (!number(
        (*(*m._VECTptr)[i]._VECTptr)[j], a[i][j], ctx))return false;
    if (!vectors)
    {
        double sum = 0, sq = 0, t = 0, t2 = 0;
        for (unsigned i = 0; i < 4; ++i)
        {
            double v;
            const giac::gen& row = (*r._VECTptr)[i];
            const giac::gen& value = row.type == giac::_VECT ? (*row._VECTptr)[i] : row;
            if (!number(value, v, ctx))return false;
            sum += v;
            sq += v * v;
            t += a[i][i];
            for (unsigned j = 0; j < 4; ++j)t2 += a[i][j] * a[j][i];
        }
        return ab(sum - t) < 0.003 * (1 + ab(t)) && ab(sq - t2) < 0.003 * (1 + ab(t2));
    }
    for (unsigned j = 0; j < 4; ++j)
    {
        double v[4], av[4] = {0, 0, 0, 0}, norm = 0, dot = 0;
        for (unsigned i = 0; i < 4; ++i)
        {
            const giac::gen& row = (*r._VECTptr)[i];
            if (row.type != giac::_VECT || row._VECTptr->size() != 4 || !number((*row._VECTptr)[j], v[i], ctx))return
                false;
            norm += v[i] * v[i];
        }
        if (norm < 1e-9)return false;
        for (unsigned i = 0; i < 4; ++i)
        {
            for (unsigned k = 0; k < 4; ++k)av[i] += a[i][k] * v[k];
            dot += v[i] * av[i];
        }
        double lambda = dot / norm;
        for (unsigned i = 0; i < 4; ++i)if (ab(av[i] - lambda * v[i]) > 0.003 * (1 + ab(av[i])))return false;
    }
    return true;
}

static bool validate(unsigned n, const giac::gen& r, giac::context* ctx)
{
    const Workload& w = workloads[n];
    if (giac::is_undef(r) || (r.type == giac::_STRNG && r.subtype == -1) || giac::interrupted)return false;
    const giac::gen aux = giac::eval(giac::gen(w.aux, ctx), giac::eval_level(ctx), ctx);
    if (w.kind == 0 || w.kind == 1)return equivalent(r, aux, w.kind == 1, ctx);
    if (w.kind == 2)
    {
        if (r.is_symb_of_sommet(giac::at_integrate))return false;
        const giac::gen x("x", ctx), d = giac::derive(r, x, ctx);
        const double points[] = {0.17, 0.37, 0.53, 0.71, 0.83};
        for (unsigned i = 0; i < 5; ++i)if (!equivalent(giac::subst(d, x, giac::gen(points[i]), false, ctx),
                                                        giac::subst(aux, x, giac::gen(points[i]), false, ctx), true,
                                                        ctx))return false;
        return true;
    }
    if (w.kind == 3)return r.is_symb_of_sommet(giac::at_prod) && equivalent(r, aux, false, ctx);
    if (w.kind == 4)
    {
        if (r.type != giac::_VECT || r._VECTptr->size() != 5)return false;
        const giac::gen x("x", ctx);
        for (unsigned i = 0; i < 5; ++i)if (!giac::is_zero(
            giac::normal(giac::subst(aux, x, (*r._VECTptr)[i], false, ctx), ctx)))return false;
        return true;
    }
    if (w.kind == 5)return equivalent(strip_order(r), aux, false, ctx);
    if (w.kind == 6)
    {
        const unsigned points = complex_points(aux);
        // Inspect samples without formatting a potentially very large curve string.
        sprintf((char*)0xFB0000, "KHICAS_V2_PLOT|%u|%u\n", n, points);
        // nodisp must discard an actually calculated sampled plot; no graph UI is called.
        return points > 20 && r.type == giac::_STRNG && *r._STRNGptr == "Done";
    }
    if (w.kind == 7)return r.type == giac::_VECT && r._VECTptr->size() >= 3;
    if (w.kind == 8 || w.kind == 9)return eigencheck(r, aux, w.kind == 9, ctx);
    return false;
}

int khicas_run_benchmarks(giac::context* ctx)
{
    const volatile char* selector = khicas_suite_selector;
    const unsigned offset = sizeof("KHICAS_SUITE_SELECT=") - 1;
    const unsigned n = 100 * (selector[offset] - '0') + 10 * (selector[offset + 1] - '0') + selector[offset + 2] - '0';
    if (n >= sizeof(workloads) / sizeof(workloads[0]))return 1;
    dconsole_mode = 0; // all console diagnostics are disabled before Console_Init
    giac::angle_radian(true, ctx);
    giac::my_gprintf = &quiet_steps; // suppress diagnostic console rendering for CAS-only timing
    sprintf((char*)0xFB0000, "KHICAS_V2_BEGIN|%u|%lu\n", n, (unsigned long)CLOCKS_PER_SEC);
    for (unsigned repeat = 0; repeat < 4; ++repeat)
    {
        srand(12345);
        giac::ctrl_c = giac::kbd_interrupted = giac::interrupted = false;
        sprintf((char*)0xFB0000, "KHICAS_V2_START|%u|%u\n", n, repeat);
        const clock_t start = clock();
        giac::gen input(workloads[n].expr, ctx);
        const clock_t parsed = clock();
        giac::gen result = giac::eval(input, giac::eval_level(ctx), ctx);
        const clock_t evaluated = clock();
        auto text = result.print(ctx);
        const clock_t printed = clock();
        for (unsigned j = 0; j < text.size(); ++j)if (text[j] == '\n' || text[j] == '\r')text[j] = ' ';
        sprintf((char*)0xFB0000, "KHICAS_V2_SAMPLE|%u|%u|%lu|%lu|%lu|%s\n", n, repeat, (unsigned long)(parsed - start),
                (unsigned long)(evaluated - parsed), (unsigned long)(printed - evaluated), text.c_str());
        if (giac::interrupted)
        {
            sprintf((char*)0xFB0000, "KHICAS_V2_HEALTH|%u|%u|%u\n", n, unsigned(oom_hit), unsigned(giac::interrupted));
            sprintf((char*)0xFB0000, "KHICAS_V2_DONE\n");
            return 1;
        }
        if (repeat == 3)
        {
            const bool ok = validate(n, result, ctx);
            sprintf((char*)0xFB0000, "KHICAS_V2_CHECK|%u|%u\n", n, unsigned(ok));
        }
    }
    sprintf((char*)0xFB0000, "KHICAS_V2_DONE\n");
    return 0;
}

#endif

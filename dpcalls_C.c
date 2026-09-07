/* dpcalls_C.c — реализация. См. dpcalls_C.h */

#include "dpcalls_C.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <locale.h>
#include <wchar.h>

#if defined(_WIN32) && !defined(DP_NO_WIN32_CONSOLE)
#  include <windows.h>
#endif

/* =====================================================================
 * 0. Базовые предикаты, память
 * ===================================================================== */

size_t dp_charsize(dp_chartype t)
{
    switch (t) {
        case DP_CHAR:   return sizeof(char);
        case DP_WCHAR:  return sizeof(wchar_t);
        case DP_CHAR16: return sizeof(dp_char16);
        case DP_CHAR32: return sizeof(dp_char32);
    }
    return 1;
}

int dp_is_ascii_space(dp_char32 c)
{
    return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D || c == 0x0B || c == 0x0C;
}

int dp_is_valid_codepoint(dp_char32 c)
{
    return c <= 0x10FFFFu && !(c >= 0xD800u && c <= 0xDFFFu);
}

static void dp__default_oom(size_t requested)
{
    fprintf(stderr, "dpcalls: не удалось выделить %lu байт\n",
            (unsigned long)requested);
    abort();
}

static dp_oom_handler dp__oom = dp__default_oom;

void dp_set_oom_handler(dp_oom_handler h)
{
    dp__oom = h ? h : dp__default_oom;
}

void *dp_xmalloc(size_t n)
{
    void *p;
    if (n == 0) n = 1;
    p = malloc(n);
    if (!p) { dp__oom(n); return NULL; }
    return p;
}

void *dp_xrealloc(void *p, size_t n)
{
    void *q;
    if (n == 0) n = 1;
    q = realloc(p, n);
    if (!q) { dp__oom(n); return NULL; }
    return q;
}

void dp_free(void *p)
{
    free(p);
}

/* =====================================================================
 * 1. Динамические буферы
 * ===================================================================== */

static size_t dp__grow(size_t cap, size_t need)
{
    size_t c = cap ? cap : 16;
    while (c < need) {
        if (c > ((size_t)-1) / 2) { c = need; break; }
        c *= 2;
    }
    return c;
}

/* --- dp_bytes --- */

dp_bytes dp_bytes_new(void)
{
    dp_bytes b;
    b.p = NULL; b.n = 0; b.cap = 0;
    return b;
}

void dp_bytes_reserve(dp_bytes *b, size_t cap)
{
    if (cap <= b->cap) return;
    b->p = (char*)dp_xrealloc(b->p, cap);
    b->cap = cap;
}

void dp_bytes_push(dp_bytes *b, char c)
{
    if (b->n + 1 > b->cap) dp_bytes_reserve(b, dp__grow(b->cap, b->n + 1));
    b->p[b->n++] = c;
}

void dp_bytes_append(dp_bytes *b, const char *p, size_t n)
{
    if (n == 0) return;
    if (b->n + n > b->cap) dp_bytes_reserve(b, dp__grow(b->cap, b->n + n));
    memcpy(b->p + b->n, p, n);
    b->n += n;
}

void dp_bytes_free(dp_bytes *b)
{
    if (!b) return;
    free(b->p);
    b->p = NULL; b->n = 0; b->cap = 0;
}

char *dp_bytes_release(dp_bytes *b)
{
    char *r;
    dp_bytes_reserve(b, b->n + 1);
    b->p[b->n] = '\0';
    r = b->p;
    b->p = NULL; b->n = 0; b->cap = 0;
    return r;
}

/* --- dp_u16 --- */

dp_u16 dp_u16_new(void)
{
    dp_u16 u;
    u.p = NULL; u.n = 0; u.cap = 0;
    return u;
}

void dp_u16_push(dp_u16 *u, dp_char16 c)
{
    if (u->n + 1 > u->cap) {
        u->cap = dp__grow(u->cap, u->n + 1);
        u->p = (dp_char16*)dp_xrealloc(u->p, u->cap * sizeof(dp_char16));
    }
    u->p[u->n++] = c;
}

void dp_u16_free(dp_u16 *u)
{
    if (!u) return;
    free(u->p);
    u->p = NULL; u->n = 0; u->cap = 0;
}

/* --- dp_u32 --- */

dp_u32 dp_u32_new(void)
{
    dp_u32 u;
    u.p = NULL; u.n = 0; u.cap = 0;
    return u;
}

void dp_u32_reserve(dp_u32 *u, size_t cap)
{
    if (cap <= u->cap) return;
    u->p = (dp_char32*)dp_xrealloc(u->p, cap * sizeof(dp_char32));
    u->cap = cap;
}

void dp_u32_push(dp_u32 *u, dp_char32 c)
{
    if (u->n + 1 > u->cap) dp_u32_reserve(u, dp__grow(u->cap, u->n + 1));
    u->p[u->n++] = c;
}

void dp_u32_append(dp_u32 *u, const dp_char32 *p, size_t n)
{
    if (n == 0) return;
    if (u->n + n > u->cap) dp_u32_reserve(u, dp__grow(u->cap, u->n + n));
    memcpy(u->p + u->n, p, n * sizeof(dp_char32));
    u->n += n;
}

dp_u32 dp_u32_copy(const dp_char32 *p, size_t n)
{
    dp_u32 u = dp_u32_new();
    dp_u32_append(&u, p, n);
    return u;
}

dp_u32 dp_u32_sub(const dp_u32 *u, size_t pos, size_t count)
{
    dp_u32 r = dp_u32_new();
    size_t n;
    if (!u || pos >= u->n) return r;
    n = u->n - pos;
    if (count != DP_AUTOLEN && count < n) n = count;
    dp_u32_append(&r, u->p + pos, n);
    return r;
}

void dp_u32_free(dp_u32 *u)
{
    if (!u) return;
    free(u->p);
    u->p = NULL; u->n = 0; u->cap = 0;
}

/* --- dp_slist --- */

dp_slist dp_slist_new(dp_chartype t)
{
    dp_slist l;
    l.items = NULL; l.count = 0; l.cap = 0; l.type = t;
    return l;
}

void dp_slist_push(dp_slist *l, void *owned)
{
    if (l->count + 1 > l->cap) {
        l->cap = dp__grow(l->cap, l->count + 1);
        l->items = (void**)dp_xrealloc(l->items, l->cap * sizeof(void*));
    }
    l->items[l->count++] = owned;
}

void dp_slist_push_copy(dp_slist *l, const void *s)
{
    size_t es = dp_charsize(l->type);
    size_t n  = dp_strlen_x(s, l->type);
    void  *c  = dp_xmalloc((n + 1) * es);
    memcpy(c, s, n * es);
    memset((char*)c + n * es, 0, es);
    dp_slist_push(l, c);
}

void dp_slist_free(dp_slist *l)
{
    size_t i;
    if (!l) return;
    for (i = 0; i < l->count; ++i) free(l->items[i]);
    free(l->items);
    l->items = NULL; l->count = 0; l->cap = 0;
}

void *dp_slist_at(const dp_slist *l, size_t i)
{
    if (!l || i >= l->count) return NULL;
    return l->items[i];
}

char      *dp_slist_s (const dp_slist *l, size_t i) { return (char*)      dp_slist_at(l, i); }
wchar_t   *dp_slist_w (const dp_slist *l, size_t i) { return (wchar_t*)   dp_slist_at(l, i); }
dp_char16 *dp_slist_16(const dp_slist *l, size_t i) { return (dp_char16*) dp_slist_at(l, i); }
dp_char32 *dp_slist_32(const dp_slist *l, size_t i) { return (dp_char32*) dp_slist_at(l, i); }

/* --- dp_u32list --- */

dp_u32list dp_u32list_new(void)
{
    dp_u32list l;
    l.items = NULL; l.count = 0; l.cap = 0;
    return l;
}

void dp_u32list_push(dp_u32list *l, dp_u32 u)
{
    if (l->count + 1 > l->cap) {
        l->cap = dp__grow(l->cap, l->count + 1);
        l->items = (dp_u32*)dp_xrealloc(l->items, l->cap * sizeof(dp_u32));
    }
    l->items[l->count++] = u;
}

void dp_u32list_free(dp_u32list *l)
{
    size_t i;
    if (!l) return;
    for (i = 0; i < l->count; ++i) dp_u32_free(&l->items[i]);
    free(l->items);
    l->items = NULL; l->count = 0; l->cap = 0;
}

/* =====================================================================
 * 2. UTF-8 / UTF-16 / UTF-32
 * ===================================================================== */

void dp_utf8_append(dp_char32 cp, dp_bytes *out)
{
    if (!dp_is_valid_codepoint(cp)) cp = DP_REPLACEMENT;
    if (cp < 0x80u) {
        dp_bytes_push(out, (char)cp);
    } else if (cp < 0x800u) {
        dp_bytes_push(out, (char)(0xC0u | (cp >> 6)));
        dp_bytes_push(out, (char)(0x80u | (cp & 0x3Fu)));
    } else if (cp < 0x10000u) {
        dp_bytes_push(out, (char)(0xE0u | (cp >> 12)));
        dp_bytes_push(out, (char)(0x80u | ((cp >> 6) & 0x3Fu)));
        dp_bytes_push(out, (char)(0x80u | (cp & 0x3Fu)));
    } else {
        dp_bytes_push(out, (char)(0xF0u | (cp >> 18)));
        dp_bytes_push(out, (char)(0x80u | ((cp >> 12) & 0x3Fu)));
        dp_bytes_push(out, (char)(0x80u | ((cp >> 6) & 0x3Fu)));
        dp_bytes_push(out, (char)(0x80u | (cp & 0x3Fu)));
    }
}

size_t dp_utf8_next(const char *s, size_t n, size_t i, dp_char32 *cp)
{
    unsigned char b0 = (unsigned char)s[i];
    size_t need, k;
    dp_char32 v, minv;

    if (b0 < 0x80u) { *cp = b0; return 1; }

    if      ((b0 & 0xE0u) == 0xC0u) { need = 1; v = b0 & 0x1Fu; minv = 0x80u; }
    else if ((b0 & 0xF0u) == 0xE0u) { need = 2; v = b0 & 0x0Fu; minv = 0x800u; }
    else if ((b0 & 0xF8u) == 0xF0u) { need = 3; v = b0 & 0x07u; minv = 0x10000u; }
    else                            { *cp = DP_REPLACEMENT; return 1; }

    if (i + need >= n) { *cp = DP_REPLACEMENT; return 1; }

    for (k = 1; k <= need; ++k) {
        unsigned char b = (unsigned char)s[i + k];
        if ((b & 0xC0u) != 0x80u) { *cp = DP_REPLACEMENT; return 1; }
        v = (v << 6) | (b & 0x3Fu);
    }
    /* overlong / суррогат / вне диапазона */
    if (v < minv || !dp_is_valid_codepoint(v)) { *cp = DP_REPLACEMENT; return 1; }

    *cp = v;
    return need + 1;
}

dp_u32 dp_utf8_to_u32(const char *s, size_t n)
{
    dp_u32 out = dp_u32_new();
    size_t i = 0;
    if (!s) return out;
    if (n == DP_AUTOLEN) n = strlen(s);
    dp_u32_reserve(&out, n + 1);
    while (i < n) {
        dp_char32 cp;
        i += dp_utf8_next(s, n, i, &cp);
        dp_u32_push(&out, cp);
    }
    return out;
}

dp_bytes dp_u32_to_utf8(const dp_u32 *u)
{
    dp_bytes b = dp_bytes_new();
    size_t i;
    if (!u) return b;
    dp_bytes_reserve(&b, u->n + 1);
    for (i = 0; i < u->n; ++i) dp_utf8_append(u->p[i], &b);
    return b;
}

void dp_utf16_append(dp_char32 cp, dp_u16 *out)
{
    if (!dp_is_valid_codepoint(cp)) cp = DP_REPLACEMENT;
    if (cp < 0x10000u) {
        dp_u16_push(out, (dp_char16)cp);
    } else {
        cp -= 0x10000u;
        dp_u16_push(out, (dp_char16)(0xD800u + (cp >> 10)));
        dp_u16_push(out, (dp_char16)(0xDC00u + (cp & 0x3FFu)));
    }
}

dp_u32 dp_utf16_to_u32(const dp_char16 *s, size_t n)
{
    dp_u32 out = dp_u32_new();
    size_t i;
    if (!s) return out;
    if (n == DP_AUTOLEN) { n = 0; while (s[n]) ++n; }
    dp_u32_reserve(&out, n + 1);
    for (i = 0; i < n; ++i) {
        dp_char32 u = (dp_char32)(s[i] & 0xFFFFu);
        if (u >= 0xD800u && u <= 0xDBFFu) {
            if (i + 1 < n) {
                dp_char32 lo = (dp_char32)(s[i + 1] & 0xFFFFu);
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    dp_u32_push(&out, 0x10000u + ((u - 0xD800u) << 10) + (lo - 0xDC00u));
                    ++i;
                    continue;
                }
            }
            dp_u32_push(&out, DP_REPLACEMENT);
        } else if (u >= 0xDC00u && u <= 0xDFFFu) {
            dp_u32_push(&out, DP_REPLACEMENT);   /* одиночный low surrogate */
        } else {
            dp_u32_push(&out, u);
        }
    }
    return out;
}

dp_u16 dp_u32_to_utf16(const dp_u32 *u)
{
    dp_u16 r = dp_u16_new();
    size_t i;
    if (!u) return r;
    for (i = 0; i < u->n; ++i) dp_utf16_append(u->p[i], &r);
    return r;
}

/* =====================================================================
 * 3. Мост «любой тип символа <-> dp_u32»
 * ===================================================================== */

size_t dp_strlen_x(const void *s, dp_chartype t)
{
    size_t n = 0;
    if (!s) return 0;
    switch (t) {
        case DP_CHAR:   return strlen((const char*)s);
        case DP_WCHAR:  { const wchar_t   *p = (const wchar_t*)s;   while (p[n]) ++n; return n; }
        case DP_CHAR16: { const dp_char16 *p = (const dp_char16*)s; while (p[n]) ++n; return n; }
        case DP_CHAR32: { const dp_char32 *p = (const dp_char32*)s; while (p[n]) ++n; return n; }
    }
    return 0;
}

dp_u32 dp_to32(const void *s, size_t n, dp_chartype t)
{
    dp_u32 out = dp_u32_new();
    size_t i;
    if (!s) return out;
    if (n == DP_AUTOLEN) n = dp_strlen_x(s, t);

    switch (t) {
        case DP_CHAR:
            return dp_utf8_to_u32((const char*)s, n);

        case DP_CHAR16:
            return dp_utf16_to_u32((const dp_char16*)s, n);

        case DP_CHAR32: {
            const dp_char32 *p = (const dp_char32*)s;
            dp_u32_reserve(&out, n + 1);
            for (i = 0; i < n; ++i)
                dp_u32_push(&out, dp_is_valid_codepoint(p[i]) ? p[i] : DP_REPLACEMENT);
            return out;
        }

        case DP_WCHAR: {
            const wchar_t *p = (const wchar_t*)s;
            if (sizeof(wchar_t) >= 4) {
                dp_u32_reserve(&out, n + 1);
                for (i = 0; i < n; ++i) {
                    dp_char32 c = (dp_char32)p[i];
                    dp_u32_push(&out, dp_is_valid_codepoint(c) ? c : DP_REPLACEMENT);
                }
                return out;
            } else {
                /* wchar_t = 2 байта (Windows) — это UTF-16 */
                dp_char16 *tmp = (dp_char16*)dp_xmalloc((n + 1) * sizeof(dp_char16));
                for (i = 0; i < n; ++i) tmp[i] = (dp_char16)((dp_char32)p[i] & 0xFFFFu);
                tmp[n] = 0;
                out = dp_utf16_to_u32(tmp, n);
                free(tmp);
                return out;
            }
        }
    }
    return out;
}

void *dp_from32(const dp_u32 *u, dp_chartype t, size_t *out_len)
{
    size_t i;
    if (out_len) *out_len = 0;
    if (!u) {
        void *e = dp_xmalloc(dp_charsize(t));
        memset(e, 0, dp_charsize(t));
        return e;
    }

    switch (t) {
        case DP_CHAR: {
            dp_bytes b = dp_u32_to_utf8(u);
            size_t n = b.n;
            char *r = dp_bytes_release(&b);
            if (out_len) *out_len = n;
            return r;
        }

        case DP_CHAR32: {
            dp_char32 *r = (dp_char32*)dp_xmalloc((u->n + 1) * sizeof(dp_char32));
            for (i = 0; i < u->n; ++i)
                r[i] = dp_is_valid_codepoint(u->p[i]) ? u->p[i] : DP_REPLACEMENT;
            r[u->n] = 0;
            if (out_len) *out_len = u->n;
            return r;
        }

        case DP_CHAR16: {
            dp_u16 w = dp_u32_to_utf16(u);
            dp_char16 *r = (dp_char16*)dp_xmalloc((w.n + 1) * sizeof(dp_char16));
            memcpy(r, w.p, w.n * sizeof(dp_char16));
            r[w.n] = 0;
            if (out_len) *out_len = w.n;
            dp_u16_free(&w);
            return r;
        }

        case DP_WCHAR: {
            if (sizeof(wchar_t) >= 4) {
                wchar_t *r = (wchar_t*)dp_xmalloc((u->n + 1) * sizeof(wchar_t));
                for (i = 0; i < u->n; ++i)
                    r[i] = (wchar_t)(dp_is_valid_codepoint(u->p[i]) ? u->p[i] : DP_REPLACEMENT);
                r[u->n] = 0;
                if (out_len) *out_len = u->n;
                return r;
            } else {
                dp_u16 w = dp_u32_to_utf16(u);
                wchar_t *r = (wchar_t*)dp_xmalloc((w.n + 1) * sizeof(wchar_t));
                for (i = 0; i < w.n; ++i) r[i] = (wchar_t)w.p[i];
                r[w.n] = 0;
                if (out_len) *out_len = w.n;
                dp_u16_free(&w);
                return r;
            }
        }
    }
    return NULL;
}

void *dp_convert(const void *s, dp_chartype from, dp_chartype to)
{
    dp_u32 u = dp_to32(s, DP_AUTOLEN, from);
    void *r = dp_from32(&u, to, NULL);
    dp_u32_free(&u);
    return r;
}

void *dp_lit_x(const char *ascii, dp_chartype t)
{
    dp_u32 u = dp_u32_new();
    void *r;
    const char *p = ascii;
    while (p && *p) dp_u32_push(&u, (dp_char32)(unsigned char)*p++);
    r = dp_from32(&u, t, NULL);
    dp_u32_free(&u);
    return r;
}

wchar_t   *dp_lit_w (const char *a) { return (wchar_t*)   dp_lit_x(a, DP_WCHAR);  }
dp_char16 *dp_lit_16(const char *a) { return (dp_char16*) dp_lit_x(a, DP_CHAR16); }
dp_char32 *dp_lit_32(const char *a) { return (dp_char32*) dp_lit_x(a, DP_CHAR32); }

/* =====================================================================
 * 4. Кодировки байтовых потоков
 * ===================================================================== */

const dp_char32 *dp_table_cp1251(void)
{
    static const dp_char32 t[128] = {
        0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021, /* 80 */
        0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F, /* 88 */
        0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014, /* 90 */
        0xFFFD,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F, /* 98 */
        0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7, /* A0 */
        0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407, /* A8 */
        0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7, /* B0 */
        0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457, /* B8 */
        0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417, /* C0 */
        0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F, /* C8 */
        0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427, /* D0 */
        0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F, /* D8 */
        0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437, /* E0 */
        0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F, /* E8 */
        0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447, /* F0 */
        0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F  /* F8 */
    };
    return t;
}

const dp_char32 *dp_table_cp866(void)
{
    static const dp_char32 t[128] = {
        0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417, /* 80 */
        0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F, /* 88 */
        0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427, /* 90 */
        0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F, /* 98 */
        0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437, /* A0 */
        0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F, /* A8 */
        0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556, /* B0 */
        0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510, /* B8 */
        0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F, /* C0 */
        0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567, /* C8 */
        0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B, /* D0 */
        0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580, /* D8 */
        0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447, /* E0 */
        0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F, /* E8 */
        0x0401,0x0451,0x0404,0x0454,0x0407,0x0457,0x040E,0x045E, /* F0 */
        0x00B0,0x2219,0x00B7,0x221A,0x2116,0x00A4,0x25A0,0x00A0  /* F8 */
    };
    return t;
}

const dp_char32 *dp_table_koi8r(void)
{
    static const dp_char32 t[128] = {
        0x2500,0x2502,0x250C,0x2510,0x2514,0x2518,0x251C,0x2524, /* 80 */
        0x252C,0x2534,0x253C,0x2580,0x2584,0x2588,0x258C,0x2590, /* 88 */
        0x2591,0x2592,0x2593,0x2320,0x25A0,0x2219,0x221A,0x2248, /* 90 */
        0x2264,0x2265,0x00A0,0x2321,0x00B0,0x00B2,0x00B7,0x00F7, /* 98 */
        0x2550,0x2551,0x2552,0x0451,0x2553,0x2554,0x2555,0x2556, /* A0 */
        0x2557,0x2558,0x2559,0x255A,0x255B,0x255C,0x255D,0x255E, /* A8 */
        0x255F,0x2560,0x2561,0x0401,0x2562,0x2563,0x2564,0x2565, /* B0 */
        0x2566,0x2567,0x2568,0x2569,0x256A,0x256B,0x256C,0x00A9, /* B8 */
        0x044E,0x0430,0x0431,0x0446,0x0434,0x0435,0x0444,0x0433, /* C0 */
        0x0445,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E, /* C8 */
        0x043F,0x044F,0x0440,0x0441,0x0442,0x0443,0x0436,0x0432, /* D0 */
        0x044C,0x044B,0x0437,0x0448,0x044D,0x0449,0x0447,0x044A, /* D8 */
        0x042E,0x0410,0x0411,0x0426,0x0414,0x0415,0x0424,0x0413, /* E0 */
        0x0425,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E, /* E8 */
        0x041F,0x042F,0x0420,0x0421,0x0422,0x0423,0x0416,0x0412, /* F0 */
        0x042C,0x042B,0x0417,0x0428,0x042D,0x0429,0x0427,0x042A  /* F8 */
    };
    return t;
}

const dp_char32 *dp_table_latin1(void)
{
    static dp_char32 t[128];
    static int init = 0;
    if (!init) {
        int i;
        for (i = 0; i < 128; ++i) t[i] = (dp_char32)(0x80 + i);
        init = 1;
    }
    return t;
}

static dp_charset dp__cs(dp_cs_kind k, const dp_char32 *tab, const char *name)
{
    dp_charset c;
    c.kind = k; c.table = tab; c.name = name;
    return c;
}

dp_charset dp_cs_auto   (void) { return dp__cs(DP_CS_AUTO,    NULL, "auto"); }
dp_charset dp_cs_utf8   (void) { return dp__cs(DP_CS_UTF8,    NULL, "utf-8"); }
dp_charset dp_cs_utf16le(void) { return dp__cs(DP_CS_UTF16LE, NULL, "utf-16le"); }
dp_charset dp_cs_utf16be(void) { return dp__cs(DP_CS_UTF16BE, NULL, "utf-16be"); }
dp_charset dp_cs_utf32le(void) { return dp__cs(DP_CS_UTF32LE, NULL, "utf-32le"); }
dp_charset dp_cs_utf32be(void) { return dp__cs(DP_CS_UTF32BE, NULL, "utf-32be"); }
dp_charset dp_cs_cp1251 (void) { return dp__cs(DP_CS_SBCS, dp_table_cp1251(), "cp1251"); }
dp_charset dp_cs_cp866  (void) { return dp__cs(DP_CS_SBCS, dp_table_cp866(),  "cp866"); }
dp_charset dp_cs_koi8r  (void) { return dp__cs(DP_CS_SBCS, dp_table_koi8r(),  "koi8-r"); }
dp_charset dp_cs_latin1 (void) { return dp__cs(DP_CS_SBCS, dp_table_latin1(), "latin-1"); }
dp_charset dp_cs_local  (void) { return dp__cs(DP_CS_LOCAL, NULL, "locale"); }

dp_charset dp_cs_custom(const dp_char32 *table128, const char *name)
{
    return dp__cs(DP_CS_SBCS, table128, name ? name : "custom");
}

dp_charset dp_detect_bom(const char *b, size_t n, size_t *bom_len)
{
    unsigned char c0 = n > 0 ? (unsigned char)b[0] : 0;
    unsigned char c1 = n > 1 ? (unsigned char)b[1] : 0;
    unsigned char c2 = n > 2 ? (unsigned char)b[2] : 0;
    unsigned char c3 = n > 3 ? (unsigned char)b[3] : 0;

    if (bom_len) *bom_len = 0;

    if (n >= 4 && c0 == 0xFF && c1 == 0xFE && c2 == 0x00 && c3 == 0x00) { if (bom_len) *bom_len = 4; return dp_cs_utf32le(); }
    if (n >= 4 && c0 == 0x00 && c1 == 0x00 && c2 == 0xFE && c3 == 0xFF) { if (bom_len) *bom_len = 4; return dp_cs_utf32be(); }
    if (n >= 2 && c0 == 0xFF && c1 == 0xFE)                             { if (bom_len) *bom_len = 2; return dp_cs_utf16le(); }
    if (n >= 2 && c0 == 0xFE && c1 == 0xFF)                             { if (bom_len) *bom_len = 2; return dp_cs_utf16be(); }
    if (n >= 3 && c0 == 0xEF && c1 == 0xBB && c2 == 0xBF)               { if (bom_len) *bom_len = 3; return dp_cs_utf8(); }
    return dp_cs_utf8();
}

const char *dp_bom_bytes(dp_charset cs, size_t *out_n)
{
    static const char b_u8  [3] = { (char)0xEF, (char)0xBB, (char)0xBF };
    static const char b_16le[2] = { (char)0xFF, (char)0xFE };
    static const char b_16be[2] = { (char)0xFE, (char)0xFF };
    static const char b_32le[4] = { (char)0xFF, (char)0xFE, 0, 0 };
    static const char b_32be[4] = { 0, 0, (char)0xFE, (char)0xFF };

    switch (cs.kind) {
        case DP_CS_UTF8:    if (out_n) *out_n = 3; return b_u8;
        case DP_CS_UTF16LE: if (out_n) *out_n = 2; return b_16le;
        case DP_CS_UTF16BE: if (out_n) *out_n = 2; return b_16be;
        case DP_CS_UTF32LE: if (out_n) *out_n = 4; return b_32le;
        case DP_CS_UTF32BE: if (out_n) *out_n = 4; return b_32be;
        default:            if (out_n) *out_n = 0; return NULL;
    }
}

dp_u32 dp_decode(const char *bytes, size_t n, dp_charset cs)
{
    dp_u32 out = dp_u32_new();
    size_t start = 0, i;

    if (!bytes) return out;
    if (n == DP_AUTOLEN) n = strlen(bytes);

    if (cs.kind == DP_CS_AUTO) {
        size_t bl = 0;
        cs = dp_detect_bom(bytes, n, &bl);
        start = bl;
    } else {
        /* Явно заданная UTF-кодировка: BOM, если он есть, отбрасываем. */
        size_t bl = 0;
        dp_charset guess = dp_detect_bom(bytes, n, &bl);
        if (bl > 0 && guess.kind == cs.kind) start = bl;
    }

    switch (cs.kind) {
        case DP_CS_AUTO:
        case DP_CS_UTF8: {
            i = start;
            dp_u32_reserve(&out, n - start + 1);
            while (i < n) {
                dp_char32 cp;
                i += dp_utf8_next(bytes, n, i, &cp);
                dp_u32_push(&out, cp);
            }
            break;
        }
        case DP_CS_UTF16LE:
        case DP_CS_UTF16BE: {
            int le = (cs.kind == DP_CS_UTF16LE);
            dp_u16 tmp = dp_u16_new();
            for (i = start; i + 1 < n; i += 2) {
                unsigned lo = (unsigned char)bytes[i];
                unsigned hi = (unsigned char)bytes[i + 1];
                dp_u16_push(&tmp, (dp_char16)(le ? (lo | (hi << 8)) : (hi | (lo << 8))));
            }
            out = dp_utf16_to_u32(tmp.p, tmp.n);
            dp_u16_free(&tmp);
            break;
        }
        case DP_CS_UTF32LE:
        case DP_CS_UTF32BE: {
            int le = (cs.kind == DP_CS_UTF32LE);
            for (i = start; i + 3 < n; i += 4) {
                dp_char32 b0 = (unsigned char)bytes[i];
                dp_char32 b1 = (unsigned char)bytes[i + 1];
                dp_char32 b2 = (unsigned char)bytes[i + 2];
                dp_char32 b3 = (unsigned char)bytes[i + 3];
                dp_char32 v = le ? (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24))
                                 : (b3 | (b2 << 8) | (b1 << 16) | (b0 << 24));
                dp_u32_push(&out, dp_is_valid_codepoint(v) ? v : DP_REPLACEMENT);
            }
            break;
        }
        case DP_CS_SBCS: {
            const dp_char32 *t = cs.table ? cs.table : dp_table_latin1();
            dp_u32_reserve(&out, n - start + 1);
            for (i = start; i < n; ++i) {
                unsigned char b = (unsigned char)bytes[i];
                dp_u32_push(&out, (b < 0x80) ? (dp_char32)b : t[b - 0x80]);
            }
            break;
        }
        case DP_CS_LOCAL: {
            mbstate_t st;
            const char *p = bytes + start;
            size_t left = n - start;
            memset(&st, 0, sizeof(st));
            while (left > 0) {
                wchar_t wc = 0;
                size_t k = mbrtowc(&wc, p, left, &st);
                if (k == (size_t)-1 || k == (size_t)-2) {
                    dp_u32_push(&out, DP_REPLACEMENT);
                    ++p; --left;
                    memset(&st, 0, sizeof(st));
                    continue;
                }
                if (k == 0) { dp_u32_push(&out, 0); k = 1; }
                else        { dp_u32_push(&out, (dp_char32)wc); }
                p += k; left -= k;
            }
            /* На Windows wchar_t = UTF-16, суррогаты нужно склеить. */
            if (sizeof(wchar_t) < 4) {
                dp_u16 tmp = dp_u16_new();
                dp_u32 fixed;
                for (i = 0; i < out.n; ++i) dp_u16_push(&tmp, (dp_char16)(out.p[i] & 0xFFFFu));
                fixed = dp_utf16_to_u32(tmp.p, tmp.n);
                dp_u16_free(&tmp);
                dp_u32_free(&out);
                out = fixed;
            }
            break;
        }
    }
    return out;
}

dp_bytes dp_encode(const dp_u32 *text, dp_charset cs, int with_bom)
{
    dp_bytes out = dp_bytes_new();
    size_t i;

    if (cs.kind == DP_CS_AUTO) cs = dp_cs_utf8();

    if (with_bom) {
        size_t bn = 0;
        const char *bp = dp_bom_bytes(cs, &bn);
        if (bp && bn) dp_bytes_append(&out, bp, bn);
    }
    if (!text) return out;

    switch (cs.kind) {
        case DP_CS_AUTO:
        case DP_CS_UTF8:
            dp_bytes_reserve(&out, out.n + text->n + 1);
            for (i = 0; i < text->n; ++i) dp_utf8_append(text->p[i], &out);
            break;

        case DP_CS_UTF16LE:
        case DP_CS_UTF16BE: {
            int le = (cs.kind == DP_CS_UTF16LE);
            dp_u16 tmp = dp_u32_to_utf16(text);
            for (i = 0; i < tmp.n; ++i) {
                unsigned v = (unsigned)(tmp.p[i] & 0xFFFFu);
                if (le) { dp_bytes_push(&out, (char)(v & 0xFF)); dp_bytes_push(&out, (char)((v >> 8) & 0xFF)); }
                else    { dp_bytes_push(&out, (char)((v >> 8) & 0xFF)); dp_bytes_push(&out, (char)(v & 0xFF)); }
            }
            dp_u16_free(&tmp);
            break;
        }
        case DP_CS_UTF32LE:
        case DP_CS_UTF32BE: {
            int le = (cs.kind == DP_CS_UTF32LE);
            for (i = 0; i < text->n; ++i) {
                dp_char32 v = dp_is_valid_codepoint(text->p[i]) ? text->p[i] : DP_REPLACEMENT;
                char b[4];
                b[0] = (char)(v & 0xFF);
                b[1] = (char)((v >> 8) & 0xFF);
                b[2] = (char)((v >> 16) & 0xFF);
                b[3] = (char)((v >> 24) & 0xFF);
                if (le) { dp_bytes_push(&out, b[0]); dp_bytes_push(&out, b[1]); dp_bytes_push(&out, b[2]); dp_bytes_push(&out, b[3]); }
                else    { dp_bytes_push(&out, b[3]); dp_bytes_push(&out, b[2]); dp_bytes_push(&out, b[1]); dp_bytes_push(&out, b[0]); }
            }
            break;
        }
        case DP_CS_SBCS: {
            const dp_char32 *t = cs.table ? cs.table : dp_table_latin1();
            for (i = 0; i < text->n; ++i) {
                dp_char32 c = text->p[i];
                char mapped = DP_UNMAPPABLE;
                int k;
                if (c < 0x80u) { dp_bytes_push(&out, (char)c); continue; }
                for (k = 0; k < 128; ++k) {
                    if (t[k] == c) { mapped = (char)(0x80 + k); break; }
                }
                dp_bytes_push(&out, mapped);
            }
            break;
        }
        case DP_CS_LOCAL: {
            mbstate_t st;
            char buf[MB_LEN_MAX + 8];
            const dp_char32 *src = text->p;
            size_t srcn = text->n;
            dp_u16 tmp = dp_u16_new();
            dp_u32 wide = dp_u32_new();

            memset(&st, 0, sizeof(st));
            /* На Windows wchar_t = UTF-16: разбиваем на суррогатные пары. */
            if (sizeof(wchar_t) < 4) {
                tmp = dp_u32_to_utf16(text);
                for (i = 0; i < tmp.n; ++i) dp_u32_push(&wide, (dp_char32)tmp.p[i]);
                src = wide.p; srcn = wide.n;
            }
            for (i = 0; i < srcn; ++i) {
                size_t k = wcrtomb(buf, (wchar_t)src[i], &st);
                if (k == (size_t)-1) {
                    dp_bytes_push(&out, DP_UNMAPPABLE);
                    memset(&st, 0, sizeof(st));
                } else {
                    dp_bytes_append(&out, buf, k);
                }
            }
            dp_u16_free(&tmp);
            dp_u32_free(&wide);
            break;
        }
    }
    return out;
}

dp_bytes dp_recode(const char *bytes, size_t n,
                   dp_charset from, dp_charset to, int with_bom)
{
    dp_u32 u = dp_decode(bytes, n, from);
    dp_bytes b = dp_encode(&u, to, with_bom);
    dp_u32_free(&u);
    return b;
}

/* =====================================================================
 * 5. Операции над строками
 * ===================================================================== */

size_t dp_length_x(const void *s, dp_chartype t)
{
    dp_u32 u = dp_to32(s, DP_AUTOLEN, t);
    size_t n = u.n;
    dp_u32_free(&u);
    return n;
}

void *dp_reverse_x(const void *s, dp_chartype t)
{
    dp_u32 u = dp_to32(s, DP_AUTOLEN, t);
    dp_u32 r = dp_u32_new();
    void *out;
    size_t i;
    dp_u32_reserve(&r, u.n + 1);
    for (i = u.n; i > 0; --i) dp_u32_push(&r, u.p[i - 1]);
    out = dp_from32(&r, t, NULL);
    dp_u32_free(&u);
    dp_u32_free(&r);
    return out;
}

void *dp_substr_cp_x(const void *s, dp_chartype t, size_t pos, size_t count)
{
    dp_u32 u = dp_to32(s, DP_AUTOLEN, t);
    dp_u32 r = dp_u32_sub(&u, pos, count);
    void *out = dp_from32(&r, t, NULL);
    dp_u32_free(&u);
    dp_u32_free(&r);
    return out;
}

dp_char32 dp_to_upper_cp(dp_char32 c)
{
    if (c >= 0x61 && c <= 0x7A) return c - 0x20;              /* a-z          */
    if (c >= 0x430 && c <= 0x44F) return c - 0x20;            /* а-я          */
    if (c >= 0x450 && c <= 0x45F) return c - 0x50;            /* ё, ђ, ...    */
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 0x20; /* latin-1 supp */
    return c;
}

dp_char32 dp_to_lower_cp(dp_char32 c)
{
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;
    if (c >= 0x410 && c <= 0x42F) return c + 0x20;
    if (c >= 0x400 && c <= 0x40F) return c + 0x50;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 0x20;
    return c;
}

static void *dp__map_case(const void *s, dp_chartype t, int up)
{
    dp_u32 u = dp_to32(s, DP_AUTOLEN, t);
    void *out;
    size_t i;
    for (i = 0; i < u.n; ++i)
        u.p[i] = up ? dp_to_upper_cp(u.p[i]) : dp_to_lower_cp(u.p[i]);
    out = dp_from32(&u, t, NULL);
    dp_u32_free(&u);
    return out;
}

void *dp_upper_x(const void *s, dp_chartype t) { return dp__map_case(s, t, 1); }
void *dp_lower_x(const void *s, dp_chartype t) { return dp__map_case(s, t, 0); }

void *dp_trim_x(const void *s, dp_chartype t)
{
    dp_u32 u = dp_to32(s, DP_AUTOLEN, t);
    dp_u32 r;
    void *out;
    size_t a = 0, b = u.n;
    while (a < b && dp_is_ascii_space(u.p[a])) ++a;
    while (b > a && dp_is_ascii_space(u.p[b - 1])) --b;
    r = dp_u32_sub(&u, a, b - a);
    out = dp_from32(&r, t, NULL);
    dp_u32_free(&u);
    dp_u32_free(&r);
    return out;
}

/* Общее ядро для words/split: разделитель задаётся предикатом. */
static dp_slist dp__split_core(const void *line, dp_chartype t,
                               int by_space, dp_char32 delim, int keep_empty)
{
    dp_slist  res = dp_slist_new(t);
    dp_u32    u   = dp_to32(line, DP_AUTOLEN, t);
    dp_u32    cur = dp_u32_new();
    size_t    i;

    for (i = 0; i < u.n; ++i) {
        int is_sep = by_space ? dp_is_ascii_space(u.p[i]) : (u.p[i] == delim);
        if (is_sep) {
            if (cur.n > 0 || keep_empty) dp_slist_push(&res, dp_from32(&cur, t, NULL));
            cur.n = 0;
        } else {
            dp_u32_push(&cur, u.p[i]);
        }
    }
    if (cur.n > 0 || keep_empty) dp_slist_push(&res, dp_from32(&cur, t, NULL));

    dp_u32_free(&u);
    dp_u32_free(&cur);
    return res;
}

dp_slist dp_words_x(const void *line, dp_chartype t, int keep_empty)
{
    return dp__split_core(line, t, 1, 0, keep_empty);
}

dp_slist dp_split_x(const void *line, dp_chartype t, dp_char32 delim, int keep_empty)
{
    return dp__split_core(line, t, 0, delim, keep_empty);
}

void *dp_join_x(const dp_slist *parts, const void *sep)
{
    dp_u32 acc = dp_u32_new();
    dp_u32 sp;
    void  *out;
    size_t i;

    if (!parts) return dp_from32(&acc, DP_CHAR, NULL);

    sp = dp_to32(sep, DP_AUTOLEN, parts->type);
    for (i = 0; i < parts->count; ++i) {
        dp_u32 part = dp_to32(parts->items[i], DP_AUTOLEN, parts->type);
        if (i) dp_u32_append(&acc, sp.p, sp.n);
        dp_u32_append(&acc, part.p, part.n);
        dp_u32_free(&part);
    }
    out = dp_from32(&acc, parts->type, NULL);
    dp_u32_free(&acc);
    dp_u32_free(&sp);
    return out;
}

void *dp_to_str_x(long long value, int base, dp_chartype t)
{
    char buf[80];
    int n = 0, i;
    int neg;
    unsigned long long v;
    dp_u32 u = dp_u32_new();
    void *out;

    if (base < 2)  base = 2;
    if (base > 36) base = 36;

    neg = (value < 0);
    v = neg ? (0ULL - (unsigned long long)value) : (unsigned long long)value;

    do {
        int d = (int)(v % (unsigned long long)base);
        buf[n++] = (char)(d < 10 ? ('0' + d) : ('a' + d - 10));
        v /= (unsigned long long)base;
    } while (v != 0);
    if (neg) buf[n++] = '-';

    dp_u32_reserve(&u, (size_t)n + 1);
    for (i = n - 1; i >= 0; --i) dp_u32_push(&u, (dp_char32)(unsigned char)buf[i]);

    out = dp_from32(&u, t, NULL);
    dp_u32_free(&u);
    return out;
}

int dp_stoll_safe_x(const void *s, dp_chartype t, long long *out, int base)
{
    dp_u32 u = dp_to32(s, DP_AUTOLEN, t);
    size_t i = 0;
    int neg = 0, any = 0, ok = 0;
    long long acc = 0;

    if (base < 2)  base = 2;
    if (base > 36) base = 36;

    while (i < u.n && dp_is_ascii_space(u.p[i])) ++i;
    if (i < u.n && (u.p[i] == '+' || u.p[i] == '-')) { neg = (u.p[i] == '-'); ++i; }

    for (; i < u.n; ++i) {
        dp_char32 c = u.p[i];
        int d;
        if      (c >= '0' && c <= '9') d = (int)(c - '0');
        else if (c >= 'a' && c <= 'z') d = (int)(c - 'a') + 10;
        else if (c >= 'A' && c <= 'Z') d = (int)(c - 'A') + 10;
        else break;
        if (d >= base) break;
        acc = acc * base + d;
        any = 1;
    }
    while (i < u.n && dp_is_ascii_space(u.p[i])) ++i;

    if (any && i == u.n) {
        if (out) *out = neg ? -acc : acc;
        ok = 1;
    }
    dp_u32_free(&u);
    return ok;
}

/* =====================================================================
 * 6. Файлы
 * ===================================================================== */

dp_bytes dp_read_all_bytes(const char *path, int *ok)
{
    dp_bytes b = dp_bytes_new();
    FILE *f;
    char chunk[8192];
    size_t got;

    if (ok) *ok = 0;
    f = fopen(path, "rb");
    if (!f) return b;

    while ((got = fread(chunk, 1, sizeof(chunk), f)) > 0)
        dp_bytes_append(&b, chunk, got);

    if (ok) *ok = !ferror(f);
    fclose(f);
    return b;
}

int dp_write_all_bytes(const char *path, const char *bytes, size_t n, int append)
{
    FILE *f = fopen(path, append ? "ab" : "wb");
    size_t put;
    if (!f) return 0;
    put = (n > 0) ? fwrite(bytes, 1, n, f) : 0;
    if (fclose(f) != 0) return 0;
    return put == n;
}

dp_u32list dp_split_lines_u32(const dp_u32 *text)
{
    dp_u32list lines = dp_u32list_new();
    dp_u32 cur = dp_u32_new();
    size_t i;

    if (!text) return lines;

    for (i = 0; i < text->n; ++i) {
        dp_char32 c = text->p[i];
        if (c == 0x0D) {                                   /* \r или \r\n */
            if (i + 1 < text->n && text->p[i + 1] == 0x0A) ++i;
            dp_u32list_push(&lines, cur);
            cur = dp_u32_new();
        } else if (c == 0x0A) {                            /* \n */
            dp_u32list_push(&lines, cur);
            cur = dp_u32_new();
        } else {
            dp_u32_push(&cur, c);
        }
    }
    if (cur.n > 0) dp_u32list_push(&lines, cur);           /* хвост без \n */
    else           dp_u32_free(&cur);
    return lines;
}

dp_slist dp_read_lines_x(const char *path, dp_charset cs, dp_chartype t, int *ok)
{
    dp_slist   res;
    dp_bytes   raw = dp_read_all_bytes(path, ok);
    dp_u32     text = dp_decode(raw.p, raw.n, cs);
    dp_u32list lines = dp_split_lines_u32(&text);
    size_t i;

    res = dp_slist_new(t);
    for (i = 0; i < lines.count; ++i)
        dp_slist_push(&res, dp_from32(&lines.items[i], t, NULL));

    dp_u32list_free(&lines);
    dp_u32_free(&text);
    dp_bytes_free(&raw);
    return res;
}

int dp_write_lines_x(const char *path, const dp_slist *lines,
                     dp_charset cs, int with_bom, const char *eol)
{
    dp_u32   text = dp_u32_new();
    dp_bytes raw;
    size_t   i;
    int      r;

    if (!eol) eol = "\n";
    if (lines) {
        for (i = 0; i < lines->count; ++i) {
            dp_u32 one = dp_to32(lines->items[i], DP_AUTOLEN, lines->type);
            const char *p;
            dp_u32_append(&text, one.p, one.n);
            for (p = eol; *p; ++p) dp_u32_push(&text, (dp_char32)(unsigned char)*p);
            dp_u32_free(&one);
        }
    }
    raw = dp_encode(&text, cs, with_bom);
    r = dp_write_all_bytes(path, raw.p, raw.n, 0);
    dp_bytes_free(&raw);
    dp_u32_free(&text);
    return r;
}

void *dp_read_all_text_x(const char *path, dp_charset cs, dp_chartype t, int *ok)
{
    dp_bytes raw = dp_read_all_bytes(path, ok);
    dp_u32   u   = dp_decode(raw.p, raw.n, cs);
    void    *out = dp_from32(&u, t, NULL);
    dp_u32_free(&u);
    dp_bytes_free(&raw);
    return out;
}

int dp_write_all_text_x(const char *path, const void *text, dp_chartype t,
                        dp_charset cs, int with_bom)
{
    dp_u32   u   = dp_to32(text, DP_AUTOLEN, t);
    dp_bytes raw = dp_encode(&u, cs, with_bom);
    int      r   = dp_write_all_bytes(path, raw.p, raw.n, 0);
    dp_bytes_free(&raw);
    dp_u32_free(&u);
    return r;
}

/* =====================================================================
 * 7. Вывод в консоль
 * ===================================================================== */

void dp_init_console(void)
{
    setlocale(LC_ALL, "");
#if defined(_WIN32) && !defined(DP_NO_WIN32_CONSOLE)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void dp_print_x(const dp_slist *lines, dp_charset cs)
{
    dp_u32 text = dp_u32_new();
    dp_bytes raw;
    size_t i;

    if (lines) {
        for (i = 0; i < lines->count; ++i) {
            dp_u32 one = dp_to32(lines->items[i], DP_AUTOLEN, lines->type);
            dp_u32_append(&text, one.p, one.n);
            dp_u32_push(&text, 0x0A);
            dp_u32_free(&one);
        }
    }
    raw = dp_encode(&text, cs, 0);
    if (raw.n) fwrite(raw.p, 1, raw.n, stdout);
    fflush(stdout);
    dp_bytes_free(&raw);
    dp_u32_free(&text);
}

void dp_print_str_x(const void *s, dp_chartype t, dp_charset cs)
{
    dp_u32   u   = dp_to32(s, DP_AUTOLEN, t);
    dp_bytes raw;
    dp_u32_push(&u, 0x0A);
    raw = dp_encode(&u, cs, 0);
    if (raw.n) fwrite(raw.p, 1, raw.n, stdout);
    fflush(stdout);
    dp_bytes_free(&raw);
    dp_u32_free(&u);
}

void *dp_input_line_x(dp_chartype t, dp_charset cs)
{
    dp_bytes line = dp_bytes_new();
    dp_u32   u;
    void    *out;
    int      c;
    int      got = 0;

    while ((c = fgetc(stdin)) != EOF) {
        got = 1;
        if (c == '\n') break;
        dp_bytes_push(&line, (char)c);
    }
    if (!got) { dp_bytes_free(&line); return NULL; }

    if (line.n > 0 && line.p[line.n - 1] == '\r') --line.n;

    u   = dp_decode(line.p, line.n, cs);
    out = dp_from32(&u, t, NULL);
    dp_u32_free(&u);
    dp_bytes_free(&line);
    return out;
}

/* =====================================================================
 * 8. Случайные числа — xorshift64*, без модульного смещения
 * ===================================================================== */

static uint64_t dp__state = 0;

static void dp__seed_once(void)
{
    if (dp__state == 0) {
        uintptr_t addr = (uintptr_t)(void*)&dp__state;
        dp__state = (uint64_t)time(NULL) * 0x9E3779B97F4A7C15ULL
                  ^ (uint64_t)addr
                  ^ 0xD1B54A32D192ED03ULL;
        if (dp__state == 0) dp__state = 0x853C49E6748FEA9BULL;
    }
}

void dp_rnd_seed(uint64_t s)
{
    dp__state = s ? s : 0x853C49E6748FEA9BULL;
}

static uint64_t dp__next(void)
{
    uint64_t x;
    dp__seed_once();
    x = dp__state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    dp__state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

/* Равномерное [0, bound) без модульного смещения (rejection sampling). */
static uint64_t dp__below(uint64_t bound)
{
    uint64_t limit, r;
    if (bound == 0) return 0;
    limit = (uint64_t)-1 - ((uint64_t)-1 % bound);
    do { r = dp__next(); } while (r >= limit);
    return r % bound;
}

int dp_rnd(int max)
{
    if (max <= 0) return 0;
    return (int)dp__below((uint64_t)max);
}

int dp_rnd_range(int min, int max)
{
    if (min > max) { int t = min; min = max; max = t; }
    return min + (int)dp__below((uint64_t)((long long)max - (long long)min + 1));
}

double dp_rndf(void)
{
    /* 53 значащих бита — полная мантисса double */
    return (double)(dp__next() >> 11) * (1.0 / 9007199254740992.0);
}

/* =====================================================================
 * 9. JSON — числа, независимые от локали
 * ===================================================================== */

char dp_json_locale_point(void)
{
    struct lconv *lc = localeconv();
    return (lc && lc->decimal_point && lc->decimal_point[0]) ? lc->decimal_point[0] : '.';
}

double dp_json_strtod(const char *ascii)
{
    char pt = dp_json_locale_point();
    char buf[512];
    size_t n, k;

    if (!ascii) return 0.0;
    if (pt == '.') return strtod(ascii, NULL);

    n = strlen(ascii);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (k = 0; k < n; ++k) buf[k] = (ascii[k] == '.') ? pt : ascii[k];
    buf[n] = '\0';
    return strtod(buf, NULL);
}

void dp_json_dtoa(double v, char *buf, size_t n)
{
    char pt = dp_json_locale_point();
    int prec;

    if (n == 0) return;
    /* NaN и бесконечности в JSON не представимы */
    if (v != v || v > 1.7976931348623157e308 || v < -1.7976931348623157e308) {
        snprintf(buf, n, "null");
        return;
    }
    for (prec = 15; prec <= 17; ++prec) {
        char *p;
        snprintf(buf, n, "%.*g", prec, v);
        if (pt != '.')
            for (p = buf; *p; ++p) if (*p == pt) *p = '.';
        if (dp_json_strtod(buf) == v) return;
    }
}

/* =====================================================================
 * 10. JSON — создание и освобождение
 * ===================================================================== */

static dp_json *dp__json_alloc(dp_json_type t)
{
    dp_json *v = (dp_json*)dp_xmalloc(sizeof(dp_json));
    v->type = t;
    v->b = 0;
    v->d = 0.0;
    v->i = 0;
    v->is_int = 0;
    v->s = dp_u32_new();
    v->items = NULL;
    v->keys = NULL;
    v->count = 0;
    v->cap = 0;
    return v;
}

dp_json *dp_json_new_null(void)   { return dp__json_alloc(DP_JSON_NULL); }
dp_json *dp_json_new_array(void)  { return dp__json_alloc(DP_JSON_ARRAY); }
dp_json *dp_json_new_object(void) { return dp__json_alloc(DP_JSON_OBJECT); }

dp_json *dp_json_new_bool(int x)
{
    dp_json *v = dp__json_alloc(DP_JSON_BOOL);
    v->b = x ? 1 : 0;
    return v;
}

dp_json *dp_json_new_int(long long x)
{
    dp_json *v = dp__json_alloc(DP_JSON_NUMBER);
    v->i = x;
    v->d = (double)x;
    v->is_int = 1;
    return v;
}

dp_json *dp_json_new_double(double x)
{
    dp_json *v = dp__json_alloc(DP_JSON_NUMBER);
    v->d = x;
    v->i = (long long)x;
    v->is_int = 0;
    return v;
}

dp_json *dp_json_new_str_x(const void *s, dp_chartype t)
{
    dp_json *v = dp__json_alloc(DP_JSON_STRING);
    v->s = dp_to32(s, DP_AUTOLEN, t);
    return v;
}

dp_json *dp_json_new_str(const char *utf8)
{
    return dp_json_new_str_x(utf8 ? utf8 : "", DP_CHAR);
}

void dp_json_free(dp_json *v)
{
    size_t k;
    if (!v) return;
    for (k = 0; k < v->count; ++k) {
        dp_json_free(v->items[k]);
        if (v->keys) dp_u32_free(&v->keys[k]);
    }
    free(v->items);
    free(v->keys);
    dp_u32_free(&v->s);
    free(v);
}

dp_json *dp_json_clone(const dp_json *v)
{
    dp_json *r;
    size_t k;
    if (!v) return dp_json_new_null();

    r = dp__json_alloc(v->type);
    r->b = v->b;
    r->d = v->d;
    r->i = v->i;
    r->is_int = v->is_int;
    dp_u32_append(&r->s, v->s.p, v->s.n);

    if (v->count) {
        r->items = (dp_json**)dp_xmalloc(v->count * sizeof(dp_json*));
        if (v->keys) r->keys = (dp_u32*)dp_xmalloc(v->count * sizeof(dp_u32));
        r->cap = v->count;
        for (k = 0; k < v->count; ++k) {
            r->items[k] = dp_json_clone(v->items[k]);
            if (v->keys) r->keys[k] = dp_u32_copy(v->keys[k].p, v->keys[k].n);
        }
        r->count = v->count;
    }
    return r;
}

/* =====================================================================
 * 11. JSON — опрос и чтение
 * ===================================================================== */

dp_json_type dp_json_typeof(const dp_json *v) { return v ? v->type : DP_JSON_NULL; }

int dp_json_is_null  (const dp_json *v) { return !v || v->type == DP_JSON_NULL; }
int dp_json_is_bool  (const dp_json *v) { return v && v->type == DP_JSON_BOOL; }
int dp_json_is_number(const dp_json *v) { return v && v->type == DP_JSON_NUMBER; }
int dp_json_is_int   (const dp_json *v) { return v && v->type == DP_JSON_NUMBER && v->is_int; }
int dp_json_is_string(const dp_json *v) { return v && v->type == DP_JSON_STRING; }
int dp_json_is_array (const dp_json *v) { return v && v->type == DP_JSON_ARRAY; }
int dp_json_is_object(const dp_json *v) { return v && v->type == DP_JSON_OBJECT; }

int dp_json_as_bool(const dp_json *v, int def)
{
    if (!v) return def;
    if (v->type == DP_JSON_BOOL) return v->b;
    if (v->type == DP_JSON_NUMBER) return v->d != 0.0;
    return def;
}

double dp_json_as_double(const dp_json *v, double def)
{
    if (!v) return def;
    if (v->type == DP_JSON_NUMBER) return v->is_int ? (double)v->i : v->d;
    if (v->type == DP_JSON_BOOL) return v->b ? 1.0 : 0.0;
    return def;
}

long long dp_json_as_int(const dp_json *v, long long def)
{
    if (!v) return def;
    if (v->type == DP_JSON_NUMBER) return v->is_int ? v->i : (long long)v->d;
    if (v->type == DP_JSON_BOOL) return v->b ? 1 : 0;
    return def;
}

void *dp_json_as_str_x(const dp_json *v, dp_chartype t)
{
    dp_u32 empty;
    if (v && v->type == DP_JSON_STRING) return dp_from32(&v->s, t, NULL);
    empty = dp_u32_new();
    return dp_from32(&empty, t, NULL);
}

char    *dp_json_as_utf8(const dp_json *v) { return (char*)   dp_json_as_str_x(v, DP_CHAR);  }
wchar_t *dp_json_as_wstr(const dp_json *v) { return (wchar_t*)dp_json_as_str_x(v, DP_WCHAR); }

const dp_u32 *dp_json_raw_str(const dp_json *v)
{
    return v ? &v->s : NULL;
}

size_t dp_json_size(const dp_json *v)
{
    if (!v) return 0;
    if (v->type == DP_JSON_ARRAY || v->type == DP_JSON_OBJECT) return v->count;
    return 0;
}

dp_json *dp_json_at(const dp_json *v, size_t idx)
{
    if (!v || idx >= v->count) return NULL;
    return v->items[idx];
}

static size_t dp__json_find(const dp_json *v, const dp_u32 *key)
{
    size_t k;
    if (!v || v->type != DP_JSON_OBJECT || !v->keys) return (size_t)-1;
    for (k = v->count; k > 0; --k) {          /* последний ключ побеждает */
        const dp_u32 *e = &v->keys[k - 1];
        if (e->n == key->n &&
            (e->n == 0 || memcmp(e->p, key->p, e->n * sizeof(dp_char32)) == 0))
            return k - 1;
    }
    return (size_t)-1;
}

dp_json *dp_json_get_x(const dp_json *v, const void *key, dp_chartype t)
{
    dp_u32 k = dp_to32(key, DP_AUTOLEN, t);
    size_t idx = dp__json_find(v, &k);
    dp_u32_free(&k);
    return (idx == (size_t)-1) ? NULL : v->items[idx];
}

dp_json *dp_json_get(const dp_json *v, const char *key_utf8)
{
    return dp_json_get_x(v, key_utf8 ? key_utf8 : "", DP_CHAR);
}

int dp_json_has(const dp_json *v, const char *key_utf8)
{
    return dp_json_get(v, key_utf8) != NULL;
}

void *dp_json_key_at_x(const dp_json *v, size_t idx, dp_chartype t)
{
    dp_u32 empty;
    if (v && v->type == DP_JSON_OBJECT && v->keys && idx < v->count)
        return dp_from32(&v->keys[idx], t, NULL);
    empty = dp_u32_new();
    return dp_from32(&empty, t, NULL);
}

char *dp_json_key_at(const dp_json *v, size_t idx)
{
    return (char*)dp_json_key_at_x(v, idx, DP_CHAR);
}

/* =====================================================================
 * 12. JSON — изменение
 * ===================================================================== */

static void dp__json_reserve(dp_json *v, size_t need, int with_keys)
{
    if (need <= v->cap) return;
    v->cap = dp__grow(v->cap, need);
    v->items = (dp_json**)dp_xrealloc(v->items, v->cap * sizeof(dp_json*));
    if (with_keys) v->keys = (dp_u32*)dp_xrealloc(v->keys, v->cap * sizeof(dp_u32));
}

void dp_json_push(dp_json *arr, dp_json *val)
{
    if (!arr) { dp_json_free(val); return; }
    if (arr->type != DP_JSON_ARRAY) {
        /* превращаем в массив, теряя прежнее скалярное содержимое */
        dp_u32_free(&arr->s);
        arr->type = DP_JSON_ARRAY;
    }
    dp__json_reserve(arr, arr->count + 1, 0);
    arr->items[arr->count++] = val ? val : dp_json_new_null();
}

dp_json *dp_json_emplace_back(dp_json *arr)
{
    dp_json *slot = dp_json_new_null();
    dp_json_push(arr, slot);
    return slot;
}

void dp_json_set_x(dp_json *obj, const void *key, dp_chartype t, dp_json *val)
{
    dp_u32 k;
    size_t idx;

    if (!obj) { dp_json_free(val); return; }
    if (obj->type != DP_JSON_OBJECT) {
        dp_u32_free(&obj->s);
        obj->type = DP_JSON_OBJECT;
    }
    k = dp_to32(key, DP_AUTOLEN, t);
    idx = dp__json_find(obj, &k);
    if (idx != (size_t)-1) {
        dp_json_free(obj->items[idx]);
        obj->items[idx] = val ? val : dp_json_new_null();
        dp_u32_free(&k);
        return;
    }
    dp__json_reserve(obj, obj->count + 1, 1);
    obj->keys[obj->count] = k;                     /* владение переходит объекту */
    obj->items[obj->count] = val ? val : dp_json_new_null();
    ++obj->count;
}

void dp_json_set(dp_json *obj, const char *key_utf8, dp_json *val)
{
    dp_json_set_x(obj, key_utf8 ? key_utf8 : "", DP_CHAR, val);
}

/* Ключ задан явной длиной: буфер dp_u32 нулём не завершён, поэтому
 * сканировать его до терминатора нельзя. Забирает владение ключом. */
static dp_json *dp__json_emplace_key(dp_json *obj, dp_u32 k)
{
    size_t idx;
    dp_json *slot;

    if (!obj) { dp_u32_free(&k); return NULL; }
    if (obj->type != DP_JSON_OBJECT) {
        dp_u32_free(&obj->s);
        obj->type = DP_JSON_OBJECT;
    }
    idx = dp__json_find(obj, &k);
    if (idx != (size_t)-1) {
        dp_u32_free(&k);
        return obj->items[idx];
    }
    slot = dp_json_new_null();
    dp__json_reserve(obj, obj->count + 1, 1);
    obj->keys[obj->count] = k;
    obj->items[obj->count] = slot;
    ++obj->count;
    return slot;
}

dp_json *dp_json_emplace_x(dp_json *obj, const void *key, dp_chartype t)
{
    dp_u32 k;
    size_t idx;
    dp_json *slot;

    if (!obj) return NULL;
    if (obj->type != DP_JSON_OBJECT) {
        dp_u32_free(&obj->s);
        obj->type = DP_JSON_OBJECT;
    }
    k = dp_to32(key, DP_AUTOLEN, t);
    idx = dp__json_find(obj, &k);
    if (idx != (size_t)-1) {
        dp_u32_free(&k);
        return obj->items[idx];
    }
    slot = dp_json_new_null();
    dp__json_reserve(obj, obj->count + 1, 1);
    obj->keys[obj->count] = k;
    obj->items[obj->count] = slot;
    ++obj->count;
    return slot;
}

int dp_json_remove(dp_json *obj, const char *key_utf8)
{
    dp_u32 k;
    size_t idx, j;

    if (!obj || obj->type != DP_JSON_OBJECT) return 0;
    k = dp_to32(key_utf8 ? key_utf8 : "", DP_AUTOLEN, DP_CHAR);
    idx = dp__json_find(obj, &k);
    dp_u32_free(&k);
    if (idx == (size_t)-1) return 0;

    dp_json_free(obj->items[idx]);
    dp_u32_free(&obj->keys[idx]);
    for (j = idx + 1; j < obj->count; ++j) {
        obj->items[j - 1] = obj->items[j];
        obj->keys[j - 1] = obj->keys[j];
    }
    --obj->count;
    return 1;
}

/* =====================================================================
 * 13. JSON — парсер
 * ===================================================================== */

dp_json_options dp_json_options_default(void)
{
    dp_json_options o;
    o.allow_comments = 0;
    o.allow_trailing_commas = 0;
    o.max_depth = 200;
    return o;
}

dp_json_options dp_json_options_relaxed(void)
{
    dp_json_options o = dp_json_options_default();
    o.allow_comments = 1;
    o.allow_trailing_commas = 1;
    return o;
}

void dp_json_error_str(const dp_json_error *e, char *buf, size_t n)
{
    if (!buf || n == 0) return;
    if (!e || e->ok) { buf[0] = '\0'; return; }
    snprintf(buf, n, "строка %lu, столбец %lu: %s",
             (unsigned long)e->line, (unsigned long)e->column, e->message);
}

typedef struct {
    const dp_u32          *s;
    size_t                 i, line, col;
    const dp_json_options *opt;
    dp_json_error         *err;
} dp__jp;

static int  dp__jp_eof(const dp__jp *p) { return p->i >= p->s->n; }
static dp_char32 dp__jp_cur(const dp__jp *p) { return p->i < p->s->n ? p->s->p[p->i] : 0; }
static dp_char32 dp__jp_peek(const dp__jp *p, size_t k)
{
    return (p->i + k) < p->s->n ? p->s->p[p->i + k] : 0;
}

static void dp__jp_adv(dp__jp *p)
{
    if (p->i >= p->s->n) return;
    if (p->s->p[p->i] == 0x0A) { ++p->line; p->col = 1; }
    else ++p->col;
    ++p->i;
}

static void dp__jp_fail(dp__jp *p, const char *msg)
{
    if (!p->err->ok) return;
    p->err->ok = 0;
    snprintf(p->err->message, sizeof(p->err->message), "%s", msg);
    p->err->line = p->line;
    p->err->column = p->col;
    p->err->offset = p->i;
}

static void dp__jp_ws(dp__jp *p)
{
    for (;;) {
        while (!dp__jp_eof(p)) {
            dp_char32 c = dp__jp_cur(p);
            if (c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D) dp__jp_adv(p);
            else break;
        }
        if (!p->opt->allow_comments || dp__jp_eof(p) || dp__jp_cur(p) != '/') return;

        if (dp__jp_peek(p, 1) == '/') {
            while (!dp__jp_eof(p) && dp__jp_cur(p) != 0x0A) dp__jp_adv(p);
        } else if (dp__jp_peek(p, 1) == '*') {
            dp__jp_adv(p); dp__jp_adv(p);
            for (;;) {
                if (dp__jp_eof(p)) { dp__jp_fail(p, "незакрытый комментарий"); return; }
                if (dp__jp_cur(p) == '*' && dp__jp_peek(p, 1) == '/') {
                    dp__jp_adv(p); dp__jp_adv(p);
                    break;
                }
                dp__jp_adv(p);
            }
        } else {
            return;
        }
    }
}

static int dp__jp_literal(dp__jp *p, const char *word)
{
    size_t k = 0, j;
    while (word[k]) {
        if (dp__jp_peek(p, k) != (dp_char32)(unsigned char)word[k]) return 0;
        ++k;
    }
    for (j = 0; j < k; ++j) dp__jp_adv(p);
    return 1;
}

static int dp__jp_hex4(dp__jp *p, unsigned *v)
{
    int k;
    *v = 0;
    for (k = 0; k < 4; ++k) {
        dp_char32 c;
        unsigned d;
        if (dp__jp_eof(p)) { dp__jp_fail(p, "оборванная \\u-последовательность"); return 0; }
        c = dp__jp_cur(p);
        if      (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a') + 10;
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A') + 10;
        else { dp__jp_fail(p, "ожидалась шестнадцатеричная цифра"); return 0; }
        *v = (*v << 4) | d;
        dp__jp_adv(p);
    }
    return 1;
}

static int dp__jp_string(dp__jp *p, dp_u32 *out)
{
    out->n = 0;
    if (dp__jp_cur(p) != '"') { dp__jp_fail(p, "ожидалась строка"); return 0; }
    dp__jp_adv(p);

    for (;;) {
        dp_char32 c, e;
        if (dp__jp_eof(p)) { dp__jp_fail(p, "незакрытая строка"); return 0; }
        c = dp__jp_cur(p);
        if (c == '"') { dp__jp_adv(p); return 1; }
        if (c < 0x20) { dp__jp_fail(p, "управляющий символ в строке"); return 0; }
        if (c != '\\') { dp_u32_push(out, c); dp__jp_adv(p); continue; }

        dp__jp_adv(p);
        if (dp__jp_eof(p)) { dp__jp_fail(p, "незакрытая escape-последовательность"); return 0; }
        e = dp__jp_cur(p);
        switch (e) {
            case '"':  dp_u32_push(out, '"');  dp__jp_adv(p); break;
            case '\\': dp_u32_push(out, '\\'); dp__jp_adv(p); break;
            case '/':  dp_u32_push(out, '/');  dp__jp_adv(p); break;
            case 'b':  dp_u32_push(out, 0x08); dp__jp_adv(p); break;
            case 'f':  dp_u32_push(out, 0x0C); dp__jp_adv(p); break;
            case 'n':  dp_u32_push(out, 0x0A); dp__jp_adv(p); break;
            case 'r':  dp_u32_push(out, 0x0D); dp__jp_adv(p); break;
            case 't':  dp_u32_push(out, 0x09); dp__jp_adv(p); break;
            case 'u': {
                unsigned hi, lo;
                dp__jp_adv(p);
                if (!dp__jp_hex4(p, &hi)) return 0;
                if (hi >= 0xD800 && hi <= 0xDBFF) {
                    if (dp__jp_cur(p) == '\\' && dp__jp_peek(p, 1) == 'u') {
                        size_t si = p->i, sl = p->line, sc = p->col;
                        dp__jp_adv(p); dp__jp_adv(p);
                        if (!dp__jp_hex4(p, &lo)) return 0;
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            dp_u32_push(out, (dp_char32)(0x10000u + ((hi - 0xD800u) << 10)
                                                                  + (lo - 0xDC00u)));
                            break;
                        }
                        p->i = si; p->line = sl; p->col = sc;
                    }
                    dp_u32_push(out, DP_REPLACEMENT);   /* непарный high surrogate */
                } else if (hi >= 0xDC00 && hi <= 0xDFFF) {
                    dp_u32_push(out, DP_REPLACEMENT);   /* непарный low surrogate  */
                } else {
                    dp_u32_push(out, (dp_char32)hi);
                }
                break;
            }
            default:
                dp__jp_fail(p, "недопустимая escape-последовательность");
                return 0;
        }
    }
}

static void dp__jp_number(dp__jp *p, dp_json *out)
{
    char raw[512];
    size_t rn = 0;
    int is_int = 1, neg = 0;

#define DP__RAW_PUT(ch) do { if (rn + 1 < sizeof(raw)) raw[rn++] = (char)(ch); } while (0)

    if (dp__jp_cur(p) == '-') { neg = 1; DP__RAW_PUT('-'); dp__jp_adv(p); }

    if (dp__jp_eof(p)) { dp__jp_fail(p, "оборванное число"); return; }

    if (dp__jp_cur(p) == '0') {
        DP__RAW_PUT('0');
        dp__jp_adv(p);
    } else if (dp__jp_cur(p) >= '1' && dp__jp_cur(p) <= '9') {
        while (!dp__jp_eof(p) && dp__jp_cur(p) >= '0' && dp__jp_cur(p) <= '9') {
            DP__RAW_PUT(dp__jp_cur(p));
            dp__jp_adv(p);
        }
    } else {
        dp__jp_fail(p, "ожидалась цифра");
        return;
    }

    if (!dp__jp_eof(p) && dp__jp_cur(p) == '.') {
        is_int = 0;
        DP__RAW_PUT('.');
        dp__jp_adv(p);
        if (dp__jp_eof(p) || dp__jp_cur(p) < '0' || dp__jp_cur(p) > '9') {
            dp__jp_fail(p, "ожидалась цифра после точки");
            return;
        }
        while (!dp__jp_eof(p) && dp__jp_cur(p) >= '0' && dp__jp_cur(p) <= '9') {
            DP__RAW_PUT(dp__jp_cur(p));
            dp__jp_adv(p);
        }
    }

    if (!dp__jp_eof(p) && (dp__jp_cur(p) == 'e' || dp__jp_cur(p) == 'E')) {
        is_int = 0;
        DP__RAW_PUT('e');
        dp__jp_adv(p);
        if (!dp__jp_eof(p) && (dp__jp_cur(p) == '+' || dp__jp_cur(p) == '-')) {
            DP__RAW_PUT(dp__jp_cur(p));
            dp__jp_adv(p);
        }
        if (dp__jp_eof(p) || dp__jp_cur(p) < '0' || dp__jp_cur(p) > '9') {
            dp__jp_fail(p, "ожидалась цифра в экспоненте");
            return;
        }
        while (!dp__jp_eof(p) && dp__jp_cur(p) >= '0' && dp__jp_cur(p) <= '9') {
            DP__RAW_PUT(dp__jp_cur(p));
            dp__jp_adv(p);
        }
    }
    raw[rn] = '\0';
#undef DP__RAW_PUT

    if (is_int) {
        /* Пытаемся сохранить точное целое; при переполнении падаем в double. */
        const char *q = raw;
        unsigned long long acc = 0, limit;
        int overflow = 0;
        if (*q == '-') ++q;
        for (; *q; ++q) {
            unsigned d = (unsigned)(*q - '0');
            if (acc > (0xFFFFFFFFFFFFFFFFull - d) / 10ull) { overflow = 1; break; }
            acc = acc * 10ull + d;
        }
        limit = neg ? 9223372036854775808ull : 9223372036854775807ull;
        if (!overflow && acc <= limit) {
            out->type = DP_JSON_NUMBER;
            out->is_int = 1;
            out->i = neg ? (long long)(0ull - acc) : (long long)acc;
            out->d = (double)out->i;
            return;
        }
    }
    out->type = DP_JSON_NUMBER;
    out->is_int = 0;
    out->d = dp_json_strtod(raw);
    out->i = (long long)out->d;
}

static void dp__jp_value(dp__jp *p, dp_json *out, size_t depth);

static void dp__jp_array(dp__jp *p, dp_json *out, size_t depth)
{
    out->type = DP_JSON_ARRAY;
    dp__jp_adv(p);                       /* [ */
    dp__jp_ws(p);
    if (!p->err->ok) return;
    if (dp__jp_cur(p) == ']') { dp__jp_adv(p); return; }

    for (;;) {
        dp__jp_ws(p);
        if (!p->err->ok) return;
        if (p->opt->allow_trailing_commas && dp__jp_cur(p) == ']') { dp__jp_adv(p); return; }

        dp__jp_value(p, dp_json_emplace_back(out), depth + 1);
        if (!p->err->ok) return;

        dp__jp_ws(p);
        if (!p->err->ok) return;
        if (dp__jp_cur(p) == ',') { dp__jp_adv(p); continue; }
        if (dp__jp_cur(p) == ']') { dp__jp_adv(p); return; }
        dp__jp_fail(p, "ожидалась ',' или ']'");
        return;
    }
}

static void dp__jp_object(dp__jp *p, dp_json *out, size_t depth)
{
    out->type = DP_JSON_OBJECT;
    dp__jp_adv(p);                       /* { */
    dp__jp_ws(p);
    if (!p->err->ok) return;
    if (dp__jp_cur(p) == '}') { dp__jp_adv(p); return; }

    for (;;) {
        dp_u32   key = dp_u32_new();
        dp_json *slot;

        dp__jp_ws(p);
        if (!p->err->ok) { dp_u32_free(&key); return; }
        if (p->opt->allow_trailing_commas && dp__jp_cur(p) == '}') {
            dp__jp_adv(p); dp_u32_free(&key); return;
        }
        if (!dp__jp_string(p, &key)) { dp_u32_free(&key); return; }

        dp__jp_ws(p);
        if (!p->err->ok) { dp_u32_free(&key); return; }
        if (dp__jp_cur(p) != ':') { dp__jp_fail(p, "ожидалось ':'"); dp_u32_free(&key); return; }
        dp__jp_adv(p);
        dp__jp_ws(p);
        if (!p->err->ok) { dp_u32_free(&key); return; }

        slot = dp__json_emplace_key(out, key);   /* владение ключом переходит объекту */
        if (!slot) { dp__jp_fail(p, "внутренняя ошибка"); return; }

        dp__jp_value(p, slot, depth + 1);
        if (!p->err->ok) return;

        dp__jp_ws(p);
        if (!p->err->ok) return;
        if (dp__jp_cur(p) == ',') { dp__jp_adv(p); continue; }
        if (dp__jp_cur(p) == '}') { dp__jp_adv(p); return; }
        dp__jp_fail(p, "ожидалась ',' или '}'");
        return;
    }
}

static void dp__jp_value(dp__jp *p, dp_json *out, size_t depth)
{
    dp_char32 c;

    if (!p->err->ok) return;
    if (depth > p->opt->max_depth) { dp__jp_fail(p, "превышена максимальная вложенность"); return; }
    if (dp__jp_eof(p)) { dp__jp_fail(p, "неожиданный конец данных"); return; }

    c = dp__jp_cur(p);
    if (c == '{') { dp__jp_object(p, out, depth); return; }
    if (c == '[') { dp__jp_array(p, out, depth);  return; }
    if (c == '"') {
        out->type = DP_JSON_STRING;
        if (!dp__jp_string(p, &out->s)) out->type = DP_JSON_NULL;
        return;
    }
    if (c == 't') {
        if (dp__jp_literal(p, "true"))  { out->type = DP_JSON_BOOL; out->b = 1; return; }
        dp__jp_fail(p, "ожидалось true");  return;
    }
    if (c == 'f') {
        if (dp__jp_literal(p, "false")) { out->type = DP_JSON_BOOL; out->b = 0; return; }
        dp__jp_fail(p, "ожидалось false"); return;
    }
    if (c == 'n') {
        if (dp__jp_literal(p, "null"))  { out->type = DP_JSON_NULL; return; }
        dp__jp_fail(p, "ожидалось null");  return;
    }
    if (c == '-' || (c >= '0' && c <= '9')) { dp__jp_number(p, out); return; }
    dp__jp_fail(p, "ожидалось значение");
}

dp_json *dp_json_parse_u32(const dp_u32 *text, dp_json_error *err, const dp_json_options *opt)
{
    dp_json_error  local;
    dp_json_options defopt = dp_json_options_default();
    dp_u32          empty = dp_u32_new();
    dp__jp          p;
    dp_json        *root;

    if (!err) err = &local;
    err->ok = 1;
    err->message[0] = '\0';
    err->line = 1; err->column = 1; err->offset = 0;

    p.s = text ? text : &empty;
    p.i = 0; p.line = 1; p.col = 1;
    p.opt = opt ? opt : &defopt;
    p.err = err;

    root = dp_json_new_null();
    dp__jp_ws(&p);
    dp__jp_value(&p, root, 0);
    if (err->ok) {
        dp__jp_ws(&p);
        if (p.i < p.s->n) dp__jp_fail(&p, "лишние данные после значения");
    }
    if (!err->ok) { dp_json_free(root); root = NULL; }
    return root;
}

dp_json *dp_json_parse_x(const void *text, dp_chartype t,
                         dp_json_error *err, const dp_json_options *opt)
{
    dp_u32 u = dp_to32(text, DP_AUTOLEN, t);
    dp_json *v = dp_json_parse_u32(&u, err, opt);
    dp_u32_free(&u);
    return v;
}

dp_json *dp_json_parse(const char *utf8, dp_json_error *err, const dp_json_options *opt)
{
    return dp_json_parse_x(utf8 ? utf8 : "", DP_CHAR, err, opt);
}

dp_json *dp_json_parse_bytes(const char *bytes, size_t n, dp_charset cs,
                             dp_json_error *err, const dp_json_options *opt)
{
    dp_u32 u = dp_decode(bytes, n, cs);
    dp_json *v = dp_json_parse_u32(&u, err, opt);
    dp_u32_free(&u);
    return v;
}

dp_json *dp_json_load(const char *path, dp_charset cs,
                      dp_json_error *err, const dp_json_options *opt)
{
    int ok = 0;
    dp_bytes raw = dp_read_all_bytes(path, &ok);
    dp_json *v;

    if (!ok) {
        dp_bytes_free(&raw);
        if (err) {
            err->ok = 0;
            snprintf(err->message, sizeof(err->message), "не удалось открыть файл");
            err->line = 1; err->column = 1; err->offset = 0;
        }
        return NULL;
    }
    v = dp_json_parse_bytes(raw.p, raw.n, cs, err, opt);
    dp_bytes_free(&raw);
    return v;
}

/* =====================================================================
 * 14. JSON — печать
 * ===================================================================== */

static void dp__jw_ascii(dp_u32 *out, const char *s)
{
    while (*s) dp_u32_push(out, (dp_char32)(unsigned char)*s++);
}

static void dp__jw_hex4(dp_u32 *out, unsigned v)
{
    static const char hex[] = "0123456789abcdef";
    dp_u32_push(out, (dp_char32)hex[(v >> 12) & 0xF]);
    dp_u32_push(out, (dp_char32)hex[(v >> 8) & 0xF]);
    dp_u32_push(out, (dp_char32)hex[(v >> 4) & 0xF]);
    dp_u32_push(out, (dp_char32)hex[v & 0xF]);
}

static void dp__jw_string(dp_u32 *out, const dp_u32 *s, int ensure_ascii)
{
    size_t k;
    dp_u32_push(out, '"');
    for (k = 0; s && k < s->n; ++k) {
        dp_char32 c = s->p[k];
        switch (c) {
            case '"':  dp__jw_ascii(out, "\\\""); continue;
            case '\\': dp__jw_ascii(out, "\\\\"); continue;
            case 0x08: dp__jw_ascii(out, "\\b");  continue;
            case 0x0C: dp__jw_ascii(out, "\\f");  continue;
            case 0x0A: dp__jw_ascii(out, "\\n");  continue;
            case 0x0D: dp__jw_ascii(out, "\\r");  continue;
            case 0x09: dp__jw_ascii(out, "\\t");  continue;
            default: break;
        }
        if (c < 0x20) {
            dp__jw_ascii(out, "\\u");
            dp__jw_hex4(out, (unsigned)c);
        } else if (ensure_ascii && c > 0x7E) {
            if (c <= 0xFFFF) {
                dp__jw_ascii(out, "\\u");
                dp__jw_hex4(out, (unsigned)c);
            } else {
                dp_char32 v = c - 0x10000;
                dp__jw_ascii(out, "\\u");
                dp__jw_hex4(out, (unsigned)(0xD800 + (v >> 10)));
                dp__jw_ascii(out, "\\u");
                dp__jw_hex4(out, (unsigned)(0xDC00 + (v & 0x3FF)));
            }
        } else {
            dp_u32_push(out, c);
        }
    }
    dp_u32_push(out, '"');
}

static void dp__jw_indent(dp_u32 *out, int indent, int level)
{
    int k;
    if (indent <= 0) return;
    dp_u32_push(out, 0x0A);
    for (k = 0; k < indent * level; ++k) dp_u32_push(out, ' ');
}

static void dp__jw_value(const dp_json *v, dp_u32 *out, int indent, int ensure_ascii, int level)
{
    size_t k;

    if (!v) { dp__jw_ascii(out, "null"); return; }

    switch (v->type) {
        case DP_JSON_NULL: dp__jw_ascii(out, "null"); break;
        case DP_JSON_BOOL: dp__jw_ascii(out, v->b ? "true" : "false"); break;

        case DP_JSON_NUMBER:
            if (v->is_int) {
                dp_char32 *n = (dp_char32*)dp_to_str_x(v->i, 10, DP_CHAR32);
                size_t j = 0;
                while (n[j]) dp_u32_push(out, n[j++]);
                dp_free(n);
            } else {
                char buf[64];
                dp_json_dtoa(v->d, buf, sizeof(buf));
                dp__jw_ascii(out, buf);
            }
            break;

        case DP_JSON_STRING:
            dp__jw_string(out, &v->s, ensure_ascii);
            break;

        case DP_JSON_ARRAY:
            if (v->count == 0) { dp__jw_ascii(out, "[]"); break; }
            dp_u32_push(out, '[');
            for (k = 0; k < v->count; ++k) {
                if (k) dp_u32_push(out, ',');
                dp__jw_indent(out, indent, level + 1);
                dp__jw_value(v->items[k], out, indent, ensure_ascii, level + 1);
            }
            dp__jw_indent(out, indent, level);
            dp_u32_push(out, ']');
            break;

        case DP_JSON_OBJECT:
            if (v->count == 0) { dp__jw_ascii(out, "{}"); break; }
            dp_u32_push(out, '{');
            for (k = 0; k < v->count; ++k) {
                if (k) dp_u32_push(out, ',');
                dp__jw_indent(out, indent, level + 1);
                dp__jw_string(out, v->keys ? &v->keys[k] : NULL, ensure_ascii);
                dp_u32_push(out, ':');
                if (indent > 0) dp_u32_push(out, ' ');
                dp__jw_value(v->items[k], out, indent, ensure_ascii, level + 1);
            }
            dp__jw_indent(out, indent, level);
            dp_u32_push(out, '}');
            break;
    }
}

dp_u32 dp_json_dump_u32(const dp_json *v, int indent, int ensure_ascii)
{
    dp_u32 out = dp_u32_new();
    dp__jw_value(v, &out, indent, ensure_ascii, 0);
    return out;
}

char *dp_json_dump(const dp_json *v, int indent, int ensure_ascii)
{
    dp_u32 u = dp_json_dump_u32(v, indent, ensure_ascii);
    char *r = (char*)dp_from32(&u, DP_CHAR, NULL);
    dp_u32_free(&u);
    return r;
}

dp_bytes dp_json_dump_bytes(const dp_json *v, dp_charset cs, int indent,
                            int ensure_ascii, int with_bom)
{
    dp_u32 u = dp_json_dump_u32(v, indent, ensure_ascii);
    dp_bytes b = dp_encode(&u, cs, with_bom);
    dp_u32_free(&u);
    return b;
}

int dp_json_save(const char *path, const dp_json *v, dp_charset cs,
                 int indent, int ensure_ascii, int with_bom)
{
    dp_bytes b = dp_json_dump_bytes(v, cs, indent, ensure_ascii, with_bom);
    int r = dp_write_all_bytes(path, b.p, b.n, 0);
    dp_bytes_free(&b);
    return r;
}

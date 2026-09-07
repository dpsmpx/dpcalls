/* test_C.c — те же 17 групп тестов, что и в test.cpp для C++ версии.
 * Сборка: gcc -std=c99 -Wall -Wextra -o test_C test_C.c dpcalls_C.c
 */

#include "dpcalls_C.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* сравнение строк любого типа символа через UTF-8 */
static int eq_x(const void *a, dp_chartype ta, const void *b, dp_chartype tb)
{
    char *x = (char*)dp_convert(a, ta, DP_CHAR);
    char *y = (char*)dp_convert(b, tb, DP_CHAR);
    int r = (strcmp(x, y) == 0);
    dp_free(x); dp_free(y);
    return r;
}

int main(void)
{
    dp_init_console();

    /* ---- 1. reverse корректен для UTF-8 ---- */
    {
        const char *s = "Привет, мир!";
        char *r  = dp_reverse(s);
        char *rr = dp_reverse(r);
        printf("reverse utf8 : %s\n", r);
        assert(strcmp(rr, s) == 0);
        assert(dp_length(s) == 12);
        printf("length       : %lu (байт %lu)\n",
               (unsigned long)dp_length(s), (unsigned long)strlen(s));
        dp_free(r); dp_free(rr);
    }

    /* ---- 2. то же самое в wchar_t ---- */
    {
        const char *s = "Привет, мир!";
        wchar_t *w  = (wchar_t*)dp_convert(s, DP_CHAR, DP_WCHAR);
        wchar_t *wr = dp_reverse_w(w);
        char    *r  = dp_reverse(s);
        assert(eq_x(wr, DP_WCHAR, r, DP_CHAR));
        assert(dp_length_w(w) == 12);
        dp_free(w); dp_free(wr); dp_free(r);
    }

    /* ---- 3. и в char16_t / char32_t ---- */
    {
        const char *s = "Привет, мир!";
        dp_char16 *s16 = (dp_char16*)dp_convert(s, DP_CHAR, DP_CHAR16);
        dp_char32 *s32 = (dp_char32*)dp_convert(s, DP_CHAR, DP_CHAR32);
        dp_char16 *r16 = dp_reverse_16(s16);
        dp_char32 *r32 = dp_reverse_32(s32);
        char *r = dp_reverse(s);
        assert(dp_length_16(s16) == 12);
        assert(dp_length_32(s32) == 12);
        assert(eq_x(r16, DP_CHAR16, r, DP_CHAR));
        assert(eq_x(r32, DP_CHAR32, r, DP_CHAR));
        dp_free(s16); dp_free(s32); dp_free(r16); dp_free(r32); dp_free(r);
    }

    /* ---- 4. суррогатные пары (вне BMP) ---- */
    {
        const char *emoji = "a\xF0\x9F\x98\x80z";     /* a U+1F600 z */
        dp_char16 *e16;
        wchar_t   *ew;
        char *back16, *backw, *rev, *revrev;
        assert(dp_length(emoji) == 3);

        e16 = (dp_char16*)dp_convert(emoji, DP_CHAR, DP_CHAR16);
        ew  = (wchar_t*)  dp_convert(emoji, DP_CHAR, DP_WCHAR);
        back16 = (char*)dp_convert(e16, DP_CHAR16, DP_CHAR);
        backw  = (char*)dp_convert(ew,  DP_WCHAR,  DP_CHAR);
        assert(strcmp(back16, emoji) == 0);
        assert(strcmp(backw,  emoji) == 0);

        rev = dp_reverse(emoji);
        revrev = dp_reverse(rev);
        assert(strcmp(revrev, emoji) == 0);
        printf("emoji rev    : %s\n", rev);
        dp_free(e16); dp_free(ew); dp_free(back16); dp_free(backw);
        dp_free(rev); dp_free(revrev);
    }

    /* ---- 5. однобайтовые кодировки ---- */
    {
        const char *s = "Привет, мир!";
        dp_u32   u    = dp_utf8_to_u32(s, DP_AUTOLEN);
        dp_bytes c1251 = dp_encode(&u, dp_cs_cp1251(), 0);
        dp_bytes c866, koi, back;

        printf("cp1251 size  : %lu байт\n", (unsigned long)c1251.n);
        assert(c1251.n == 12);

        back = dp_recode(c1251.p, c1251.n, dp_cs_cp1251(), dp_cs_utf8(), 0);
        assert(back.n == strlen(s) && memcmp(back.p, s, back.n) == 0);
        dp_bytes_free(&back);

        c866 = dp_recode(c1251.p, c1251.n, dp_cs_cp1251(), dp_cs_cp866(), 0);
        assert(c866.n == 12);
        back = dp_recode(c866.p, c866.n, dp_cs_cp866(), dp_cs_utf8(), 0);
        assert(back.n == strlen(s) && memcmp(back.p, s, back.n) == 0);
        dp_bytes_free(&back);

        koi = dp_recode(s, DP_AUTOLEN, dp_cs_utf8(), dp_cs_koi8r(), 0);
        back = dp_recode(koi.p, koi.n, dp_cs_koi8r(), dp_cs_utf8(), 0);
        assert(back.n == strlen(s) && memcmp(back.p, s, back.n) == 0);

        dp_bytes_free(&back); dp_bytes_free(&koi);
        dp_bytes_free(&c866); dp_bytes_free(&c1251);
        dp_u32_free(&u);
    }

    /* ---- 6. файлы: запись cp1251, чтение обратно ---- */
    {
        dp_slist lines = dp_slist_new(DP_CHAR);
        dp_slist back;
        dp_slist_push_copy(&lines, "Первая строка");
        dp_slist_push_copy(&lines, "");
        dp_slist_push_copy(&lines, "Третья строка");
        assert(dp_WriteAllLines("c1251.txt", &lines, dp_cs_cp1251(), 0));

        back = dp_ReadAllLines("c1251.txt", dp_cs_cp1251());
        assert(back.count == 3);
        assert(strcmp(dp_slist_s(&back, 0), "Первая строка") == 0);
        assert(dp_slist_s(&back, 1)[0] == '\0');
        assert(strcmp(dp_slist_s(&back, 2), "Третья строка") == 0);
        dp_slist_free(&back);

        /* ---- 7. UTF-16 с BOM, автоопределение ---- */
        assert(dp_WriteAllLines("c16.txt", &lines, dp_cs_utf16le(), 1));
        back = dp_ReadAllLines("c16.txt", dp_cs_auto());
        assert(back.count == 3);
        assert(strcmp(dp_slist_s(&back, 0), "Первая строка") == 0);
        assert(strcmp(dp_slist_s(&back, 2), "Третья строка") == 0);
        dp_slist_free(&back);

        /* ---- 8. UTF-32BE ---- */
        assert(dp_WriteAllLines("c32.txt", &lines, dp_cs_utf32be(), 1));
        back = dp_ReadAllLines("c32.txt", dp_cs_auto());
        assert(back.count == 3);
        assert(strcmp(dp_slist_s(&back, 0), "Первая строка") == 0);
        dp_slist_free(&back);

        dp_slist_free(&lines);
    }

    /* ---- 9. CRLF и последняя строка без \n ---- */
    {
        const char *raw = "aa\r\nбб\r\nвв";
        dp_slist l;
        assert(dp_write_all_bytes("c_crlf.txt", raw, strlen(raw), 0));
        l = dp_ReadAllLines("c_crlf.txt", dp_cs_auto());
        assert(l.count == 3);
        assert(strcmp(dp_slist_s(&l, 0), "aa") == 0);
        assert(strcmp(dp_slist_s(&l, 1), "бб") == 0);
        assert(strcmp(dp_slist_s(&l, 2), "вв") == 0);
        dp_slist_free(&l);
    }

    /* ---- 10. words / split / join / trim ---- */
    {
        dp_slist ws = dp_words("  раз   два\tтри  ", 0);
        dp_slist wws, parts;
        wchar_t *wline;
        char *joined, *tr;

        assert(ws.count == 3);
        assert(strcmp(dp_slist_s(&ws, 0), "раз") == 0);
        assert(strcmp(dp_slist_s(&ws, 1), "два") == 0);
        assert(strcmp(dp_slist_s(&ws, 2), "три") == 0);
        dp_slist_free(&ws);

        wline = (wchar_t*)dp_convert("один два", DP_CHAR, DP_WCHAR);
        wws = dp_words_w(wline, 0);
        assert(wws.count == 2);
        assert(eq_x(dp_slist_w(&wws, 1), DP_WCHAR, "два", DP_CHAR));
        dp_slist_free(&wws);
        dp_free(wline);

        parts = dp_split("a;b;;c", (dp_char32)';', 1);
        assert(parts.count == 4);
        assert(dp_slist_s(&parts, 2)[0] == '\0');
        joined = dp_join(&parts, ";");
        assert(strcmp(joined, "a;b;;c") == 0);
        dp_free(joined);
        dp_slist_free(&parts);

        tr = dp_trim("  тест \t");
        assert(strcmp(tr, "тест") == 0);
        dp_free(tr);
    }

    /* ---- 11. itos / to_str / stoll_safe ---- */
    {
        char *a = dp_itos(-12345, 10);
        char *b = dp_itos(255, 16);
        wchar_t   *w = dp_itow(1000, 10);
        dp_char32 *u = (dp_char32*)dp_to_str_x(0, 10, DP_CHAR32);
        long long v = 0;

        assert(strcmp(a, "-12345") == 0);
        assert(strcmp(b, "ff") == 0);
        assert(eq_x(w, DP_WCHAR, "1000", DP_CHAR));
        assert(eq_x(u, DP_CHAR32, "0", DP_CHAR));
        assert(dp_stoll_safe("  -42  ", &v, 10) && v == -42);
        assert(!dp_stoll_safe("42abc", &v, 10));
        dp_free(a); dp_free(b); dp_free(w); dp_free(u);
    }

    /* ---- 12. регистр ---- */
    {
        char *up = dp_upper("привет ёж");
        char *lo = dp_lower("ПРИВЕТ ЁЖ");
        wchar_t *wsrc = (wchar_t*)dp_convert("abc-щи", DP_CHAR, DP_WCHAR);
        wchar_t *wup  = dp_upper_w(wsrc);
        assert(strcmp(up, "ПРИВЕТ ЁЖ") == 0);
        assert(strcmp(lo, "привет ёж") == 0);
        assert(eq_x(wup, DP_WCHAR, "ABC-ЩИ", DP_CHAR));
        dp_free(up); dp_free(lo); dp_free(wsrc); dp_free(wup);
    }

    /* ---- 13. substr по кодовым точкам ---- */
    {
        char *a = dp_substr_cp("Привет", 0, 3);
        char *b = dp_substr_cp("Привет", 3, DP_AUTOLEN);
        assert(strcmp(a, "При") == 0);
        assert(strcmp(b, "вет") == 0);
        dp_free(a); dp_free(b);
    }

    /* ---- 14. битый UTF-8 не роняет программу ---- */
    {
        const char *broken = "ab\xFF\xC3";
        dp_u32 bu = dp_utf8_to_u32(broken, DP_AUTOLEN);
        assert(bu.n == 4);
        assert(bu.p[2] == DP_REPLACEMENT && bu.p[3] == DP_REPLACEMENT);
        dp_u32_free(&bu);
    }

    /* ---- 15. непредставимый символ в однобайтовой кодировке ---- */
    {
        dp_char32 src[4];
        dp_u32 u;
        dp_bytes b;
        src[0] = 'a'; src[1] = 0x4E2D; src[2] = 'b'; src[3] = 0;
        u = dp_u32_copy(src, 3);
        b = dp_encode(&u, dp_cs_cp1251(), 0);
        assert(b.n == 3 && memcmp(b.p, "a?b", 3) == 0);
        dp_bytes_free(&b);
        dp_u32_free(&u);
    }

    /* ---- 16. rnd ---- */
    {
        int i, first, varied = 0;
        for (i = 0; i < 1000; ++i) {
            int a = dp_rnd(10);
            int b = dp_rnd_range(-5, 5);
            assert(a >= 0 && a < 10);
            assert(b >= -5 && b <= 5);
        }
        first = dp_rnd(1000000);
        for (i = 0; i < 10 && !varied; ++i)
            if (dp_rnd(1000000) != first) varied = 1;
        assert(varied);   /* старая версия с srand в теле давала одно и то же */
        assert(dp_rndf() >= 0.0 && dp_rndf() < 1.0);
    }

    /* ---- 17. печать в разных кодировках ---- */
    {
        dp_slist lines = dp_slist_new(DP_CHAR);
        dp_bytes c866;
        dp_slist_push_copy(&lines, "Первая строка");
        dp_slist_push_copy(&lines, "");
        dp_slist_push_copy(&lines, "Третья строка");
        dp_print_x(&lines, dp_cs_utf8());
        c866 = dp_recode("Привет, мир!", DP_AUTOLEN, dp_cs_utf8(), dp_cs_cp866(), 0);
        printf("cp866 bytes  : %lu\n", (unsigned long)c866.n);
        dp_bytes_free(&c866);
        dp_slist_free(&lines);
    }

    /* ---- 18. своя таблица кодировки + системная локаль ---- */
    {
        static dp_char32 mytab[128];
        dp_charset my;
        const char bytes[3] = { (char)0x80, (char)0x81, 'A' };
        dp_u32 d, u;
        dp_bytes e, loc;
        int i;
        for (i = 0; i < 128; ++i) mytab[i] = (dp_char32)(0x2500 + i);
        my = dp_cs_custom(mytab, "my");

        d = dp_decode(bytes, 3, my);
        assert(d.n == 3 && d.p[0] == 0x2500 && d.p[1] == 0x2501 && d.p[2] == 'A');
        e = dp_encode(&d, my, 0);
        assert(e.n == 3 && memcmp(e.p, bytes, 3) == 0);
        dp_bytes_free(&e);
        dp_u32_free(&d);

        u = dp_utf8_to_u32("Привет", DP_AUTOLEN);
        loc = dp_encode(&u, dp_cs_local(), 0);
        d = dp_decode(loc.p, loc.n, dp_cs_local());
        {
            dp_bytes rt = dp_u32_to_utf8(&d);
            printf("locale rt    : %.*s (%lu байт)\n",
                   (int)rt.n, rt.p, (unsigned long)loc.n);
            dp_bytes_free(&rt);
        }
        dp_u32_free(&d); dp_bytes_free(&loc); dp_u32_free(&u);
    }

    printf("\nsizeof(wchar_t) = %d\nВСЕ ТЕСТЫ ПРОЙДЕНЫ\n", (int)sizeof(wchar_t));
    return 0;
}

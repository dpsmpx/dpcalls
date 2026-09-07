/* test_json_C.c — те же тесты JSON, что и в test_json.cpp, для C-версии.
 * Сборка: gcc -std=c99 -Wall -Wextra -o test_json_C test_json_C.c dpcalls_C.c
 */

#include "dpcalls_C.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>

static dp_json *P(const char *s, const dp_json_options *o)
{
    dp_json_error e;
    dp_json *v = dp_json_parse(s, &e, o);
    if (!v) {
        char buf[256];
        dp_json_error_str(&e, buf, sizeof(buf));
        fprintf(stderr, "неожиданная ошибка: %s для %s\n", buf, s);
        assert(0);
    }
    return v;
}

static void bad(const char *s, const dp_json_options *o)
{
    dp_json_error e;
    dp_json *v = dp_json_parse(s, &e, o);
    if (v) { fprintf(stderr, "должно было упасть: %s\n", s); dp_json_free(v); assert(0); }
    assert(!e.ok);
}

/* сравнить строковое значение с UTF-8 эталоном */
static int str_eq(const dp_json *v, const char *utf8)
{
    char *s = dp_json_as_utf8(v);
    int r = (strcmp(s, utf8) == 0);
    dp_free(s);
    return r;
}

static int dump_eq(const dp_json *v, const char *utf8)
{
    char *s = dp_json_dump(v, 0, 0);
    int r = (strcmp(s, utf8) == 0);
    dp_free(s);
    return r;
}

static int dump_has(const dp_json *v, const char *needle)
{
    char *s = dp_json_dump(v, 0, 0);
    int r = (strstr(s, needle) != NULL);
    dp_free(s);
    return r;
}

int main(void)
{
    dp_init_console();

    /* ---- 1. скаляры ---- */
    {
        dp_json *a;
        a = P("null", NULL);   assert(dp_json_is_null(a));                     dp_json_free(a);
        a = P("true", NULL);   assert(dp_json_as_bool(a, 0) == 1);             dp_json_free(a);
        a = P("false", NULL);  assert(dp_json_as_bool(a, 1) == 0);             dp_json_free(a);
        a = P("42", NULL);     assert(dp_json_is_int(a) && dp_json_as_int(a,0) == 42); dp_json_free(a);
        a = P("-0", NULL);     assert(dp_json_as_int(a, 9) == 0);              dp_json_free(a);
        a = P("3.5", NULL);    assert(dp_json_as_double(a,0) == 3.5 && !dp_json_is_int(a)); dp_json_free(a);
        a = P("1e3", NULL);    assert(dp_json_as_double(a,0) == 1000.0);       dp_json_free(a);
        a = P("-1.5e-2", NULL);assert(dp_json_as_double(a,0) == -0.015);       dp_json_free(a);
        a = P("\"текст\"", NULL); assert(str_eq(a, "текст"));                  dp_json_free(a);
    }

    /* ---- 2. точные большие целые ---- */
    {
        dp_json *v = P("9007199254740993", NULL);          /* 2^53 + 1 */
        assert(dp_json_is_int(v) && dp_json_as_int(v,0) == 9007199254740993LL);
        assert(dump_eq(v, "9007199254740993"));
        dp_json_free(v);

        v = P("9223372036854775807", NULL);
        assert(dp_json_as_int(v,0) == 9223372036854775807LL);
        dp_json_free(v);

        v = P("-9223372036854775808", NULL);
        assert(dp_json_as_int(v,0) == (-9223372036854775807LL - 1));
        dp_json_free(v);

        v = P("99999999999999999999", NULL);               /* переполнение -> double */
        assert(dp_json_is_number(v) && !dp_json_is_int(v));
        dp_json_free(v);
    }

    /* ---- 3. массивы и объекты ---- */
    {
        dp_json *a = P("[1, 2, [3, 4], {\"k\": 5}]", NULL);
        dp_json *o;
        char *k0;

        assert(dp_json_is_array(a) && dp_json_size(a) == 4);
        assert(dp_json_as_int(dp_json_at(a,0), 0) == 1);
        assert(dp_json_size(dp_json_at(a,2)) == 2);
        assert(dp_json_as_int(dp_json_at(dp_json_at(a,2), 1), 0) == 4);
        assert(dp_json_as_int(dp_json_get(dp_json_at(a,3), "k"), 0) == 5);
        assert(dp_json_at(a, 99) == NULL);              /* выход за границы */
        assert(dp_json_is_null(dp_json_at(a, 99)));    /* NULL трактуется как null */
        dp_json_free(a);

        o = P("{\"a\":1,\"b\":\"два\",\"c\":[true,null]}", NULL);
        assert(dp_json_is_object(o) && dp_json_size(o) == 3);
        assert(dp_json_as_int(dp_json_get(o,"a"), 0) == 1);
        assert(str_eq(dp_json_get(o,"b"), "два"));
        assert(dp_json_as_bool(dp_json_at(dp_json_get(o,"c"), 0), 0));
        assert(dp_json_is_null(dp_json_at(dp_json_get(o,"c"), 1)));
        assert(dp_json_get(o, "нет") == NULL);
        assert(dp_json_has(o,"a") && !dp_json_has(o,"нет"));

        k0 = dp_json_key_at(o, 0);
        assert(strcmp(k0, "a") == 0);
        dp_free(k0);
        k0 = dp_json_key_at(o, 2);
        assert(strcmp(k0, "c") == 0);
        dp_free(k0);
        dp_json_free(o);
    }

    /* ---- 4. escape-последовательности ---- */
    {
        dp_json *v;
        const dp_u32 *raw;

        v = P("\"a\\nb\"", NULL);      assert(str_eq(v, "a\nb"));    dp_json_free(v);
        v = P("\"\\u0041\"", NULL);    assert(str_eq(v, "A"));       dp_json_free(v);
        v = P("\"\\u041F\\u0440\\u0438\"", NULL); assert(str_eq(v, "При")); dp_json_free(v);
        v = P("\"\\/\\\\\\\"\"", NULL); assert(str_eq(v, "/\\\""));  dp_json_free(v);

        v = P("\"\\ud83d\\ude00\"", NULL);              /* суррогатная пара */
        assert(str_eq(v, "\xF0\x9F\x98\x80"));
        raw = dp_json_raw_str(v);
        assert(raw->n == 1 && raw->p[0] == 0x1F600);
        dp_json_free(v);

        v = P("\"\\ud83d\"", NULL);                     /* непарный суррогат */
        raw = dp_json_raw_str(v);
        assert(raw->n == 1 && raw->p[0] == DP_REPLACEMENT);
        dp_json_free(v);
    }

    /* ---- 5. строгость по RFC 8259 ---- */
    bad("", NULL);
    bad("{", NULL);
    bad("[1,]", NULL);
    bad("{\"a\":1,}", NULL);
    bad("{'a':1}", NULL);
    bad("[01]", NULL);
    bad("[1.]", NULL);
    bad("[.5]", NULL);
    bad("[+1]", NULL);
    bad("[1 2]", NULL);
    bad("nul", NULL);
    bad("[1]extra", NULL);
    bad("\"незакрытая", NULL);
    bad("{\"a\" 1}", NULL);
    bad("[\"\\x41\"]", NULL);
    bad("[\"a\tb\"]", NULL);

    /* ---- 6. послабления для конфигов ---- */
    {
        dp_json_options o = dp_json_options_relaxed();
        dp_json *v = P("{\n // комментарий\n \"a\": 1, /* и такой */ \"b\": [1,2,],\n}", &o);
        assert(dp_json_as_int(dp_json_get(v,"a"), 0) == 1);
        assert(dp_json_size(dp_json_get(v,"b")) == 2);
        dp_json_free(v);
        bad("{ /* незакрытый", &o);
    }

    /* ---- 7. защита от глубокой вложенности ---- */
    {
        size_t N = 2000, i;
        char *deep = (char*)malloc(2*N + 1);
        dp_json_error e;
        dp_json *v;
        dp_json_options o;

        for (i = 0; i < N; ++i) deep[i] = '[';
        for (i = 0; i < N; ++i) deep[N+i] = ']';
        deep[2*N] = '\0';

        v = dp_json_parse(deep, &e, NULL);
        assert(!v && !e.ok);                    /* нормальная ошибка, не срыв стека */

        o = dp_json_options_default();
        o.max_depth = 5000;
        v = dp_json_parse(deep, &e, &o);
        assert(v && e.ok);
        dp_json_free(v);
        free(deep);
    }

    /* ---- 8. позиция ошибки ---- */
    {
        dp_json_error e;
        char buf[256];
        dp_json *v = dp_json_parse("{\n  \"a\": 1,\n  \"b\": tru\n}", &e, NULL);
        assert(!v && !e.ok && e.line == 3);
        dp_json_error_str(&e, buf, sizeof(buf));
        printf("ошибка       : %s\n", buf);
    }

    /* ---- 9. сериализация ---- */
    {
        dp_json *o = dp_json_new_object();
        dp_json *arr = dp_json_new_array();
        dp_json *back;
        char *c, *pretty, *a;
        size_t i;

        dp_json_set(o, "имя",   dp_json_new_str("Борис"));
        dp_json_set(o, "лет",   dp_json_new_int(30));
        dp_json_set(o, "дробь", dp_json_new_double(0.5));
        dp_json_set(o, "флаг",  dp_json_new_bool(1));
        dp_json_set(o, "пусто", dp_json_new_null());
        dp_json_push(arr, dp_json_new_int(1));
        dp_json_push(arr, dp_json_new_str("два"));
        dp_json_set(o, "список", arr);            /* владение перешло объекту */

        assert(dump_has(o, "\"имя\":\"Борис\""));
        assert(dump_has(o, "\"лет\":30"));
        assert(dump_has(o, "\"пусто\":null"));

        c = dp_json_dump(o, 0, 0);
        back = P(c, NULL);
        assert(str_eq(dp_json_get(back,"имя"), "Борис"));
        assert(dp_json_as_int(dp_json_get(back,"лет"), 0) == 30);
        assert(dp_json_as_double(dp_json_get(back,"дробь"), 0) == 0.5);
        assert(str_eq(dp_json_at(dp_json_get(back,"список"), 1), "два"));
        assert(dump_eq(back, c));                 /* круговой прогон */
        dp_json_free(back);

        pretty = dp_json_dump(o, 2, 0);
        printf("pretty       :\n%s\n", pretty);
        dp_free(pretty);

        a = dp_json_dump(o, 0, 1);                /* ensure_ascii */
        for (i = 0; a[i]; ++i) assert((unsigned char)a[i] < 0x80);
        back = P(a, NULL);
        assert(str_eq(dp_json_get(back,"имя"), "Борис"));
        dp_json_free(back);
        dp_free(a);
        dp_free(c);
        dp_json_free(o);
    }

    /* ---- 10. круговой прогон double ---- */
    {
        double vals[6];
        int i;
        double zero = 0.0;
        dp_json *v;

        vals[0] = 0.1; vals[1] = 1.0/3.0; vals[2] = 1e-300;
        vals[3] = 1e300; vals[4] = -2.5e-17; vals[5] = 123456789.123456789;

        for (i = 0; i < 6; ++i) {
            char *s;
            dp_json *n = dp_json_new_double(vals[i]);
            s = dp_json_dump(n, 0, 0);
            v = P(s, NULL);
            assert(dp_json_as_double(v, 0) == vals[i]);
            dp_json_free(v);
            dp_free(s);
            dp_json_free(n);
        }
        v = dp_json_new_double(1.0 / zero);       /* +inf -> null */
        assert(dump_eq(v, "null"));
        dp_json_free(v);
    }

    /* ---- 11. независимость от локали ---- */
    {
        const char *loc = setlocale(LC_NUMERIC, "de_DE.UTF-8");
        if (!loc) loc = setlocale(LC_NUMERIC, "ru_RU.UTF-8");
        if (loc) {
            dp_json *v = P("1.5", NULL);
            dp_json *n = dp_json_new_double(1.5);
            assert(dp_json_as_double(v, 0) == 1.5);
            assert(dump_eq(n, "1.5"));
            printf("локаль       : %s — разбор и печать не сломались\n", loc);
            dp_json_free(v); dp_json_free(n);
        } else {
            printf("локаль       : de/ru нет в системе, проверка пропущена\n");
        }
        setlocale(LC_NUMERIC, "C");
        /* прямая проверка помощников */
        assert(dp_json_strtod("1.5") == 1.5);
        assert(dp_json_strtod("-0.015") == -0.015);
        {
            char b[64];
            dp_json_dtoa(1.5, b, sizeof(b));
            assert(strcmp(b, "1.5") == 0);
        }
    }

    /* ---- 12. любые типы символов на входе ---- */
    {
        wchar_t   *w   = (wchar_t*)  dp_convert("{\"ключ\": \"значение\"}", DP_CHAR, DP_WCHAR);
        dp_char16 *s16 = (dp_char16*)dp_convert("[1,2,3]", DP_CHAR, DP_CHAR16);
        dp_char32 *s32 = (dp_char32*)dp_convert("{\"a\":\"я\"}", DP_CHAR, DP_CHAR32);
        dp_json *v;
        const dp_u32 *raw;

        v = dp_json_parse_x(w, DP_WCHAR, NULL, NULL);
        assert(v && str_eq(dp_json_get(v, "ключ"), "значение"));
        dp_json_free(v);

        v = dp_json_parse_x(s16, DP_CHAR16, NULL, NULL);
        assert(v && dp_json_size(v) == 3);
        dp_json_free(v);

        v = dp_json_parse_x(s32, DP_CHAR32, NULL, NULL);
        raw = dp_json_raw_str(dp_json_get(v, "a"));
        assert(raw->n == 1);
        dp_json_free(v);

        dp_free(w); dp_free(s16); dp_free(s32);
    }

    /* ---- 13. любые кодировки на входе ---- */
    {
        const char *src = "{\"город\":\"Москва\",\"код\":495}";
        dp_bytes c1251 = dp_recode(src, DP_AUTOLEN, dp_cs_utf8(), dp_cs_cp1251(), 0);
        dp_bytes u16;
        dp_json_error e;
        dp_json *v;

        v = dp_json_parse_bytes(c1251.p, c1251.n, dp_cs_cp1251(), &e, NULL);
        assert(v && e.ok);
        assert(str_eq(dp_json_get(v, "город"), "Москва"));
        assert(dp_json_as_int(dp_json_get(v, "код"), 0) == 495);
        dp_json_free(v);
        dp_bytes_free(&c1251);

        /* UTF-16 с BOM — распознаётся автоматически */
        u16 = dp_recode(src, DP_AUTOLEN, dp_cs_utf8(), dp_cs_utf16le(), 1);
        v = dp_json_parse_bytes(u16.p, u16.n, dp_cs_auto(), &e, NULL);
        assert(v && e.ok && str_eq(dp_json_get(v, "город"), "Москва"));
        dp_json_free(v);
        dp_bytes_free(&u16);
    }

    /* ---- 14. файлы ---- */
    {
        dp_json *o = dp_json_new_object();
        dp_json_error e;
        dp_json *v;

        dp_json_set(o, "проект", dp_json_new_str("dpcalls"));
        dp_json_set(o, "версия", dp_json_new_int(2));

        assert(dp_json_save("cjson_utf8.json", o, dp_cs_utf8(),   2, 0, 0));
        assert(dp_json_save("cjson_1251.json", o, dp_cs_cp1251(), 2, 0, 0));

        v = dp_json_load("cjson_utf8.json", dp_cs_auto(), &e, NULL);
        assert(v && e.ok && str_eq(dp_json_get(v, "проект"), "dpcalls"));
        dp_json_free(v);

        v = dp_json_load("cjson_1251.json", dp_cs_cp1251(), &e, NULL);
        assert(v && e.ok);
        assert(str_eq(dp_json_get(v, "проект"), "dpcalls"));
        assert(dp_json_as_int(dp_json_get(v, "версия"), 0) == 2);
        dp_json_free(v);

        v = dp_json_load("нет-такого-файла.json", dp_cs_auto(), &e, NULL);
        assert(!v && !e.ok);
        dp_json_free(o);
    }

    /* ---- 15. изменение ---- */
    {
        dp_json *o = P("{\"a\":1,\"b\":2}", NULL);
        dp_json *a = dp_json_new_array();
        char *k;
        int i;

        dp_json_set(o, "a", dp_json_new_int(10));      /* перезапись */
        assert(dp_json_size(o) == 2);
        assert(dp_json_as_int(dp_json_get(o,"a"), 0) == 10);
        k = dp_json_key_at(o, 0);
        assert(strcmp(k, "a") == 0);                   /* порядок сохранён */
        dp_free(k);
        assert(dp_json_remove(o, "b") && dp_json_size(o) == 1);
        assert(!dp_json_remove(o, "b"));
        dp_json_free(o);

        for (i = 0; i < 3; ++i) dp_json_push(a, dp_json_new_int(i));
        assert(dump_eq(a, "[0,1,2]"));
        dp_json_free(a);
    }

    /* ---- 16. дубликаты ключей: побеждает последний ---- */
    {
        dp_json *v = P("{\"a\":1,\"a\":2}", NULL);
        assert(dp_json_as_int(dp_json_get(v,"a"), 0) == 2);
        dp_json_free(v);
    }

    /* ---- 17. клонирование независимо ---- */
    {
        dp_json *o = P("{\"a\":[1,2],\"b\":{\"c\":\"я\"}}", NULL);
        dp_json *c = dp_json_clone(o);
        dp_json_set(o, "a", dp_json_new_int(0));       /* меняем оригинал */
        assert(dp_json_size(dp_json_get(c, "a")) == 2);
        assert(str_eq(dp_json_get(dp_json_get(c,"b"), "c"), "я"));
        dp_json_free(o);
        dp_json_free(c);
    }

    printf("\nВСЕ JSON-ТЕСТЫ ПРОЙДЕНЫ (C)\n");
    return 0;
}

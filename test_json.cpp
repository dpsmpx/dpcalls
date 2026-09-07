// test_json.cpp — тесты JSON для C++ версии dpcalls.h
// Сборка: g++ -std=c++11 -Wall -Wextra -o test_json test_json.cpp

#include "dpcalls.h"
#include <cassert>
#include <iostream>

static dp::Json P(const std::string& s, const dp::JsonOptions& o = dp::JsonOptions())
{
    dp::JsonError e;
    dp::Json v = dp::json_parse(s, &e, o);
    if (!e.ok) { std::cerr << "неожиданная ошибка: " << e.what() << " для " << s << "\n"; assert(0); }
    return v;
}

static void bad(const std::string& s, const dp::JsonOptions& o = dp::JsonOptions())
{
    dp::JsonError e;
    dp::json_parse(s, &e, o);
    if (e.ok) { std::cerr << "должно было упасть: " << s << "\n"; assert(0); }
}

int main()
{
    dp::init_console();

    // ---- 1. скаляры ----
    assert(P("null").is_null());
    assert(P("true").as_bool() == true);
    assert(P("false").as_bool() == false);
    assert(P("42").is_int() && P("42").as_int() == 42);
    assert(P("-0").as_int() == 0);
    assert(P("3.5").as_double() == 3.5);
    assert(!P("3.5").is_int());
    assert(P("1e3").as_double() == 1000.0);
    assert(P("-1.5e-2").as_double() == -0.015);
    assert(P("\"текст\"").as_utf8() == "текст");

    // ---- 2. точные большие целые (не влезают в мантиссу double) ----
    {
        dp::Json v = P("9007199254740993");          // 2^53 + 1
        assert(v.is_int() && v.as_int() == 9007199254740993LL);
        assert(v.dump() == "9007199254740993");
        assert(P("9223372036854775807").as_int() == 9223372036854775807LL);
        assert(P("-9223372036854775808").as_int() == (-9223372036854775807LL - 1));
        // переполнение -> double, без падения
        assert(P("99999999999999999999").is_number());
        assert(!P("99999999999999999999").is_int());
    }

    // ---- 3. массивы и объекты ----
    {
        dp::Json a = P("[1, 2, [3, 4], {\"k\": 5}]");
        assert(a.is_array() && a.size() == 4);
        assert(a[0].as_int() == 1);
        assert(a[2].size() == 2 && a[2][1].as_int() == 4);
        assert(a[3]["k"].as_int() == 5);
        assert(a[99].is_null());          // выход за границы -> null, не UB

        dp::Json o = P("{\"a\":1,\"b\":\"два\",\"c\":[true,null]}");
        assert(o.is_object() && o.size() == 3);
        assert(o["a"].as_int() == 1);
        assert(o["b"].as_utf8() == "два");
        assert(o["c"][0].as_bool());
        assert(o["c"][1].is_null());
        assert(o["нет"].is_null());
        assert(o.has("a") && !o.has("нет"));

        std::vector<std::string> k = o.keys();
        assert(k.size() == 3 && k[0] == "a" && k[1] == "b" && k[2] == "c");
    }

    // ---- 4. escape-последовательности ----
    {
        assert(P("\"a\\nb\"").as_utf8() == "a\nb");
        assert(P("\"\\u0041\"").as_utf8() == "A");
        assert(P("\"\\u041F\\u0440\\u0438\"").as_utf8() == "При");
        assert(P("\"\\/\\\\\\\"\"").as_utf8() == "/\\\"");
        // суррогатная пара -> U+1F600
        dp::Json e = P("\"\\ud83d\\ude00\"");
        assert(e.as_utf8() == "\xF0\x9F\x98\x80");
        assert(dp::length(e.as_utf8()) == 1);
        // непарный суррогат -> U+FFFD, без падения
        assert(P("\"\\ud83d\"").raw_str().size() == 1);
        assert(P("\"\\ud83d\"").raw_str()[0] == dp::REPLACEMENT);
    }

    // ---- 5. строгость по RFC 8259 ----
    bad("");
    bad("{");
    bad("[1,]");
    bad("{\"a\":1,}");
    bad("{'a':1}");
    bad("[01]");
    bad("[1.]");
    bad("[.5]");
    bad("[+1]");
    bad("[1 2]");
    bad("nul");
    bad("[1]extra");
    bad("\"незакрытая");
    bad("{\"a\" 1}");
    bad("[\"\\x41\"]");
    bad("[\"a\tb\"]");             // сырой управляющий символ в строке

    // ---- 6. послабления для конфигов ----
    {
        dp::JsonOptions o = dp::JsonOptions::relaxed();
        dp::Json v = P("{\n // комментарий\n \"a\": 1, /* и такой */ \"b\": [1,2,],\n}", o);
        assert(v["a"].as_int() == 1);
        assert(v["b"].size() == 2);
        bad("{ /* незакрытый", o);
    }

    // ---- 7. защита от глубокой вложенности ----
    {
        std::string deep;
        for (int i = 0; i < 2000; ++i) deep += "[";
        for (int i = 0; i < 2000; ++i) deep += "]";
        dp::JsonError e;
        dp::json_parse(deep, &e);
        assert(!e.ok);                    // не переполнение стека, а нормальная ошибка

        dp::JsonOptions o;
        o.max_depth = 5000;
        dp::JsonError e2;
        dp::json_parse(deep, &e2, o);
        assert(e2.ok);
    }

    // ---- 8. позиция ошибки ----
    {
        dp::JsonError e;
        dp::json_parse(std::string("{\n  \"a\": 1,\n  \"b\": tru\n}"), &e);
        assert(!e.ok);
        assert(e.line == 3);
        std::cout << "ошибка       : " << e.what() << "\n";
    }

    // ---- 9. сериализация ----
    {
        dp::Json o = dp::Json::object();
        o.set("имя", dp::Json("Борис"));
        o.set("лет", dp::Json(30));
        o.set("дробь", dp::Json(0.5));
        o.set("флаг", dp::Json(true));
        o.set("пусто", dp::Json());
        dp::Json arr = dp::Json::array();
        arr.push_back(dp::Json(1));
        arr.push_back(dp::Json("два"));
        o.set("список", arr);

        std::string c = o.dump();
        assert(c.find("\"имя\":\"Борис\"") != std::string::npos);
        assert(c.find("\"лет\":30") != std::string::npos);
        assert(c.find("\"пусто\":null") != std::string::npos);

        // круговой прогон
        dp::Json back = P(c);
        assert(back["имя"].as_utf8() == "Борис");
        assert(back["лет"].as_int() == 30);
        assert(back["дробь"].as_double() == 0.5);
        assert(back["список"][1].as_utf8() == "два");
        assert(back.dump() == c);

        std::cout << "pretty       :\n" << o.dump(2) << "\n";

        // ensure_ascii
        std::string a = o.dump(0, true);
        for (std::size_t i = 0; i < a.size(); ++i)
            assert((unsigned char)a[i] < 0x80);
        assert(P(a)["имя"].as_utf8() == "Борис");
    }

    // ---- 10. круговой прогон double ----
    {
        double vals[] = { 0.1, 1.0/3.0, 1e-300, 1e300, -2.5e-17, 123456789.123456789 };
        for (int i = 0; i < 6; ++i) {
            dp::Json v(vals[i]);
            assert(P(v.dump()).as_double() == vals[i]);
        }
        // NaN и inf -> null, потому что в JSON их нет
        double zero = 0.0;
        assert(dp::Json(1.0 / zero).dump() == "null");
    }

    // ---- 11. независимость от локали ----
    {
        const char* loc = std::setlocale(LC_NUMERIC, "de_DE.UTF-8");
        if (!loc) loc = std::setlocale(LC_NUMERIC, "ru_RU.UTF-8");
        if (loc) {
            assert(P("1.5").as_double() == 1.5);
            assert(dp::Json(1.5).dump() == "1.5");
            std::cout << "локаль       : " << loc << " — разбор и печать не сломались\n";
        } else {
            std::cout << "локаль       : de/ru нет в системе, проверка пропущена\n";
        }
        std::setlocale(LC_NUMERIC, "C");
    }

    // ---- 12. любые типы символов на входе ----
    {
        std::wstring w = L"{\"ключ\": \"значение\"}";
        assert(dp::json_parse(w)["ключ"].as_wstr() == L"значение");
        std::u16string s16 = dp::convert<char16_t>(std::string("[1,2,3]"));
        assert(dp::json_parse(s16).size() == 3);
        std::u32string s32 = dp::convert<char32_t>(std::string("{\"a\":\"я\"}"));
        assert(dp::json_parse(s32)["a"].as_str<char32_t>().size() == 1);
    }

    // ---- 13. любые кодировки на входе и выходе ----
    {
        std::string src = "{\"город\":\"Москва\",\"код\":495}";
        std::string cp1251 = dp::recode(src, dp::cs_utf8(), dp::cs_cp1251());
        dp::JsonError e;
        dp::Json v = dp::json_parse_bytes(cp1251, dp::cs_cp1251(), &e);
        assert(e.ok);
        assert(v["город"].as_utf8() == "Москва");
        assert(v["код"].as_int() == 495);

        // UTF-16 с BOM — распознаётся автоматически
        std::string u16 = dp::encode(dp::conv<char>::to32(src), dp::cs_utf16le(), true);
        dp::Json v2 = dp::json_parse_bytes(u16, dp::Charset(dp::Charset::Auto), &e);
        assert(e.ok && v2["город"].as_utf8() == "Москва");
    }

    // ---- 14. файлы ----
    {
        dp::Json o = dp::Json::object();
        o.set("проект", dp::Json("dpcalls"));
        o.set("версия", dp::Json(2));

        assert(dp::json_save("cfg_utf8.json", o, dp::cs_utf8(), 2));
        assert(dp::json_save("cfg_1251.json", o, dp::cs_cp1251(), 2));

        dp::JsonError e;
        dp::Json a = dp::json_load("cfg_utf8.json", dp::Charset(dp::Charset::Auto), &e);
        assert(e.ok && a["проект"].as_utf8() == "dpcalls");

        dp::Json b = dp::json_load("cfg_1251.json", dp::cs_cp1251(), &e);
        assert(e.ok && b["проект"].as_utf8() == "dpcalls" && b["версия"].as_int() == 2);

        dp::json_load("нет-такого-файла.json", dp::Charset(dp::Charset::Auto), &e);
        assert(!e.ok);
    }

    // ---- 15. мутация ----
    {
        dp::Json o = P("{\"a\":1,\"b\":2}");
        o.set("a", dp::Json(10));                 // перезапись
        assert(o.size() == 2 && o["a"].as_int() == 10);
        assert(o.keys()[0] == "a");               // порядок сохранён
        assert(o.remove("b") && o.size() == 1);
        assert(!o.remove("b"));

        dp::Json a = dp::Json::array();
        for (int i = 0; i < 3; ++i) a.push_back(dp::Json(i));
        assert(a.dump() == "[0,1,2]");
    }

    // ---- 16. дубликаты ключей: побеждает последний ----
    {
        dp::Json v = P("{\"a\":1,\"a\":2}");
        assert(v["a"].as_int() == 2);
    }

    std::cout << "\nВСЕ JSON-ТЕСТЫ ПРОЙДЕНЫ (C++)\n";
    return 0;
}

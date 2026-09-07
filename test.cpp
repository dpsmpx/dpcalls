#include "dpcalls.h"
#include <cassert>
#include <iostream>

int main()
{
    dp::init_console();

    // ---- 1. reverse корректен для UTF-8 ----
    std::string s = "Привет, мир!";
    std::string r = dp::reverse(s);
    std::cout << "reverse utf8 : " << r << "\n";
    assert(dp::reverse(r) == s);
    assert(dp::length(s) == 12);
    std::cout << "length       : " << dp::length(s) << " (байт " << s.size() << ")\n";

    // ---- 2. то же самое в wchar_t ----
    std::wstring w = L"Привет, мир!";
    std::wstring wr = dp::reverse(w);
    assert(dp::convert<char>(wr) == r);
    assert(dp::length(w) == 12);

    // ---- 3. и в char16_t / char32_t ----
    std::u16string s16 = dp::convert<char16_t>(s);
    std::u32string s32 = dp::convert<char32_t>(s);
    assert(dp::length(s16) == 12);
    assert(dp::length(s32) == 12);
    assert(dp::convert<char>(dp::reverse(s16)) == r);
    assert(dp::convert<char>(dp::reverse(s32)) == r);

    // ---- 4. суррогатные пары (вне BMP) ----
    std::string emoji = "a\xF0\x9F\x98\x80z";        // a U+1F600 z
    assert(dp::length(emoji) == 3);
    assert(dp::convert<char>(dp::convert<char16_t>(emoji)) == emoji);
    assert(dp::convert<char>(dp::convert<wchar_t>(emoji)) == emoji);
    assert(dp::reverse(dp::reverse(emoji)) == emoji);
    std::cout << "emoji rev    : " << dp::reverse(emoji) << "\n";

    // ---- 5. однобайтовые кодировки ----
    std::string cp1251 = dp::encode(dp::conv<char>::to32(s), dp::cs_cp1251());
    std::cout << "cp1251 size  : " << cp1251.size() << " байт\n";
    assert(cp1251.size() == 12);
    assert(dp::u32_to_utf8(dp::decode(cp1251, dp::cs_cp1251())) == s);

    std::string cp866 = dp::recode(cp1251, dp::cs_cp1251(), dp::cs_cp866());
    assert(cp866.size() == 12);
    assert(dp::recode(cp866, dp::cs_cp866(), dp::cs_utf8()) == s);

    std::string koi = dp::recode(s, dp::cs_utf8(), dp::cs_koi8r());
    assert(dp::recode(koi, dp::cs_koi8r(), dp::cs_utf8()) == s);

    // ---- 6. файлы: запись cp1251, чтение обратно ----
    std::vector<std::string> lines;
    lines.push_back("Первая строка");
    lines.push_back("");
    lines.push_back("Третья строка");
    assert(dp::WriteAllLines("t1251.txt", lines, dp::cs_cp1251()));

    std::vector<std::string> back = dp::ReadAllLines("t1251.txt", dp::cs_cp1251());
    assert(back.size() == 3);
    assert(back[0] == lines[0] && back[1].empty() && back[2] == lines[2]);

    // ---- 7. UTF-16 с BOM, автоопределение ----
    assert(dp::WriteAllLines("t16.txt", lines, dp::cs_utf16le(), true));
    std::vector<std::string> auto16 = dp::ReadAllLines("t16.txt");   // Auto
    assert(auto16.size() == 3 && auto16[0] == lines[0] && auto16[2] == lines[2]);

    // ---- 8. UTF-32BE ----
    assert(dp::WriteAllLines("t32.txt", lines, dp::cs_utf32be(), true));
    std::vector<std::string> auto32 = dp::ReadAllLines("t32.txt");
    assert(auto32.size() == 3 && auto32[0] == lines[0]);

    // ---- 9. CRLF и последняя строка без \n ----
    dp::WriteAllBytes("t_crlf.txt", std::string("aa\r\nбб\r\nвв"));
    std::vector<std::string> crlf = dp::ReadAllLines("t_crlf.txt");
    assert(crlf.size() == 3 && crlf[0] == "aa" && crlf[1] == "бб" && crlf[2] == "вв");

    // ---- 10. words / split / join / trim ----
    std::vector<std::string> ws = dp::words(std::string("  раз   два\tтри  "));
    assert(ws.size() == 3 && ws[0] == "раз" && ws[1] == "два" && ws[2] == "три");

    std::vector<std::wstring> wws = dp::words(std::wstring(L"один два"));
    assert(wws.size() == 2 && wws[1] == L"два");

    std::vector<std::string> parts = dp::split(std::string("a;b;;c"), U';');
    assert(parts.size() == 4 && parts[2].empty());
    assert(dp::join(parts, std::string(";")) == "a;b;;c");
    assert(dp::trim(std::string("  тест \t")) == "тест");

    // ---- 11. itos / to_str / stoll_safe ----
    assert(dp::itos(-12345) == "-12345");
    assert(dp::itos(255, 16) == "ff");
    assert(dp::to_str<wchar_t>(1000) == L"1000");
    assert(dp::to_str<char32_t>(0) == U"0");
    long long v = 0;
    assert(dp::stoll_safe(std::string("  -42  "), v) && v == -42);
    assert(!dp::stoll_safe(std::string("42abc"), v));

    // ---- 12. регистр ----
    assert(dp::upper(std::string("привет ёж")) == "ПРИВЕТ ЁЖ");
    assert(dp::lower(std::string("ПРИВЕТ ЁЖ")) == "привет ёж");
    assert(dp::upper(std::wstring(L"abc-щи")) == L"ABC-ЩИ");

    // ---- 13. substr по кодовым точкам ----
    assert(dp::substr_cp(std::string("Привет"), 0, 3) == "При");
    assert(dp::substr_cp(std::string("Привет"), 3) == "вет");

    // ---- 14. битый UTF-8 не роняет программу ----
    std::string broken = "ab\xFF\xC3";
    std::u32string bu = dp::utf8_to_u32(broken);
    assert(bu.size() == 4 && bu[2] == dp::REPLACEMENT && bu[3] == dp::REPLACEMENT);

    // ---- 15. непредставимый символ в однобайтовой кодировке ----
    assert(dp::encode(U"a\u4E2Db", dp::cs_cp1251()) == "a?b");

    // ---- 16. rnd ----
    for (int i = 0; i < 1000; ++i) {
        int a = dp::rnd(10);
        int b = dp::rnd(-5, 5);
        assert(a >= 0 && a < 10);
        assert(b >= -5 && b <= 5);
    }
    bool varied = false;
    int first = dp::rnd(1000000);
    for (int i = 0; i < 10 && !varied; ++i) if (dp::rnd(1000000) != first) varied = true;
    assert(varied);   // старая версия с srand в теле возвращала одно и то же

    // ---- 17. печать в разных кодировках ----
    dp::print(lines);                       // UTF-8
    std::cout << "cp866 bytes  : " << dp::recode(s, dp::cs_utf8(), dp::cs_cp866()).size() << "\n";

    std::cout << "\nsizeof(wchar_t) = " << sizeof(wchar_t) << "\nВСЕ ТЕСТЫ ПРОЙДЕНЫ\n";
    return 0;
}

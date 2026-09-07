// dpcalls.h — универсальные утилиты для работы с текстом
// Требуется: C++11 или новее. Header-only, без внешних зависимостей.
//
// Модель работы:
//   файл на диске = БАЙТЫ + КОДИРОВКА (dp::Charset)
//   строка в программе = ЮНИКОД в представлении типа CharT
//     char     -> UTF-8
//     wchar_t  -> UTF-16 (Windows) или UTF-32 (Linux/Android) — определяется автоматически
//     char16_t -> UTF-16
//     char32_t -> UTF-32
//     char8_t  -> UTF-8 (только C++20)
//
// Внутренний канонический формат — std::u32string (по одному кодовой точке на элемент).
// Все операции, зависящие от символов (reverse, длина, регистр), идут через него.
//
// Совместимость со старым кодом: имена ReadAllLines / WriteAllLines / print /
// rnd / reverse / itos / words сохранены. В конце файла есть `using namespace dp;`,
// его можно отключить через #define DP_NO_GLOBAL_USING перед #include.

#ifndef DPCALLS_H
#define DPCALLS_H

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <random>
#include <ctime>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cwchar>
#include <clocale>
#include <cstdio>

#if defined(_WIN32) && !defined(DP_NO_WIN32_CONSOLE)
#  include <windows.h>
#endif

namespace dp {

// =====================================================================
// 0. Базовые определения
// =====================================================================

// Символ-заменитель для всего, что не удалось декодировать/закодировать.
const char32_t REPLACEMENT = 0xFFFDu;
// Байт-заменитель для однобайтовых кодировок.
const char UNMAPPABLE = '?';

// Приведение ASCII-литерала к любому типу символа.
// Работает потому, что все поддерживаемые кодировки совпадают с ASCII в диапазоне 0..127.
template <class CharT>
inline CharT chr(char c)
{
    return static_cast<CharT>(static_cast<unsigned char>(c));
}

// ASCII-строка в любом типе символа: dp::lit<wchar_t>("hello")
template <class CharT>
inline std::basic_string<CharT> lit(const char* s)
{
    std::basic_string<CharT> r;
    while (*s) r += chr<CharT>(*s++);
    return r;
}

inline bool is_ascii_space(char32_t c)
{
    return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D || c == 0x0B || c == 0x0C;
}

inline bool is_valid_codepoint(char32_t c)
{
    return c <= 0x10FFFFu && !(c >= 0xD800u && c <= 0xDFFFu);
}

// =====================================================================
// 1. UTF-8 / UTF-16 / UTF-32 — кодирование и декодирование
// =====================================================================

inline void utf8_append(char32_t cp, std::string& out)
{
    if (!is_valid_codepoint(cp)) cp = REPLACEMENT;
    if (cp < 0x80u) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800u) {
        out += static_cast<char>(0xC0u | (cp >> 6));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    } else if (cp < 0x10000u) {
        out += static_cast<char>(0xE0u | (cp >> 12));
        out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    } else {
        out += static_cast<char>(0xF0u | (cp >> 18));
        out += static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
        out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    }
}

// Возвращает количество съеденных байт (всегда >= 1). Битые последовательности -> U+FFFD.
inline std::size_t utf8_next(const std::string& s, std::size_t i, char32_t& cp)
{
    const std::size_t n = s.size();
    unsigned char b0 = static_cast<unsigned char>(s[i]);

    if (b0 < 0x80u) { cp = b0; return 1; }

    std::size_t need;
    char32_t v, minv;
    if ((b0 & 0xE0u) == 0xC0u)      { need = 1; v = b0 & 0x1Fu; minv = 0x80u; }
    else if ((b0 & 0xF0u) == 0xE0u) { need = 2; v = b0 & 0x0Fu; minv = 0x800u; }
    else if ((b0 & 0xF8u) == 0xF0u) { need = 3; v = b0 & 0x07u; minv = 0x10000u; }
    else                            { cp = REPLACEMENT; return 1; }

    if (i + need >= n) { cp = REPLACEMENT; return 1; }

    for (std::size_t k = 1; k <= need; ++k) {
        unsigned char b = static_cast<unsigned char>(s[i + k]);
        if ((b & 0xC0u) != 0x80u) { cp = REPLACEMENT; return 1; }
        v = (v << 6) | (b & 0x3Fu);
    }
    // overlong / surrogate / вне диапазона
    if (v < minv || !is_valid_codepoint(v)) { cp = REPLACEMENT; return 1; }

    cp = v;
    return need + 1;
}

inline std::u32string utf8_to_u32(const std::string& s)
{
    std::u32string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        char32_t cp;
        i += utf8_next(s, i, cp);
        out += cp;
    }
    return out;
}

inline std::string u32_to_utf8(const std::u32string& s)
{
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) utf8_append(s[i], out);
    return out;
}

// --- UTF-16 -----------------------------------------------------------

template <class U16>
inline void utf16_append(char32_t cp, std::basic_string<U16>& out)
{
    if (!is_valid_codepoint(cp)) cp = REPLACEMENT;
    if (cp < 0x10000u) {
        out += static_cast<U16>(cp);
    } else {
        cp -= 0x10000u;
        out += static_cast<U16>(0xD800u + (cp >> 10));
        out += static_cast<U16>(0xDC00u + (cp & 0x3FFu));
    }
}

template <class U16>
inline std::u32string utf16_to_u32(const std::basic_string<U16>& s)
{
    std::u32string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        char32_t u = static_cast<char32_t>(static_cast<unsigned long>(s[i]) & 0xFFFFu);
        if (u >= 0xD800u && u <= 0xDBFFu) {
            if (i + 1 < s.size()) {
                char32_t lo = static_cast<char32_t>(static_cast<unsigned long>(s[i + 1]) & 0xFFFFu);
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    out += 0x10000u + ((u - 0xD800u) << 10) + (lo - 0xDC00u);
                    ++i;
                    continue;
                }
            }
            out += REPLACEMENT;
        } else if (u >= 0xDC00u && u <= 0xDFFFu) {
            out += REPLACEMENT;          // одиночный low surrogate
        } else {
            out += u;
        }
    }
    return out;
}

// =====================================================================
// 2. Мост «любой тип символа <-> std::u32string»
// =====================================================================

template <class CharT> struct conv;   // первичный шаблон намеренно не определён

template <> struct conv<char> {
    static std::u32string to32(const std::string& s)   { return utf8_to_u32(s); }
    static std::string    from32(const std::u32string& s) { return u32_to_utf8(s); }
};

template <> struct conv<char16_t> {
    static std::u32string to32(const std::u16string& s) { return utf16_to_u32(s); }
    static std::u16string from32(const std::u32string& s)
    {
        std::u16string out;
        out.reserve(s.size());
        for (std::size_t i = 0; i < s.size(); ++i) utf16_append(s[i], out);
        return out;
    }
};

template <> struct conv<char32_t> {
    static std::u32string to32(const std::u32string& s)   { return s; }
    static std::u32string from32(const std::u32string& s) { return s; }
};

// wchar_t: 2 байта (Windows, UTF-16) или 4 байта (POSIX, UTF-32) — решается по sizeof.
template <> struct conv<wchar_t> {
    static std::u32string to32(const std::wstring& s)
    {
        if (sizeof(wchar_t) >= 4) {
            std::u32string out;
            out.reserve(s.size());
            for (std::size_t i = 0; i < s.size(); ++i) {
                char32_t c = static_cast<char32_t>(s[i]);
                out += is_valid_codepoint(c) ? c : REPLACEMENT;
            }
            return out;
        }
        return utf16_to_u32(s);
    }
    static std::wstring from32(const std::u32string& s)
    {
        std::wstring out;
        out.reserve(s.size());
        if (sizeof(wchar_t) >= 4) {
            for (std::size_t i = 0; i < s.size(); ++i) {
                char32_t c = s[i];
                out += static_cast<wchar_t>(is_valid_codepoint(c) ? c : REPLACEMENT);
            }
            return out;
        }
        for (std::size_t i = 0; i < s.size(); ++i) utf16_append(s[i], out);
        return out;
    }
};

#if defined(__cpp_char8_t)
template <> struct conv<char8_t> {
    static std::u32string to32(const std::u8string& s)
    {
        return utf8_to_u32(std::string(reinterpret_cast<const char*>(s.data()), s.size()));
    }
    static std::u8string from32(const std::u32string& s)
    {
        std::string t = u32_to_utf8(s);
        return std::u8string(reinterpret_cast<const char8_t*>(t.data()), t.size());
    }
};
#endif

// Универсальная перекодировка между любыми двумя типами символов.
template <class To, class From>
inline std::basic_string<To> convert(const std::basic_string<From>& s)
{
    return conv<To>::from32(conv<From>::to32(s));
}

// =====================================================================
// 3. Кодировки байтовых потоков
// =====================================================================

// --- Таблицы однобайтовых кодировок: байты 0x80..0xFF -> Unicode ---

inline const char32_t* table_cp1251()
{
    static const char32_t t[128] = {
        0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021, // 80
        0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F, // 88
        0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014, // 90
        0xFFFD,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F, // 98
        0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7, // A0
        0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407, // A8
        0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7, // B0
        0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457, // B8
        0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417, // C0
        0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F, // C8
        0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427, // D0
        0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F, // D8
        0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437, // E0
        0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F, // E8
        0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447, // F0
        0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F  // F8
    };
    return t;
}

inline const char32_t* table_cp866()
{
    static const char32_t t[128] = {
        0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417, // 80
        0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F, // 88
        0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427, // 90
        0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F, // 98
        0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437, // A0
        0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F, // A8
        0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556, // B0
        0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510, // B8
        0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F, // C0
        0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567, // C8
        0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B, // D0
        0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580, // D8
        0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447, // E0
        0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F, // E8
        0x0401,0x0451,0x0404,0x0454,0x0407,0x0457,0x040E,0x045E, // F0
        0x00B0,0x2219,0x00B7,0x221A,0x2116,0x00A4,0x25A0,0x00A0  // F8
    };
    return t;
}

inline const char32_t* table_koi8r()
{
    static const char32_t t[128] = {
        0x2500,0x2502,0x250C,0x2510,0x2514,0x2518,0x251C,0x2524, // 80
        0x252C,0x2534,0x253C,0x2580,0x2584,0x2588,0x258C,0x2590, // 88
        0x2591,0x2592,0x2593,0x2320,0x25A0,0x2219,0x221A,0x2248, // 90
        0x2264,0x2265,0x00A0,0x2321,0x00B0,0x00B2,0x00B7,0x00F7, // 98
        0x2550,0x2551,0x2552,0x0451,0x2553,0x2554,0x2555,0x2556, // A0
        0x2557,0x2558,0x2559,0x255A,0x255B,0x255C,0x255D,0x255E, // A8
        0x255F,0x2560,0x2561,0x0401,0x2562,0x2563,0x2564,0x2565, // B0
        0x2566,0x2567,0x2568,0x2569,0x256A,0x256B,0x256C,0x00A9, // B8
        0x044E,0x0430,0x0431,0x0446,0x0434,0x0435,0x0444,0x0433, // C0
        0x0445,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E, // C8
        0x043F,0x044F,0x0440,0x0441,0x0442,0x0443,0x0436,0x0432, // D0
        0x044C,0x044B,0x0437,0x0448,0x044D,0x0449,0x0447,0x044A, // D8
        0x042E,0x0410,0x0411,0x0426,0x0414,0x0415,0x0424,0x0413, // E0
        0x0425,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E, // E8
        0x041F,0x042F,0x0420,0x0421,0x0422,0x0423,0x0416,0x0412, // F0
        0x042C,0x042B,0x0417,0x0428,0x042D,0x0429,0x0427,0x042A  // F8
    };
    return t;
}

inline const char32_t* table_latin1()
{
    static char32_t t[128];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 128; ++i) t[i] = static_cast<char32_t>(0x80 + i);
        init = true;
    }
    return t;
}

struct Charset
{
    enum Kind {
        Auto,      // определить по BOM, иначе UTF-8
        Utf8,
        Utf16LE, Utf16BE,
        Utf32LE, Utf32BE,
        Sbcs,      // однобайтовая кодировка по таблице table
        Local      // системная локаль (setlocale/mbrtowc)
    };

    Kind kind;
    const char32_t* table;   // 128 элементов для Sbcs, иначе 0
    const char* name;

    Charset(Kind k = Auto, const char32_t* t = 0, const char* n = "auto")
        : kind(k), table(t), name(n) {}
};

inline Charset cs_auto()    { return Charset(Charset::Auto,    0, "auto"); }
inline Charset cs_utf8()    { return Charset(Charset::Utf8,    0, "utf-8"); }
inline Charset cs_utf16le() { return Charset(Charset::Utf16LE, 0, "utf-16le"); }
inline Charset cs_utf16be() { return Charset(Charset::Utf16BE, 0, "utf-16be"); }
inline Charset cs_utf32le() { return Charset(Charset::Utf32LE, 0, "utf-32le"); }
inline Charset cs_utf32be() { return Charset(Charset::Utf32BE, 0, "utf-32be"); }
inline Charset cs_cp1251()  { return Charset(Charset::Sbcs, table_cp1251(), "cp1251"); }
inline Charset cs_cp866()   { return Charset(Charset::Sbcs, table_cp866(),  "cp866"); }
inline Charset cs_koi8r()   { return Charset(Charset::Sbcs, table_koi8r(),  "koi8-r"); }
inline Charset cs_latin1()  { return Charset(Charset::Sbcs, table_latin1(), "latin-1"); }
inline Charset cs_local()   { return Charset(Charset::Local, 0, "locale"); }

// Своя однобайтовая кодировка: массив из 128 кодовых точек для байтов 0x80..0xFF.
inline Charset cs_custom(const char32_t* table128, const char* name = "custom")
{
    return Charset(Charset::Sbcs, table128, name);
}

// --- BOM ---------------------------------------------------------------

// Определяет кодировку по BOM. bom_len — длина найденного BOM в байтах.
inline Charset detect_bom(const std::string& b, std::size_t& bom_len)
{
    bom_len = 0;
    unsigned char c0 = b.size() > 0 ? static_cast<unsigned char>(b[0]) : 0;
    unsigned char c1 = b.size() > 1 ? static_cast<unsigned char>(b[1]) : 0;
    unsigned char c2 = b.size() > 2 ? static_cast<unsigned char>(b[2]) : 0;
    unsigned char c3 = b.size() > 3 ? static_cast<unsigned char>(b[3]) : 0;

    if (b.size() >= 4 && c0 == 0xFF && c1 == 0xFE && c2 == 0x00 && c3 == 0x00) { bom_len = 4; return cs_utf32le(); }
    if (b.size() >= 4 && c0 == 0x00 && c1 == 0x00 && c2 == 0xFE && c3 == 0xFF) { bom_len = 4; return cs_utf32be(); }
    if (b.size() >= 2 && c0 == 0xFF && c1 == 0xFE)                             { bom_len = 2; return cs_utf16le(); }
    if (b.size() >= 2 && c0 == 0xFE && c1 == 0xFF)                             { bom_len = 2; return cs_utf16be(); }
    if (b.size() >= 3 && c0 == 0xEF && c1 == 0xBB && c2 == 0xBF)               { bom_len = 3; return cs_utf8(); }
    return cs_utf8();
}

inline std::string bom_bytes(const Charset& cs)
{
    switch (cs.kind) {
        case Charset::Utf8:    return std::string("\xEF\xBB\xBF", 3);
        case Charset::Utf16LE: return std::string("\xFF\xFE", 2);
        case Charset::Utf16BE: return std::string("\xFE\xFF", 2);
        case Charset::Utf32LE: return std::string("\xFF\xFE\x00\x00", 4);
        case Charset::Utf32BE: return std::string("\x00\x00\xFE\xFF", 4);
        default: return std::string();
    }
}

// --- Байты -> Unicode --------------------------------------------------

inline std::u32string decode(const std::string& bytes, const Charset& cs_in)
{
    Charset cs = cs_in;
    std::size_t start = 0;

    if (cs.kind == Charset::Auto) {
        std::size_t bl = 0;
        cs = detect_bom(bytes, bl);
        start = bl;
    } else {
        // Явно заданная UTF-кодировка: BOM, если он есть, отбрасываем.
        std::size_t bl = 0;
        Charset guess = detect_bom(bytes, bl);
        if (bl > 0 && guess.kind == cs.kind) start = bl;
    }

    std::u32string out;

    switch (cs.kind) {
        case Charset::Utf8:
        case Charset::Auto: {
            std::size_t i = start;
            while (i < bytes.size()) {
                char32_t cp;
                i += utf8_next(bytes, i, cp);
                out += cp;
            }
            break;
        }
        case Charset::Utf16LE:
        case Charset::Utf16BE: {
            bool le = (cs.kind == Charset::Utf16LE);
            std::u16string tmp;
            for (std::size_t i = start; i + 1 < bytes.size(); i += 2) {
                unsigned lo = static_cast<unsigned char>(bytes[i]);
                unsigned hi = static_cast<unsigned char>(bytes[i + 1]);
                tmp += static_cast<char16_t>(le ? (lo | (hi << 8)) : (hi | (lo << 8)));
            }
            out = utf16_to_u32(tmp);
            break;
        }
        case Charset::Utf32LE:
        case Charset::Utf32BE: {
            bool le = (cs.kind == Charset::Utf32LE);
            for (std::size_t i = start; i + 3 < bytes.size(); i += 4) {
                unsigned b0 = static_cast<unsigned char>(bytes[i]);
                unsigned b1 = static_cast<unsigned char>(bytes[i + 1]);
                unsigned b2 = static_cast<unsigned char>(bytes[i + 2]);
                unsigned b3 = static_cast<unsigned char>(bytes[i + 3]);
                char32_t v = le ? (b0 | (b1 << 8) | (b2 << 16) | (static_cast<char32_t>(b3) << 24))
                                : (b3 | (b2 << 8) | (b1 << 16) | (static_cast<char32_t>(b0) << 24));
                out += is_valid_codepoint(v) ? v : REPLACEMENT;
            }
            break;
        }
        case Charset::Sbcs: {
            const char32_t* t = cs.table ? cs.table : table_latin1();
            out.reserve(bytes.size() - start);
            for (std::size_t i = start; i < bytes.size(); ++i) {
                unsigned char b = static_cast<unsigned char>(bytes[i]);
                out += (b < 0x80) ? static_cast<char32_t>(b) : t[b - 0x80];
            }
            break;
        }
        case Charset::Local: {
            std::mbstate_t st;
            std::memset(&st, 0, sizeof(st));
            const char* p = bytes.c_str() + start;
            std::size_t left = bytes.size() - start;
            while (left > 0) {
                wchar_t wc = 0;
                std::size_t n = std::mbrtowc(&wc, p, left, &st);
                if (n == static_cast<std::size_t>(-1) || n == static_cast<std::size_t>(-2)) {
                    out += REPLACEMENT;
                    ++p; --left;
                    std::memset(&st, 0, sizeof(st));
                    continue;
                }
                if (n == 0) { out += static_cast<char32_t>(0); n = 1; }
                else        { out += static_cast<char32_t>(wc); }
                p += n; left -= n;
            }
            // На Windows wchar_t = UTF-16, суррогаты нужно склеить.
            if (sizeof(wchar_t) < 4) {
                std::u16string tmp;
                for (std::size_t i = 0; i < out.size(); ++i)
                    tmp += static_cast<char16_t>(out[i] & 0xFFFFu);
                out = utf16_to_u32(tmp);
            }
            break;
        }
    }
    return out;
}

// --- Unicode -> байты --------------------------------------------------

inline std::string encode(const std::u32string& text, const Charset& cs_in, bool with_bom = false)
{
    Charset cs = cs_in;
    if (cs.kind == Charset::Auto) cs = cs_utf8();

    std::string out;
    if (with_bom) out += bom_bytes(cs);

    switch (cs.kind) {
        case Charset::Utf8:
        case Charset::Auto:
            for (std::size_t i = 0; i < text.size(); ++i) utf8_append(text[i], out);
            break;

        case Charset::Utf16LE:
        case Charset::Utf16BE: {
            bool le = (cs.kind == Charset::Utf16LE);
            std::u16string tmp;
            for (std::size_t i = 0; i < text.size(); ++i) utf16_append(text[i], tmp);
            for (std::size_t i = 0; i < tmp.size(); ++i) {
                unsigned v = static_cast<unsigned>(tmp[i]) & 0xFFFFu;
                if (le) { out += static_cast<char>(v & 0xFF); out += static_cast<char>((v >> 8) & 0xFF); }
                else    { out += static_cast<char>((v >> 8) & 0xFF); out += static_cast<char>(v & 0xFF); }
            }
            break;
        }
        case Charset::Utf32LE:
        case Charset::Utf32BE: {
            bool le = (cs.kind == Charset::Utf32LE);
            for (std::size_t i = 0; i < text.size(); ++i) {
                char32_t v = is_valid_codepoint(text[i]) ? text[i] : REPLACEMENT;
                char b[4];
                b[0] = static_cast<char>(v & 0xFF);
                b[1] = static_cast<char>((v >> 8) & 0xFF);
                b[2] = static_cast<char>((v >> 16) & 0xFF);
                b[3] = static_cast<char>((v >> 24) & 0xFF);
                if (le) { out += b[0]; out += b[1]; out += b[2]; out += b[3]; }
                else    { out += b[3]; out += b[2]; out += b[1]; out += b[0]; }
            }
            break;
        }
        case Charset::Sbcs: {
            const char32_t* t = cs.table ? cs.table : table_latin1();
            for (std::size_t i = 0; i < text.size(); ++i) {
                char32_t c = text[i];
                if (c < 0x80u) { out += static_cast<char>(c); continue; }
                char mapped = UNMAPPABLE;
                for (int k = 0; k < 128; ++k) {
                    if (t[k] == c) { mapped = static_cast<char>(0x80 + k); break; }
                }
                out += mapped;
            }
            break;
        }
        case Charset::Local: {
            std::mbstate_t st;
            std::memset(&st, 0, sizeof(st));
            char buf[MB_LEN_MAX + 8];
            std::u32string src = text;
            // На Windows wchar_t = UTF-16: разбиваем на суррогатные пары.
            if (sizeof(wchar_t) < 4) {
                std::u16string tmp;
                for (std::size_t i = 0; i < text.size(); ++i) utf16_append(text[i], tmp);
                src.clear();
                for (std::size_t i = 0; i < tmp.size(); ++i) src += static_cast<char32_t>(tmp[i]);
            }
            for (std::size_t i = 0; i < src.size(); ++i) {
                std::size_t n = std::wcrtomb(buf, static_cast<wchar_t>(src[i]), &st);
                if (n == static_cast<std::size_t>(-1)) {
                    out += UNMAPPABLE;
                    std::memset(&st, 0, sizeof(st));
                } else {
                    out.append(buf, n);
                }
            }
            break;
        }
    }
    return out;
}

// Перекодировка байт -> байты (например, cp1251-файл в utf8-файл).
inline std::string recode(const std::string& bytes, const Charset& from, const Charset& to, bool with_bom = false)
{
    return encode(decode(bytes, from), to, with_bom);
}

// =====================================================================
// 4. Операции над строками (любой тип символа)
// =====================================================================

// Длина в кодовых точках, а не в элементах строки.
template <class CharT>
inline std::size_t length(const std::basic_string<CharT>& s)
{
    return conv<CharT>::to32(s).size();
}

// Разворот строки. Корректен для многобайтовых кодировок (работает по кодовым точкам).
template <class CharT>
inline std::basic_string<CharT> reverse(const std::basic_string<CharT>& s)
{
    std::u32string u = conv<CharT>::to32(s);
    std::u32string r;
    r.reserve(u.size());
    for (std::size_t i = u.size(); i > 0; --i) r += u[i - 1];
    return conv<CharT>::from32(r);
}

inline std::string reverse(const char* s)     { return reverse(std::string(s)); }
inline std::wstring reverse(const wchar_t* s) { return reverse(std::wstring(s)); }

// Подстрока по кодовым точкам.
template <class CharT>
inline std::basic_string<CharT> substr_cp(const std::basic_string<CharT>& s,
                                          std::size_t pos,
                                          std::size_t count = std::basic_string<CharT>::npos)
{
    std::u32string u = conv<CharT>::to32(s);
    if (pos >= u.size()) return std::basic_string<CharT>();
    std::size_t n = (count == std::basic_string<CharT>::npos || pos + count > u.size())
                        ? u.size() - pos : count;
    return conv<CharT>::from32(u.substr(pos, n));
}

// Регистр: ASCII + основная кириллица. Для полного Юникода нужна ICU.
inline char32_t to_upper_cp(char32_t c)
{
    if (c >= 0x61 && c <= 0x7A) return c - 0x20;          // a-z
    if (c >= 0x430 && c <= 0x44F) return c - 0x20;        // а-я
    if (c >= 0x450 && c <= 0x45F) return c - 0x50;        // ё, ђ, ...
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 0x20; // latin-1 supplement
    return c;
}

inline char32_t to_lower_cp(char32_t c)
{
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;
    if (c >= 0x410 && c <= 0x42F) return c + 0x20;
    if (c >= 0x400 && c <= 0x40F) return c + 0x50;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 0x20;
    return c;
}

template <class CharT>
inline std::basic_string<CharT> upper(const std::basic_string<CharT>& s)
{
    std::u32string u = conv<CharT>::to32(s);
    for (std::size_t i = 0; i < u.size(); ++i) u[i] = to_upper_cp(u[i]);
    return conv<CharT>::from32(u);
}

template <class CharT>
inline std::basic_string<CharT> lower(const std::basic_string<CharT>& s)
{
    std::u32string u = conv<CharT>::to32(s);
    for (std::size_t i = 0; i < u.size(); ++i) u[i] = to_lower_cp(u[i]);
    return conv<CharT>::from32(u);
}

template <class CharT>
inline std::basic_string<CharT> trim(const std::basic_string<CharT>& s)
{
    std::u32string u = conv<CharT>::to32(s);
    std::size_t a = 0, b = u.size();
    while (a < b && is_ascii_space(u[a])) ++a;
    while (b > a && is_ascii_space(u[b - 1])) --b;
    return conv<CharT>::from32(u.substr(a, b - a));
}

// Разбиение по пробельным символам ASCII.
// keep_empty = false — подряд идущие разделители не порождают пустых слов.
template <class CharT>
inline std::vector<std::basic_string<CharT> > words(const std::basic_string<CharT>& line,
                                                    bool keep_empty = false)
{
    std::vector<std::basic_string<CharT> > result;
    std::u32string u = conv<CharT>::to32(line);
    std::u32string cur;
    for (std::size_t i = 0; i < u.size(); ++i) {
        if (is_ascii_space(u[i])) {
            if (!cur.empty() || keep_empty) result.push_back(conv<CharT>::from32(cur));
            cur.clear();
        } else {
            cur += u[i];
        }
    }
    if (!cur.empty() || keep_empty) result.push_back(conv<CharT>::from32(cur));
    return result;
}

inline std::vector<std::string> words(const char* line, bool keep_empty = false)
{
    return words(std::string(line), keep_empty);
}

// Разбиение по произвольному разделителю (кодовая точка).
template <class CharT>
inline std::vector<std::basic_string<CharT> > split(const std::basic_string<CharT>& line,
                                                    char32_t delim,
                                                    bool keep_empty = true)
{
    std::vector<std::basic_string<CharT> > result;
    std::u32string u = conv<CharT>::to32(line);
    std::u32string cur;
    for (std::size_t i = 0; i < u.size(); ++i) {
        if (u[i] == delim) {
            if (!cur.empty() || keep_empty) result.push_back(conv<CharT>::from32(cur));
            cur.clear();
        } else {
            cur += u[i];
        }
    }
    if (!cur.empty() || keep_empty) result.push_back(conv<CharT>::from32(cur));
    return result;
}

template <class CharT>
inline std::basic_string<CharT> join(const std::vector<std::basic_string<CharT> >& parts,
                                     const std::basic_string<CharT>& sep)
{
    std::basic_string<CharT> out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

// Число -> строка любого типа символа (std::to_string умеет только char/wchar_t).
template <class CharT>
inline std::basic_string<CharT> to_str(long long value, int base = 10)
{
    if (base < 2) base = 2;
    if (base > 36) base = 36;

    bool neg = (value < 0);
    unsigned long long v = neg ? (0ULL - static_cast<unsigned long long>(value))
                               : static_cast<unsigned long long>(value);
    char buf[80];
    int n = 0;
    do {
        int d = static_cast<int>(v % static_cast<unsigned long long>(base));
        buf[n++] = static_cast<char>(d < 10 ? ('0' + d) : ('a' + d - 10));
        v /= static_cast<unsigned long long>(base);
    } while (v != 0);
    if (neg) buf[n++] = '-';

    std::basic_string<CharT> out;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = n - 1; i >= 0; --i) out += chr<CharT>(buf[i]);
    return out;
}

inline std::string  itos(long long v, int base = 10) { return to_str<char>(v, base); }
inline std::wstring itow(long long v, int base = 10) { return to_str<wchar_t>(v, base); }

// Строка -> число. Возвращает false, если строка не является числом целиком.
template <class CharT>
inline bool stoll_safe(const std::basic_string<CharT>& s, long long& out, int base = 10)
{
    std::u32string u = conv<CharT>::to32(s);
    std::size_t i = 0;
    while (i < u.size() && is_ascii_space(u[i])) ++i;
    bool neg = false;
    if (i < u.size() && (u[i] == '+' || u[i] == '-')) { neg = (u[i] == '-'); ++i; }
    if (i >= u.size()) return false;

    long long acc = 0;
    bool any = false;
    for (; i < u.size(); ++i) {
        char32_t c = u[i];
        int d;
        if (c >= '0' && c <= '9')      d = static_cast<int>(c - '0');
        else if (c >= 'a' && c <= 'z') d = static_cast<int>(c - 'a') + 10;
        else if (c >= 'A' && c <= 'Z') d = static_cast<int>(c - 'A') + 10;
        else break;
        if (d >= base) break;
        acc = acc * base + d;
        any = true;
    }
    while (i < u.size() && is_ascii_space(u[i])) ++i;
    if (!any || i != u.size()) return false;
    out = neg ? -acc : acc;
    return true;
}

// =====================================================================
// 5. Файлы
// =====================================================================

inline std::string ReadAllBytes(const std::string& path, bool* ok = 0)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) { if (ok) *ok = false; return std::string(); }
    std::ostringstream ss;
    ss << f.rdbuf();
    if (ok) *ok = true;
    return ss.str();
}

inline bool WriteAllBytes(const std::string& path, const std::string& bytes, bool append = false)
{
    std::ios::openmode m = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
    std::ofstream f(path.c_str(), m);
    if (!f) return false;
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return f.good();
}

// Разбиение текста на строки: \n, \r\n и одиночный \r.
inline std::vector<std::u32string> split_lines_u32(const std::u32string& text)
{
    std::vector<std::u32string> lines;
    std::u32string cur;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char32_t c = text[i];
        if (c == U'\r') {
            if (i + 1 < text.size() && text[i + 1] == U'\n') ++i;
            lines.push_back(cur);
            cur.clear();
        } else if (c == U'\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) lines.push_back(cur);   // последняя строка без перевода строки
    return lines;
}

// Чтение файла в строки указанного типа символа.
// Кодировка по умолчанию — Auto (BOM, иначе UTF-8).
template <class CharT>
inline std::vector<std::basic_string<CharT> > ReadLines(const std::string& path,
                                                        const Charset& cs = Charset(Charset::Auto),
                                                        bool* ok = 0)
{
    std::string bytes = ReadAllBytes(path, ok);
    std::vector<std::u32string> u = split_lines_u32(decode(bytes, cs));
    std::vector<std::basic_string<CharT> > result;
    result.reserve(u.size());
    for (std::size_t i = 0; i < u.size(); ++i)
        result.push_back(conv<CharT>::from32(u[i]));
    return result;
}

template <class CharT>
inline bool WriteLines(const std::string& path,
                       const std::vector<std::basic_string<CharT> >& lines,
                       const Charset& cs = Charset(Charset::Utf8),
                       bool with_bom = false,
                       const char* eol = "\n")
{
    std::u32string text;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        text += conv<CharT>::to32(lines[i]);
        for (const char* p = eol; *p; ++p) text += static_cast<char32_t>(*p);
    }
    return WriteAllBytes(path, encode(text, cs, with_bom));
}

// Весь файл одной строкой.
template <class CharT>
inline std::basic_string<CharT> ReadAllText(const std::string& path,
                                            const Charset& cs = Charset(Charset::Auto),
                                            bool* ok = 0)
{
    return conv<CharT>::from32(decode(ReadAllBytes(path, ok), cs));
}

template <class CharT>
inline bool WriteAllText(const std::string& path,
                         const std::basic_string<CharT>& text,
                         const Charset& cs = Charset(Charset::Utf8),
                         bool with_bom = false)
{
    return WriteAllBytes(path, encode(conv<CharT>::to32(text), cs, with_bom));
}

// --- Совместимые обёртки со старыми именами ---------------------------

inline std::vector<std::string> ReadAllLines(const std::string& path,
                                             const Charset& cs = Charset(Charset::Auto))
{
    return ReadLines<char>(path, cs);
}

inline std::vector<std::wstring> ReadAllLinesW(const std::string& path,
                                               const Charset& cs = Charset(Charset::Auto))
{
    return ReadLines<wchar_t>(path, cs);
}

inline bool WriteAllLines(const std::string& path,
                          const std::vector<std::string>& lines,
                          const Charset& cs = Charset(Charset::Utf8),
                          bool with_bom = false)
{
    return WriteLines<char>(path, lines, cs, with_bom);
}

inline bool WriteAllLines(const std::string& path,
                          const std::vector<std::wstring>& lines,
                          const Charset& cs = Charset(Charset::Utf8),
                          bool with_bom = false)
{
    return WriteLines<wchar_t>(path, lines, cs, with_bom);
}

// =====================================================================
// 6. Вывод в консоль
// =====================================================================

// Настройка локали и кодовой страницы консоли. Вызывать один раз в начале main().
inline void init_console()
{
    std::setlocale(LC_ALL, "");
#if defined(_WIN32) && !defined(DP_NO_WIN32_CONSOLE)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

// Печать вектора строк любого типа символа. Байты выводятся в кодировке cs.
template <class CharT>
inline void print(const std::vector<std::basic_string<CharT> >& lines,
                  const Charset& cs = Charset(Charset::Utf8))
{
    std::string bytes;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        bytes += encode(conv<CharT>::to32(lines[i]), cs, false);
        bytes += '\n';
    }
    std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    std::cout.flush();
}

template <class CharT>
inline void print(const std::basic_string<CharT>& s,
                  const Charset& cs = Charset(Charset::Utf8))
{
    std::string bytes = encode(conv<CharT>::to32(s), cs, false);
    bytes += '\n';
    std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    std::cout.flush();
}

inline void print(const char* s, const Charset& cs = Charset(Charset::Utf8))
{
    print(std::string(s), cs);
}

inline void print(const wchar_t* s, const Charset& cs = Charset(Charset::Utf8))
{
    print(std::wstring(s), cs);
}

// Чтение строки со стандартного ввода в нужном типе символа.
template <class CharT>
inline bool input_line(std::basic_string<CharT>& out,
                       const Charset& cs = Charset(Charset::Utf8))
{
    std::string raw;
    if (!std::getline(std::cin, raw)) return false;
    if (!raw.empty() && raw[raw.size() - 1] == '\r') raw.erase(raw.size() - 1);
    out = conv<CharT>::from32(decode(raw, cs));
    return true;
}

// =====================================================================
// 7. Случайные числа
// =====================================================================

inline std::mt19937& rng()
{
    static std::mt19937 gen(static_cast<unsigned>(std::time(0)) ^
                            static_cast<unsigned>(reinterpret_cast<std::size_t>(&gen)));
    return gen;
}

inline void rnd_seed(unsigned s) { rng().seed(s); }

// [0, max-1] — как в старой версии.
inline int rnd(int max)
{
    if (max <= 0) return 0;
    std::uniform_int_distribution<int> d(0, max - 1);
    return d(rng());
}

// [min, max] включительно.
inline int rnd(int min, int max)
{
    if (min > max) { int t = min; min = max; max = t; }
    std::uniform_int_distribution<int> d(min, max);
    return d(rng());
}

inline double rndf()
{
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(rng());
}

// =====================================================================
// 8. JSON
// =====================================================================
//
// Разбор идёт по кодовым точкам (std::u32string), поэтому парсер не зависит
// от кодировки исходных байт: json_parse_bytes() сначала декодирует вход
// через dp::Charset, а дальше работает единый код. Escape-последовательности
// \uXXXX с суррогатными парами собираются в настоящие кодовые точки.
//
// Числа: JSON не различает целые и дробные, но Json запоминает, было ли
// значение записано как целое, и хранит точное long long. Это важно для
// идентификаторов, которые не влезают в мантиссу double.
//
// Разбор и печать чисел не зависят от локали: точка приводится к
// разделителю текущей локали только на время вызова strtod/snprintf.

enum JsonType {
    JSON_NULL = 0,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
};

// --- Числа, независимые от локали ------------------------------------

inline char json_locale_point()
{
    const std::lconv* lc = std::localeconv();
    return (lc && lc->decimal_point && lc->decimal_point[0]) ? lc->decimal_point[0] : '.';
}

// Разбор ASCII-записи числа с точкой в качестве разделителя.
inline double json_strtod(const std::string& s)
{
    char pt = json_locale_point();
    if (pt == '.') return std::strtod(s.c_str(), 0);
    std::string tmp(s);
    for (std::size_t i = 0; i < tmp.size(); ++i)
        if (tmp[i] == '.') tmp[i] = pt;
    return std::strtod(tmp.c_str(), 0);
}

// Печать double так, чтобы обратный разбор дал ровно то же значение.
inline std::string json_dtoa(double v)
{
    if (v != v) return "null";                       // NaN
    if (v > 1.7976931348623157e308) return "null";   // +inf
    if (v < -1.7976931348623157e308) return "null";  // -inf

    char buf[64];
    char pt = json_locale_point();
    for (int prec = 15; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
        if (pt != '.') {
            for (char* p = buf; *p; ++p) if (*p == pt) *p = '.';
        }
        if (json_strtod(std::string(buf)) == v) break;
    }
    return std::string(buf);
}

// --- Настройки и ошибки ----------------------------------------------

struct JsonOptions {
    bool allow_comments;          // // ... и /* ... */
    bool allow_trailing_commas;   // [1,2,]  {"a":1,}
    std::size_t max_depth;        // защита от переполнения стека

    JsonOptions()
        : allow_comments(false), allow_trailing_commas(false), max_depth(200) {}

    // Готовый набор послаблений для конфигов.
    static JsonOptions relaxed()
    {
        JsonOptions o;
        o.allow_comments = true;
        o.allow_trailing_commas = true;
        return o;
    }
};

struct JsonError {
    bool ok;
    std::string message;
    std::size_t line;     // с 1
    std::size_t column;   // с 1, в кодовых точках
    std::size_t offset;   // с 0, в кодовых точках

    JsonError() : ok(true), line(1), column(1), offset(0) {}

    std::string what() const
    {
        if (ok) return std::string();
        std::ostringstream ss;
        ss << "строка " << line << ", столбец " << column << ": " << message;
        return ss.str();
    }
};

// --- Значение ---------------------------------------------------------

class Json {
public:
    Json() : t_(JSON_NULL), b_(false), d_(0.0), i_(0), isint_(false) {}
    Json(bool v) : t_(JSON_BOOL), b_(v), d_(v ? 1.0 : 0.0), i_(v ? 1 : 0), isint_(false) {}
    Json(int v) : t_(JSON_NUMBER), b_(false), d_((double)v), i_(v), isint_(true) {}
    Json(long long v) : t_(JSON_NUMBER), b_(false), d_((double)v), i_(v), isint_(true) {}
    Json(double v) : t_(JSON_NUMBER), b_(false), d_(v), i_((long long)v), isint_(false) {}

    Json(const std::string& v)  : t_(JSON_STRING), b_(false), d_(0), i_(0), isint_(false), s_(conv<char>::to32(v)) {}
    Json(const char* v)         : t_(JSON_STRING), b_(false), d_(0), i_(0), isint_(false), s_(conv<char>::to32(std::string(v ? v : ""))) {}
    Json(const std::wstring& v) : t_(JSON_STRING), b_(false), d_(0), i_(0), isint_(false), s_(conv<wchar_t>::to32(v)) {}
    Json(const wchar_t* v)      : t_(JSON_STRING), b_(false), d_(0), i_(0), isint_(false), s_(conv<wchar_t>::to32(std::wstring(v ? v : L""))) {}
    Json(const std::u16string& v) : t_(JSON_STRING), b_(false), d_(0), i_(0), isint_(false), s_(conv<char16_t>::to32(v)) {}
    Json(const std::u32string& v) : t_(JSON_STRING), b_(false), d_(0), i_(0), isint_(false), s_(v) {}

    static Json array()  { Json j; j.t_ = JSON_ARRAY;  return j; }
    static Json object() { Json j; j.t_ = JSON_OBJECT; return j; }

    // Строка из любого типа символа.
    template <class CharT>
    static Json str(const std::basic_string<CharT>& v)
    {
        Json j;
        j.t_ = JSON_STRING;
        j.s_ = conv<CharT>::to32(v);
        return j;
    }

    JsonType type() const { return t_; }

    bool is_null()   const { return t_ == JSON_NULL; }
    bool is_bool()   const { return t_ == JSON_BOOL; }
    bool is_number() const { return t_ == JSON_NUMBER; }
    bool is_int()    const { return t_ == JSON_NUMBER && isint_; }
    bool is_string() const { return t_ == JSON_STRING; }
    bool is_array()  const { return t_ == JSON_ARRAY; }
    bool is_object() const { return t_ == JSON_OBJECT; }

    bool as_bool(bool def = false) const
    {
        if (t_ == JSON_BOOL) return b_;
        if (t_ == JSON_NUMBER) return d_ != 0.0;
        return def;
    }
    double as_double(double def = 0.0) const
    {
        if (t_ == JSON_NUMBER) return isint_ ? (double)i_ : d_;
        if (t_ == JSON_BOOL) return b_ ? 1.0 : 0.0;
        return def;
    }
    long long as_int(long long def = 0) const
    {
        if (t_ == JSON_NUMBER) return isint_ ? i_ : (long long)d_;
        if (t_ == JSON_BOOL) return b_ ? 1 : 0;
        return def;
    }

    template <class CharT>
    std::basic_string<CharT> as_str() const { return conv<CharT>::from32(s_); }

    std::string    as_utf8() const { return conv<char>::from32(s_); }
    std::wstring   as_wstr() const { return conv<wchar_t>::from32(s_); }
    const std::u32string& raw_str() const { return s_; }

    // Размер массива или объекта; для остальных типов 0.
    std::size_t size() const
    {
        if (t_ == JSON_ARRAY)  return arr_.size();
        if (t_ == JSON_OBJECT) return obj_.size();
        return 0;
    }
    bool empty() const { return size() == 0; }

    // --- массив ---
    const Json& at(std::size_t i) const
    {
        if (t_ == JSON_ARRAY && i < arr_.size()) return arr_[i];
        return null_ref();
    }
    const Json& operator[](std::size_t i) const { return at(i); }

    void push_back(const Json& v)
    {
        if (t_ != JSON_ARRAY) { *this = array(); }
        arr_.push_back(v);
    }

    // Добавляет в массив пустой элемент и возвращает ссылку на него.
    // Нужно, чтобы строить дерево на месте, без копирования поддеревьев:
    // разбор больших вложенных документов иначе получается квадратичным.
    Json& emplace_back()
    {
        if (t_ != JSON_ARRAY) { *this = array(); }
        arr_.push_back(Json());
        return arr_.back();
    }

    // --- объект ---
    // Ключи хранятся в порядке вставки. При повторе ключа побеждает последний,
    // как в JSON.parse; сам порядок при этом не меняется.
    bool has(const std::u32string& key) const { return find(key) != (std::size_t)-1; }
    bool has(const std::string& key) const    { return has(conv<char>::to32(key)); }
    bool has(const char* key) const           { return has(std::string(key ? key : "")); }

    const Json& at(const std::u32string& key) const
    {
        std::size_t k = find(key);
        return (k == (std::size_t)-1) ? null_ref() : obj_[k].second;
    }
    const Json& at(const std::string& key) const { return at(conv<char>::to32(key)); }

    // Перегрузок под const char* здесь намеренно нет: они делали бы j[0]
    // неоднозначным (0 — валидный нулевой указатель). Строковый литерал
    // доходит сюда через неявное построение std::string.
    const Json& operator[](const std::u32string& key) const { return at(key); }
    const Json& operator[](const std::string& key) const    { return at(key); }

    void set(const std::u32string& key, const Json& v)
    {
        if (t_ != JSON_OBJECT) { *this = object(); }
        std::size_t k = find(key);
        if (k == (std::size_t)-1) obj_.push_back(std::make_pair(key, v));
        else                      obj_[k].second = v;
    }
    void set(const std::string& key, const Json& v) { set(conv<char>::to32(key), v); }
    void set(const char* key, const Json& v)        { set(std::string(key ? key : ""), v); }

    // Ссылка на значение ключа, создавая его при необходимости. См. emplace_back().
    Json& emplace(const std::u32string& key)
    {
        if (t_ != JSON_OBJECT) { *this = object(); }
        std::size_t k = find(key);
        if (k != (std::size_t)-1) return obj_[k].second;
        obj_.push_back(std::make_pair(key, Json()));
        return obj_.back().second;
    }

    bool remove(const std::u32string& key)
    {
        std::size_t k = find(key);
        if (k == (std::size_t)-1) return false;
        obj_.erase(obj_.begin() + (std::ptrdiff_t)k);
        return true;
    }
    bool remove(const std::string& key) { return remove(conv<char>::to32(key)); }

    // Ключ по индексу — для обхода объекта в порядке вставки.
    template <class CharT>
    std::basic_string<CharT> key_at(std::size_t i) const
    {
        if (t_ == JSON_OBJECT && i < obj_.size()) return conv<CharT>::from32(obj_[i].first);
        return std::basic_string<CharT>();
    }
    std::string key_utf8(std::size_t i) const { return key_at<char>(i); }

    const Json& value_at(std::size_t i) const
    {
        if (t_ == JSON_OBJECT && i < obj_.size()) return obj_[i].second;
        return at(i);
    }

    std::vector<std::string> keys() const
    {
        std::vector<std::string> r;
        for (std::size_t i = 0; i < obj_.size(); ++i)
            r.push_back(conv<char>::from32(obj_[i].first));
        return r;
    }

    // --- сериализация ---
    // indent = 0 — компактно, > 0 — с отступами.
    // ensure_ascii — экранировать всё вне ASCII как \uXXXX.
    std::u32string dump_u32(int indent = 0, bool ensure_ascii = false) const
    {
        std::u32string out;
        write(out, indent, ensure_ascii, 0);
        return out;
    }
    std::string dump(int indent = 0, bool ensure_ascii = false) const
    {
        return conv<char>::from32(dump_u32(indent, ensure_ascii));
    }

    static const Json& null_ref()
    {
        static const Json n;
        return n;
    }

private:
    JsonType       t_;
    bool           b_;
    double         d_;
    long long      i_;
    bool           isint_;
    std::u32string s_;
    std::vector<Json> arr_;
    std::vector<std::pair<std::u32string, Json> > obj_;

    std::size_t find(const std::u32string& key) const
    {
        if (t_ != JSON_OBJECT) return (std::size_t)-1;
        for (std::size_t i = obj_.size(); i > 0; --i)   // последний побеждает
            if (obj_[i - 1].first == key) return i - 1;
        return (std::size_t)-1;
    }

    static void put_ascii(std::u32string& out, const char* s)
    {
        while (*s) out += static_cast<char32_t>(static_cast<unsigned char>(*s++));
    }

    static void put_hex4(std::u32string& out, unsigned v)
    {
        const char* hex = "0123456789abcdef";
        out += static_cast<char32_t>(hex[(v >> 12) & 0xF]);
        out += static_cast<char32_t>(hex[(v >> 8) & 0xF]);
        out += static_cast<char32_t>(hex[(v >> 4) & 0xF]);
        out += static_cast<char32_t>(hex[v & 0xF]);
    }

    static void write_string(std::u32string& out, const std::u32string& s, bool ensure_ascii)
    {
        out += U'"';
        for (std::size_t i = 0; i < s.size(); ++i) {
            char32_t c = s[i];
            switch (c) {
                case U'"':  put_ascii(out, "\\\""); continue;
                case U'\\': put_ascii(out, "\\\\"); continue;
                case 0x08:  put_ascii(out, "\\b");  continue;
                case 0x0C:  put_ascii(out, "\\f");  continue;
                case 0x0A:  put_ascii(out, "\\n");  continue;
                case 0x0D:  put_ascii(out, "\\r");  continue;
                case 0x09:  put_ascii(out, "\\t");  continue;
                default: break;
            }
            if (c < 0x20) {
                put_ascii(out, "\\u");
                put_hex4(out, (unsigned)c);
            } else if (ensure_ascii && c > 0x7E) {
                if (c <= 0xFFFF) {
                    put_ascii(out, "\\u");
                    put_hex4(out, (unsigned)c);
                } else {
                    char32_t v = c - 0x10000;
                    put_ascii(out, "\\u");
                    put_hex4(out, (unsigned)(0xD800 + (v >> 10)));
                    put_ascii(out, "\\u");
                    put_hex4(out, (unsigned)(0xDC00 + (v & 0x3FF)));
                }
            } else {
                out += c;
            }
        }
        out += U'"';
    }

    static void newline_indent(std::u32string& out, int indent, int level)
    {
        if (indent <= 0) return;
        out += U'\n';
        for (int i = 0; i < indent * level; ++i) out += U' ';
    }

    void write(std::u32string& out, int indent, bool ensure_ascii, int level) const
    {
        switch (t_) {
            case JSON_NULL: put_ascii(out, "null"); break;
            case JSON_BOOL: put_ascii(out, b_ ? "true" : "false"); break;

            case JSON_NUMBER:
                if (isint_) {
                    std::basic_string<char32_t> n = to_str<char32_t>(i_);
                    out += n;
                } else {
                    put_ascii(out, json_dtoa(d_).c_str());
                }
                break;

            case JSON_STRING:
                write_string(out, s_, ensure_ascii);
                break;

            case JSON_ARRAY: {
                if (arr_.empty()) { put_ascii(out, "[]"); break; }
                out += U'[';
                for (std::size_t i = 0; i < arr_.size(); ++i) {
                    if (i) out += U',';
                    newline_indent(out, indent, level + 1);
                    arr_[i].write(out, indent, ensure_ascii, level + 1);
                }
                newline_indent(out, indent, level);
                out += U']';
                break;
            }
            case JSON_OBJECT: {
                if (obj_.empty()) { put_ascii(out, "{}"); break; }
                out += U'{';
                for (std::size_t i = 0; i < obj_.size(); ++i) {
                    if (i) out += U',';
                    newline_indent(out, indent, level + 1);
                    write_string(out, obj_[i].first, ensure_ascii);
                    out += U':';
                    if (indent > 0) out += U' ';
                    obj_[i].second.write(out, indent, ensure_ascii, level + 1);
                }
                newline_indent(out, indent, level);
                out += U'}';
                break;
            }
        }
    }
};

// --- Парсер -----------------------------------------------------------

namespace detail {

class JsonParser {
public:
    JsonParser(const std::u32string& text, const JsonOptions& opt, JsonError& err)
        : s_(text), i_(0), line_(1), col_(1), opt_(opt), err_(err) {}

    Json run()
    {
        Json v;
        skip_ws();
        parse_value(v, 0);
        if (!err_.ok) return Json();
        skip_ws();
        if (i_ < s_.size()) {
            fail("лишние данные после значения");
            return Json();
        }
        return v;
    }

private:
    const std::u32string& s_;
    std::size_t i_, line_, col_;
    const JsonOptions& opt_;
    JsonError& err_;

    bool eof() const { return i_ >= s_.size(); }
    char32_t cur() const { return i_ < s_.size() ? s_[i_] : 0; }
    char32_t peek(std::size_t k) const { return i_ + k < s_.size() ? s_[i_ + k] : 0; }

    void advance()
    {
        if (i_ >= s_.size()) return;
        if (s_[i_] == U'\n') { ++line_; col_ = 1; }
        else ++col_;
        ++i_;
    }

    void fail(const char* msg)
    {
        if (!err_.ok) return;
        err_.ok = false;
        err_.message = msg;
        err_.line = line_;
        err_.column = col_;
        err_.offset = i_;
    }

    void skip_ws()
    {
        for (;;) {
            while (!eof() && (cur() == U' ' || cur() == U'\t' || cur() == U'\n' || cur() == U'\r'))
                advance();
            if (!opt_.allow_comments || eof() || cur() != U'/') return;
            if (peek(1) == U'/') {
                while (!eof() && cur() != U'\n') advance();
            } else if (peek(1) == U'*') {
                advance(); advance();
                for (;;) {
                    if (eof()) { fail("незакрытый комментарий"); return; }
                    if (cur() == U'*' && peek(1) == U'/') { advance(); advance(); break; }
                    advance();
                }
            } else {
                return;
            }
        }
    }

    bool literal(const char* word)
    {
        std::size_t k = 0;
        while (word[k]) {
            if (peek(k) != static_cast<char32_t>(static_cast<unsigned char>(word[k]))) return false;
            ++k;
        }
        for (std::size_t j = 0; j < k; ++j) advance();
        return true;
    }

    // Значение строится прямо в out — никаких копий поддеревьев.
    void parse_value(Json& out, std::size_t depth)
    {
        if (!err_.ok) return;
        if (depth > opt_.max_depth) { fail("превышена максимальная вложенность"); return; }
        if (eof()) { fail("неожиданный конец данных"); return; }

        switch (cur()) {
            case U'{': parse_object(out, depth); return;
            case U'[': parse_array(out, depth);  return;
            case U'"': {
                std::u32string str;
                if (!parse_string(str)) return;
                out = Json(str);
                return;
            }
            case U't': if (literal("true"))  { out = Json(true);  return; } fail("ожидалось true");  return;
            case U'f': if (literal("false")) { out = Json(false); return; } fail("ожидалось false"); return;
            case U'n': if (literal("null"))  { out = Json();      return; } fail("ожидалось null");  return;
            default: break;
        }
        if (cur() == U'-' || (cur() >= U'0' && cur() <= U'9')) { parse_number(out); return; }
        fail("ожидалось значение");
    }

    bool parse_string(std::u32string& out)
    {
        out.clear();
        if (cur() != U'"') { fail("ожидалась строка"); return false; }
        advance();
        for (;;) {
            if (eof()) { fail("незакрытая строка"); return false; }
            char32_t c = cur();
            if (c == U'"') { advance(); return true; }
            if (c < 0x20) { fail("управляющий символ в строке"); return false; }
            if (c != U'\\') { out += c; advance(); continue; }

            advance();                       // \.
            if (eof()) { fail("незакрытая escape-последовательность"); return false; }
            char32_t e = cur();
            switch (e) {
                case U'"':  out += U'"';  advance(); break;
                case U'\\': out += U'\\'; advance(); break;
                case U'/':  out += U'/';  advance(); break;
                case U'b':  out += (char32_t)0x08; advance(); break;
                case U'f':  out += (char32_t)0x0C; advance(); break;
                case U'n':  out += (char32_t)0x0A; advance(); break;
                case U'r':  out += (char32_t)0x0D; advance(); break;
                case U't':  out += (char32_t)0x09; advance(); break;
                case U'u': {
                    advance();
                    unsigned hi;
                    if (!hex4(hi)) return false;
                    if (hi >= 0xD800 && hi <= 0xDBFF) {
                        if (cur() == U'\\' && peek(1) == U'u') {
                            std::size_t save_i = i_, save_l = line_, save_c = col_;
                            advance(); advance();
                            unsigned lo;
                            if (!hex4(lo)) return false;
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                out += (char32_t)(0x10000u + ((hi - 0xD800u) << 10) + (lo - 0xDC00u));
                                break;
                            }
                            i_ = save_i; line_ = save_l; col_ = save_c;
                        }
                        out += REPLACEMENT;          // непарный high surrogate
                    } else if (hi >= 0xDC00 && hi <= 0xDFFF) {
                        out += REPLACEMENT;          // непарный low surrogate
                    } else {
                        out += (char32_t)hi;
                    }
                    break;
                }
                default:
                    fail("недопустимая escape-последовательность");
                    return false;
            }
        }
    }

    bool hex4(unsigned& v)
    {
        v = 0;
        for (int k = 0; k < 4; ++k) {
            if (eof()) { fail("оборванная \\u-последовательность"); return false; }
            char32_t c = cur();
            unsigned d;
            if      (c >= U'0' && c <= U'9') d = (unsigned)(c - U'0');
            else if (c >= U'a' && c <= U'f') d = (unsigned)(c - U'a') + 10;
            else if (c >= U'A' && c <= U'F') d = (unsigned)(c - U'A') + 10;
            else { fail("ожидалась шестнадцатеричная цифра"); return false; }
            v = (v << 4) | d;
            advance();
        }
        return true;
    }

    void parse_number(Json& out)
    {
        std::string raw;                 // ASCII-копия для strtod
        bool is_int = true;
        bool neg = false;

        if (cur() == U'-') { neg = true; raw += '-'; advance(); }

        if (eof()) { fail("оборванное число"); return; }

        if (cur() == U'0') {
            raw += '0';
            advance();
        } else if (cur() >= U'1' && cur() <= U'9') {
            while (!eof() && cur() >= U'0' && cur() <= U'9') {
                raw += (char)cur();
                advance();
            }
        } else {
            fail("ожидалась цифра");
            return;
        }

        if (!eof() && cur() == U'.') {
            is_int = false;
            raw += '.';
            advance();
            if (eof() || cur() < U'0' || cur() > U'9') { fail("ожидалась цифра после точки"); return; }
            while (!eof() && cur() >= U'0' && cur() <= U'9') { raw += (char)cur(); advance(); }
        }

        if (!eof() && (cur() == U'e' || cur() == U'E')) {
            is_int = false;
            raw += 'e';
            advance();
            if (!eof() && (cur() == U'+' || cur() == U'-')) { raw += (char)cur(); advance(); }
            if (eof() || cur() < U'0' || cur() > U'9') { fail("ожидалась цифра в экспоненте"); return; }
            while (!eof() && cur() >= U'0' && cur() <= U'9') { raw += (char)cur(); advance(); }
        }

        if (is_int) {
            // Пытаемся сохранить точное целое; при переполнении падаем в double.
            const char* p = raw.c_str();
            if (*p == '-') ++p;
            unsigned long long acc = 0;
            bool overflow = false;
            for (; *p; ++p) {
                unsigned d = (unsigned)(*p - '0');
                if (acc > (0xFFFFFFFFFFFFFFFFull - d) / 10ull) { overflow = true; break; }
                acc = acc * 10ull + d;
            }
            unsigned long long limit = neg ? 9223372036854775808ull : 9223372036854775807ull;
            if (!overflow && acc <= limit) {
                out = Json(neg ? (long long)(0ull - acc) : (long long)acc);
                return;
            }
        }
        out = Json(json_strtod(raw));
    }

    void parse_array(Json& out, std::size_t depth)
    {
        out = Json::array();
        advance();                       // [
        skip_ws();
        if (!err_.ok) return;
        if (cur() == U']') { advance(); return; }

        for (;;) {
            skip_ws();
            if (!err_.ok) return;
            if (opt_.allow_trailing_commas && cur() == U']') { advance(); return; }

            parse_value(out.emplace_back(), depth + 1);
            if (!err_.ok) return;

            skip_ws();
            if (!err_.ok) return;
            if (cur() == U',') { advance(); continue; }
            if (cur() == U']') { advance(); return; }
            fail("ожидалась ',' или ']'");
            return;
        }
    }

    void parse_object(Json& out, std::size_t depth)
    {
        out = Json::object();
        advance();                       // {
        skip_ws();
        if (!err_.ok) return;
        if (cur() == U'}') { advance(); return; }

        for (;;) {
            skip_ws();
            if (!err_.ok) return;
            if (opt_.allow_trailing_commas && cur() == U'}') { advance(); return; }

            std::u32string key;
            if (!parse_string(key)) return;

            skip_ws();
            if (!err_.ok) return;
            if (cur() != U':') { fail("ожидалось ':'"); return; }
            advance();
            skip_ws();
            if (!err_.ok) return;

            parse_value(out.emplace(key), depth + 1);
            if (!err_.ok) return;

            skip_ws();
            if (!err_.ok) return;
            if (cur() == U',') { advance(); continue; }
            if (cur() == U'}') { advance(); return; }
            fail("ожидалась ',' или '}'");
            return;
        }
    }
};

} // namespace detail

// --- Точки входа ------------------------------------------------------

inline Json json_parse_u32(const std::u32string& text,
                           JsonError* err = 0,
                           const JsonOptions& opt = JsonOptions())
{
    JsonError local;
    JsonError& e = err ? *err : local;
    e = JsonError();
    detail::JsonParser p(text, opt, e);
    return p.run();
}

// Разбор строки любого типа символа.
template <class CharT>
inline Json json_parse(const std::basic_string<CharT>& text,
                       JsonError* err = 0,
                       const JsonOptions& opt = JsonOptions())
{
    return json_parse_u32(conv<CharT>::to32(text), err, opt);
}

inline Json json_parse(const char* text,
                       JsonError* err = 0,
                       const JsonOptions& opt = JsonOptions())
{
    return json_parse(std::string(text ? text : ""), err, opt);
}

// Разбор сырых байт в произвольной кодировке (Auto — по BOM, иначе UTF-8).
inline Json json_parse_bytes(const std::string& bytes,
                             const Charset& cs = Charset(Charset::Auto),
                             JsonError* err = 0,
                             const JsonOptions& opt = JsonOptions())
{
    return json_parse_u32(decode(bytes, cs), err, opt);
}

inline Json json_load(const std::string& path,
                      const Charset& cs = Charset(Charset::Auto),
                      JsonError* err = 0,
                      const JsonOptions& opt = JsonOptions())
{
    bool ok = false;
    std::string bytes = ReadAllBytes(path, &ok);
    if (!ok) {
        if (err) {
            err->ok = false;
            err->message = "не удалось открыть файл";
            err->line = err->column = 1;
            err->offset = 0;
        }
        return Json();
    }
    return json_parse_bytes(bytes, cs, err, opt);
}

inline std::string json_dump_bytes(const Json& v,
                                   const Charset& cs = Charset(Charset::Utf8),
                                   int indent = 0,
                                   bool ensure_ascii = false,
                                   bool with_bom = false)
{
    return encode(v.dump_u32(indent, ensure_ascii), cs, with_bom);
}

inline bool json_save(const std::string& path,
                      const Json& v,
                      const Charset& cs = Charset(Charset::Utf8),
                      int indent = 2,
                      bool ensure_ascii = false,
                      bool with_bom = false)
{
    return WriteAllBytes(path, json_dump_bytes(v, cs, indent, ensure_ascii, with_bom));
}

} // namespace dp

#ifndef DP_NO_GLOBAL_USING
using namespace dp;
#endif

#endif // DPCALLS_H

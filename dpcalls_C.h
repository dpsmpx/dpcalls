/* dpcalls_C.h — универсальные утилиты для работы с текстом, версия на C
 *
 * Требуется: C99 или новее. Реализация — в dpcalls_C.c
 *
 * Модель работы (та же, что в C++ версии dpcalls.h):
 *
 *     файл на диске = БАЙТЫ + КОДИРОВКА (dp_charset)
 *     строка в программе = ЮНИКОД в представлении выбранного типа символа
 *
 *         DP_CHAR    -> char      -> UTF-8
 *         DP_WCHAR   -> wchar_t   -> UTF-16 (Windows) или UTF-32 (Linux/Android),
 *                                    определяется автоматически по sizeof(wchar_t)
 *         DP_CHAR16  -> dp_char16 -> UTF-16
 *         DP_CHAR32  -> dp_char32 -> UTF-32
 *
 * Внутренний канонический формат — dp_u32 (одна кодовая точка на элемент).
 * Всё, что зависит от символов (reverse, длина, регистр, substr), идёт через
 * него. Байты и конкретный тип символа — только на границах.
 *
 * ОТЛИЧИЯ ОТ C++ ВЕРСИИ — следствие языка, а не смены дизайна:
 *
 *   1. Нет шаблонов. Вместо conv<CharT> — рантайм-параметр dp_chartype
 *      и семейства функций с суффиксами:
 *          _x  — общая форма с явным dp_chartype
 *          без суффикса — char, _w — wchar_t, _16 — dp_char16, _32 — dp_char32
 *
 *   2. Нет значимой семантики. Всё, что возвращает указатель или структуру
 *      с полем p, выделено через malloc и ДОЛЖНО быть освобождено вызывающим:
 *          void*      -> dp_free(p)
 *          dp_bytes   -> dp_bytes_free(&b)
 *          dp_u16     -> dp_u16_free(&u)
 *          dp_u32     -> dp_u32_free(&u)
 *          dp_slist   -> dp_slist_free(&l)
 *          dp_u32list -> dp_u32list_free(&l)
 *
 *   3. Нет исключений. При нехватке памяти печатается сообщение в stderr
 *      и вызывается abort() — аналог std::bad_alloc. Переопределяется
 *      через dp_set_oom_handler().
 *
 *   4. Строки, возвращаемые как void*, всегда завершены нулевым элементом.
 *      Байтовые буферы (dp_bytes) нулём НЕ завершены — у них есть поле n,
 *      потому что байты UTF-16/UTF-32 содержат нули внутри.
 *
 * Совместимость со старыми именами (ReadAllLines, WriteAllLines, print, rnd,
 * reverse, itos, words) — в конце файла, отключается через
 *     #define DP_NO_GLOBAL_ALIASES
 * перед #include.
 */

#ifndef DPCALLS_C_H
#define DPCALLS_C_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * 0. Базовые типы и определения
 * ===================================================================== */

typedef uint_least16_t dp_char16;   /* совместим по представлению с C11 char16_t */
typedef uint_least32_t dp_char32;   /* совместим по представлению с C11 char32_t */

/* Символ-заменитель для всего, что не удалось декодировать. */
#define DP_REPLACEMENT ((dp_char32)0xFFFDu)
/* Байт-заменитель для однобайтовых кодировок. */
#define DP_UNMAPPABLE  '?'
/* «Длину определить самостоятельно, по нулевому элементу». */
#define DP_AUTOLEN     ((size_t)-1)

typedef enum {
    DP_CHAR = 0,    /* char      — UTF-8             */
    DP_WCHAR,       /* wchar_t   — UTF-16 или UTF-32 */
    DP_CHAR16,      /* dp_char16 — UTF-16            */
    DP_CHAR32       /* dp_char32 — UTF-32            */
} dp_chartype;

/* Размер одного элемента строки данного типа, в байтах. */
size_t dp_charsize(dp_chartype t);

/* Обработчик нехватки памяти. По умолчанию: сообщение в stderr + abort(). */
typedef void (*dp_oom_handler)(size_t requested);
void dp_set_oom_handler(dp_oom_handler h);

void *dp_xmalloc(size_t n);
void *dp_xrealloc(void *p, size_t n);
void  dp_free(void *p);

int dp_is_ascii_space(dp_char32 c);
int dp_is_valid_codepoint(dp_char32 c);

/* ASCII-литерал в нужном типе символа (аналог dp::lit<CharT>).
 * Возвращает malloc-буфер, завершённый нулём. */
void      *dp_lit_x (const char *ascii, dp_chartype t);
wchar_t   *dp_lit_w (const char *ascii);
dp_char16 *dp_lit_16(const char *ascii);
dp_char32 *dp_lit_32(const char *ascii);

/* =====================================================================
 * 1. Динамические буферы
 * ===================================================================== */

typedef struct { char      *p; size_t n; size_t cap; } dp_bytes;  /* сырые байты   */
typedef struct { dp_char16 *p; size_t n; size_t cap; } dp_u16;    /* UTF-16        */
typedef struct { dp_char32 *p; size_t n; size_t cap; } dp_u32;    /* кодовые точки */

dp_bytes dp_bytes_new(void);
void     dp_bytes_reserve(dp_bytes *b, size_t cap);
void     dp_bytes_push(dp_bytes *b, char c);
void     dp_bytes_append(dp_bytes *b, const char *p, size_t n);
void     dp_bytes_free(dp_bytes *b);
/* Отдать содержимое как NUL-терминированную C-строку; буфер обнуляется.
 * Осмысленно только если внутри нет нулевых байт. */
char    *dp_bytes_release(dp_bytes *b);

dp_u16 dp_u16_new(void);
void   dp_u16_push(dp_u16 *u, dp_char16 c);
void   dp_u16_free(dp_u16 *u);

dp_u32 dp_u32_new(void);
void   dp_u32_reserve(dp_u32 *u, size_t cap);
void   dp_u32_push(dp_u32 *u, dp_char32 c);
void   dp_u32_append(dp_u32 *u, const dp_char32 *p, size_t n);
dp_u32 dp_u32_copy(const dp_char32 *p, size_t n);
dp_u32 dp_u32_sub(const dp_u32 *u, size_t pos, size_t count);
void   dp_u32_free(dp_u32 *u);

/* Список строк одного типа символа. Владеет своими элементами. */
typedef struct {
    void       **items;
    size_t       count;
    size_t       cap;
    dp_chartype  type;
} dp_slist;

dp_slist   dp_slist_new(dp_chartype t);
void       dp_slist_push(dp_slist *l, void *owned);        /* забирает владение */
void       dp_slist_push_copy(dp_slist *l, const void *s); /* копирует          */
void       dp_slist_free(dp_slist *l);
void      *dp_slist_at(const dp_slist *l, size_t i);
char      *dp_slist_s (const dp_slist *l, size_t i);
wchar_t   *dp_slist_w (const dp_slist *l, size_t i);
dp_char16 *dp_slist_16(const dp_slist *l, size_t i);
dp_char32 *dp_slist_32(const dp_slist *l, size_t i);

typedef struct { dp_u32 *items; size_t count; size_t cap; } dp_u32list;
dp_u32list dp_u32list_new(void);
void       dp_u32list_push(dp_u32list *l, dp_u32 u);       /* забирает владение */
void       dp_u32list_free(dp_u32list *l);

/* =====================================================================
 * 2. UTF-8 / UTF-16 / UTF-32
 * ===================================================================== */

void   dp_utf8_append(dp_char32 cp, dp_bytes *out);
/* Возвращает число съеденных байт (всегда >= 1). Битые последовательности -> U+FFFD. */
size_t dp_utf8_next(const char *s, size_t n, size_t i, dp_char32 *cp);
dp_u32   dp_utf8_to_u32(const char *s, size_t n);      /* n может быть DP_AUTOLEN */
dp_bytes dp_u32_to_utf8(const dp_u32 *u);

void   dp_utf16_append(dp_char32 cp, dp_u16 *out);
dp_u32 dp_utf16_to_u32(const dp_char16 *s, size_t n);  /* n может быть DP_AUTOLEN */
dp_u16 dp_u32_to_utf16(const dp_u32 *u);

/* =====================================================================
 * 3. Мост «любой тип символа <-> dp_u32»
 * ===================================================================== */

/* Число элементов в строке до нулевого элемента. */
size_t dp_strlen_x(const void *s, dp_chartype t);

/* Строка типа t -> кодовые точки. n = DP_AUTOLEN — считать до нулевого элемента. */
dp_u32 dp_to32(const void *s, size_t n, dp_chartype t);

/* Кодовые точки -> строка типа t. Результат malloc, завершён нулём.
 * Если out_len != NULL, туда пишется число элементов без нулевого. */
void *dp_from32(const dp_u32 *u, dp_chartype t, size_t *out_len);

/* Перекодировка между любыми двумя типами символов. */
void *dp_convert(const void *s, dp_chartype from, dp_chartype to);

/* =====================================================================
 * 4. Кодировки байтовых потоков
 * ===================================================================== */

typedef enum {
    DP_CS_AUTO = 0,   /* определить по BOM, иначе UTF-8 */
    DP_CS_UTF8,
    DP_CS_UTF16LE, DP_CS_UTF16BE,
    DP_CS_UTF32LE, DP_CS_UTF32BE,
    DP_CS_SBCS,       /* однобайтовая кодировка по таблице table */
    DP_CS_LOCAL       /* системная локаль (setlocale/mbrtowc)    */
} dp_cs_kind;

typedef struct {
    dp_cs_kind       kind;
    const dp_char32 *table;   /* 128 элементов для DP_CS_SBCS, иначе NULL */
    const char      *name;
} dp_charset;

/* Таблицы однобайтовых кодировок: байты 0x80..0xFF -> Unicode. */
const dp_char32 *dp_table_cp1251(void);
const dp_char32 *dp_table_cp866 (void);
const dp_char32 *dp_table_koi8r (void);
const dp_char32 *dp_table_latin1(void);

dp_charset dp_cs_auto   (void);
dp_charset dp_cs_utf8   (void);
dp_charset dp_cs_utf16le(void);
dp_charset dp_cs_utf16be(void);
dp_charset dp_cs_utf32le(void);
dp_charset dp_cs_utf32be(void);
dp_charset dp_cs_cp1251 (void);
dp_charset dp_cs_cp866  (void);
dp_charset dp_cs_koi8r  (void);
dp_charset dp_cs_latin1 (void);
dp_charset dp_cs_local  (void);
/* Своя однобайтовая кодировка: 128 кодовых точек для байтов 0x80..0xFF. */
dp_charset dp_cs_custom(const dp_char32 *table128, const char *name);

dp_charset  dp_detect_bom(const char *b, size_t n, size_t *bom_len);
const char *dp_bom_bytes (dp_charset cs, size_t *out_n);

dp_u32   dp_decode(const char *bytes, size_t n, dp_charset cs);
dp_bytes dp_encode(const dp_u32 *text, dp_charset cs, int with_bom);
dp_bytes dp_recode(const char *bytes, size_t n,
                   dp_charset from, dp_charset to, int with_bom);

/* =====================================================================
 * 5. Операции над строками
 * ===================================================================== */

/* Длина в кодовых точках, а не в элементах строки. */
size_t dp_length_x(const void *s, dp_chartype t);

/* Разворот строки. Корректен для многобайтовых кодировок. */
void *dp_reverse_x(const void *s, dp_chartype t);

/* Подстрока по кодовым точкам. count = DP_AUTOLEN — до конца строки. */
void *dp_substr_cp_x(const void *s, dp_chartype t, size_t pos, size_t count);

/* Регистр: ASCII + основная кириллица. Для полного Юникода нужна ICU. */
dp_char32 dp_to_upper_cp(dp_char32 c);
dp_char32 dp_to_lower_cp(dp_char32 c);
void *dp_upper_x(const void *s, dp_chartype t);
void *dp_lower_x(const void *s, dp_chartype t);
void *dp_trim_x (const void *s, dp_chartype t);

/* Разбиение по пробельным символам ASCII.
 * keep_empty = 0 — подряд идущие разделители не порождают пустых слов. */
dp_slist dp_words_x(const void *line, dp_chartype t, int keep_empty);

/* Разбиение по произвольной кодовой точке. */
dp_slist dp_split_x(const void *line, dp_chartype t, dp_char32 delim, int keep_empty);

/* Склейка. sep — строка того же типа, что и список. */
void *dp_join_x(const dp_slist *parts, const void *sep);

/* Число -> строка. base от 2 до 36. */
void *dp_to_str_x(long long value, int base, dp_chartype t);

/* Строка -> число. Возвращает 0, если строка не является числом целиком. */
int dp_stoll_safe_x(const void *s, dp_chartype t, long long *out, int base);

/* =====================================================================
 * 6. Файлы
 * ===================================================================== */

dp_bytes dp_read_all_bytes(const char *path, int *ok);
int      dp_write_all_bytes(const char *path, const char *bytes, size_t n, int append);

/* Разбиение текста на строки: \n, \r\n и одиночный \r. */
dp_u32list dp_split_lines_u32(const dp_u32 *text);

/* Чтение файла в строки указанного типа символа. */
dp_slist dp_read_lines_x(const char *path, dp_charset cs, dp_chartype t, int *ok);
int      dp_write_lines_x(const char *path, const dp_slist *lines,
                          dp_charset cs, int with_bom, const char *eol);

/* Весь файл одной строкой. */
void *dp_read_all_text_x (const char *path, dp_charset cs, dp_chartype t, int *ok);
int   dp_write_all_text_x(const char *path, const void *text, dp_chartype t,
                          dp_charset cs, int with_bom);

/* =====================================================================
 * 7. Вывод в консоль
 * ===================================================================== */

/* Настройка локали и кодовой страницы консоли. Вызывать один раз в начале main(). */
void dp_init_console(void);

void dp_print_x    (const dp_slist *lines, dp_charset cs);
void dp_print_str_x(const void *s, dp_chartype t, dp_charset cs);

/* Чтение строки со stdin. Возвращает malloc-строку или NULL при EOF. */
void *dp_input_line_x(dp_chartype t, dp_charset cs);

/* =====================================================================
 * 8. Случайные числа
 * ===================================================================== */

void   dp_rnd_seed(uint64_t s);
int    dp_rnd(int max);                 /* [0, max-1] — как в старой версии */
int    dp_rnd_range(int min, int max);  /* [min, max] включительно          */
double dp_rndf(void);                   /* [0.0, 1.0)                       */

/* =====================================================================
 * 11. JSON
 * =====================================================================
 *
 * Разбор идёт по кодовым точкам (dp_u32), поэтому парсер не зависит от
 * кодировки исходных байт: dp_json_parse_bytes() сначала декодирует вход
 * через dp_charset, дальше работает единый код. Escape \uXXXX с
 * суррогатными парами собираются в настоящие кодовые точки.
 *
 * Числа: JSON не различает целые и дробные, но dp_json запоминает, было ли
 * значение записано как целое, и хранит точное long long. Это важно для
 * идентификаторов, которые не влезают в мантиссу double.
 *
 * Разбор и печать чисел не зависят от локали: точка приводится к
 * разделителю текущей локали только на время вызова strtod/snprintf.
 *
 * ВЛАДЕНИЕ. dp_json* всегда выделен через malloc и освобождается
 * dp_json_free() — рекурсивно, вместе со всем поддеревом.
 * Функции dp_json_push() / dp_json_set() ЗАБИРАЮТ владение переданным
 * значением: после них освобождать его отдельно нельзя.
 * Функции доступа (dp_json_at, dp_json_get) возвращают заимствованный
 * указатель внутрь дерева — его освобождать НЕ нужно.
 */

typedef enum {
    DP_JSON_NULL = 0,
    DP_JSON_BOOL,
    DP_JSON_NUMBER,
    DP_JSON_STRING,
    DP_JSON_ARRAY,
    DP_JSON_OBJECT
} dp_json_type;

typedef struct dp_json dp_json;

struct dp_json {
    dp_json_type type;

    int          b;        /* DP_JSON_BOOL   */
    double       d;        /* DP_JSON_NUMBER */
    long long    i;        /* DP_JSON_NUMBER, если is_int */
    int          is_int;
    dp_u32       s;        /* DP_JSON_STRING */

    dp_json    **items;    /* элементы массива или значения объекта */
    dp_u32      *keys;     /* ключи объекта, параллельно items; NULL для массива */
    size_t       count;
    size_t       cap;
};

/* --- Числа, независимые от локали --- */
char   dp_json_locale_point(void);
double dp_json_strtod(const char *ascii);          /* разделитель — точка   */
void   dp_json_dtoa(double v, char *buf, size_t n); /* круговой прогон точен */

/* --- Создание --- */
dp_json *dp_json_new_null(void);
dp_json *dp_json_new_bool(int v);
dp_json *dp_json_new_int(long long v);
dp_json *dp_json_new_double(double v);
dp_json *dp_json_new_str_x(const void *s, dp_chartype t);
dp_json *dp_json_new_str(const char *utf8);
dp_json *dp_json_new_array(void);
dp_json *dp_json_new_object(void);
dp_json *dp_json_clone(const dp_json *v);
void     dp_json_free(dp_json *v);

/* --- Опрос типа --- */
dp_json_type dp_json_typeof(const dp_json *v);
int dp_json_is_null  (const dp_json *v);
int dp_json_is_bool  (const dp_json *v);
int dp_json_is_number(const dp_json *v);
int dp_json_is_int   (const dp_json *v);
int dp_json_is_string(const dp_json *v);
int dp_json_is_array (const dp_json *v);
int dp_json_is_object(const dp_json *v);

/* --- Чтение значений --- */
int       dp_json_as_bool  (const dp_json *v, int def);
double    dp_json_as_double(const dp_json *v, double def);
long long dp_json_as_int   (const dp_json *v, long long def);
/* malloc-строка, освобождать через dp_free() */
void     *dp_json_as_str_x (const dp_json *v, dp_chartype t);
char     *dp_json_as_utf8  (const dp_json *v);
wchar_t  *dp_json_as_wstr  (const dp_json *v);
/* Заимствованная ссылка на внутреннее представление строки. */
const dp_u32 *dp_json_raw_str(const dp_json *v);

/* --- Массив и объект (возвращают заимствованные указатели) --- */
size_t   dp_json_size(const dp_json *v);
dp_json *dp_json_at(const dp_json *v, size_t idx);
dp_json *dp_json_get_x(const dp_json *v, const void *key, dp_chartype t);
dp_json *dp_json_get(const dp_json *v, const char *key_utf8);
int      dp_json_has(const dp_json *v, const char *key_utf8);
/* Ключ по индексу — для обхода объекта в порядке вставки. malloc. */
void    *dp_json_key_at_x(const dp_json *v, size_t idx, dp_chartype t);
char    *dp_json_key_at(const dp_json *v, size_t idx);

/* --- Изменение (забирают владение значением val) --- */
void dp_json_push(dp_json *arr, dp_json *val);
void dp_json_set_x(dp_json *obj, const void *key, dp_chartype t, dp_json *val);
void dp_json_set(dp_json *obj, const char *key_utf8, dp_json *val);
int  dp_json_remove(dp_json *obj, const char *key_utf8);
/* Добавляет пустой элемент и возвращает заимствованную ссылку на него.
 * Позволяет строить дерево на месте, без копирования поддеревьев. */
dp_json *dp_json_emplace_back(dp_json *arr);
dp_json *dp_json_emplace_x(dp_json *obj, const void *key, dp_chartype t);

/* --- Настройки и ошибки --- */
typedef struct {
    int    allow_comments;         /* "//" и блочные комментарии */
    int    allow_trailing_commas;  /* [1,2,]  {"a":1,}    */
    size_t max_depth;              /* защита от переполнения стека */
} dp_json_options;

dp_json_options dp_json_options_default(void);
dp_json_options dp_json_options_relaxed(void);

typedef struct {
    int    ok;
    char   message[128];
    size_t line;     /* с 1 */
    size_t column;   /* с 1, в кодовых точках */
    size_t offset;   /* с 0, в кодовых точках */
} dp_json_error;

/* Человекочитаемое описание ошибки в предоставленный буфер. */
void dp_json_error_str(const dp_json_error *e, char *buf, size_t n);

/* --- Разбор. Возвращает NULL при ошибке; подробности в err. --- */
dp_json *dp_json_parse_u32  (const dp_u32 *text, dp_json_error *err, const dp_json_options *opt);
dp_json *dp_json_parse_x    (const void *text, dp_chartype t, dp_json_error *err, const dp_json_options *opt);
dp_json *dp_json_parse      (const char *utf8, dp_json_error *err, const dp_json_options *opt);
dp_json *dp_json_parse_bytes(const char *bytes, size_t n, dp_charset cs,
                             dp_json_error *err, const dp_json_options *opt);
dp_json *dp_json_load       (const char *path, dp_charset cs,
                             dp_json_error *err, const dp_json_options *opt);

/* --- Печать. indent = 0 — компактно, > 0 — с отступами. --- */
dp_u32   dp_json_dump_u32  (const dp_json *v, int indent, int ensure_ascii);
char    *dp_json_dump      (const dp_json *v, int indent, int ensure_ascii); /* malloc utf8 */
dp_bytes dp_json_dump_bytes(const dp_json *v, dp_charset cs, int indent,
                            int ensure_ascii, int with_bom);
int      dp_json_save      (const char *path, const dp_json *v, dp_charset cs,
                            int indent, int ensure_ascii, int with_bom);

/* =====================================================================
 * 9. Типизированные обёртки
 * ===================================================================== */

static inline size_t dp_length   (const char *s)      { return dp_length_x(s, DP_CHAR);   }
static inline size_t dp_length_w (const wchar_t *s)   { return dp_length_x(s, DP_WCHAR);  }
static inline size_t dp_length_16(const dp_char16 *s) { return dp_length_x(s, DP_CHAR16); }
static inline size_t dp_length_32(const dp_char32 *s) { return dp_length_x(s, DP_CHAR32); }

static inline char      *dp_reverse   (const char *s)      { return (char*)      dp_reverse_x(s, DP_CHAR);   }
static inline wchar_t   *dp_reverse_w (const wchar_t *s)   { return (wchar_t*)   dp_reverse_x(s, DP_WCHAR);  }
static inline dp_char16 *dp_reverse_16(const dp_char16 *s) { return (dp_char16*) dp_reverse_x(s, DP_CHAR16); }
static inline dp_char32 *dp_reverse_32(const dp_char32 *s) { return (dp_char32*) dp_reverse_x(s, DP_CHAR32); }

static inline char    *dp_substr_cp  (const char *s, size_t pos, size_t n)    { return (char*)   dp_substr_cp_x(s, DP_CHAR,  pos, n); }
static inline wchar_t *dp_substr_cp_w(const wchar_t *s, size_t pos, size_t n) { return (wchar_t*)dp_substr_cp_x(s, DP_WCHAR, pos, n); }

static inline char    *dp_upper  (const char *s)    { return (char*)   dp_upper_x(s, DP_CHAR);  }
static inline wchar_t *dp_upper_w(const wchar_t *s) { return (wchar_t*)dp_upper_x(s, DP_WCHAR); }
static inline char    *dp_lower  (const char *s)    { return (char*)   dp_lower_x(s, DP_CHAR);  }
static inline wchar_t *dp_lower_w(const wchar_t *s) { return (wchar_t*)dp_lower_x(s, DP_WCHAR); }
static inline char    *dp_trim   (const char *s)    { return (char*)   dp_trim_x (s, DP_CHAR);  }
static inline wchar_t *dp_trim_w (const wchar_t *s) { return (wchar_t*)dp_trim_x (s, DP_WCHAR); }

static inline dp_slist dp_words  (const char *s, int keep_empty)    { return dp_words_x(s, DP_CHAR,  keep_empty); }
static inline dp_slist dp_words_w(const wchar_t *s, int keep_empty) { return dp_words_x(s, DP_WCHAR, keep_empty); }

static inline dp_slist dp_split  (const char *s, dp_char32 d, int ke)    { return dp_split_x(s, DP_CHAR,  d, ke); }
static inline dp_slist dp_split_w(const wchar_t *s, dp_char32 d, int ke) { return dp_split_x(s, DP_WCHAR, d, ke); }

static inline char    *dp_join  (const dp_slist *l, const char *sep)    { return (char*)   dp_join_x(l, sep); }
static inline wchar_t *dp_join_w(const dp_slist *l, const wchar_t *sep) { return (wchar_t*)dp_join_x(l, sep); }

static inline char    *dp_itos(long long v, int base) { return (char*)   dp_to_str_x(v, base, DP_CHAR);  }
static inline wchar_t *dp_itow(long long v, int base) { return (wchar_t*)dp_to_str_x(v, base, DP_WCHAR); }

static inline int dp_stoll_safe  (const char *s, long long *out, int base)    { return dp_stoll_safe_x(s, DP_CHAR,  out, base); }
static inline int dp_stoll_safe_w(const wchar_t *s, long long *out, int base) { return dp_stoll_safe_x(s, DP_WCHAR, out, base); }

static inline dp_slist dp_ReadAllLines (const char *path, dp_charset cs) { return dp_read_lines_x(path, cs, DP_CHAR,  NULL); }
static inline dp_slist dp_ReadAllLinesW(const char *path, dp_charset cs) { return dp_read_lines_x(path, cs, DP_WCHAR, NULL); }

static inline int dp_WriteAllLines(const char *path, const dp_slist *lines,
                                   dp_charset cs, int with_bom)
{
    return dp_write_lines_x(path, lines, cs, with_bom, "\n");
}

static inline char    *dp_ReadAllText (const char *path, dp_charset cs) { return (char*)   dp_read_all_text_x(path, cs, DP_CHAR,  NULL); }
static inline wchar_t *dp_ReadAllTextW(const char *path, dp_charset cs) { return (wchar_t*)dp_read_all_text_x(path, cs, DP_WCHAR, NULL); }

static inline int dp_WriteAllText (const char *path, const char *t, dp_charset cs, int bom)    { return dp_write_all_text_x(path, t, DP_CHAR,  cs, bom); }
static inline int dp_WriteAllTextW(const char *path, const wchar_t *t, dp_charset cs, int bom) { return dp_write_all_text_x(path, t, DP_WCHAR, cs, bom); }

static inline void dp_print  (const dp_slist *l, dp_charset cs) { dp_print_x(l, cs); }
static inline void dp_print_s(const char *s, dp_charset cs)     { dp_print_str_x(s, DP_CHAR,  cs); }
static inline void dp_print_w(const wchar_t *s, dp_charset cs)  { dp_print_str_x(s, DP_WCHAR, cs); }

static inline char    *dp_input_line  (dp_charset cs) { return (char*)   dp_input_line_x(DP_CHAR,  cs); }
static inline wchar_t *dp_input_line_w(dp_charset cs) { return (wchar_t*)dp_input_line_x(DP_WCHAR, cs); }

/* =====================================================================
 * 10. Совместимость со старыми именами
 * ===================================================================== */

#ifndef DP_NO_GLOBAL_ALIASES

static inline dp_slist ReadAllLines(const char *path, dp_charset cs)
{
    return dp_ReadAllLines(path, cs);
}
static inline dp_slist ReadAllLinesW(const char *path, dp_charset cs)
{
    return dp_ReadAllLinesW(path, cs);
}
static inline int WriteAllLines(const char *path, const dp_slist *lines,
                                dp_charset cs, int with_bom)
{
    return dp_WriteAllLines(path, lines, cs, with_bom);
}
static inline void print(const dp_slist *lines, dp_charset cs) { dp_print_x(lines, cs); }
static inline char *reverse(const char *s)                     { return dp_reverse(s);  }
static inline char *itos(long long v)                          { return dp_itos(v, 10); }
static inline dp_slist words(const char *s)                    { return dp_words(s, 0); }
static inline int rnd(int max)                                 { return dp_rnd(max);    }
static inline int rnd_range(int min, int max)                  { return dp_rnd_range(min, max); }

#endif /* DP_NO_GLOBAL_ALIASES */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DPCALLS_C_H */

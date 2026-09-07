# dpcalls_C — сборка и краткая шпаргалка

## Сборка

```sh
gcc -std=c99 -Wall -Wextra -c dpcalls_C.c
gcc -std=c99 -o myapp main.c dpcalls_C.o
```

Проверено на gcc 13 в режимах c99 / c11 / c17 / c2x, с `-Wall -Wextra -pedantic
-Wshadow -Wconversion` — без предупреждений. ASan + UBSan + LeakSanitizer чисто.
Отдельно проверена ветка `sizeof(wchar_t) == 2` (Windows) через `-fshort-wchar`.

## Главное отличие от C++ версии: владение памятью

Всё, что возвращает указатель или структуру с полем `p`, выделено через malloc.
Освобождать обязан вызывающий:

| Что вернули   | Чем освобождать        |
|---------------|------------------------|
| `void*`       | `dp_free(p)`           |
| `dp_bytes`    | `dp_bytes_free(&b)`    |
| `dp_u16`      | `dp_u16_free(&u)`      |
| `dp_u32`      | `dp_u32_free(&u)`      |
| `dp_slist`    | `dp_slist_free(&l)`    |
| `dp_u32list`  | `dp_u32list_free(&l)`  |

`dp_slist` владеет своими элементами — отдельно их освобождать не нужно.
При нехватке памяти библиотека печатает сообщение и вызывает `abort()`
(аналог `std::bad_alloc`); поведение меняется через `dp_set_oom_handler()`.

## Соответствие имён

| C++                          | C                                        |
|------------------------------|------------------------------------------|
| `dp::conv<CharT>`            | `dp_to32(s, n, t)` / `dp_from32(u, t, &n)` |
| `dp::convert<To>(s)`         | `dp_convert(s, from, to)`                |
| `dp::reverse(s)`             | `dp_reverse` / `_w` / `_16` / `_32` / `_x` |
| `dp::length(s)`              | `dp_length` / `_w` / `_16` / `_32` / `_x`  |
| `dp::Charset` / `cs_utf8()`  | `dp_charset` / `dp_cs_utf8()`            |
| `dp::decode/encode/recode`   | `dp_decode` / `dp_encode` / `dp_recode`  |
| `dp::ReadLines<CharT>`       | `dp_read_lines_x(path, cs, t, &ok)`      |
| `dp::WriteLines<CharT>`      | `dp_write_lines_x(...)`                  |
| `std::vector<std::string>`   | `dp_slist` (+ поле `type`)               |
| `std::u32string`             | `dp_u32`                                 |
| `std::mt19937`               | xorshift64* + rejection sampling         |

Суффиксы: без суффикса — `char`, `_w` — `wchar_t`, `_16` — `dp_char16`,
`_32` — `dp_char32`, `_x` — общая форма с явным `dp_chartype`.

## Минимальный пример

```c
#include "dpcalls_C.h"

int main(void)
{
    dp_init_console();

    /* читаем cp1251-файл, пишем его же в UTF-8 */
    dp_slist lines = dp_ReadAllLines("in.txt", dp_cs_cp1251());
    dp_WriteAllLines("out.txt", &lines, dp_cs_utf8(), 0);
    dp_print_x(&lines, dp_cs_utf8());
    dp_slist_free(&lines);

    /* разворот строки с кириллицей */
    char *r = dp_reverse("Привет");
    dp_print_s(r, dp_cs_utf8());
    dp_free(r);

    /* работа в wchar_t */
    wchar_t *w = (wchar_t*)dp_convert("Привет", DP_CHAR, DP_WCHAR);
    wchar_t *u = dp_upper_w(w);
    dp_print_w(u, dp_cs_utf8());
    dp_free(w);
    dp_free(u);

    return 0;
}
```

---

# JSON

Добавлен в обе версии. Разбор идёт по кодовым точкам, поэтому парсер не зависит
от кодировки исходных байт: `*_parse_bytes()` сначала декодирует вход через
`Charset`/`dp_charset`, дальше работает единый код. Обе реализации на одном
документе дают побайтово одинаковый вывод.

## Что сделано одинаково в C++ и C

- Строгий RFC 8259 по умолчанию; послабления (`//` и `/* */`, висячие запятые)
  включаются опцией — удобно для конфигов.
- Точные целые: `9007199254740993` (2^53+1) переживает круговой прогон, потому
  что целые хранятся как `long long` отдельно от `double`. При переполнении
  `long long` значение молча уходит в `double`.
- Печать `double` подбирает точность 15→17 знаков, пока обратный разбор не даст
  ровно исходное значение. `NaN` и бесконечности печатаются как `null` — в JSON
  их нет.
- Разбор и печать чисел не зависят от локали: точка приводится к разделителю
  текущей локали только на время вызова `strtod`/`snprintf`. Без этого
  `setlocale(LC_ALL, "")` с немецкой или русской локалью ломал бы `1.5`.
- `\uXXXX` с суррогатными парами собирается в настоящие кодовые точки;
  непарный суррогат становится U+FFFD, а не роняет разбор.
- Ограничение вложенности (по умолчанию 200) — глубокий вход даёт нормальную
  ошибку, а не переполнение стека.
- Ошибка сообщает строку, столбец и смещение в кодовых точках.
- Порядок ключей сохраняется; при дубликате побеждает последний, как в
  `JSON.parse`.
- Дерево строится на месте (`emplace_back` / `emplace`), поэтому разбор
  линейный. Наивный `push_back(значение)` давал бы копирование поддерева на
  каждом уровне и квадратичное время.

## Соответствие имён

| C++                                | C                                        |
|------------------------------------|------------------------------------------|
| `dp::Json`                         | `dp_json*`                               |
| `Json::object()` / `array()`       | `dp_json_new_object()` / `_new_array()`  |
| `Json(42)` / `Json("текст")`       | `dp_json_new_int(42)` / `_new_str(...)`  |
| `v["ключ"]`                        | `dp_json_get(v, "ключ")`                 |
| `v[3]`                             | `dp_json_at(v, 3)`                       |
| `v.as_int()` / `as_utf8()`         | `dp_json_as_int(v, def)` / `_as_utf8(v)` |
| `v.set(k, x)` / `push_back(x)`     | `dp_json_set(v,k,x)` / `dp_json_push(v,x)` |
| `v.dump(2)`                        | `dp_json_dump(v, 2, 0)`                  |
| `json_parse` / `_bytes` / `_load`  | `dp_json_parse` / `_bytes` / `_load`     |
| `json_save`                        | `dp_json_save`                           |
| `JsonOptions::relaxed()`           | `dp_json_options_relaxed()`              |
| `JsonError::what()`                | `dp_json_error_str(&e, buf, n)`          |

## Владение памятью в C

- `dp_json*` освобождается `dp_json_free()` — рекурсивно, со всем поддеревом.
- `dp_json_push()` и `dp_json_set()` **забирают** владение переданным значением:
  освобождать его отдельно нельзя.
- `dp_json_at()` и `dp_json_get()` возвращают **заимствованный** указатель
  внутрь дерева — его освобождать не нужно. Отсутствующий ключ даёт `NULL`,
  и все `dp_json_as_*` / `dp_json_is_null` корректно принимают `NULL`.
- `dp_json_as_utf8()`, `dp_json_dump()`, `dp_json_key_at()` возвращают
  malloc-строку → `dp_free()`.
- `dp_json_parse*()` возвращает `NULL` при ошибке; подробности в `dp_json_error`.

## Пример на C

```c
dp_json_error e;
dp_json *cfg = dp_json_load("config.json", dp_cs_auto(), &e, NULL);
if (!cfg) {
    char buf[256];
    dp_json_error_str(&e, buf, sizeof(buf));
    fprintf(stderr, "JSON: %s\n", buf);
    return 1;
}

long long port = dp_json_as_int(dp_json_get(cfg, "порт"), 8080);
char *host = dp_json_as_utf8(dp_json_get(cfg, "хост"));

dp_json_set(cfg, "запусков", dp_json_new_int(
    dp_json_as_int(dp_json_get(cfg, "запусков"), 0) + 1));
dp_json_save("config.json", cfg, dp_cs_utf8(), 2, 0, 0);

dp_free(host);
dp_json_free(cfg);
```

## Пример на C++

```cpp
dp::JsonError e;
dp::Json cfg = dp::json_load("config.json", dp::Charset(dp::Charset::Auto), &e);
if (!e.ok) { std::cerr << "JSON: " << e.what() << "\n"; return 1; }

long long port = cfg["порт"].as_int(8080);
std::string host = cfg["хост"].as_utf8();

cfg.set("запусков", dp::Json(cfg["запусков"].as_int(0) + 1));
dp::json_save("config.json", cfg, dp::cs_utf8(), 2);
```

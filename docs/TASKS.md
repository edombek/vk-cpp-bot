# Задачи и статусы

Обновлено: 2026-10-22. Легенда: ✅ выполнено · 🚧 в работе · ⬜ не начато · ❌ не делаем

## Аудит проекта

| Задача | Статус |
|---|---|
| Ревью всех исходников, поиск ошибок | ✅ (2026-10-22) |
| Проверка сборки (Linux, с Python и без) | ✅ (см. `TESTS.md`) |
| Проверка жизнеспособности pybind11-миграции | ✅ (2026-10-22) |
| Полный отчёт проверок | ✅ — `/home/edombek/.opencode/plan/BUILD-RESULTS.md` |
| План работ по фазам | ✅ — `/home/edombek/.opencode/plan/PLAN.md` |

## Фаза 0 — критические баги (все ⬜, правки кода ещё не вносились)

| Задача | Где | Статус |
|---|---|---|
| Затенение `string buff` → бесконечный цикл longpoll | `src/lp.cpp:29-30` | ⬜ |
| Саморекурсия `fs::rename` (нужен `::rename`) | `src/fs.cpp:97` | ⬜ |
| vox(): валидация WAV-заголовка, чтение `data[-1]` при пустом буфере | `src/cmds.cpp` | ⬜ |
| Инициализация полей `Event`/`Doc` по умолчанию | `include/events.h`, `include/doc.h` | ⬜ |
| Починить `operator=` (ссылка на локальную переменную) | `include/events.h`, `include/img.h` | ⬜ |
| `getNumber()`: `mt19937`+`shuffle` вместо `srand(time)`/`random_shuffle` | `src/cmds.cpp` | ⬜ |
| users: двойная запись `msgs`, хак `msgs--` в `setc` | `src/users.cpp`, `src/cmds.cpp` | ⬜ |

## Фаза 1 — сборка/Makefile

| Задача | Статус |
|---|---|
| Убрать boost-python (линуется pybind11 header-only) | ⬜ |
| pybind11-инклюды (`python3 -m pybind11 --includes`) | ⬜ |
| Автоопределение OpenCV (`/usr/include/opencv*`, pkg-config) | ⬜ |
| `-MMD -MP` (зависимости от заголовков) | ⬜ |
| `-Ofast` → `-O2` | ⬜ |

## Фаза 2 — Python-интеграция на pybind11 + расширение API

| Задача | Статус |
|---|---|
| `py.h`/`py.cpp` на pybind11: embedded module, `subinterpreter` RAII | ⬜ |
| `gil_scoped_release` в главном потоке (без него deadlock — проверено) | ⬜ |
| Type casters: `table_t↔dict`, `nlohmann::json↔py` (свои, `json.h` в pybind11 3.x удалён) | ⬜ |
| Ошибки скриптов через `error_already_set::what()` | ⬜ |
| try/catch в `Worker::work` (исключение скрипта не роняет пул) | ⬜ |
| Расширение API: `fs.*`, `str.*`, `timer.*`, `Doc()`, `Net.send(dict)`, `Event` read-only поля | ⬜ |
| Регресс-тест `tests/py_integration.cpp` (мок Net, без VK-сети) | ⬜ |

## Фаза 3 — оптимизации

| Задача | Статус |
|---|---|
| `dat.json`: сохранение по dirty-флагу/таймеру вместо записи на каждое сообщение | ⬜ |
| UTF-8 lower без per-call `wstring_convert` (и без `<codecvt>`) | ⬜ |
| `neon()`/`crt()`: векторный HSV вместо попиксельного цикла, убрать лишние clone | ⬜ |
| Ретраи Net/Vk: итерация + лимит вместо бесконечной рекурсии | ⬜ |
| Переход на `-std=c++17` (после замены random_shuffle/codecvt) | ⬜ |

## Фаза 4 — Windows

| Задача | Статус |
|---|---|
| Windows-сборка | ❌ по решению владельца (только заметка unsupported в README) |

## Фаза 5 — документация

| Задача | Статус |
|---|---|
| README: убрать libboost-python-dev, добавить pybind11, описать Python-API `/py` | ⬜ |

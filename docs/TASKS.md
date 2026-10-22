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

## Фаза 0 — критические баги ✅ (завершена 2026-10-22)

| Задача | Где | Статус |
|---|---|---|
| Затенение `string buff` → бесконечный цикл longpoll | `src/lp.cpp:29-30` | ✅ |
| Саморекурсия `fs::rename` (нужен `::rename`) | `src/fs.cpp:97` | ✅ (runtime-тест safeWriteData — OK) |
| vox(): валидация WAV-заголовка, чтение `data[-1]` при пустом буфере | `src/cmds.cpp` | ✅ (711 реальных wav проверены; 2 битых теперь пропускаются) |
| Инициализация полей `Event`/`Doc`/`users::user` по умолчанию | `include/events.h`, `include/doc.h`, `include/users.h` | ✅ |
| Починить `operator=` (ссылка на локальную переменную) | `include/events.h`, `include/img.h` | ✅ |
| `getNumber()`: `mt19937`+`shuffle` вместо `srand(time)`/`random_shuffle` | `src/cmds.cpp` | ✅ |
| users: двойная запись `msgs`, хак `msgs--` в `setc` | `src/users.cpp`, `src/cmds.cpp` | ✅ (`getUser(..., inc=false)`) |
| Мелочь: `-Wall`-чистота (reorder, sign-compare, анонимные typedef) | `img.*`, `events.cpp`, `cmd.cpp`, заголовки | ✅ |

## Фаза 1 — сборка/Makefile: ❌ пропущена по решению владельца (сразу CMake)

## Фаза 1.5 — переход на CMake (вместо Makefile) ✅ (завершена 2026-10-22)

| Задача | Статус |
|---|---|
| `CMakeLists.txt`: конфигурация `VKBOT_WITH_PYTHON` ON/OFF | ✅ |
| find_package: CURL, OpenCV (5.0 найдена), Python3::Development, Boost.Python (config-режим + fallback) | ✅ |
| `GIT_URL`/`GIT_VER` через git на configure (fallback «unknown») | ✅ |
| `-Wall` для не-MSVC; CMP0167 (удалённый FindBoost в CMake 4) обработан | ✅ |
| Makefile → тонкий шим (`make`, `make NO_PYTHON=1`), объекты в `./build` | ✅ |
| CI: `.github/workflows/build.yml` — обе конфигурации на ubuntu-latest | ✅ (отправлен, ждёт первого прогона на GitHub) |
| Попутно (smoke-тест): голый `throw;` в `Vk` → `std::runtime_error`; try/catch в `main` | ✅ |

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

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

## Фаза 1 — сборка/Makefile

| Задача | Статус |
|---|---|
| Убрать boost-python (линуется pybind11 header-only) | ⬜ |
| pybind11-инклюды (`python3 -m pybind11 --includes`) | ⬜ |
| Автоопределение OpenCV (`/usr/include/opencv*`, pkg-config) | ⬜ |
| `-MMD -MP` (зависимости от заголовков) | ⬜ |
| `-Ofast` → `-O2` | ⬜ |

## Фаза 1.5 — переход на CMake (вместо Makefile)

| Задача | Статус |
|---|---|
| `CMakeLists.txt`: две конфигурации (`VKBOT_WITH_PYTHON` ON/OFF), опции как кэшированные переменные | ⬜ |
| автопоиск Python (`find_package(Python3 ... Development)`), OpenCV (`find_package(OpenCV ...)`), CURL | ⬜ |
| pybind11 через `find_package(pybind11)` / `python3 -m pybind11 --includes` | ⬜ |
| версии GIT_URL/GIT_VER через `git describe` на этапе configure (замена `-DGIT_URL/-DGIT_VER`) | ⬜ |
| цель-alinester предупреждений (компиляция с `-Wall` как регресс-проверка из `docs/TESTS.md`) | ⬜ |
| сборка в отдельном build-каталоге (инкрементальные зависимости «из коробки») | ⬜ |
| Makefile оставить тонким шимом над cmake (или удалить после проверки) | ⬜ |

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

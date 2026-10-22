# Задачи и статусы

Обновлено: 2026-10-23. Легенда: ✅ выполнено · 🚧 в работе · ⬜ не начато · ❌ не делаем

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
| CI: `.github/workflows/build.yml` — обе конфигурации на ubuntu-latest | ⏸ (файл готов; прогон вручную через `gh workflow run build.yml` — автоматика временно отключена) |
| Попутно (smoke-тест): голый `throw;` в `Vk` → `std::runtime_error`; try/catch в `main` | ✅ |

## Фаза 2 — Python-интеграция на pybind11 + расширение API ✅ (завершена 2026-10-23)

| Задача | Статус |
|---|---|
| `py.h`/`py.cpp` на pybind11: embedded module `vkcppbot`, `py::subinterpreter` RAII; PImpl (pybind11 не просачивается в другие TU) | ✅ |
| `gil_scoped_release` в главном потоке навсегда (`PyMainThread`) — без него deadlock | ✅ (регресс-тест T9) |
| Type casters: `table_t↔dict` (значения через str), `nlohmann::json↔py` (свои, `json.h` в pybind11 3.x удалён; binary_t нет в vendored json 3.4 — ветка bytes опущена) | ✅ `include/py_casters.h` |
| Ошибки скриптов: `pyF::exec` возвращает traceback (`error_already_set::what()`) | ✅ (тесты T2/T3) |
| try/catch в `Worker::work` (исключение скрипта не роняет пул) | ✅ |
| Расширение API: `vkcppbot.fs.*`, `vkcppbot.str.*`, `vkcppbot.timer.*`, `Doc()`, `Event.send()->dict`, read-only поля `Event`, `User`, `Net.send(url, dict)`, `uploadDoc/uploadPhoto` | ✅ |
| Регресс-тест `tests/py_integration.cpp` (реальный `pyF`, без VK-сети; цель `vkbot_pytest` + ctest) | ✅ 16/16 проверок (см. `TESTS.md`) |
| Попутно: `Vk(Net&)` развязан от сети, валидация токена — `Vk::validate()` (вызывает `Lp`); заодно убраны 5 повторяющихся API-вызова на потоки пула | ✅ |
| Попутно: некорректное определение `timer::getTime` вне класса | ✅ |
| Предпосылка: стандарт поднят до **C++14** (требование pybind11 3.x); C++17 — в фазе 3 | ✅ |

## Фаза 3 — оптимизации ✅ (завершена 2026-10-23)

| Задача | Статус |
|---|---|
| `dat.json`: `bd.touch()` в worker + `bd.saveIfDirty()` по таймеру (30 с) в `Lp::loop` вместо записи файла на каждое сообщение | ✅ (тест U3) |
| UTF-8 lower без `<codecvt>` (ручное декодирование/кодирование: ASCII + кириллица/Ё) | ✅ (тесты U4a–U4f) |
| Переход на `-std=c++17` | ✅ (после замены codecvt; `-Wall` чист) |
| `neon()`: векторный HSV (`img::neonColors`: cvtColor/split/merge) вместо попиксельного цикла; точность — сравнение с эталонным scalar-кодом в тесте (max diff ≤ 1 LSB) | ✅ (U7; 1280x720: 23ms → 13ms) |
| `crt()`/`neon()`: `std::move` вместо лишнего `clone()` в bilateral-циклах | ✅ |
| Ретраи Net: `netRetries=5` (цикл вместо бесконечной рекурсии, в `send` и `upload`) | ✅ (тест U1: нескладывающийся хост → пустой ответ за конечное время) |
| Ретраи Vk: цикл; error 6 — 5 повторов по 1 с; error 9/10 — по `request_delay` из ответа VK (макс. 60 с), до 3 повторов; раньше — `sleep(3600000)` + бесконечная рекурсия | ✅ |
| Попутно: `fs::file::read()` читает поток целиком (старый построчный вариант терял завершающий `\n` и портил бинарные данные) | ✅ (тест U6a) |
| Попутно: `timer::getTime` определена вне класса (фаза 2) | ✅ |

## Фаза 4 — Windows

| Задача | Статус |
|---|---|
| Windows-сборка | ❌ по решению владельца (только заметка unsupported в README) |
| Заметка «не поддерживается» в README | ✅ (2026-10-23) |

## Фаза 5 — документация

| Задача | Статус |
|---|---|
| README: убрать libboost-python-dev, добавить pybind11 | ✅ (фаза 2) |
| README: описать Python-API `/py` (globals, vkcppbot.fs/str/timer, классы, пример) | ✅ (2026-10-23) |
| AGENTS.md / docs: текущее состояние (pybind11, тесты, фазы) | ✅ |

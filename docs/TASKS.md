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

## Фаза 6 — кнопки ВКонтакте (клавиатуры) + кнопки в командах ✅ (завершена 2026-10-23)

| Задача | Статус |
|---|---|
| `Keyboard` — билдер JSON-клавиатур VK (text/callback/ссылка, цвета, rows, inline/one_time) | ✅ `include/keyboard.h` |
| `Event`: поля `keyboard` (приложение к ответу) и `event_id`; сериализация в `messages.send` (`keyboard`) | ✅ (U9, U10) |
| Обработка `message_event`: payload как полноценный ввод команды + `messages.sendMessageEventAnswer` | ✅ (U11) |
| Категории команд (`cmd_t::cat`) и справка с навигацией по разделам кнопками | ✅ (U12) |
| `/bc`: клавиатура «цифры 0–9 / стереть / проверить / сброс» с черновиком числа | ✅ |
| `/w`: кнопки выбора городов при пропущенном аргументе | ✅ |
| `/set`: кнопки уровней доступа при пропущенных аргументах | ✅ |
| pybind11: класс `Keyboard` и `Event.keyboard` для скриптов `/py` | ✅ (py T10) |
| Тестируемость: виртуальные `Net::send/upload`, перегрузка `Worker::work(event, net, vk)` | ✅ |
| Тесты U8–U12 + py T10 (FakeNet) | ✅ 38/38 + 18/18 |

## Фаза 7 — полноценное локальное тестирование (без сети) ✅ (завершена 2026-10-23)

| Задача | Статус |
|---|---|
| Инфраструктура mock-тестов: `FakeNet` (запись запросов, ответы по подстроке URL) в `tests/fakenet.h` | ✅ |
| Тесты парсинга longpoll-событий: message_new (photo/doc-вложения, fwd), wall_post_new→createComment, message_event | ✅ U13, U14, U9 |
| Тесты диспетчера `cmd::start`: неизвестная команда, права (`acess`), выполнение | ✅ U15 |
| Тесты `users::getUser/changeUser` (создание нового, инкремент msgs, inc=false) | ✅ U16 |
| Тесты `Doc` (`get()`, устойчивость к отсутствующему `date`), `game`, логики `/bc` кнопками, `/w`, `/set` | ✅ U17–U21 |
| Герметичность тестов (сброс `conf`/`bd`, повторные запуски) | ✅ |
| Тесты `Lp::loop` | ✅ решено (2026-10-23): не юнит-тестируется — фичи вызываются мимо цикла (`Worker::work`), longpoll проверяется только вживую (порядок — docs/TESTS.md) |
| Документирование покрытия в docs/TESTS.md | ✅ (68/68) |

## Фаза 9 — CI + живая проверка ✅ (CI завершена 2026-10-23; live-часть — ручная, порядок в docs/TESTS.md)

| Задача | Статус |
|---|---|
| Вернуть CI: `build.yml` — push/PR, apt-кэш OpenCV (`actions/cache`), `--no-install-recommends`, ретраи apt, `timeout-minutes` | ✅ 2026-10-23 |
| `ctest` обеих конфигураций в CI (unit 76 + py 18) | ✅ |
| Зависимость python-слоя в CI: `pybind11` из pip (apt 2.11 без subinterpreter), boost убран из установки | ✅ |
| CodeQL: actions v2→v3/v4, триггеры push/PR/schedule возвращены, зависимости для C++-автобилда | ✅ |
| Первый зелёный прогон CI на GitHub | ✅ 2026-10-23: Build 2м03с (ctest обеих конфигураций); сохранение apt-кэша починено (chown после sudo apt), в следующем прогоне подтверждён `Cache hit` (~185 МБ пакетов OpenCV) |
| AppVeyor | ✅ (решение) остаётся `.disabled` — Windows не поддерживается |
| Мажоры actions: checkout v7, cache v6, upload-artifact v7, codeql v4 (без Node 20 deprecation) | ✅ проверено зелёным прогоном |
| Живая проверка бота с реальными токенами VK (longpoll, кнопки, игры) | ⬜ нужен `config.json` с токенами |

## Фаза 10 — актуальный VK API + безопасное хранение токена ✅ (2026-10-23)

| Задача | Статус |
|---|---|
| Эндпоинт `api.vk.com` → `https://api.vk.ru/method/` (актуальный домен по dev.vk.ru), пинг в `/stat` — тоже на https/api.vk.ru | ✅ |
| Версия API `5.92` → `5.131` (актуальная по docs «Формат запросов») | ✅ |
| Токен не в репозитории: `config.json` покрыт `*.json` в .gitignore (проверено `git check-ignore`), README — предупреждение о секретах | ✅ |
| `user_token` сделан опциональным (нужен только `/video` и аудиосообщения); без него — warning вместо отказа стартовать; smoke-сообщение: `Vk: не задан token...` | ✅ |
| Тесты: URL-ожидания обновлены на api.vk.ru; U7b (бенчмарк) — минимум из 3 замеров, чтобы CI-шум не ронял тест | ✅ |
| Живой запуск с боевым ключом сообщества (vk_cpp_bot, id 178228638): `getTokenPermissions` ок, longpoll-сессия стоит | ✅ 2026-10-23 |
| Живая проверка вскрыла `WORK ERROR type_error.302`: парсинг сообщений без текста (`text: null`) и `Doc` с неполными вложениями (photo без `sizes` — UB на `ext`) | ✅ исправлено, тест U23 |
| Живая проверка-2: бот молчал после поднятия API — с 5.103 `object` у `message_new` обёрнут в `{ message, client_info }`; парсер спускается во вложенный `message`, `date` guarded | ✅ исправлено, тест U24 (83/83) |

## Фаза 8 — структурный рефакторинг (поведение не меняется) ✅ (завершена 2026-10-23)

| Задача | Статус |
|---|---|
| Явные параметры вместо макроса `cmdHead` (обновить AGENTS.md) | ✅ 8.1 (`include/cmds.h`) |
| Разбивка `cmds.cpp` на модули (interface/games/media/utils/admin) | ✅ 8.1 (`src/cmds_*.cpp`) |
| `const`-корректность, const-ссылки в параметрах, единый стиль заголовков | ✅ 8.3 (Net/Vk/users/fs/str/Doc/BD/img/worker) |
| Убрать `goto` в `gameF`/`apod`/`bc`; `game*` new/delete → значение/unique_ptr | ✅ 8.2 (+RAII-блокировки, shared_ptr) |
| Имена (права в `cmd::start` по входящему событию, `net/vk` скриптам из `eventIn`) и устранить дубли копирования `Event` | ✅ 8.3 (`copyBody`) |
| Новый тест конвейера `/g` (создание/вход/ходы/стоп) | ✅ U22 |
| Обновить docs/ARCHITECTURE.md под новую структуру | ✅ (этот документ) |

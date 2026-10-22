# vk-cpp-bot — AGENTS.md

Бот для групп ВКонтакте на C++ (Linux-first), с встроенным интерпретатором Python для команды `/py`.
Комментарии и пользовательские строки — на русском; новый код придерживается этого стиля.

## Git-процесс

- Рабочая ветка ИИ-оптимизации: **`ai-optimization`** (в `origin`,
  `git@github.com:edombek/vk-cpp-bot.git`). Все исправления и нововведения — коммитить и
  пушить в неё; `master` не трогать без явного запроса.
- Каждый завершённый кусок работ (фаза/задача из `docs/TASKS.md`) — отдельный коммит с
  русским описанием + пуш в `ai-optimization`.

## Рабочая документация (вести при работе)

- `docs/TASKS.md` — задачи и их статусы (обязательно обновлять при выполнении/изменении плана).
- `docs/TESTS.md` — состояние сборок/проверок, команды воспроизведения, план тестирования.
- `docs/ARCHITECTURE.md` — блочная схема и описание потоков выполнения.

## Сборка (фаза 1.5: CMake; Makefile — шим)

```
git submodule update --init vox thr   # vox = wav-файлы для /vox, thr = ThreadPool
make                                  # шим: cmake -B build + build, с Python
make NO_PYTHON=1                      # то же без Python
```

- Требуется: cmake ≥3.16, libcurl-dev, OpenCV (core/imgproc/imgcodecs/objdetect/photo,
  любые /usr-инсталляции находятся через find_package), Python dev, Boost.Python
  (фаза 2 заменит его на pybind11 — header-only).
- Все объекты и `vkbot` — в `./build/`; `build/` в .gitignore.
- `GIT_URL`/`GIT_VER` определяются cmake на configure (`git` в PATH; при его отсутствии —
  «unknown»).
- **`-DNO_PYTHON` меняет состав классов** (`Vk::send(dict)` есть только с Python) — CMake
  сам пересобирает при переключении `VKBOT_WITH_PYTHON`, но смешивать старые объекты вручную
  нельзя.
- **CI временно ОТКЛЮЧЕНА (2026-10-22, решение владельца)**: `build.yml` и CodeQL переведены
  в `workflow_dispatch` (ручной запуск: `gh workflow run build.yml`), `.appveyor.yml` →
  `.appveyor.yml.disabled`. Причина зависания Build: apt-установка OpenCV на раннере.
  До включения — обязательная локальная проверка обеих конфигураций (`make`,
  `make NO_PYTHON=1`). Автоматические триггеры закомментированы в файлах, раскомментируются
  при возврате.
- Windows (`.appveyor.yml`, `*.vcxproj`, `win32deps`) рассчитан на устаревший стек
  (VS2015, boost 1.68, OpenCV 2.4, x86) и сейчас **не поддерживается** — не чинить без явного запроса.

## Проверка изменений

Тестов и линтера нет; CI — только CodeQL. Единственная верификация — успешная компиляция и
линковка обеих конфигураций (`make` и `make NO_PYTHON=1`). Запустить бота end-to-end нельзя
без реальных токенов VK в `config.json` (`token`, `user_token`) и созданного `dat.json`
(см. README). Для проверки Python-слоя пишите изолированные мини-тесты (пример архитектуры —
встраивание + под-интерпретатор в потоках), не запуская бота целиком.

## Архитектура (неочевидно с первого взгляда)

- Поток: `src/main.cpp` (глобальные `conf`=`config.json`, `bd`=`dat.json`) → `Lp::loop()`
  (longpoll) → `Worker::work()` (пул из 5 потоков, `thr/ThreadPool.h`) → `cmd::start()`
  (таблица команд в `src/cmd.h`) → функции в `src/cmds.cpp`.
- **Макрос `cmdHead`** (`include/cmd.h`) раскрывается в `Event &eventIn, Event &eventOut,
  Worker *worker` — функции команд объявляются как `void help(cmdHead)` без видимых параметров.
  Это конвенция, а не ошибка.
- В `src/worker.cpp` — `thread_local Net net; thread_local Vk vk(net);` на каждый поток пула.
- Изменение глобальных `conf`/`bd` требует блокировки их `lock` (mutex) и вызова `save()`;
  образец — `users::getUser`, `weather`, `setCfg`.
- `apod.py` — отдельный standalone-скрипт (краон-постинг), не часть бота.

## Python-интеграция (`/py`) — pybind11

- Реализация (фаза 2): pybind11, embedded-модуль `vkcppbot` (`src/py.cpp`), каждый `/py` —
  новый под-интерпретатор (`py::subinterpreter`) с изолированным namespace. PImpl в
  `include/py.h`: pybind11 не просачивается в другие TU.
- Скриптам доступны: `eventIn`/`eventOut` (события; read-only поля защищены), `net`, `vk`,
  модули `vkcppbot.fs`, `vkcppbot.str`, `vkcppbot.timer`, классы `Doc`/`User`/`Event`.
- Кастеры `table_t↔dict` и `json↔py` — в `include/py_casters.h` (значения dict приводятся
  через str() — нестроковые аргументы больше не роняют бота).
- Критический инвариант: `PyMainThread` удерживает `gil_scoped_release` навсегда; без него
  создание под-интерпретатора из worker-потока — deadlock (закрыт тестом T9 в ctest).
- `Vk(Net&)` не трогает сеть; сетевая валидация токена — `Vk::validate()` (вызывает Lp).
  Это же позволяет тестам строить Vk без сети.

## Ловушки (актуальный статус)

- Исправлено в фазе 0 (2026-10): бесконечный цикл longpoll при пустом ответе (`lp.cpp`),
  саморекурсия `fs::rename`, невалидный парсинг WAV в vox, неинициализированные поля
  `Event`/`Doc`/`user`, висячие `operator=`, `srand(time)` в генераторе чисел.
  История — `docs/TASKS.md`, фаза 0.
- Исправлено в фазе 2 (2026-10): `Worker::work` в try/catch (исключение скрипта/сети не
  роняет поток пула); `terminate` при битом config.json (нормальные `runtime_error` +
  try/catch в `main`); нестроковые значения в dict-аргументах VK/Net (кастер через str).
- Стандарт C++14: pybind11-требование; `<codecvt>` ещё жив (замена в фазе 3 вместе с C++17).

Аудит-отчёт с полным списком: `/home/edombek/.opencode/plan/BUILD-RESULTS.md` (вне репозитория).

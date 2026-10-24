# vk-cpp-bot — AGENTS.md

Бот для групп ВКонтакте на C++ (Linux-first), с встроенным интерпретатором Python для команды `/py`.
Комментарии и пользовательские строки — на русском; новый код придерживается этого стиля.

## Git-процесс

- Рабочая ветка ИИ-оптимизации: **`ai-optimization`** (в `origin`,
  `git@github.com:edombek/vk-cpp-bot.git`). Все исправления и нововведения — коммитить и
  пушить в неё; `master` не трогать без явного запроса.
- Каждый завершённый кусок работ (фаза/задача из `docs/TASKS.md`) — отдельный коммит с
  русским описанием + пуш в `ai-optimization`.
- **Каждый коммит содержит трейлер `Co-Authored-By: Qwen (AI assistant) <noreply@qwen.ai>`**
  (работы ведутся ИИ; по согласованию с владельцем репозитория).

## Рабочая документация (вести при работе)

- `docs/TASKS.md` — задачи и их статусы (обязательно обновлять при выполнении/изменении плана).
- `docs/TESTS.md` — состояние сборок/проверок, команды воспроизведения, план тестирования.
- `docs/ARCHITECTURE.md` — блочная схема и описание потоков выполнения.

## Сборка (фаза 1.5: CMake; Makefile — шим)

```
git submodule update --init vox   # vox = wav-файлы для /vox (ThreadPool — уже в include/)
make                                  # шим: cmake -B build + build, с Python
make NO_PYTHON=1                      # то же без Python
```

- Требуется: cmake ≥3.16, libcurl-dev, OpenCV (core/imgproc/imgcodecs/objdetect/photo,
  любые /usr-инсталляции находятся через find_package), Python dev, **pybind11 из pip**
  (в apt-версии нет `py::subinterpreter` — CMake подхватывает pip через `python3 -m pybind11 --includes`).
- Все объекты и `vkbot` — в `./build/`; `build/` в .gitignore. `make NO_PYTHON=1` — по
  умолчанию в отдельном каталоге `build-nopy` (основной `build/` не переконфигурирует).
- `GIT_URL`/`GIT_VER` определяются cmake на configure (`git` в PATH; при его отсутствии —
  «unknown»).
- **`-DNO_PYTHON` меняет состав классов** (`Vk::send(dict)` есть только с Python) — CMake
  сам пересобирает при переключении `VKBOT_WITH_PYTHON`, но смешивать старые объекты вручную
  нельзя.
- **CI включена (2026-10-23)**: `build.yml` — push в `master`/`ai-optimization` и PR в
  `master`; ставит зависимости через apt с кэшем `actions/cache` (`/var/cache/apt/archives`,
  OpenCV — самая тяжёлая зависимость), `--no-install-recommends`, `Acquire::Retries` и
  `timeout-minutes` (прежнее зависание apt больше не блокирует прогон). Обе конфигурации
  собираются и прогоняют `ctest`. CodeQL (actions v3) — push/PR/schedule. Зависимость
  python-слоя на раннере — `pybind11` из pip (в apt-версии нет `py::subinterpreter`;
  boost не нужен). `.appveyor.yml` остаётся `.disabled` (Windows не поддерживается).
  Локальная проверка обеих конфигураций — по-прежнему обязательна перед пушем.
- Windows (`.appveyor.yml`, `*.vcxproj`, `win32deps`) рассчитан на устаревший стек
  (VS2015, boost 1.68, OpenCV 2.4, x86) и сейчас **не поддерживается** — не чинить без явного запроса.

## Проверка изменений

- Автотесты: `make test` (шим для `ctest`) — две цели: `vkbot_unit` (C++-слой:
  Net-ретраи, BD dirty, str/fs, эталонное сравнение `img::neonColors`) и
  `vkbot_pytest` (python-слой через реальный `pyF`, без сети). `vkbot_unit`
  строится и работает и в `NO_PYTHON`; `vkbot_pytest` — только с Python.
  Подробности и список проверок — `docs/TESTS.md`.
- Кроме тестов обязательна успешная компиляция и линковка обеих конфигураций
  (`make` и `make NO_PYTHON=1`) с чистым `-Wall` (кроме точечного `-Wno-attributes`
  на `src/py.cpp`). Линтера нет; статанализ — CodeQL в CI.
- **Токены в репозиторий не коммитим**: `config.json`/`dat.json` покрыты `*.json`
  в `.gitignore` и никогда не добавляются в git. VK API: `https://api.vk.ru/method/`,
  версия `5.131` (`ver` в `src/vk.cpp`). Обязателен только `token` (ключ сообщества);
  `user_token` опционален — без него бот предупреждает и работает (кроме `/video`
  и аудиосообщений).
- `./tools/vk_user_token.py` — получение user-токена VK (auth code flow через
  `http://localhost:8765/callback` или `--paste` implicit) и сохранение в
  `config.json["user_token"]`; `--check` проверяет текущий. Нужен client_id/secret
  VK-приложения (https://vk.com/apps?act=manage — «Создать приложение»); значения
  кэшируются в config.json
  (`vk_client_id`/`vk_client_secret`, в git не попадают). Этим токеном можно
  автоматизировать живые проверки бота (писать сообществу от имени пользователя).
- Запустить бота end-to-end нельзя без реального ключа сообщества в `config.json`.
  Smoke-проверка (с временно перенесённым `config.json`): `./build/vkbot` печатает
  `FATAL: Vk: не задан token...` и выходит с кодом 1. Новые тесты добавляйте в
  `tests/units.cpp` / `tests/py_integration.cpp` (CTest-registrация в CMakeLists.txt),
  а не отдельными скриптами.
- **Живые проверки** (то, что юнитами не покрыто — longpoll, реальные API):
  `./tools/live_check.py --self "/stat" "/w Москва" ...` — публикует комментарии-команды
  токеном самого сообщества под закреплённым постом 40581 с маркером «@@ » (бот принимает
  только свои комментарии с маркером — U36; без маркера свои комментарии игнорируются — U35),
  ответ бота читается из его stderr-лога (строка `└─> …`; `wall.getComments` group-токеном
  закрыт). Автоигра «Быки и коровы» через комментарии — `./tools/bc_play.py`. Комментарии
  чекера group-токеном не удаляются — чистить вручную (ссылки печатает чекер).

## Архитектура (неочевидно с первого взгляда)

- Поток: `src/main.cpp` (глобальные `conf`=`config.json`, `bd`=`dat.json`) → `Lp::loop()`
  (longpoll) → `Worker::work()` (пул потоков, `include/thread_pool.h + src/thread_pool.cpp` — часть проекта,
  размер = `pool_size` из `config.json`, default 5) → `cmd::start()`
  (таблица команд в `src/cmd.cpp`) → функции в `src/cmds_*.cpp` (модули по категориям).
- **Кнопки VK (фаза 6)**: `Keyboard` (`include/keyboard.h`) собирает JSON-клавиатуру,
  `Event::keyboard` прикрепляет её к `messages.send`. Нажатие callback-кнопки приходит
  событием `message_event` (payload = команда, `event_id`); `Worker::workButton` отвечает
  `messages.sendMessageEventAnswer` и диспетчеризует payload как обычную команду.
- **Команды в комментариях стены (фаза 11)**: событие `wall_reply_new` диспетчеризуется как
  обычная команда, ответ — `wall.createComment` с `reply_to_cid` (в ветку комментария). Свои
  комментарии бот игнорирует, кроме помеченных «@@ » (режим самопроверки `live_check.py --self`,
  маркер вырезается). Текст ответа печатается в stderr (`└─> …`). `user.id`/`peer_id`/`cid` —
  знаковый `int64_t` (сообщества отрицательные, id диалогов 2026 ~2·10¹¹).
- **Функции-команды** объявлены в `include/cmds.h` с явными параметрами
  `Event &eventIn, Event &eventOut, Worker *worker` (макрос `cmdHead` убран в фазе 8);
  реализации разнесены по `src/cmds_{interface,utils,games,media,admin,py}.cpp`,
  таблица команд и `/help` — `src/cmd.cpp`.
- В `src/worker.cpp` — `thread_local Net net; thread_local Vk vk(net);` на каждый поток пула;
  `Worker::work(event, net, vk)` — публичная перегрузка с явной сетью (её используют тесты).
- Изменение глобальных `conf`/`bd` требует блокировки их `lock` (mutex); `conf` после правки
  сохраняется `save()` сразу, `bd` — только `touch()`, а пишет его пакетом `Lp::loop`
  (`saveIfDirty()` раз в 30 с).
- `Net::send/upload` виртуальные: тесты подменяют сеть моком `FakeNet` (`tests/fakenet.h`),
  прогоняя весь конвейер команд без сети и токенов.
- `apod.py` — отдельный standalone-скрипт (краон-постинг), не часть бота.

## Python-интеграция (`/py`) — pybind11

- Реализация (фаза 2): pybind11, embedded-модуль `vkcppbot` (`src/py.cpp`), каждый `/py` —
  новый под-интерпретатор (`py::subinterpreter`) с изолированным namespace. PImpl в
  `include/py.h`: pybind11 не просачивается в другие TU.
- `print()` в скриптах отправляет отдельные сообщения (`ScriptPrinter` в `py.cpp`,
  преамбула переопределяет print и sys.stdout). Флуд-контроль: `py_print_interval_ms`
  (default 1000 мс) и `py_print_max_msgs` (default 30 на скрипт, потом уведомление
  об обрезке); `print(..., file=...)` не перехватывается; `pyF::printsSent()` —
  сколько послано (pycmd по нему подавляет лишний «done!»).
- Скриптам доступны: `eventIn`/`eventOut` (события; read-only поля защищены, `keyboard` —
  JSON-строка клавиатуры), `net`, `vk`, модули `vkcppbot.fs`, `vkcppbot.str`, `vkcppbot.timer`,
  классы `Doc`/`User`/`Event`/`Keyboard`.
- Кастеры `table_t↔dict` и `json↔py` — в `include/py_casters.h` (значения dict приводятся
  через str() — нестроковые аргументы больше не роняют бота).
- `/pywiki` — справка по `/py`, реализована самим python-движком: текст разделён на
  print-секции в `pyWikiScript()` (`py.cpp`), команда в `cmds_admin.cpp` (доступ 1).
- Критический инвариант: `PyMainThread` удерживает `gil_scoped_release` навсегда; без него
  создание под-интерпретатора из worker-потока — deadlock (закрыт тестом T9 в ctest).
- `Vk(Net&)` не трогает сеть; сетевая валидация токена — `Vk::validate()` (вызывает Lp).
  Это же позволяет тестам строить Vk без сети.

## Ловушки (актуальный статус)

- Формат callback-кнопок (стр. 911 при нарушении): `payload` — только JSON-строка в
  фигурных скобках ≤255 (у нас `{"cmd": "<команда>"}`, генерируется в `Keyboard::button`);
  ≤5 кнопок в ряду, пустые ряды запрещены (билдер чинит сам); label ≤40 символов.
  Ответ на нажатие — `messages.sendMessageEventAnswer` с параметром `event_data` (не `answer`).
- С API ≥ 5.103 `object` события `message_new` — обёртка `{ message, client_info }`
  (не плоское сообщение, как было до 5.103): `events.cpp` спускается в `lpEv["message"]`;
  `client_info` (возможности клиента: keyboard/inline/carousel) пока не используется.
- Исправлено в фазе 0 (2026-10): бесконечный цикл longpoll при пустом ответе (`lp.cpp`),
  саморекурсия `fs::rename`, невалидный парсинг WAV в vox, неинициализированные поля
  `Event`/`Doc`/`user`, висячие `operator=`, `srand(time)` в генераторе чисел.
  История — `docs/TASKS.md`, фаза 0.
- Исправлено в фазе 2 (2026-10): `Worker::work` в try/catch (исключение скрипта/сети не
  роняет поток пула); `terminate` при битом config.json (нормальные `runtime_error` +
  try/catch в `main`); нестроковые значения в dict-аргументах VK/Net (кастер через str).
- Стандарт C++17 (фаза 3): `<codecvt>` заменён ручной UTF-8 реализацией в `str.cpp`
  (понижается регистр ASCII и кириллицы/Ё, остальное pass-through).
- `bd` (dat.json) сохраняется пакетом: команды вызывают `bd.touch()`, `Lp::loop`
  сбрасывает изменения `saveIfDirty()` раз в 30 с. Ретраи Net/Vk конечны (см. TASKS, фаза 3).
- **Загрузка фото, VK 2026**: `photos.getMessagesUploadServer` отдаёт `photo` в новом формате —
  JSON-строка `[{"photo":"<токен>:x",…}]`; `saveMessagesPhoto` принимает её **как есть**
  (вложенный токен отдельно — «Invalid hash»). Upload-сервер периодически возвращает пустой
  `photo` — `uploadPhoto` повторяет попытку и не шлёт пустой токен (U40). Формат
  `docs.getMessagesUploadServer` не менялся (`file` — токен). Приложить фото к комментарию
  group-токеном нельзя: `getMessagesUploadServer` с сообщественным peer → 901,
  `getWallUploadServer` → 27 (user-only) — арт-команды отвечают объяснением.
- **Group-токены в 2026**: закрыт доступ к чужим стенам (`wall.createComment` → 15,
  `wall.post` → 214 — только своё сообщество); `wall.getComments`/`wall.deleteComment`/
  `comments.*` group-авторизацией не работают. Погода `/w` — wttr.in (без токена;
  `openweathermap_token` более не нужен и из README/кода убран).
- **Python-аддоны (фаза 12, ветка `python-commands`)**: `cmd::loadAddons()` регистрирует
  `py_addons/*.py` (`cmd/info/acess/cat` + `run(eventIn, eventOut, net, vk)`); каждый вызов —
  свежий под-интерпретатор (`pyF::execAddon`). Таблица `cmdsList` под мьютексом,
  `msg_func_t` — `std::function`; C++-функция `/rename` переименована в `renameUser`
  (`rename` конфликтовал с POSIX `::rename` из `<cstdio>/<filesystem>` — GCC ругался
  невнятным «could not convert» на всю инициализацию таблицы). Временный под-интерпретатор
  в `pyAddonMeta` обязан быть объявлен **до** `try` (внутри try раскрутка стека убивает его
  раньше catch с `error_already_set` — double free). Const `operator[]` nlohmann по
  отсутствующему ключу — UB в Release (пойман флaky SEGFAULT юнитов в `apodReply`; такие
  поля читать через `find()`). В python `net.send`/`fs.readData` отдают `bytes` для не-UTF-8;
  `Doc.get()` — строка attachment, данные вложения — `img.download(doc, net)`/`net.send(doc.url)`.

Аудит-отчёт с полным списком: `/home/edombek/.opencode/plan/BUILD-RESULTS.md` (вне репозитория).

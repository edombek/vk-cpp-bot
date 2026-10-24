# VK C++ BOT [![Build](https://github.com/edombek/vk-cpp-bot/actions/workflows/build.yml/badge.svg)](https://github.com/edombek/vk-cpp-bot/actions/workflows/build.yml) [![CodeQL](https://github.com/edombek/vk-cpp-bot/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/edombek/vk-cpp-bot/actions/workflows/codeql-analysis.yml)

Бот для групп платформы ВКонтакте (Linux-first, C++17), с встроенным интерпретатором
Python (`/py`), кнопками VK, играми, медиа-фильтрами и астрофото (NASA APOD).

## Установка зависимостей

Для Ubuntu/Debian:
```
sudo apt install git cmake make libcurl4-openssl-dev python3-dev python3-pip \
    libopencv-core-dev libopencv-imgproc-dev libopencv-imgcodecs-dev \
    libopencv-objdetect-dev libopencv-photo-dev
python3 -m pip install pybind11
```
⚠ pybind11 нужен **из pip**: в apt-версии (2.11) нет `py::subinterpreter` —
CMake подхватит pip-пакет через `python3 -m pybind11 --includes`.

Для ArchLinux:
```
sudo pacman -S git cmake make curl opencv python python-pip
python3 -m pip install pybind11
```

## Сборка

```
git submodule update --init vox
cmake -S . -B build -DVKBOT_WITH_PYTHON=ON
cmake --build build
```

Для сборки без python: `-DVKBOT_WITH_PYTHON=OFF`.

Привычный `make` тоже работает — он теперь тонкий шим над cmake
(`make`, `make NO_PYTHON=1` — без Python в отдельном каталоге `build-nopy`);
артефакты в `./build`.

Зависимости для cmake: `cmake`, `libcurl-dev`, OpenCV (компоненты
core imgproc imgcodecs objdetect photo — находятся через `find_package`, любая
/usr-инсталляция, включая OpenCV 5), для Python-слоя — pybind11 (pip).

## Тесты

```
make test    # шим для ctest
```

Две цели (сеть и токены не нужны, VK API подменён моком `FakeNet`):

- `vkbot_unit` — C++-слой: U1–U40 (137 проверок: Net/BD/str/fs, клавиатуры,
  парсинг событий 2026, игры, арт-команды, загрузка фото) — строится и в `NO_PYTHON`;
- `vkbot_pytest` — python-слой через реальный `pyF`: T1–T16 (27 проверок,
  включая регрессию на GIL-deadlock) — только с Python.

Подробности — `docs/TESTS.md` и `tests/README.md`. Беглая smoke-проверка запуска:
без `config.json` бот печатает `FATAL: Vk: не задан token...` и выходит с кодом 1.

## Команды бота

Полный список с правами — `/help` (разделы кнопками или текстом: `/утилиты`, `/все`).

| Раздел | Команды |
|---|---|
| интерфейс | `/help`, `/stat`, `/rename`, `/pywiki` |
| утилиты | `/w` — погода (wttr.in, без API-ключей) |
| игры | `/g` — «линксы» 4×4, `/bc` — «Быки и коровы» (клавиатура цифр) |
| медиа | `/pix` (8bit art), `/crt` (cartoon), `/neon` (неон), `/vox` (Half-Life), `/vid` (ур. 3, нужен `user_token`) |
| космос | `/apod`, `/apoddate` (дата/ДР из профиля), `/apodrand` |
| админ (ур. 5) | `/py` — python-скрипт, `/set` — права, `/setc` — конфиг, `/con` — консоль, `/u` — загрузка файла |

Арт-команды берут картинку из вложения (фото/стикер сообщения или комментария),
а без вложения — фото профиля автора. Работают и в личных сообщениях, и в
комментариях к записям стены (событие `wall_reply_new`, ответ в ветку комментария;
фото под стеной приложить нельзя политикой VK — бот подскажет).

## Настройка

config.json:
```
{
    "token": "ключ доступа сообщества (обязателен; API https://api.vk.ru/method/, v=5.131)",
    "user_token": "ключ пользователя — опционален, нужен только для /vid и аудиосообщений",
    "pool_size": 5,
    "py_print_interval_ms": 1000,
    "py_print_max_msgs": 30
}
```

- `pool_size` — размер пула worker-потоков (default 5);
- `py_print_*` — флуд-контроль print() в `/py`-скриптах;
- ключ `checker_token` используется только инструментами проверки (см. ниже), ботом не читается;
- ключ `openweathermap_token` более не нужен: `/w` работает через wttr.in без ключей.

ВНИМАНИЕ: `config.json` содержит секреты и не должен попадать в репозиторий
(покрыт `.gitignore`). Ключ сообщества — «Управление → Дополнительно → Работа с API».

Права создателя задаются в dat.json (потом — через `/set`):
```
{
    "users": {
        "323871959": {
            "acess": 5,
            "msgs": 0,
            "name": "edombek"
        }
    }
}
```

## Инструменты (`tools/`)

- `live_check.py` — живые проверки команд через комментарии под закреплённым
  постом: `python3 tools/live_check.py --self "/stat" "/w Москва"` (режим
  самопроверки токеном своего сообщества с маркером «@@ »);
- `bc_play.py` — автоигра «Быки и коровы» с ботом через комментарии;
- `vk_user_token.py` — получение/проверка user-токена VK (`--check`);
- `apod.py` — standalone-скрипт краон-постинга (не часть бота).

## Команда `/py` — Python API

Каждый скрипт `/py` выполняется в изолированном под-интерпретаторе: глобалы между
вызовами не сохраняются. Ошибки скрипта возвращаются в чат как traceback, бот при
этом не падает. Скрипту доступны глобалы `eventIn`, `eventOut`, `net`, `vk` и
модуль `vkcppbot`:

- `eventIn` / `eventOut` (класс `Event`): `msg` (string, можно менять), `type`,
  `from_id`, `peer_id`, `id`, `timestamp`, `is_chat`, `words` (list), `user` — read-only;
  `docs`/`fwds` (списки `Doc`/`Event`), `keyboard` (string — JSON-клавиатура, см. ниже);
  методы `send()` → dict ответа VK, `pysend()` → JSON-строка, `getOut()` → новый `Event`-ответ
- `vk.send(method, args={}, user=False)` → dict (ответ VK API; `user=True` — юзер-токеном)
- `net.send(url)` / `net.send(url, params: dict, post=True)` / `net.upload(url, filename, data)`
- `vkcppbot.fs`: `exists(path)`, `readData(path)`, `writeData(path, data)`
- `vkcppbot.str`: `words(s, delim=' ')`, `low(s)` (ASCII+кириллица), `summ(list, start)`,
  `replase(s, find, replace)`
- `vkcppbot.timer`: `sleep(мс)`, `getWorkTime()` — аптайм бота (`Xd:hh:mm:ss`)
- классы: `Doc()` (поля `type/owner_id/doc_id/acess_key/ext/url/timestamp`,
  `get()`, `uploadDoc(filename, data, net, vk, peer_id, audio_message=False)`,
  `uploadPhoto(...)`), `User` (`name/id/acess/msgs`),
  `Keyboard(one_time=False, inline=False)` — клавиатура VK: `button(label, payload, color)`
  (callback-кнопка, payload — команда боту), `textButton(label)`, `link(label, url)`,
  `row()`, `dump()` → JSON для `eventOut.keyboard`

Нажатия callback-кнопок приходят боту как событие `message_event`: бот отвечает
`messages.sendMessageEventAnswer` и обрабатывает payload как обычную команду.

Пример:

```
/py import vkcppbot
w = vkcppbot.str.words(eventIn.msg)
eventOut.msg = "Привет, " + eventIn.user.name + "! Ты написал " + str(len(w) - 1) + " слов."
```

`print()` в скрипте отправляет отдельное сообщение в чат (обёртка над print;
`sys.stdout` тоже перехвачен, `print(..., file=...)` — нет). Флуд-контроль:
не чаще одного сообщения в `py_print_interval_ms` мс (default 1000) и не более
`py_print_max_msgs` (default 30) на скрипт — значения в `config.json`.

Справка по возможностям `/py` — команда `/pywiki` (разделы уходят отдельными
сообщениями; текст вики — `pyWikiScript()` в `py.cpp`).

## Windows — не поддерживается

`.vcxproj`, AppVeyor и `win32deps` рассчитаны на устаревший стек (VS2015, boost 1.68,
OpenCV 2.4, x86) и остаются только как историческое наследие — починка не планируется.
Для справки (на свой риск): поместить python3 в PATH, выполнить `winlib.bat`, собрать
проект в VS; для запуска вне VS скопировать `*.exe`, `vox` и `win32deps/libs` рядом с ботом.

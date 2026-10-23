# VK C++ BOT [![Windows Build Status](https://ci.appveyor.com/api/projects/status/github/edombek/vk-cpp-bot?svg=true)](https://ci.appveyor.com/project/edombek/vk-cpp-bot) [![CodeQL](https://github.com/edombek/vk-cpp-bot/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/edombek/vk-cpp-bot/actions/workflows/codeql-analysis.yml)

Бот для групп платформы ВКонтакте.

## Установка зависимостей

Для Ubuntu/Debian:
```
git cmake make libcurl4-openssl-dev pybind11-dev python3-dev python3-pip libopencv-imgcodecs-dev libopencv-imgproc-dev libopencv-objdetect-dev libopencv-photo-dev libblas-dev liblapack-dev libdlib-dev python-dev-is-python3
````

Для ArchLinux:
```
git cmake make curl opencv pybind11 python-pip python dlib(aur или вручную)
```

## Сборка

```
git submodule update --init vox
cmake -S . -B build -DVKBOT_WITH_PYTHON=ON
cmake --build build
```

Для сборки без python: `-DVKBOT_WITH_PYTHON=OFF`.

Привычный `make` тоже работает — он теперь тонкий шим над cmake
(`make`, `make NO_PYTHON=1`); артефакты в `./build`.

Зависимости для cmake: `cmake`, `libcurl-dev`, OpenCV (>=3.4, компоненты
core imgproc imgcodecs objdetect photo), для Python-слоя — pybind11
(`pybind11-dev` или `pip install pybind11`).

## Тесты

```
make test    # ctest: интеграционный тест python-слоя (сеть не требуется)
```

## Windows — не поддерживается

`.vcxproj`, AppVeyor и `win32deps` рассчитаны на устаревший стек (VS2015, boost 1.68,
OpenCV 2.4, x86) и остаются только как историческое наследие — починка не планируется.
Для справки (на свой риск): поместить python3 в PATH, выполнить `winlib.bat`, собрать
проект в VS; для запуска вне VS скопировать `*.exe`, `vox` и `win32deps/libs` рядом с ботом.

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

## Настройка
config.json:
```
{
    "openweathermap_token": "получите токен на сайте openweathermap.org (требуется для команды /w)",
    "token": "ключ доступа сообщества (API https://api.vk.ru/method/, v=5.131)",
    "user_token": "ключ пользователя — опционален, нужен только для /video и аудиосообщений"
}

ВНИМАНИЕ: `config.json` содержит секреты и не должен попадать в репозиторий
(покрыт `.gitignore`). Ключ сообщества — «Управление → Дополнительно → Работа с API».

```

Выставляем права для создателя(после можно выставлять через /set с помощью самого бота)
<br>dat.json:
```
{
    "users": {
        "id админа": {
            "acess": 5,
            "msgs": 0,
            "name": "Имя"
        }
    }
}
```
например:
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

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
git submodule update --init vox thr
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

Windows
```
установите python3 в глобальный PATH
run winlib.bat
и собирите проект в VS2015 и выше
для запуска не в VS скопируйте *.exe vox и win32deps/libs в рабочую папку бота
```

## Настройка
config.json:
```
{
    "openweathermap_token": "получите токен на сайте openweathermap.org (требуется для команды /w)",
    "token": "токен группы, версия LP 6.92",
    "user_token": "токен админа группы для работы со стеной"
}

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

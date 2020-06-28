# VK C++ BOT

Бот для групп платформы ВКонтакте.

## Установка зависимостей

Для Ubuntu/Debian:
```
git gcc make libcurl4-openssl-dev libboost-python-dev python3-dev python3-pip libopencv-imgcodecs-dev libopencv-imgproc-dev libopencv-objdetect-dev libopencv-photo-dev libblas-dev liblapack-dev libdlib-dev
````

Для ArchLinux:
```
git gcc make curl opencv boost python-pip python dlib(aur или вручную)
```

## Сборка

```
git submodule update --init vox thr
make
```

Для сборки без python:
```
make NO_PYTHON=1
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

# Блочная схема проекта

Обновлено: 2026-10-22. Схема соответствует текущему коду (`src/`, `include/`).

## Основной поток данных

```mermaid
flowchart LR
    subgraph bot ["Процесс vkbot (src/main.cpp)"]
        MAIN["main()<br/>global conf=config.json<br/>global bd=dat.json"]
        LP["Lp (src/lp.cpp)<br/>longpoll-цикл<br/>groups.getLongPollServer"]
        W["Worker (src/worker.cpp)<br/>ThreadPool(5) из thr/<br/>thread_local Net+Vk"]
        EV["Event (src/events.cpp)<br/>разбор update, fwds, docs"]
        CMD["cmd::start (src/cmd.h)<br/>таблица cmdsList (src/cmd.cpp)"]
        CMDS["Команды (src/cmds.cpp)<br/>/help /w /py /pix /vox /bc ..."]
        PY["pyF (src/py.cpp)<br/>Python-скрипт /py<br/>под-интерпретатор на вызов"]
        USERS["users (src/users.cpp)<br/>уровни доступа, счётчики"]
        BD[("BD (src/bd.cpp)<br/>config.json / dat.json<br/>mutex-lock + save()")]
        VK["Vk (src/vk.cpp)<br/>VK API v5.92, ретраи<br/>error 6→1с, 9/10→1ч"]
        NET["Net (src/net.cpp)<br/>libcurl easy, mime-upload"]
        IMG["img/doc (src/img.cpp, doc.cpp)<br/>OpenCV-фильтры, загрузка фото/доков"]
    end

    VKS["ВКонтакте API<br/>+ upload-серверы"]
    EXT["Внешние API<br/>openweathermap · nasa apod"]
    VOX[("vox/*.wav<br/>(сабмодуль)")]

    MAIN --> LP
    LP -- "updates → add_event" --> W --> EV --> CMD --> CMDS
    CMDS -- "/py" --> PY
    EV --> USERS
    USERS <--> BD
    CMDS <--> BD
    PY -- "eventOut, net, vk" --> CMDS
    CMDS --> IMG --> NET
    CMD --> VK --> NET
    CMDS -- "/w /apod" --> NET
    NET <--> VKS
    NET <--> EXT
    CMDS -- "/vox" --> VOX
```

## Модули

| Модуль | Файлы | Ответственность |
|---|---|---|
| Точка входа | `src/main.cpp` | инициализация Python (опц.), curl global, запуск longpoll |
| Longpoll | `lp.*` | получение updates, переподключение при `failed` |
| Пул/диспетчер | `worker.*`, `cmd.*` | разбор команды по первому слову, права (`acess`) |
| Команды | `cmds.cpp` | вся пользовательская функциональность |
| VK API | `vk.*`, `net.*` | JSON-RPC поверх POST, encode/upload, ретраи |
| События/вложения | `events.*`, `doc.*` | модели Event/Doc, `send()` (messages.send / wall.createComment) |
| Пользователи | `users.*` | кеш `dat.json:users`, права 1–5 |
| Хранилище | `bd.*`, `fs.*` | два JSON-файла, атомарная запись не используется (`safeWriteData` сломан) |
| Изображения | `img.*` | загрузка/декод, фильтры pix/crt/neon, HSV |
| Игры | `game.*`, bc в cmds | «линксы» 4×4 и «быки и коровы» в памяти |
| Python | `py.*` | встраивание, bindings для скриптов /py (миграция на pybind11 — план) |
| Утилиты | `str.*`, `timer.*` | UTF-8 lowercase/сплит, секундомеры |

## Потоки выполнения (важно для правок)

- Потоков два вида: main (longpoll) + 5 worker-потоков пула.
- Общие данные: `conf`, `bd` (блокировка `.lock`), `gameL`/`bcL` mutex'ы в `cmds.cpp`,
  глобальные карты `gameNew/gameUsers/randNumbers`.
- Python: каждый `/py` — новый под-интерпретатор в worker-потоке; главный поток не должен
  держать GIL (см. `docs/TASKS.md`, фаза 2).

## Сабмодули

| Путь | Что | Когда нужен |
|---|---|---|
| `thr/` | ThreadPool.h | всегда (сборка) |
| `vox/` | wav-фонемы Half-Life | runtime для `/vox` |
| `win32deps/` | win-зависимости | только Windows (не поддерживается) |

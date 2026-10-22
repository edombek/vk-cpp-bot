# Состояние тестов и проверок

Тестов в репозитории нет; CI — только CodeQL. Ниже — фактические прогоны проверок.
Обновлено: 2026-10-22.

## Матрица сборок (Linux, g++ 16.2.1, Python 3.14.8, Boost 1.92, OpenCV 5, libcurl)

| Проверка | Штатный `make` | Ручная сборка с подобранными флагами |
|---|---|---|
| `NO_PYTHON` | ❌ `-I/usr/include/opencv4` отсутствует — падают `img.cpp`/`cmds.cpp` | ✅ `/tmp/opencode/build/vkbot` |
| С Python | ❌ `-lboost_python39` нет в системе; `-I/usr/include/opencv4` нет | ✅ `/tmp/opencode/build/vkbot_python` (13 МБ) |

Пример команды, которой воспроизводится успешная сборка (артефакты — во временный каталог,
без `.o` в исходниках):

```bash
OUT=/tmp/opencode/build; mkdir -p $OUT
for f in src/*.cpp; do
  g++ -std=c++11 -c -Ofast -DGIT_URL='"test"' -DGIT_VER='"test"' -Wno-psabi \
      -Iinclude -I/usr/include/opencv5 -Ithr $(python-config --cflags) \
      "$f" -o $OUT/$(basename $f .cpp).o
done
g++ $OUT/*.o -pthread -lcurl -lopencv_core -lopencv_objdetect -lopencv_photo \
    -lopencv_imgproc -lopencv_imgcodecs $(python-config --ldflags --embed) \
    -lboost_python314 -o $OUT/vkbot_python
```

Минимальная проверка без Python: убрать `py.cpp`, `$(python-config --cflags)`,
линковка `-lboost_python314` и `--ldflags`; добавить `-DNO_PYTHON`.

## Предупреждения компилятора — тест на регресс

Компиляция с `-Wall` должна оставаться чистой (см. матрицу ниже). Регресс = возвращение
любого из ранее исправленных предупреждений.

## Предупреждения компилятора (после фазы 0)

Сборка с `-Wall` (обе конфигурации) чистая, кроме намеренно оставленных:

- ⚠️ `PyEval_InitThreads` deprecated — исчезнет при миграции на pybind11 (фаза 2)
- ⚠️ `#pragma comment` в `py.cpp` — тоже уйдёт с pybind11-миграцией

Устранены на фазе 0: infinite recursion (`fs::rename`), `-Wreturn-local-addr` (operator=
в `Event`/`img`), `-Wunused-result` (fread в vox), `-Wsign-compare` (neon/events),
`-Wreorder` (img), анонимные typedef (`-Wnon-c-typedef-for-linkage`).

Runtime-проверки фазы 0:
- `fs::safeWriteData` → файл создан с корректным содержимым (рекурсия убрана).
- 711 реальных `vox/*.wav` прогнаны через валидацию: субчанк-размеры 16/18, все проходят;
  2 битых файла (`a.wav`, `invallid.wav`, обрезанные) теперь корректно пропускаются вместо
  чтения за концом буфера.

## Python-интеграция: стресс-прогоны

| Тест | Результат |
|---|---|
| Boost.Python: 50 циклов «под-интерпретатор → exec → уничтожить» в потоках (py3.14/boost1.92) | ✅ exit 0 (`/tmp/opencode/pyt/`) |
| pybind11 3.1.0: embedded module + `per_interpreter_gil` subinterpreter, 30 посл.+3 конкурентных цикла | ✅ exit 0 (`/tmp/opencode/pyt2/t.cpp`) |
| pybind11 без `gil_scoped_release` в главном потоке | ❌ deadlock при `subinterpreter::create()` из worker-потока — воспроизведено, зафиксировано как обязательный элемент новой схемы |

## Что пока нельзя проверить автоматизированно

- End-to-end: нужны живые токены VK (`config.json`) и `dat.json`; тестовых VK-токенов нет.
- Windows-сборка: win32deps-сабмодуль пуст, toolset устарел — ❌ вне рамок (решение владельца).
- `/vox`, image-команды (`/pix`, `/crt`, `/neon`): требуют VK-загрузки; пока только статическое ревью.

## План тестирования по мере работ

1. Фаза 0/1: компиляция обеих конфигураций + чистота предупреждений (см. матрицу выше).
2. Фаза 2: `tests/py_integration.cpp` — скрипты через реальный класс `pyF` с моком `Net`
   (шаблон: `/tmp/opencode/pyt2/t.cpp`), без сети.
3. Фаза 3: сравнение времени `neon()`/`crt()` на фиксированном изображении до/после
   (локально, вне CI).

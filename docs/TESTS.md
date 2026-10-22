# Состояние тестов и проверок

CI: временно ОТКЛЮЧЕНА (2026-10-22) — workflow'ы в ручном режиме, AppVeyor отключён
переименованием; локальные проверки обязательны (см. AGENTS.md).
Юнит-тестов пока нет. Обновлено: 2026-10-22.

## Матрица сборок (Linux, g++ 16.2.1, CMake 4.4.4, Python 3.14.8, Boost 1.92, OpenCV 5)

| Конфигурация | Команда | Результат |
|---|---|---|
| С Python | `make` (шим cmake) или `cmake -B build && cmake --build build` | ✅ `-Wall` чист (кроме Python-слоя, см. ниже) |
| Без Python | `make NO_PYTHON=1` / `-DVKBOT_WITH_PYTHON=OFF` | ✅ |
| Smoke-запуск без config.json | `./build/vkbot` | ✅ чистый `FATAL: Vk: не заданы token...`, exit 1 (раньше — `terminate`) |

История: до фазы 1.5 штатный `make` не собирался (захардкоженные `-lboost_python39`,
`-I/usr/include/opencv4`), проверки делались ручной компиляцией с подобранными флагами.

## Предупреждения компилятора — тест на регресс

Сборка с `-Wall` должна оставаться чистой, кроме намеренно оставленных:

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

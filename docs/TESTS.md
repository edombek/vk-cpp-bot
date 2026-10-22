# Состояние тестов и проверок

CI: временно ОТКЛЮЧЕНА (2026-10-22) — workflow'ы в ручном режиме, AppVeyor отключён
переименованием; локальные проверки обязательны (см. AGENTS.md).
Автотесты: интеграционный тест python-слоя через ctest (фаза 2). Обновлено: 2026-10-23.

## Матрица сборок (Linux, g++ 16.2.1, CMake 4.4.4, Python 3.14.8, pybind11 3.1.0, OpenCV 5)

| Конфигурация | Команда | Результат |
|---|---|---|
| С Python | `make` (шим cmake) или `cmake -B build && cmake --build build` | ✅ `-Wall` чист (`-Wno-attributes` точечно на `src/py.cpp`, см. ниже) |
| Без Python | `make NO_PYTHON=1` / `-DVKBOT_WITH_PYTHON=OFF` | ✅ |
| Smoke-запуск без config.json | `./build/vkbot` | ✅ чистый `FATAL: Vk: не заданы token...`, exit 1 (раньше — `terminate`) |
| Автотесты | `make test` или `cd build && ctest --output-on-failure` | ✅ 1/1 тест, 16/16 проверок (см. ниже) |

История: до фазы 1.5 штатный `make` не собирался (захардкоженные `-lboost_python39`,
`-I/usr/include/opencv4`), проверки делались ручной компиляцией с подобранными флагами.

## Предупреждения компилятора — тест на регресс

Сборка с `-Wall` должна оставаться чистой, кроме намеренно оставленных:

- `-Wno-attributes` точечно на `src/py.cpp` (CMake): pybind11 объявляет свои типы с
  hidden visibility; в статическом исполняемом файле это безобидно, но варнинг мешает,
  когда pybind11-типы лежат в полях (`PyMainThread::Impl`, `pyF::Impl`).

Устранены на фазе 0: infinite recursion (`fs::rename`), `-Wreturn-local-addr` (operator=
в `Event`/`img`), `-Wunused-result` (fread в vox), `-Wsign-compare` (neon/events),
`-Wreorder` (img), анонимные typedef (`-Wnon-c-typedef-for-linkage`).
Устранены на фазе 2: deprecated `PyEval_InitThreads` и `#pragma comment` в `py.cpp`
(Boost.Python-слой полностью удалён, миграция на pybind11).

Runtime-проверки фазы 0:
- `fs::safeWriteData` → файл создан с корректным содержимым (рекурсия убрана).
- 711 реальных `vox/*.wav` прогнаны через валидацию: субчанк-размеры 16/18, все проходят;
  2 битых файла (`a.wav`, `invallid.wav`, обрезанные) теперь корректно пропускаются вместо
  чтения за концом буфера.

## Автоматический тест python-слоя (фаза 2): `vkbot_pytest` / ctest

Запуск: `make test` или `cd build && ctest --output-on-failure` (прямой запуск
`./build/vkbot_pytest` печатает все проверки). Сеть и VK-токены не нужны: `Vk(Net&)`
конструируется без сети (валидация — отдельный `Vk::validate()`, который тест не вызывает).

| # | Проверка | Результат |
|---|---|---|
| T1 | скрипт меняет `eventOut.msg`, модуль `vkcppbot.str.low` | ✅ |
| T2/T3 | runtime (`1/0`) и syntax ошибки возвращаются как traceback, процесс жив | ✅ |
| T4 | изоляция под-интерпретаторов: глобалы одного `/py` не видны в другом | ✅ |
| T5 | `vkcppbot.fs.writeData/readData` — Unicode-раундтрип через кастеры | ✅ |
| T6 | кастер `vector<string>↔list` (`str.words` + `str.summ`) | ✅ |
| T7 | биндинг `Doc` (свойства + `get()`) | ✅ |
| T8 | `eventIn` читается скриптом; read-only поля (`peer_id`) защищены (`AttributeError`) | ✅ |
| T9 | **регрессия на GIL-deadlock**: 8 потоков одновременно создают под-интерпретаторы | ✅ |

Итого: 16/16 проверок, `ctest` 1/1 passed (~0.2 c).

## Python-интеграция: стресс-прогоны (исторические, дофазовые)

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
2. Фаза 2: ✅ сделано — `tests/py_integration.cpp` через реальный `pyF` без сети (ctest).
3. Фаза 3: сравнение времени `neon()`/`crt()` на фиксированном изображении до/после
   (локально, вне CI); новые тесты на dirty-save `dat.json` и ограниченные ретраи Net/Vk.

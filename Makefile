# Тонкий шим над CMake (фаза 1.5). Привычные команды сохранены:
#   make                  -> сборка с Python
#   make NO_PYTHON=1      -> сборка без Python
# об'екты и бинарь — в ./build (CMake сам отслеживает зависимости, включая заголовки)
CMAKE ?= cmake
ifdef NO_PYTHON
OPT := -DVKBOT_WITH_PYTHON=OFF
BUILD ?= build-nopy # отдельный каталог: NO_PYTHON не должен переконфигурировать основной build/
else
OPT := -DVKBOT_WITH_PYTHON=ON
BUILD ?= build
endif

all:
	$(CMAKE) -S . -B $(BUILD) $(OPT)
	$(CMAKE) --build $(BUILD) -j

test: all
	cd $(BUILD) && ctest --output-on-failure

clean:
	rm -rf $(BUILD)

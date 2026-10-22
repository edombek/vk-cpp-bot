# Тонкий шим над CMake (фаза 1.5). Привычные команды сохранены:
#   make                  -> сборка с Python
#   make NO_PYTHON=1      -> сборка без Python
# об'екты и бинарь — в ./build (CMake сам отслеживает зависимости, включая заголовки)
CMAKE ?= cmake
BUILD ?= build

ifdef NO_PYTHON
OPT := -DVKBOT_WITH_PYTHON=OFF
else
OPT := -DVKBOT_WITH_PYTHON=ON
endif

all:
	$(CMAKE) -S . -B $(BUILD) $(OPT)
	$(CMAKE) --build $(BUILD) -j

clean:
	rm -rf $(BUILD)

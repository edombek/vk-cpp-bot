CC=g++ -Ofast
#CC=g++ -g -DDEBUG
CFLAGS= -std=c++11 -c -DGIT_URL="\"$(shell git config --get remote.origin.url)"\" -DGIT_VER="\"$(firstword $(shell git rev-parse --short=6 HEAD) unknown)"\"
LDFLAGS= -pthread -lcurl -lopencv_core -lopencv_objdetect -lopencv_photo -lopencv_imgproc -lopencv_imgcodecs
INCLUDES= -Iinclude -I/usr/include/opencv4 -Ithr
SOURCES=	\
	src/net.cpp \
	src/bd.cpp \
	src/vk.cpp \
	src/lp.cpp \
	src/fs.cpp \
	src/doc.cpp \
	src/events.cpp \
	src/worker.cpp \
	src/str.cpp \
	src/cmd.cpp \
	src/cmds.cpp \
	src/users.cpp \
	src/img.cpp \
	src/timer.cpp \
	src/game.cpp \
	src/main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=vkbot

CFLAGS+= -Wno-psabi -Wno-write-strings -Wno-unused-result

ifdef NO_PYTHON
	CFLAGS+= -DNO_PYTHON
else
	INCLUDES+=$(shell python-config --cflags) 
	LDFLAGS+=$(shell python-config --ldflags --embed) 
	ifdef TERMUX
		LDFLAGS+= -lboost_python36
	else
		LDFLAGS+= -lboost_python39
	endif
	SOURCES+= 	src/py.cpp
endif

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(EXECUTABLE)

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@
clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

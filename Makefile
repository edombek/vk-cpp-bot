CC=g++ -Ofast
#CC=g++ -Ofast -g -DDEBUG
#CC=g++ -g -DDEBUG -fsanitize-recover=address -U_FORTIFY_SOURCE -fno-omit-frame-pointer -fno-common
CFLAGS= -std=c++11 -c -DGIT_URL="\"$(shell git config --get remote.origin.url)"\" -DGIT_VER="\"$(firstword $(shell git rev-parse --short=6 HEAD) unknown)"\"
LDFLAGS= -pthread -lcurl -lgd
INCLUDES= -Iinclude
SOURCES=	\
	src/net.cpp \
	src/bd.cpp \
	src/vk.cpp \
	src/lp.cpp \
	src/fs.cpp \
	src/doc.cpp \
	src/events.cpp \
	src/workers.cpp \
	src/str.cpp \
	src/cmd.cpp \
	src/cmds.cpp \
	src/users.cpp \
	src/img.cpp \
	src/main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=vkbot

CFLAGS+= -Wno-psabi -Wno-write-strings -Wno-unused-result

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(EXECUTABLE)

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@
clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

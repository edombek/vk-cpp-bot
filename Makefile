CC=g++ -Ofast -ftree-vectorize
#CC=g++ -g -DDEBUG -fsanitize=address -fsanitize-recover=address -U_FORTIFY_SOURCE -fno-omit-frame-pointer -fno-common
CFLAGS= -std=c++11 -c
LDFLAGS= -pthread -lcurl
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
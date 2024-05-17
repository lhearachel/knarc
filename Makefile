CXXFLAGS := -std=c++2a -O2 -Wall -Wno-switch -Wpedantic -Wextra -Werror
CFLAGS   := -O2 -Wall -Wno-switch -Wpedantic -Wextra -Werror

ifeq ($(OS),Windows_NT)
C_SRCS   := fnmatch.c
LDFLAGS  += -lstdc++fs
else
C_SRCS   :=
UNAME_S  := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDFLAGS  += -lstdc++ -lc++ -lc -D_LIBCPP_NO_EXPERIMENTAL_DEPRECATION_WARNING_FILESYSTEM
else
LDFLAGS  += -lstdc++fs
endif
endif
CXX_SRCS := main.cpp narc.cpp
C_OBJS   := $(C_SRCS:%.c=%.o)
CXX_OBJS := $(CXX_SRCS:%.cpp=%.o)
OBJS     := $(C_OBJS) $(CXX_OBJS)
HEADERS  := narc.h fnmatch.h

.PHONY: all clean

all: knarc
	@:

clean:
	$(RM) knarc knarc.exe $(OBJS)

ifeq ($(OS),Windows_NT)
knarc: $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS) $(CXXFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
else
knarc: $(CXX_SRCS) $(HEADERS)
	$(CXX) $(CXX_SRCS) -o $@ $(LDFLAGS) $(CXXFLAGS)
endif

appname := webhifiaudio

CXX := g++
CXXFLAGS := -Wall -std=c++17 -lavcodec -lavformat -lavutil

srcs := $(shell find . -name "*.cpp")
objs := $(patsubst %.cpp, %.o, $(srcs))

all: $(appname)

$(appname): $(objs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(appname) $(objs) $(LDLIBS)

depend: .depend

.depend: $(srcs)
	rm -f ./.depend
	$(CXX) $(CXXFLAGS) -MM $^>>./.depend;

include .depend
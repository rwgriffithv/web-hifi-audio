appname := whfa
testname := test$(appname)
libname := lib$(appname)

bindir := ./bin
libdir := ./lib

CXX := g++
CXXFLAGS := -Wall -std=c++17 -Iinclude
LDFLAGS := -L$(libdir)
LDLIBS := -l$(appname) -lavcodec -lavformat -lavutil -lasound -lpthread 

utilsrcs := $(shell find ./src/util -name "*.cpp")
pcmsrcs := $(shell find ./src/pcm -name "*.cpp")
netsrcs := $(shell find ./src/net -name "*.cpp")
utilobjs := $(patsubst %.cpp, %.o, $(utilsrcs))
pcmobjs := $(patsubst %.cpp, %.o, $(pcmsrcs))
netobjs := $(patsubst %.cpp, %.o, $(netsrcs))

testsrcs := $(shell find ./test -name "*.cpp")
testobjs := $(patsubst %.cpp, %.o, $(testsrcs))

appsrc := ./src/$(appname).cpp
appobj := ./src/$(appname).o

srcs := $(utilsrcs) $(pcmsrcs) $(netsrcs) $(testsrcs) $(appsrc)
objs := $(utilobjs) $(pcmobjs) $(netobjs) $(testobjs) $(appobj)

# ================ main targets ================

all: app test lib

app: $(appname)

test: $(testname)

lib: $(libname)

# ================ output targets ================

$(appname): $(libname) $(appobj)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(appname) $(appobj) $(LDLIBS)
	mv $(appname) $(bindir)

$(testname): $(libname) $(testobjs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(testname) $(testobjs) $(LDLIBS)
	mv $(testname) $(bindir)

$(libname): $(utilobjs) $(pcmobjs) $(netobjs)
	ar rcs $(libname).a $(utilobjs) $(pcmobjs) $(netobjs)
	mv $(libname).a $(libdir)

### ================ object targets ================

depend: .depend

.depend: $(srcs)
	rm -f ./.depend
	$(CXX) $(CXXFLAGS) -MM $^>>./.depend;

clean:
	rm -f $(objs)

include .depend

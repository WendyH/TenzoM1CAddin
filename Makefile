ifeq ($(x32),1)
CPU = 32
else
CPU = 64
endif

SOURCES=TenzoM.cpp AddInNative.cpp

TARGETDIR = "$(CURDIR)/Package/"

ifeq ($(macos),1)
TARGET = $(TARGETDIR)TenzoM1CAddinMac.so
LIBPATHS = -Llib/macos/
LIBS= 
else
TARGET = $(TARGETDIR)TenzoM1CAddin$(CPU).so
LIBPATHS = -Llib/linux/x$(CPU)/
LIBS= 
endif

OBJECTS=$(SOURCES:.cpp=.o)
INCLUDES=-Iinclude
CXXLAGS=$(CXXFLAGS) $(INCLUDES) -m$(CPU) -finput-charset=UTF-8 -fPIC -std=c++14 -O1

all: $(TARGET)

-include $(OBJECTS:.o=.d)

%.o: %.cpp
	g++ -c  $(CXXLAGS) $*.cpp -o $*.o
	g++ -MM $(CXXLAGS) $*.cpp >  $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

$(TARGET): $(OBJECTS) Makefile
	g++ $(CXXLAGS) $(LIBPATHS) -shared $(OBJECTS) -o $(TARGET) $(addprefix -l, $(LIBS))

clean:
	rm -f $(CURDIR)/*.d $(CURDIR)/*.o

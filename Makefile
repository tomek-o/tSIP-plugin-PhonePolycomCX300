# Cross-compile the plugin on Linux with mingw-w64 (32-bit, as tSIP is a 32-bit application):
#   sudo apt install g++-mingw-w64-i686
#   make                    # -> bin/Release/PhonePolycomCX300_Win10.dll
#   make TARGET=Win7        # -> bin/Release/PhonePolycomCX300_Win7.dll
# The tSIP source tree is expected at ../tSIP/tSIP/phone/ (Phone.h etc.).
# Output directory can be changed with OUTDIR=...

CROSS   ?= i686-w64-mingw32-
CXX     = $(CROSS)g++
WINDRES = $(CROSS)windres
TARGET  ?= Win10
OUTDIR  ?= bin/Release
OBJDIR  ?= obj/$(TARGET)

CXXFLAGS += -O2 -Wall -fshort-enums -DBUILD_DLL -DTARGET_WINDOWS$(if $(filter Win7,$(TARGET)),7,10) -Ijsoncpp/include
LDFLAGS  += -shared -s -static-libgcc -static-libstdc++ -Wl,--add-stdcall-alias

SRCS = CommThread.cpp CustomConf.cpp ExtraText.cpp HidDevice.cpp Log.cpp Phone.cpp PolycomCX300.cpp Utils.cpp bin2str.cpp main.cpp \
       jsoncpp/src/lib_json/json_reader.cpp jsoncpp/src/lib_json/json_value.cpp jsoncpp/src/lib_json/json_writer.cpp
OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS)) $(OBJDIR)/resource.o
DLL  = $(OUTDIR)/PhonePolycomCX300_$(TARGET).dll

all: $(DLL)

$(DLL): $(OBJS)
	@mkdir -p $(OUTDIR)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) -lhid -lsetupapi -lws2_32 -luser32

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/resource.o: resource.rc
	@mkdir -p $(dir $@)
	$(WINDRES) $< -O coff -o $@

clean:
	rm -rf obj

.PHONY: all clean

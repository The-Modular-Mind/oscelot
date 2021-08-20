RACK_DIR ?= ~/Rack-SDK
include $(RACK_DIR)/arch.mk

ifdef ARCH_WIN
	SOURCES += $(wildcard src/osc/oscpack/ip/win32/*.cpp) 
	LDFLAGS += -lws2_32 -lwinmm
	LDFLAGS +=  -L$(RACK_DIR)/dep/lib #-lglew32 -lglfw3dll
else
	SOURCES += $(wildcard src/osc/oscpack/ip/posix/*.cpp) 
endif

SOURCES += $(wildcard src/osc/oscpack/ip/*.cpp) $(wildcard src/osc/oscpack/osc/*.cpp)
SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += $(wildcard LICENSE*) res presets

include $(RACK_DIR)/plugin.mk
RACK_DIR ?= ../../Rack-SDK

SOURCES += $(wildcard ../../oscpack/ip/*.cpp) $(wildcard ../../oscpack/osc/*.cpp) $(wildcard ../../oscpack/ip/win32/*.cpp)
SOURCES += $(wildcard src/*.cpp) $(wildcard src/mb/*.cpp) $(wildcard src/drivers/*.cpp) $(wildcard src/osc/*.cpp)

# ifeq ($(ARCH), win)
# 	SOURCES += $(wildcard ../../oscpack/ip/win32/*.cpp) 
	LDFLAGS += -lws2_32 -lwinmm
	LDFLAGS +=  -L$(RACK_DIR)/dep/lib #-lglew32 -lglfw3dll
# else
# 	SOURCES += $(wildcard ../../oscpack/ip/posix/*.cpp) 
# endif

DISTRIBUTABLES += $(wildcard LICENSE*) res presets

include $(RACK_DIR)/plugin.mk


win-dist: all
	rm -rf dist
	mkdir -p dist/$(SLUG)
	@# Strip and copy plugin binary
	cp $(TARGET) dist/$(SLUG)/
ifdef ARCH_MAC
	$(STRIP) -S dist/$(SLUG)/$(TARGET)
else
	$(STRIP) -s dist/$(SLUG)/$(TARGET)
endif
	@# Copy distributables
	cp -R $(DISTRIBUTABLES) dist/$(SLUG)/
	@# Create ZIP package
	echo "AAAAAAAAAAAcd dist && 7z.exe a $(SLUG)-$(VERSION)-$(ARCH).zip -r $(SLUG)"
	cd dist && 7z.exe a $(SLUG)-$(VERSION)-$(ARCH).zip -r $(SLUG)
	echo "cp dist/Stoermelder-P1/plugin.dll /c/Users/Martin/Documents/Rack/plugins-v1/Stoermelder-P1/"
	cp dist/Stoermelder-P1/plugin.dll /c/Users/Martin/Documents/Rack/plugins-v1/Stoermelder-P1/
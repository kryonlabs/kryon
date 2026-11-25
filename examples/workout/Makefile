# Any VRM 0.x figure. The default is the shared practice figure.
VRM ?= /mnt/storage/Projects/waozixyz/modules/practices/shared/figure/man.vrm
DEVICE ?= ZE2223BQZT
APK := droid/app/build/outputs/apk/debug/app-debug.apk

.PHONY: all desktop android install clean
all: desktop

assets/figure/man.vrm:
	@test -f "$(VRM)" || { echo "workout: $(VRM) was not found; set VRM to a VRM 0.x figure" >&2; exit 2; }
	mkdir -p assets/figure
	cp "$(VRM)" $@

desktop: assets/figure/man.vrm
	ziran tool kryon build --profile desktop

android: assets/figure/man.vrm
	ziran tool kryon build --profile android

install: android
	adb -s $(DEVICE) install -r $(APK)

clean:
	rm -r build droid

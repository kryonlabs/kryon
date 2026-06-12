clean:
	rm -rf build

clean-linux:
	rm -rf $(LINUX_OBJ_DIR) $(LINUX_BIN_DIR) $(LINUX_DIST_DIR)

clean-windows:
	rm -rf $(WINDOWS_OBJ_DIR) $(WINDOWS_BIN_DIR) $(WINDOWS_DIST_DIR)

clean-web:
	rm -rf $(WEB_BUILD_DIR)

clean-raylib:
	rm -rf $(RAYLIB_BUILD_DIR) \
		$(LINUX_OBJ_DIR)/*/native/raylib \
		$(LINUX_OBJ_DIR)/*/native/raylib-src \
		$(LINUX_OBJ_DIR)/*/raylib \
		$(LINUX_OBJ_DIR)/*/raylib-src \
		$(WINDOWS_OBJ_DIR)/*/*/raylib \
		$(WINDOWS_OBJ_DIR)/*/*/raylib-src \
		$(WEB_RAYLIB_BUILD_DIR) \
		vendor/raylib/build

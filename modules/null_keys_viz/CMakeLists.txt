if (CONFIG_NULL_KEYS_VIZ)
  zephyr_library()
  zephyr_library_sources(src/null_keys_viz.c)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)

  # Fingerprint the keymap this firmware was built from. The overlay hashes the file it
  # is displaying the same way, so it can tell you when they have drifted apart.
  # CR characters are stripped first: a CRLF working copy must hash the same as the LF
  # checkout that CI builds from.
  set(NULL_KEYS_KEYMAP_ID "unknown")
  foreach(config_dir "${ZMK_CONFIG}" "${APPLICATION_CONFIG_DIR}" "${APPLICATION_SOURCE_DIR}")
    if (NULL_KEYS_KEYMAP_ID STREQUAL "unknown" AND IS_DIRECTORY "${config_dir}")
      file(GLOB keymap_files "${config_dir}/*.keymap")
      if (keymap_files)
        list(GET keymap_files 0 keymap_file)
        file(READ "${keymap_file}" keymap_contents)
        string(REPLACE "\r" "" keymap_contents "${keymap_contents}")
        string(SHA256 keymap_hash "${keymap_contents}")
        string(SUBSTRING "${keymap_hash}" 0 12 NULL_KEYS_KEYMAP_ID)
        message(STATUS "Null Keys: keymap id ${NULL_KEYS_KEYMAP_ID} (${keymap_file})")
      endif()
    endif()
  endforeach()
  zephyr_library_compile_definitions(NULL_KEYS_KEYMAP_ID="${NULL_KEYS_KEYMAP_ID}")
endif()

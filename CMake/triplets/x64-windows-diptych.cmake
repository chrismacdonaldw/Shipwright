# Shared-engine triplet: static libraries built against the dynamic CRT, except the
# libraries whose global state must be shared between libultraship and game DLLs.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

if(PORT MATCHES "^(sdl2|spdlog|fmt)$")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()

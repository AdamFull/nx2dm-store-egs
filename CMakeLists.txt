
option(NX_MODULE_STORE_EGS "Build the Epic Games Store (EOS) storefront backend" OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/NxStoreEgs.cmake")

if (NOT NX_MODULE_STORE_EGS)
    return()
endif ()

nx_add_eos_sdk()

nx_module(store_egs
        DISPLAY "Epic Games Store (EOS) storefront backend"
        SOURCES store_egs_module.cpp store_egs_config.cpp store_egs_platform.cpp
                store_egs_services.cpp
        DEPENDS nx::eossdk nx_module_store
        PLATFORMS windows linux macos)

if (NX_BUILD_TESTS)
    add_subdirectory(tests)
endif ()

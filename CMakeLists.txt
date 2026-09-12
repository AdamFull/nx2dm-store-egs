
option(NX_MODULE_STORE_EGS "Build the Epic Games Store (EOS) storefront backend" OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/NxStoreEgs.cmake")

if (NOT NX_MODULE_STORE_EGS)
    return()
endif ()

# nx_module()'s own PLATFORMS gate runs too late to help here - a project
# that also builds for a platform the EOS SDK isn't vendored for still
# passes -DNX_MODULE_STORE_EGS=ON (its module list isn't per-platform), so
# the vendor-SDK resolution below must skip itself first, the same
# "skipped on <platform>" outcome _nx_module_gate would otherwise report -
# just early enough to not FATAL_ERROR chasing a runtime file that was
# never extracted for that platform.
set(_nx_store_egs_platforms windows linux macos)
_nx_current_platform(_nx_store_egs_here)
if (NOT _nx_store_egs_here IN_LIST _nx_store_egs_platforms)
    message(STATUS "nx2d: module 'store_egs' skipped on ${_nx_store_egs_here}")
    return()
endif ()

nx_add_eos_sdk()

nx_module(store_egs
        DISPLAY "Epic Games Store (EOS) storefront backend"
        SOURCES store_egs_module.cpp store_egs_config.cpp store_egs_platform.cpp
                store_egs_services.cpp
                store_egs_leaderboards.cpp store_egs_overlay.cpp
                store_egs_mods.cpp store_egs_scripting.cpp
        DEPENDS nx::eossdk nx_module_store
        PLATFORMS windows linux macos)

if (NX_BUILD_TESTS)
    add_subdirectory(tests)
endif ()

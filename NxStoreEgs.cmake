
# The EOS SDK (Epic Online Services) is a licence-gated download from Epic's
# developer portal - there is no fetchable URL, so it stays a required
# local, gitignored, developer-provided directory, the same shape
# modules/store_steam/NxStoreSteam.cmake already established.

set(NX_STORE_EGS_SDK_DIR "" CACHE PATH
        "An extracted EOS SDK (its own 'SDK' directory, or that directory itself). Empty uses modules/store_egs/third_party/EOSSDK.")

function(nx_add_eos_sdk)
    if (NX_STORE_EGS_SDK_DIR)
        set(_search "${NX_STORE_EGS_SDK_DIR}")
    else ()
        set(_search "${CMAKE_CURRENT_LIST_DIR}/third_party/EOSSDK")
    endif ()

    _nx_resolve_vendor_root("${_search}/SDK;${_search}" "Include/eos_init.h" _root)
    if (NOT _root)
        message(FATAL_ERROR
                "nx2d: no EOS SDK. Download it (Epic developer account "
                "required) from https://dev.epicgames.com/portal, then "
                "extract it to modules/store_egs/third_party/EOSSDK (or "
                "point NX_STORE_EGS_SDK_DIR at it). It is not fetchable "
                "here: Epic distributes it only to registered developers. "
                "Never commit it - modules/store_egs/third_party/EOSSDK is "
                "gitignored on purpose.")
    endif ()

    set(_bin "${_root}/Bin")
    set(_lib "${_root}/Lib")
    set(_implib "")
    set(_extra_runtime "")
    if (WIN32)
        if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
            message(FATAL_ERROR "nx2d: EOS SDK ships 64-bit Windows only (x64 or arm64 - only x64 is wired up here)")
        endif ()
        set(_runtime "${_bin}/EOSSDK-Win64-Shipping.dll")
        set(_implib "${_lib}/EOSSDK-Win64-Shipping.lib")
        # A companion redistributable EOS's voice chat needs alongside the
        # main DLL - staged too, or voice chat breaks at runtime with no
        # link-time symptom at all.
        if (EXISTS "${_bin}/x64/xaudio2_9redist.dll")
            set(_extra_runtime "${_bin}/x64/xaudio2_9redist.dll")
        endif ()
    elseif (APPLE AND NOT IOS)
        set(_runtime "${_bin}/libEOSSDK-Mac-Shipping.dylib")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_runtime "${_bin}/libEOSSDK-LinuxArm64-Shipping.so")
    else ()
        set(_runtime "${_bin}/libEOSSDK-Linux-Shipping.so")
    endif ()

    set(_args RUNTIME "${_runtime}")
    if (_implib)
        list(APPEND _args IMPLIB "${_implib}")
    endif ()
    _nx_imported_shared_library(nx_eossdk ${_args})
    add_library(nx::eossdk ALIAS nx_eossdk)
    set_target_properties(nx_eossdk PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_root}/Include")

    if (_extra_runtime)
        file(COPY "${_extra_runtime}" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif ()

    message(STATUS "nx2d: EOS SDK from ${_root}")
endfunction()

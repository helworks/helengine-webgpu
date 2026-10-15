# Tracks the files consumed by Emscripten's --preload-file link step.
function(helengine_track_preload_content target content_root)
    if(NOT IS_DIRECTORY "${content_root}")
        message(FATAL_ERROR "Prepared preload content directory does not exist: ${content_root}")
    endif()
    file(GLOB_RECURSE preload_files LIST_DIRECTORIES false CONFIGURE_DEPENDS "${content_root}/*")
    list(SORT preload_files)
    set(preload_inventory "")
    foreach(preload_file IN LISTS preload_files)
        file(SHA256 "${preload_file}" preload_hash)
        string(APPEND preload_inventory "${preload_hash} ${preload_file}\n")
    endforeach()
    set(preload_inventory_path "${CMAKE_CURRENT_BINARY_DIR}/${target}.preload-content.txt")
    # Official builds always configure. Hashes also catch cache-restored payloads
    # whose preserved timestamps predate the previous link. Membership changes
    # cover old-dated additions and files that no longer exist as dependencies.
    file(GENERATE OUTPUT "${preload_inventory_path}" CONTENT "${preload_inventory}")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${preload_inventory_path}" ${preload_files})
endfunction()

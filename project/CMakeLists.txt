# Source files
set(SOURCES
    src/main.cpp
    src/pch.cpp
    src/core/Matrix.cpp
    src/core/Timer.cpp
    src/core/Vector2.cpp
    src/core/Vector3.cpp
    src/core/Vector4.cpp
    src/rendering/Renderer.cpp
    src/scene/Mesh.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# C++20
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene
)

# Force include pch
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FIpch.h)
endif()

# DirectX
find_library(DXGI_LIBRARY dxgi.lib)
find_library(D3D11_LIBRARY d3d11.lib)
if(NOT DXGI_LIBRARY OR NOT D3D11_LIBRARY)
    message(FATAL_ERROR "DirectX libraries not found")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DXGI_LIBRARY}
    ${D3D11_LIBRARY}
    Profiler::Profiler
)

# --- Helper: copy files to output directory ---
function(copy_files_post_build TARGET_NAME SOURCE_DIR PATTERNS)
    foreach(PATTERN ${PATTERNS})
        file(GLOB_RECURSE FILES "${SOURCE_DIR}/${PATTERN}")
        foreach(FILE ${FILES})
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FILE}
                $<TARGET_FILE_DIR:${TARGET_NAME}>)
        endforeach()
    endforeach()
endfunction()

function(copy_to_resources TARGET_NAME SOURCE_DIR PATTERNS)
    set(OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources")
    file(MAKE_DIRECTORY ${OUT_DIR})
    foreach(PATTERN ${PATTERNS})
        file(GLOB_RECURSE FILES "${SOURCE_DIR}/${PATTERN}")
        foreach(FILE ${FILES})
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FILE} ${OUT_DIR})
        endforeach()
    endforeach()
endfunction()

# Copy assets and shaders to output resources/ folder
copy_to_resources(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/assets" "*.jpg;*.png;*.obj")
copy_to_resources(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "*.fx")

# --- Third-party libraries ---

# SDL2
set(SDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2-2.30.7")
add_library(SDL2 STATIC IMPORTED)
set_target_properties(SDL2 PROPERTIES
    IMPORTED_LOCATION "${SDL_DIR}/lib/x64/SDL2.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2)
copy_files_post_build(${PROJECT_NAME} "${SDL_DIR}/lib/x64" "*.dll;*.manifest")

# SDL2_image
set(SDL_IMAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_image-2.8.2")
add_library(SDL2_image STATIC IMPORTED)
set_target_properties(SDL2_image PROPERTIES
    IMPORTED_LOCATION "${SDL_IMAGE_DIR}/lib/x64/SDL2_image.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL_IMAGE_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2_image)
copy_files_post_build(${PROJECT_NAME} "${SDL_IMAGE_DIR}/lib/x64" "*.dll;*.manifest")

# DirectX Effects (debug/release aware)
set(FX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/dx11effects")
add_library(DX11Effects STATIC IMPORTED)
set_target_properties(DX11Effects PROPERTIES
    IMPORTED_LOCATION "${FX_DIR}/lib/x64/dx11effects.lib"
    IMPORTED_LOCATION_DEBUG "${FX_DIR}/lib/x64/dx11effects_d.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${FX_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PRIVATE DX11Effects)

# Visual Leak Detector (Debug only)
if(WIN32)
    set(VLD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/vld")
    add_library(VLD STATIC IMPORTED)
    set_target_properties(VLD PROPERTIES
        IMPORTED_LOCATION "${VLD_DIR}/lib/x64/vld.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${VLD_DIR}/include"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:VLD>)
    target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:ENABLE_VLD=1>)
    copy_files_post_build(${PROJECT_NAME} "${VLD_DIR}/lib" "*.dll;*.manifest")
endif()

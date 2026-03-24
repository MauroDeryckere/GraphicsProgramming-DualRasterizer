# Source files
set(SOURCES
    "src/main.cpp"
    "src/pch.cpp"

    "src/core/Matrix.cpp"
    "src/core/Timer.cpp"
    "src/core/Vector2.cpp"
    "src/core/Vector3.cpp"
    "src/core/Vector4.cpp"

    "src/rendering/Renderer.cpp"

    "src/scene/Mesh.cpp"
)

# Create the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories so headers resolve without relative paths
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene
)

# Force include pch.h for all source files
target_compile_options(${PROJECT_NAME} PRIVATE /FIpch.h)

# DirectX
find_library(DXGI_LIBRARY dxgi.lib)
find_library(D3D11_LIBRARY d3d11.lib)
if(DXGI_LIBRARY AND D3D11_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${DXGI_LIBRARY} ${D3D11_LIBRARY} Profiler::Profiler)
else()
    message(FATAL_ERROR "DirectX libraries not found")
endif()

# Copy assets to output folder
set(ASSETS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
file(GLOB_RECURSE ASSET_FILES
    "${ASSETS_SOURCE_DIR}/*.jpg"
    "${ASSETS_SOURCE_DIR}/*.png"
    "${ASSETS_SOURCE_DIR}/*.obj"
)
set(ASSETS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources/")
file(MAKE_DIRECTORY ${ASSETS_OUT_DIR})
foreach(ASSET ${ASSET_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${ASSET}
        ${ASSETS_OUT_DIR})
endforeach()

# Copy shaders to output folder
set(SHADERS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
file(GLOB_RECURSE SHADER_FILES "${SHADERS_SOURCE_DIR}/*.fx")
foreach(SHADER ${SHADER_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER}
        ${ASSETS_OUT_DIR})
endforeach()

# Simple Directmedia Layer
set(SDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2-2.30.7")
add_library(SDL STATIC IMPORTED)
set_target_properties(SDL PROPERTIES
    IMPORTED_LOCATION "${SDL_DIR}/lib/x64/SDL2.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL)

file(GLOB_RECURSE DLL_FILES
    "${SDL_DIR}/lib/x64/*.dll"
    "${SDL_DIR}/lib/x64/*.manifest"
)
foreach(DLL ${DLL_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${DLL}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endforeach()

# Simple Directmedia Layer Image
set(SDL_IMAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_image-2.8.2")
add_library(SDL_IMAGE STATIC IMPORTED)
set_target_properties(SDL_IMAGE PROPERTIES
    IMPORTED_LOCATION "${SDL_IMAGE_DIR}/lib/x64/SDL2_image.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL_IMAGE_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL_IMAGE)

file(GLOB_RECURSE DLL_FILES
    "${SDL_IMAGE_DIR}/lib/x64/*.dll"
    "${SDL_IMAGE_DIR}/lib/x64/*.manifest"
)
foreach(DLL ${DLL_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${DLL}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endforeach()

# DirectX Effects
set(FX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/dx11effects")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(FX_LIBRARY "${FX_DIR}/lib/x64/dx11effects_d.lib")
else()
    set(FX_LIBRARY "${FX_DIR}/lib/x64/dx11effects.lib")
endif()
add_library(FX STATIC IMPORTED)
set_target_properties(FX PROPERTIES
    IMPORTED_LOCATION "${FX_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FX_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PRIVATE FX)

# Visual Leak Detector
if(WIN32 AND CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(ENABLE_VLD=1)
    set(VLD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/vld")
    add_library(vld STATIC IMPORTED)
    set_target_properties(vld PROPERTIES
        IMPORTED_LOCATION "${VLD_DIR}/lib/x64/vld.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${VLD_DIR}/include"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE vld)

    file(GLOB_RECURSE DLL_FILES
        "${VLD_DIR}/lib/*.dll"
        "${VLD_DIR}/lib/*.manifest"
    )
    foreach(DLL ${DLL_FILES})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${DLL}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endforeach()
endif()

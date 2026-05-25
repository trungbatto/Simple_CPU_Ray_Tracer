cmake_minimum_required(VERSION 3.10)
project(RayTracer)

set(CMAKE_CXX_STANDARD 20)

# Set SDL2 paths relative to the project directory
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/SDL2")
set(SDL2_INCLUDE_DIR "${SDL2_DIR}/include")
set(SDL2_LIB_DIR "${SDL2_DIR}/lib/x64")

# Add executable
add_executable(raytracer 
    main.cc
    vec3.h color.h ray.h hittable.h sphere.h hittable_list.h rtweekend.h interval.h
    camera.h material.h aabb.h bvh.h texture.h external/stb_image.h rtw_stb_image.h
    perlin.h quad.h constant_medium.h
)

# Include SDL2 headers
target_include_directories(raytracer PRIVATE ${SDL2_INCLUDE_DIR})

# Link SDL2 libraries
target_link_libraries(raytracer PRIVATE
    "${SDL2_LIB_DIR}/SDL2.lib"
    "${SDL2_LIB_DIR}/SDL2main.lib"
)

# Optional: Tell the linker where to look for DLLs at runtime (Windows)
set_target_properties(raytracer PROPERTIES
    VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${SDL2_LIB_DIR}"
)

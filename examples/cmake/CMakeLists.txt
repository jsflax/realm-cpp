cmake_minimum_required(VERSION 3.15)

project (realm_helloworld)

set(CMAKE_CXX_STANDARD 17)
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:preprocessor /bigobj")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()
add_executable(realm_helloworld helloworld.cpp)

find_package(cpprealm REQUIRED)
if(NOT cpprealm_FOUND)
    Include(FetchContent)
    FetchContent_Declare(
            cpprealm
            GIT_REPOSITORY https://github.com/realm/realm-cpp.git
            GIT_TAG        origin/main
    )

    # The MakeAvailable command ensures the named dependencies have been populated
    FetchContent_MakeAvailable(cpprealm)
endif()

set(REALM_HAVE_UV 1)
target_link_libraries(realm_helloworld Realm::cpprealm)

if(MSVC)
    set_property(TARGET realm_helloworld PROPERTY
                 MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
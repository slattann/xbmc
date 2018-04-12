set(SOURCES WinSystemEGLStreams.cpp
            DRMUtils.cpp
            DRMAtomic.cpp)

set(HEADERS WinSystemEGLStreams.h
            DRMUtils.h
            DRMAtomic.h)

if(OPENGL_FOUND)
  list(APPEND SOURCES WinSystemEGLStreamsGLContext.cpp)
  list(APPEND HEADERS WinSystemEGLStreamsGLContext.h)
endif()

core_add_library(windowing_EGLStreams)

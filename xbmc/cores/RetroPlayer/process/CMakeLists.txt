set(SOURCES BaseRenderBuffer.cpp
            BaseRenderBufferPool.cpp
            RenderBufferGuiTexture.cpp
            RenderBufferHardware.cpp
            RenderBufferManager.cpp
            RenderBufferSysMem.cpp
            RPProcessInfo.cpp
)

set(HEADERS BaseRenderBuffer.h
            BaseRenderBufferPool.h
            IRenderBuffer.h
            IRenderBufferPool.h
            RenderBufferGuiTexture.h
            RenderBufferHardware.h
            RenderBufferManager.h
            RenderBufferSysMem.h
            RPProcessInfo.h
)

core_add_library(rp-process)

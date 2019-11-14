set(LIB_SRC
    AsyncLogging.cpp
    LogFile.cpp
    Logger.cpp
    LogStream.cpp
    Thread.cpp
)

add_library(base ${LIB_SRC})
target_link_libraries(base pthread rt)


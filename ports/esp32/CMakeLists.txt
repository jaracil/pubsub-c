set(COMPONENT_SRCS "src/pubusb.c")

set(COMPONENT_ADD_INCLUDEDIRS "src")

set_source_files_properties(
    src/pubsub.c
    PROPERTIES COMPILE_FLAGS
    -DPS_USE_GETTIMEOFDAY
)
set(COMPONENT_REQUIRES "")

register_component()
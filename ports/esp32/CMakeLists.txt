set(COMPONENT_SRCS "src/pubsub.c")
set(COMPONENT_ADD_INCLUDEDIRS "src")

register_component()

set_source_files_properties(src/pubsub.c
    PROPERTIES COMPILE_FLAGS -DPS_FREE_RTOS
)

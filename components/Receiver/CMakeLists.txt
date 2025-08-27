set(RECEIVER_SRCS
    "src/test.c"
)

idf_component_register(
    SRCS ${RECEIVER_SRCS}
    INCLUDE_DIRS "inc"
    REQUIRES log driver
)
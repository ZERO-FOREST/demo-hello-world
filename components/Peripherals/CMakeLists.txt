idf_component_register(
    SRCS "src/st7789.c"
         "src/xpt2046.c"
    INCLUDE_DIRS "inc"
    REQUIRES driver log
) 
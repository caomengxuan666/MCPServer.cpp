add_library(miniz STATIC miniz.c)

target_compile_definitions(miniz PRIVATE MZ_USE_STDIO_FILEFUNC_64)

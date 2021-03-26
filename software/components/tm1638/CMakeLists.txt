cmake_minimum_required(VERSION 3.5)

idf_component_register(
    SRCS TM1638_platform.c TM1638.c
    INCLUDE_DIRS include
)

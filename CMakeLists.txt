idf_component_register(
    SRCS
        "src/MAX31865.cpp"
        "src/MAX31865_Protocol.cpp"
    INCLUDE_DIRS
        "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

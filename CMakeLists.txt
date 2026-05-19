cmake_minimum_required(VERSION 3.16)

idf_component_register(
  SRCS "src/MAX31865.cpp"
  INCLUDE_DIRS "include"
  REQUIRES freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

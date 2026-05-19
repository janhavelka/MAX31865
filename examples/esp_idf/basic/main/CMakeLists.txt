idf_component_register(
  SRCS "main.cpp" "Max31865IdfSpi.cpp"
  INCLUDE_DIRS "."
  REQUIRES MAX31865 esp_driver_spi esp_driver_gpio esp_timer freertos
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

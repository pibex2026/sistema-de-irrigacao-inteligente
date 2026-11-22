idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "../include"
        "."
    REQUIRES
        nvs_flash
        esp_timer
        esp_adc
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        driver
        json
        esp_system
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
)

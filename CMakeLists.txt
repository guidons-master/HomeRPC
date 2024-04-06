file(GLOB SOURCES "src/*.c")

idf_component_register(SRCS "${SOURCES}"
                    INCLUDE_DIRS "include"
                    REQUIRES mdns esp_wifi nvs_flash esp_netif esp_event iot_bridge mesh_lite mqtt
                    )

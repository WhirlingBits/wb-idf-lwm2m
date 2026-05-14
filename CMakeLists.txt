idf_component_register(
    SRCS src/wb_lwm2m.c
         src/wb_lwm2m_encode.c
         src/wb_lwm2m_tlv.c
         src/wb_lwm2m_std_objects.c
    INCLUDE_DIRS include
    REQUIRES wb-idf-coap-client freertos log esp_timer
)

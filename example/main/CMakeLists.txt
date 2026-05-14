idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES wb-idf-lwm2m nvs_flash esp_wifi esp_netif
                       EMBED_TXTFILES "${PROJECT_DIR}/certificate.pem")

idf_component_register(SRCS 
                            "main.c"
                            "utils/blufi_init.c"
                            "utils/blufi_security"
                            "utils/wifi_util.c"
                            "utils/nvs_util.c"
                            "utils/mqtt_util.c"
                            "utils/sensor_util.c"
                            "utils/dht11.c"

                    INCLUDE_DIRS "utils")
# 自动收集所有子目录的源文件
file(GLOB_RECURSE SOURCES "*.c")
file(GLOB_RECURSE HEADERS "*.h")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS . utils logger task_flow drivers tests led wifi ai_agent
    REQUIRES
    nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
    esp_https_ota esp_event json spiffs console vfs app_update esp-tls
    esp_timer esp_driver_gpio driver espressif__brookesia_agent_openai
    # EMBED_FILES "canon.pcm"
)

#     # 自动收集所有子目录的源文件
# file(GLOB_RECURSE SOURCES "*.c")
# file(GLOB_RECURSE HEADERS "*.h")

# idf_component_register(
#     SRCS ${SOURCES}
#     INCLUDE_DIRS . utils logger task_flow drivers tests led wifi
#     REQUIRES
#         nvs_flash
#         esp_wifi
#         esp_netif
#         esp_http_client
#         esp_http_server
#         esp_https_ota
#         esp_event
#         json
#         spiffs
#         console
#         vfs
#         app_update
#         esp_tls
#         esp_timer          # ✅ 保留，但需要检查是否必要
#         esp_driver_gpio
#         driver
#         espressif__brookesia_agent_openai   # ⚠️ 关键：添加这个组件的依赖
# )
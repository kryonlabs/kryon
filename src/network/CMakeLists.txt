# Kryon Network Module

if(KRYON_ENABLE_NETWORK)
    add_library(kryon_network OBJECT
        http/http_client.c
        websocket/websocket_client.c
        fetch/fetch_api.c
        ssl/ssl_support.c
    )

    target_include_directories(kryon_network PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/internal
    )

    target_compile_definitions(kryon_network PRIVATE KRYON_NETWORK_MODULE=1)
endif()
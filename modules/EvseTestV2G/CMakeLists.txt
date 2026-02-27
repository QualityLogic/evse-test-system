#
# AUTO GENERATED - MARKED REGIONS WILL BE KEPT
# template version 3
#

# module setup:
#   - ${MODULE_NAME}: module name
ev_setup_cpp_module()

# ev@bcc62523-e22b-41d7-ba2f-825b493a3c97:v1
# insert your custom targets and additional config variables here
# ev@bcc62523-e22b-41d7-ba2f-825b493a3c97:v1

target_sources(${MODULE_NAME}
    PRIVATE
        "charger/ISO15118_chargerImpl.cpp"
        "extensions/iso15118_extensionsImpl.cpp"
        "tester/evse_test_v2gImpl.cpp"
        "token_provider/auth_token_providerImpl.cpp"
)

# ev@c55432ab-152c-45a9-9d2e-7281d50c69c3:v1
# insert other things like install cmds etc here

# Add pkg-config functionality
find_package(PkgConfig REQUIRED)

# search for libevent.pc
pkg_search_module(EVENT REQUIRED libevent)

target_include_directories(${MODULE_NAME} PRIVATE
    crypto
    connection
    server
    testcases
)

target_link_libraries(${MODULE_NAME} PUBLIC ${EVENT_LIBRARIES} -levent -lpthread -levent_pthreads)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        cbv2g::din
        cbv2g::iso2
        cbv2g::tp
        everest::staging::helpers
        Pal::Sigslot
)

target_sources(${MODULE_NAME}
    PRIVATE
        "connection/connection.cpp"
        "server/din_server.cpp"
        "server/iso_server.cpp"
        "testcases/chn_authorization_004.cpp"
        "testcases/chn_authorization_009.cpp"
        "testcases/chn_cable_check_006.cpp"
        "testcases/chn_cablecheck_007.cpp"
        "testcases/chn_charge_parameter_discovery_002.cpp"
        "testcases/chn_current_demand_002.cpp"
        "testcases/chn_current_demand_005.cpp"
        "testcases/chn_current_demand_007.cpp"
        "testcases/chn_precharge_006.cpp"
        "testcases/chn_service_detail_011.cpp"
        "testcases/chn_service_detail_013.cpp"
        "testcases/chn_service_detail_payment_selection_003.cpp"
        "testcases/chn_service_detail_payment_selection_012.cpp"
        "testcases/chn_service_discovery_008.cpp"
        "testcases/chn_session_setup_004.cpp"
        "testcases/chn_session_setup_007.cpp"
        "testcases/chn_welding_session_stop_001.cpp"
        "testcases/chx_smart_charging_scheduling_001.cpp"
        "testcases/chx_smart_charging_scheduling_002.cpp"
        "testcases/chx_smart_charging_scheduling_003.cpp"
        "testcases/chx_smart_charging_scheduling_base.cpp"
        "log.cpp"
        "report_tools.cpp"
        "sdp.cpp"
        "testcase.cpp"
        "TestWrapperImpl.cpp"
        "tools.cpp"
        "v2g_ctx.cpp"
        "v2g_server.cpp"
)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        everest::tls
)
target_sources(${MODULE_NAME}
    PRIVATE
        "crypto/crypto_openssl.cpp"
        "connection/tls_connection.cpp"
)
# ev@c55432ab-152c-45a9-9d2e-7281d50c69c3:v1

build_lib(
    LIBNAME pa-atp
    SOURCE_FILES
        model/aggregate_switch.cc
        model/custom_client.cc
        model/parameter_server.cc
        helper/aggregate_switch_helper.cc
        helper/custom_client_helper.cc
        helper/parameter_server_helper.cc
        utils/my_utils.cc
    HEADER_FILES
        model/aggregate_switch.h
        model/custom_client.h
        model/parameter_server.h
        helper/aggregate_switch_helper.h
        helper/custom_client_helper.h
        helper/parameter_server_helper.h
        utils/my_utils.h
    LIBRARIES_TO_LINK
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}

)

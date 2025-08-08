build_lib_example(
    NAME basic_dumbbell
    SOURCE_FILES basic_dumbbell.cc
    LIBRARIES_TO_LINK
        ${libapplications}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libpa-atp}
)

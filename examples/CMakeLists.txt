build_lib_example(
    NAME custom_module-example
    SOURCE_FILES custom_module-example.cc
    LIBRARIES_TO_LINK ${libcustom_module}
)

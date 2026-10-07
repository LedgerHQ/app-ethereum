ledger_unit_tests_init()

# tlv_library.c uses non-pedantic C constructs; suppress for this source.
set_source_files_properties(${BOLOS_SDK}/lib_tlv/tlv_library.c
  PROPERTIES COMPILE_FLAGS "-Wno-pedantic")

include(../cmake/tests_low_level.cmake)
include(../cmake/tests_gtp.cmake)
include(../cmake/tests_descriptors.cmake)
include(../cmake/tests_sign.cmake)
include(../cmake/tests_plugins.cmake)
include(../cmake/tests_nbgl.cmake)
include(../cmake/tests_address_book.cmake)

#
#
# stunserver, network path trace


set (ADDITIONAL_LIBS "")

#find_package( OpenSSL )
#if( OPENSSL_FOUND )
#  include_directories( ${OPENSSL_INCLUDE_DIR} )
#  list(APPEND ADDITIONAL_LIBS ${OPENSSL_LIBRARIES})
#endif( OPENSSL_FOUND )

add_executable ( stunserver
                 iphelper.c
                 sockethelper.c
                 ip_query.c
                 utils.c
                 stunserver.c )

target_link_libraries ( stunserver PRIVATE sockaddrutil
                        stunlib
                        pthread
                        ${ADDITIONAL_LIBS})

set_target_properties(stunserver PROPERTIES INSTALL_RPATH "${install_dir}/lib")
set_target_properties(stunserver PROPERTIES BUILD_WITH_INSTALL_RPATH ON)

include_directories ( ../include )
link_directories ( ${CHECK_LIBRARY_DIRS} )

set (crusty_files
     stunserver.c
     iphelper.c
     sockethelper.c)

UncrustifyDir(crusty_files)

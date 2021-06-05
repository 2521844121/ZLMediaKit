if(WIN32)

	find_path(OPENSSL_INCLUDE_DIR 
		NAMES openssl/ssl.h
		PATHS 
		${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/openssl/${CMAKE_SYSTEM_NAME}/include/)

	find_library(OPENSSL_LIBRARIES 
		NAMES 		 
		libssl
		PATHS 
		${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/openssl/${CMAKE_SYSTEM_NAME}/lib/)

	find_library(CRYPTO_LIBRARIES 
		NAMES 		 
		libcrypto
		PATHS 
		${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/openssl/${CMAKE_SYSTEM_NAME}/lib/)
		

else(WIN32)#linux

	find_path(OPENSSL_INCLUDE_DIR 
		NAMES openssl/ssl.h
		PATHS /usr/local/openssl/include/)

	find_library(OPENSSL_LIBRARIES 
		NAMES crypto ssl
		PATHS /usr/local/openssl/lib)

endif(WIN32)

set(OPENSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR})
set(OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES} ${CRYPTO_LIBRARIES})
message("OPENSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR}")
message("OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES}")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(OpenSSL DEFAULT_MSG OPENSSL_INCLUDE_DIR OPENSSL_LIBRARIES)

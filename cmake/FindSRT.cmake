if(WIN32)
		
	find_path(SRT_INCLUDE_DIR
		NAMES srt/srt.h
		PATHS 
		${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/srt/${CMAKE_SYSTEM_NAME}/include/)

	find_library(SRT_LIBRARY
		NAMES srt
		PATHS 
		${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/srt/${CMAKE_SYSTEM_NAME}/lib/)
		
message("===========${SRT_LIBRARY}")

else(WIN32)#linux

	find_path(SRT_INCLUDE_DIR
		NAMES srt/srt.h
		PATHS /usr/local/SRT/include/)

	find_library(SRT_LIBRARY
		NAMES srt
		PATHS /usr/local/SRT/lib)

endif(WIN32)

set(SRT_INCLUDE_DIRS ${SRT_INCLUDE_DIR})
set(SRT_LIBRARIES ${SRT_LIBRARY})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SRT DEFAULT_MSG SRT_LIBRARY SRT_INCLUDE_DIR)

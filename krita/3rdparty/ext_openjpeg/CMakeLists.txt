SET(PREFIX_ext_openjpeg "${EXTPREFIX}" )
ExternalProject_Add( ext_openjpeg
	DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
	URL https://github.com/uclouvain/openjpeg/archive/v2.4.0.tar.gz
	URL_HASH SHA256=8702ba68b442657f11aaeb2b338443ca8d5fb95b0d845757968a7be31ef7f16d

    INSTALL_DIR ${PREFIX_ext_openjpeg}
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PREFIX_ext_openjpeg} -DOPENJPEG_INSTALL_INCLUDE_DIR=include/ -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE}
	
    UPDATE_COMMAND ""
    DEPENDS ext_lcms2
)

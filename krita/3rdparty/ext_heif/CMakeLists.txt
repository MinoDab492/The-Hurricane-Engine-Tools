SET(EXTPREFIX_heif "${EXTPREFIX}" )

ExternalProject_Add(
    ext_libde265
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
    URL https://github.com/strukturag/libde265/releases/download/v1.0.8/libde265-1.0.8.tar.gz
    URL_HASH SHA1=db8fa137a4681ec092e7546d9155bbaa95938e5e

    PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/0001-Quick-check-for-SSE-support.patch

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTPREFIX_heif} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE}

    UPDATE_COMMAND ""
)

if (WIN32)
    if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        ExternalProject_Add(
            ext_nasm
            DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
            URL https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/win64/nasm-2.15.05-win64.zip
            URL_HASH SHA256=f5c93c146f52b4f1664fa3ce6579f961a910e869ab0dae431bd871bdd2584ef2

            BUILD_COMMAND ${CMAKE_COMMAND} -E echo Deploying nasm 64-bit dependencies
            CONFIGURE_COMMAND ""
            INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/rdoff ${EXTPREFIX_heif}/bin/rdoff
                    COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/nasm.exe ${EXTPREFIX_heif}/bin/nasm.exe
                    COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/ndisasm.exe ${EXTPREFIX_heif}/bin/ndisasm.exe

            UPDATE_COMMAND ""
        )
    else("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        ExternalProject_Add(
            ext_nasm
            DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
            URL https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/win32/nasm-2.15.05-win32.zip
            URL_HASH SHA256=258c7d1076e435511cf2fdf94e2281eadbdb9e3003fd57f356f446e2bce3119e

            BUILD_COMMAND ${CMAKE_COMMAND} -E echo Deploying nasm 32-bit dependencies
            CONFIGURE_COMMAND ""
            INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/rdoff ${EXTPREFIX_heif}/bin/rdoff
                    COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/nasm.exe ${EXTPREFIX_heif}/bin/nasm.exe
                    COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/ndisasm.exe ${EXTPREFIX_heif}/bin/ndisasm.exe

            UPDATE_COMMAND ""
        )
    endif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
else (WIN32)
ExternalProject_Add(
    ext_nasm
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
    URL https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.gz
    URL_HASH SHA256=9182a118244b058651c576baa9d0366ee05983c4d4ae1d9ddd3236a9f2304997

    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXTPREFIX_heif} ${GLOBAL_AUTOMAKE_PROFILE}

    UPDATE_COMMAND ""
)
endif(WIN32)


set(EXTRA_AVIF_DEPS "")

find_package(Perl)
if(NOT PERL_FOUND AND WIN32)
    ExternalProject_Add(
        ext_strawberryperl
        DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
        # From https://strawberryperl.com/download/5.32.1.1/strawberry-perl-5.32.1.1-32bit-portable.zip
        URL https://files.kde.org/krita/build/dependencies/strawberry-perl-5.32.1.1-32bit-portable.zip
        URL_HASH SHA1=28bca91cadd6651c2b2463db8587c170bf17f2fa

        BUILD_COMMAND ${CMAKE_COMMAND} -E echo Deploying Strawberry Perl portable
        CONFIGURE_COMMAND ""
        INSTALL_COMMAND ""
        UPDATE_COMMAND ""

        SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/Strawberry"
    )
    set(USE_STRAWBERRYPERL 1)
    set(LIBAOM_EXTRA_CMAKE_ARGS -DPERL_EXECUTABLE=${CMAKE_CURRENT_BINARY_DIR}/Strawberry/perl/bin/perl.exe)
    set(LIBAOM_STRAWBERRYPERL_DEP ext_strawberryperl)
elseif(PERL_FOUND AND WIN32)
    set(LIBAOM_EXTRA_CMAKE_ARGS -DPERL_EXECUTABLE=${PERL_EXECUTABLE})
endif()

if(NOT WIN32)
    set(LIBAOM_EXTRA_CMAKE_ARGS -DCONFIG_PIC=1)
endif()

if(PERL_FOUND OR USE_STRAWBERRYPERL)
    message(STATUS "Building AVIF support using libaom.")

    ExternalProject_Add(
        ext_libaom
        DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}

        URL https://storage.googleapis.com/aom-releases/libaom-3.2.0.tar.gz
        URL_HASH SHA256=8fa7418bf4ea12ef6dcf0eeb21cba932b71c6079ffdf7b9d13cf907fa46c0568

        CMAKE_ARGS ${LIBAOM_EXTRA_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${EXTPREFIX_heif} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} -DENABLE_EXAMPLES=OFF -DENABLE_TESTDATA=OFF -DENABLE_TESTS=OFF -DENABLE_TOOLS=OFF ${GLOBAL_PROFILE}

        UPDATE_COMMAND ""

        DEPENDS ext_nasm ${LIBAOM_STRAWBERRYPERL_DEP}
    )

    set(EXTRA_AVIF_DEPS ext_libaom)
else()
    message(WARNING "Perl not available, disabling AVIF support.")
endif()

# Incorporate Handbrake's extensions for building x265
# Multicoreware added detection of SSE2/3/4.1, so we can freely enable them now
# Also ship their patch for Apple silicon
if (MSVC)
    set(x265_LIBDIR "$<CONFIG>/")
    set(x265_LIBRARY x265-static.lib)
    set(x265_10bit_LIBRARY x265_main10-static.lib)
    set(x265_12bit_LIBRARY x265_main12-static.lib)
    set(x265_EXTRA_LINK_FLAGS /LIBPATH:${EXTPREFIX_heif}/lib)
else()
    set(x265_LIBDIR "")
    set(x265_LIBRARY libx265.a)
    set(x265_10bit_LIBRARY libx265_main10.a)
    set(x265_12bit_LIBRARY libx265_main12.a)
    set(x265_EXTRA_LINK_FLAGS -L${EXTPREFIX_heif}/lib)
endif()
ExternalProject_Add(ext_libx265_10bit
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
    URL https://github.com/HandBrake/HandBrake-contribs/releases/download/contribs/x265-snapshot-20220222-12712.tar.gz
    URL_HASH SHA256=e6182435b25b04a00c75770d2a1c11e7731eb5e118c377933eeec2665980db70

    SOURCE_SUBDIR source/

    PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A01-build-fix.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A02-threads-priority.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A03-threads-pool-adjustments.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/0001-Silence-nasm-2.15-warnings.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/0003-Skip-PDB-in-MinGW.patch

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTPREFIX_heif} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE} -DHIGH_BIT_DEPTH=TRUE -DEXPORT_C_API=FALSE -DENABLE_CLI=FALSE -DENABLE_SHARED=FALSE
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${x265_LIBDIR}${x265_LIBRARY} ${EXTPREFIX_heif}/lib/${x265_10bit_LIBRARY}
)
ExternalProject_Add(ext_libx265_12bit
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
    URL https://github.com/HandBrake/HandBrake-contribs/releases/download/contribs/x265-snapshot-20220222-12712.tar.gz
    URL_HASH SHA256=e6182435b25b04a00c75770d2a1c11e7731eb5e118c377933eeec2665980db70

    SOURCE_SUBDIR source/

    PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A01-build-fix.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A02-threads-priority.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A03-threads-pool-adjustments.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/0001-Silence-nasm-2.15-warnings.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/0003-Skip-PDB-in-MinGW.patch

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTPREFIX_heif} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE} 
    -DHIGH_BIT_DEPTH=TRUE -DMAIN12=TRUE -DEXPORT_C_API=FALSE  -DENABLE_CLI=FALSE -DENABLE_SHARED=FALSE
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${x265_LIBDIR}${x265_LIBRARY} ${EXTPREFIX_heif}/lib/${x265_12bit_LIBRARY}
)
ExternalProject_Add(
    ext_libx265
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
    URL https://github.com/HandBrake/HandBrake-contribs/releases/download/contribs/x265-snapshot-20220222-12712.tar.gz
    URL_HASH SHA256=e6182435b25b04a00c75770d2a1c11e7731eb5e118c377933eeec2665980db70

    SOURCE_SUBDIR source/

    PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A01-build-fix.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A02-threads-priority.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/A03-threads-pool-adjustments.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/0001-Silence-nasm-2.15-warnings.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/0003-Skip-PDB-in-MinGW.patch

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTPREFIX_heif} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE} -DENABLE_SHARED=TRUE  -DENABLE_HDR10_PLUS=TRUE -DEXTRA_LIB:String=${x265_10bit_LIBRARY}$<SEMICOLON>${x265_12bit_LIBRARY} -DEXTRA_LINK_FLAGS:String=${x265_EXTRA_LINK_FLAGS} -DLINKED_10BIT=TRUE -DLINKED_12BIT=TRUE

    UPDATE_COMMAND ""

    DEPENDS ext_nasm ext_libx265_10bit ext_libx265_12bit
)

ExternalProject_Add(
    ext_libheif
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}

    URL https://github.com/strukturag/libheif/archive/5a7b2f4ec49c3788460db698cb68223d537ea4c5.tar.gz
    URL_HASH SHA256=6be490b74bef2f91683d5649f2aa22652b0f558ff7a7f713a016fe3582b01f87

    PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/clang-aom-link-pthread.patch
        COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/0001-Don-t-assume-wide-char-streams-are-supported-on-Wind.patch

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTPREFIX_heif} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} -DBUILD_EXAMPLES=OFF ${GLOBAL_PROFILE}

    UPDATE_COMMAND ""
    DEPENDS ${EXTRA_AVIF_DEPS} ext_libde265 ext_libx265 ext_jpeg ext_png
)

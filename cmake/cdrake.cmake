find_package( cdrake QUIET )

if( NOT cdrake_FOUND )
    #
    include( FetchContent )
    fetchcontent_declare(
            cdrake
            GIT_REPOSITORY https://github.com/alessandromanzini/cdrake.git
            GIT_TAG main )
    fetchcontent_makeavailable( cdrake )
    #
endif()

include( "${cdrake_SOURCE_DIR}/CDrakeConfig.cmake" )
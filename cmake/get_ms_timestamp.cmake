function( get_ms_timestamp OUTPUT_TIMESTAMP )
    #
    if( CMAKE_HOST_UNIX )
        execute_process(
                COMMAND perl -e "use Time::HiRes qw(time); printf '%d', time()*1000"
                OUTPUT_VARIABLE TIMESTAMP_MS
                OUTPUT_STRIP_TRAILING_WHITESPACE )
    elseif( CMAKE_HOST_WIN32 )
        execute_process(
                COMMAND cmd /c "echo %time:~9,2%%date:/=%%time::=%"
                OUTPUT_VARIABLE TIMESTAMP_MS
                OUTPUT_STRIP_TRAILING_WHITESPACE )
    endif()
    #
    set( ${OUTPUT_TIMESTAMP} ${TIMESTAMP_MS} PARENT_SCOPE )
    #
endfunction()
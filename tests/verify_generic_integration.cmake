file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/input" "${WORK}/dump" "${WORK}/empty")
file(COPY "${FIXTURE}" DESTINATION "${WORK}/input")

execute_process(
    COMMAND "${XENOS_RECOMP}" --dump-hlsl "${WORK}/input" "${WORK}/dump" "${SHADER_COMMON}"
    RESULT_VARIABLE dump_result
    OUTPUT_VARIABLE dump_stdout
    ERROR_VARIABLE dump_stderr
)
if(NOT dump_result EQUAL 0)
    message(FATAL_ERROR "HLSL dump failed (${dump_result}):\n${dump_stdout}\n${dump_stderr}")
endif()

file(GLOB dumps "${WORK}/dump/*.hlsl")
list(LENGTH dumps dump_count)
if(NOT dump_count EQUAL 1)
    message(FATAL_ERROR "Expected one HLSL dump, found ${dump_count}")
endif()
list(GET dumps 0 dump)
file(READ "${dump}" hlsl)
foreach(header
        "// Source: F7DDC03EFA734623.bin"
        "// Stage: pixel"
        "// Hash: 0xF7DDC03EFA734623"
        "// Specialization mask: 0x")
    string(FIND "${hlsl}" "${header}" header_offset)
    if(header_offset EQUAL -1)
        message(FATAL_ERROR "Missing HLSL metadata header: ${header}")
    endif()
endforeach()

file(WRITE "${WORK}/cache.cpp" "previous-cache\n")
execute_process(
    COMMAND "${XENOS_RECOMP}" "${WORK}/empty" "${WORK}/cache.cpp" "${SHADER_COMMON}" --jobs 1
    RESULT_VARIABLE empty_result
)
if(empty_result EQUAL 0)
    message(FATAL_ERROR "Empty directory translation unexpectedly succeeded")
endif()
file(READ "${WORK}/cache.cpp" preserved)
if(NOT preserved STREQUAL "previous-cache\n")
    message(FATAL_ERROR "Failed directory translation replaced the previous cache")
endif()
if(EXISTS "${WORK}/cache.cpp.tmp")
    message(FATAL_ERROR "Failed directory translation left a temporary cache")
endif()

file(READ "${SHADER_COMMON}" common)
foreach(fragment
        "uint4 g_BooleanWords[2]"
        "g_BooleanWord(uint(ADDRESS) >> 5u)"
        "uint(ADDRESS) & 31u"
        "SharedConstants + 256 + uint(INDEX) * 4"
        "SharedConstants + 356 + uint(INDEX) * 4")
    string(FIND "${common}" "${fragment}" fragment_offset)
    if(fragment_offset EQUAL -1)
        message(FATAL_ERROR "Missing unified boolean/loop ABI fragment: ${fragment}")
    endif()
endforeach()

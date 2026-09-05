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

# Register-shaped reflection aliases (c4, c16, ...) must not rewrite the
# shared cbuffer's packoffset tokens when the HLSL preprocessor runs.
string(FIND "${hlsl}" "cbuffer SharedConstants : register(b2, space4)" shared_offset)
string(FIND "${hlsl}" "\tDEFINE_SHARED_CONSTANTS();" shared_end)
string(REGEX MATCH "#define [A-Za-z_][A-Za-z0-9_]* g_PixelShaderConstants\\[[^\n]+" alias "${hlsl}")
if(shared_offset EQUAL -1 OR shared_end LESS shared_offset OR alias STREQUAL "")
    message(FATAL_ERROR "Missing DXIL cbuffers or reflected Float4 alias")
endif()
string(FIND "${hlsl}" "${alias}" alias_offset)
if(alias_offset LESS shared_end)
    message(FATAL_ERROR "Float4 alias precedes the end of the DXIL cbuffers")
endif()

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
        "SharedConstants + 356 + uint(INDEX) * 4"
        "SharedConstants + 496 + uint(INDEX) * 4"
        "int4 g_TextureExponentAdjusts[4] : packoffset(c31)")
    string(FIND "${common}" "${fragment}" fragment_offset)
    if(fragment_offset EQUAL -1)
        message(FATAL_ERROR "Missing unified boolean/loop ABI fragment: ${fragment}")
    endif()
endforeach()

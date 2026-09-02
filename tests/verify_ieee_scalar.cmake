execute_process(
    COMMAND "${XENOS_RECOMP}" "${FRAG_FIXTURE}" "${FRAG_OUTPUT}" "${SHADER_COMMON}"
    RESULT_VARIABLE frag_result
    ERROR_VARIABLE frag_error
)
if(NOT frag_result EQUAL 0)
    message(FATAL_ERROR "FM2 IEEE scalar fixture failed to translate: ${frag_error}")
endif()

execute_process(
    COMMAND "${XENOS_RECOMP}" "${VERT_FIXTURE}" "${VERT_OUTPUT}" "${SHADER_COMMON}"
    RESULT_VARIABLE vert_result
    ERROR_VARIABLE vert_error
)
if(NOT vert_result EQUAL 0)
    message(FATAL_ERROR "FM2 IEEE sqrt fixture failed to translate: ${vert_error}")
endif()

file(READ "${FRAG_OUTPUT}" frag_hlsl)
file(READ "${VERT_OUTPUT}" vert_hlsl)

# Xenia emits the IEEE variants raw. The clamp variants are separate Xenos
# opcodes, so treating every LOG/RCP/RSQ as clamp changes zero and infinity.
foreach(forbidden "clamp(log2(" "clamp(rcp(" "clamp(rsqrt(")
    string(FIND "${frag_hlsl}" "${forbidden}" forbidden_pos)
    if(NOT forbidden_pos EQUAL -1)
        message(FATAL_ERROR "IEEE scalar operation was incorrectly clamped: ${forbidden}")
    endif()
endforeach()
foreach(required "log2(" "rcp(" "rsqrt(")
    string(FIND "${frag_hlsl}" "${required}" required_pos)
    if(required_pos EQUAL -1)
        message(FATAL_ERROR "Missing IEEE scalar operation: ${required}")
    endif()
endforeach()

string(FIND "${vert_hlsl}" "sqrt(max(0.0," clamped_sqrt_pos)
if(NOT clamped_sqrt_pos EQUAL -1)
    message(FATAL_ERROR "SQRT was incorrectly zero-clamped instead of matching Xenia IEEE semantics")
endif()
string(FIND "${vert_hlsl}" "sqrt(" sqrt_pos)
if(sqrt_pos EQUAL -1)
    message(FATAL_ERROR "Missing SQRT operation")
endif()
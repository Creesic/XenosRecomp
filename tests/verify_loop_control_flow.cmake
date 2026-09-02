execute_process(
    COMMAND "${XENOS_RECOMP}" "${FIXTURE}" "${OUTPUT}" "${SHADER_COMMON}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "XenosRecomp failed (${result}):\n${stdout}\n${stderr}")
endif()

file(READ "${OUTPUT}" hlsl)
foreach(required_fragment
        "g_LoopConstant(16)"
        "loopCount0"
        "for (uint loopIterator0 = 0;"
        "aL ="
        "loopStep0")
    string(FIND "${hlsl}" "${required_fragment}" fragment_offset)
    if(fragment_offset EQUAL -1)
        message(FATAL_ERROR "Missing loop-control translation fragment: ${required_fragment}")
    endif()
endforeach()
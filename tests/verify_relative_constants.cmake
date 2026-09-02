execute_process(
    COMMAND "${XENOS_RECOMP}" "${FIXTURE}" "${OUTPUT}" "${SHADER_COMMON}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "FM2 relative-constant fixture failed to translate: ${error}")
endif()

file(READ "${OUTPUT}" hlsl)

# shader_50E896828A7389ED indexes four matrix rows through a0. Dropping +a0
# makes every iteration read the first matrix and corrupts skinned vertices.
foreach(reference
        "LoadVertexShaderConstant(60 + a0)"
        "LoadVertexShaderConstant(61 + a0)"
        "LoadVertexShaderConstant(62 + a0)"
        "LoadVertexShaderConstant(63 + a0)")
    string(FIND "${hlsl}" "${reference}" reference_pos)
    if(reference_pos EQUAL -1)
        message(FATAL_ERROR "Relative constant address was dropped: ${reference}")
    endif()
endforeach()
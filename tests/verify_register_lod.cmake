execute_process(
    COMMAND "${XENOS_RECOMP}" "${FIXTURE}" "${OUTPUT}" "${SHADER_COMMON}"
    RESULT_VARIABLE recompile_result
    OUTPUT_VARIABLE recompile_stdout
    ERROR_VARIABLE recompile_stderr
)

if(NOT recompile_result EQUAL 0)
    message(FATAL_ERROR
        "XenosRecomp failed (${recompile_result})\n"
        "stdout:\n${recompile_stdout}\n"
        "stderr:\n${recompile_stderr}")
endif()

file(READ "${OUTPUT}" hlsl)
string(FIND "${hlsl}" "float textureLod = 0.0;" body_offset)
if(body_offset EQUAL -1)
    message(FATAL_ERROR "Generated shader body was not found")
endif()
string(SUBSTRING "${hlsl}" ${body_offset} -1 shader_body)

string(FIND "${hlsl}" "float2(0.00146484375, 0.00146484375)" offset_epsilon)
if(offset_epsilon EQUAL -1)
    message(FATAL_ERROR "Xenia texture-coordinate offset epsilon was dropped")
endif()

# FM2 shader 882CBBF29F4A1A9D contains `setTexLOD r1.x` followed by a tfetch2D
# with UseRegisterLOD=true. Xenia's oracle uses explicit-LOD SampleLevel here,
# not computed-LOD SampleBias.
foreach(required_fragment
        "float textureLod = 0.0;"
        "textureLod = r1.x;"
        " * exp2(float(g_TextureExponentAdjust("
        "tfetch2DL(")
    string(FIND "${shader_body}" "${required_fragment}" fragment_offset)
    if(fragment_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing register-LOD translation fragment: ${required_fragment}\n"
            "stderr:\n${recompile_stderr}")
    endif()
endforeach()

string(FIND "${shader_body}" "tfetch2DCL(" computed_lod_fetch)
if(NOT computed_lod_fetch EQUAL -1)
    message(FATAL_ERROR "Register-sourced LOD was incorrectly emitted as SampleBias")
endif()

string(FIND "${recompile_stderr}" "falls back to SampleLevel(0)" fallback_warning)
if(NOT fallback_warning EQUAL -1)
    message(FATAL_ERROR "Register-LOD translation still uses the SampleLevel(0) fallback")
endif()

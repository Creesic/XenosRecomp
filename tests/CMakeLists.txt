add_test(
    NAME fm2_cond_exec_boolean_guards
    COMMAND ${CMAKE_COMMAND}
        "-DXENOS_RECOMP=$<TARGET_FILE:XenosRecomp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/F7DDC03EFA734623.bin"
        "-DSHADER_COMMON=${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/F7DDC03EFA734623.hlsl"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_cond_exec.cmake"
)

add_test(
    NAME fm2_register_texture_lod
    COMMAND ${CMAKE_COMMAND}
        "-DXENOS_RECOMP=$<TARGET_FILE:XenosRecomp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/882CBBF29F4A1A9D.bin"
        "-DSHADER_COMMON=${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/882CBBF29F4A1A9D.hlsl"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_register_lod.cmake"
)

add_test(
    NAME fm2_ieee_scalar_semantics
    COMMAND ${CMAKE_COMMAND}
        "-DXENOS_RECOMP=$<TARGET_FILE:XenosRecomp>"
        "-DFRAG_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/fm2_ieee_scalar.frag.bin"
        "-DVERT_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/fm2_ieee_sqrt.vert.bin"
        "-DSHADER_COMMON=${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
        "-DFRAG_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fm2_ieee_scalar.hlsl"
        "-DVERT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fm2_ieee_sqrt.hlsl"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_ieee_scalar.cmake"
)

add_test(
    NAME fm2_texture_gradients
    COMMAND ${CMAKE_COMMAND}
        "-DXENOS_RECOMP=$<TARGET_FILE:XenosRecomp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/fm2_get_gradients.frag.bin"
        "-DSHADER_COMMON=${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fm2_get_gradients.hlsl"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_get_gradients.cmake"
)

add_test(
    NAME fm2_relative_constant_addressing
    COMMAND ${CMAKE_COMMAND}
        "-DXENOS_RECOMP=$<TARGET_FILE:XenosRecomp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/fm2_relative_constants.vert.bin"
        "-DSHADER_COMMON=${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fm2_relative_constants.hlsl"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_relative_constants.cmake"
)

add_test(
    NAME fm2_loop_control_flow
    COMMAND ${CMAKE_COMMAND}
        "-DXENOS_RECOMP=$<TARGET_FILE:XenosRecomp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/fm2_loop_control_flow.frag.bin"
        "-DSHADER_COMMON=${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fm2_loop_control_flow.hlsl"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_loop_control_flow.cmake"
)

add_test(
    NAME generic_dump_and_fail_closed
    COMMAND ${CMAKE_COMMAND}
        "-DXENOS_RECOMP=$<TARGET_FILE:XenosRecomp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/F7DDC03EFA734623.bin"
        "-DSHADER_COMMON=${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/generic_integration"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_generic_integration.cmake"
)

find_program(PYTHON_EXECUTABLE NAMES python python3)
set(FM2_ORACLE_DIR "${PROJECT_SOURCE_DIR}/Sampleshaders/Xenos_shaders/FM2")
if(PYTHON_EXECUTABLE AND EXISTS "${FM2_ORACLE_DIR}")
    add_test(
        NAME fm2_oracle_semantic_audit
        COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/audit_fm2_oracle.py"
            --oracle "${FM2_ORACLE_DIR}"
            --xenos-recomp "$<TARGET_FILE:XenosRecomp>"
            --shader-common "${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
            --work "${CMAKE_CURRENT_BINARY_DIR}/fm2_oracle_audit"
            --json "${CMAKE_CURRENT_BINARY_DIR}/fm2_oracle_audit.json"
    )
endif()

set(FM2_RUNTIME_SHADER_DIR
    "${PROJECT_SOURCE_DIR}/../ReXGlue080plume/FM2/assets/missed_shaders")
if(PYTHON_EXECUTABLE AND EXISTS "${FM2_RUNTIME_SHADER_DIR}")
    add_test(
        NAME fm2_runtime_semantic_audit
        COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/audit_fm2_runtime.py"
            --runtime "${FM2_RUNTIME_SHADER_DIR}"
            --xenos-recomp "$<TARGET_FILE:XenosRecomp>"
            --shader-common "${PROJECT_SOURCE_DIR}/XenosRecomp/shader_common.h"
            --work "${CMAKE_CURRENT_BINARY_DIR}/fm2_runtime_audit"
            --json "${CMAKE_CURRENT_BINARY_DIR}/fm2_runtime_audit.json"
    )
endif()

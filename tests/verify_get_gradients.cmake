execute_process(
    COMMAND "${XENOS_RECOMP}" "${FIXTURE}" "${OUTPUT}" "${SHADER_COMMON}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "FM2 getGradients fixture failed to translate: ${error}")
endif()

file(READ "${OUTPUT}" hlsl)

# shader_0EBEDF47218631B0 writes ddx(src.xy) to result.xy and ddy(src.xy) to
# result.zw. Before this regression, GetTextureGradients was accepted by the
# decoder but silently returned without emitting any instruction.
string(FIND "${hlsl}" "getGradients2D(r1.zz).xy" gradients_pos)
if(gradients_pos EQUAL -1)
    message(FATAL_ERROR "GetTextureGradients was silently dropped")
endif()

# Xenos packs getGradients as { ddx(src.x), ddy(src.x),
# ddx(src.y), ddy(src.y) }. Packing the vector derivatives as
# float4(ddx(src.xy), ddy(src.xy)) instead produces { ddx.x, ddx.y,
# ddy.x, ddy.y }; for a duplicated source swizzle such as r1.zz this turns
# result.xy into two horizontal derivatives and can collapse analytic alpha.
string(FIND "${hlsl}"
    "float4(ddx_coarse(value.x), ddy_coarse(value.x), ddx_coarse(value.y), ddy_coarse(value.y))"
    gradient_pack_pos)
if(gradient_pack_pos EQUAL -1)
    message(FATAL_ERROR "GetTextureGradients components are not packed as Xenos {ddx.x, ddy.x, ddx.y, ddy.y}")
endif()
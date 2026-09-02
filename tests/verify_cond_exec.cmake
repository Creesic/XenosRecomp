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

# FM2 shader F7DDC03EFA734623 corresponds to Xenia oracle shader
# C197FD225DAC819C. Its mutually-exclusive `cexec b130` and `cexec !b130`
# blocks must each be guarded. Running both blocks unconditionally makes the
# second texture path overwrite the first.
string(REGEX MATCHALL "if \\(\\(g_Booleans &" cond_exec_guards "${hlsl}")
list(LENGTH cond_exec_guards cond_exec_guard_count)
if(cond_exec_guard_count LESS 2)
    message(FATAL_ERROR
        "Expected two g_Booleans CondExec guards for b130, found "
        "${cond_exec_guard_count}")
endif()
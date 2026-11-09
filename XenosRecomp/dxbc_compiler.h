#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct DxbcCompiler
{
    static std::vector<uint8_t> compile(const std::string& source, bool pixelShader);
};

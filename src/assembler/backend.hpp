#pragma once

#include "ir.hpp"

#include <string>
#include <vector>

namespace Assembler {

struct BackendArtifact {
    std::vector<uint8_t> bytes;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    bool ok() const {
        return errors.empty();
    }
};

class AssemblerBackend {
public:
    virtual ~AssemblerBackend() = default;

    virtual IRTarget target() const = 0;
    virtual BackendArtifact emit(const IRProgram& program) = 0;
};

} // namespace Assembler
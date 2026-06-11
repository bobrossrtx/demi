# DemiEngine Documentation

## Quick Navigation

| Category | Document | Description |
|----------|----------|-------------|
| **Getting Started** | [Main README](../README.md) | Project overview and quick start |
| | [Quick Reference](reference/QUICK_REFERENCE.md) | Fast command and instruction lookup |
| | [Usage Guide](usage/README.md) | Programming guide and tutorials |
| **Development** | [Roadmap](../roadmap.md) | Development roadmap |
| | [Future Opcodes](development/FUTURE_OPCODES.md) | Planned opcodes |
| | [Demi Language Plan](development/DEMI_LANGUAGE_PLAN.md) | High-level language spec |
| | [Native Compiler Plan](development/NATIVE_COMPILER_PLAN.md) | x86-64 code generation |
| **Testing** | [Test Framework Design](testing/TEST_FRAMEWORK_DESIGN.md) | Testing architecture |
| | [Test Quick Reference](testing/TEST_QUICK_REFERENCE.md) | Running and writing tests |
| | [Test Flags](testing/TEST_FLAGS.md) | CPU flags testing |
| **Codebase** | [Architecture](codebase/README.md) | Core system design |
| | [API Reference](codebase/API_REFERENCE.md) | API documentation |
| | [CPU Module](codebase/modules/cpu.md) | CPU implementation |
| | [Assembler Module](codebase/modules/assembler.md) | Assembler architecture |
| | [Device Manager](codebase/modules/device_manager.md) | Device I/O system |
| **Support** | [Troubleshooting](usage/TROUBLESHOOTING.md) | Common issues and solutions |
| | [Contributing](../CONTRIBUTING.md) | Development guidelines |

## Directory Structure

```
docs/
├── README.md                    # This file
├── reference/
│   ├── QUICK_REFERENCE.md       # Fast lookup reference
│   ├── FPU_REFERENCE.md         # FPU instruction reference
│   ├── SIMD_REFERENCE.md        # SIMD instruction reference
│   └── SIMD_COMPARISON.md       # SIMD comparison guide
├── development/
│   ├── FUTURE_OPCODES.md
│   ├── DEMI_LANGUAGE_PLAN.md
│   ├── NATIVE_COMPILER_PLAN.md
│   └── REGISTER_IMPLEMENTATION_NOTES.md
├── testing/
│   ├── TEST_FRAMEWORK_DESIGN.md
│   ├── TEST_QUICK_REFERENCE.md
│   ├── TEST_FLAGS.md
│   └── ...
├── codebase/
│   ├── README.md
│   ├── API_REFERENCE.md
│   └── modules/
│       ├── cpu.md
│       ├── assembler.md
│       ├── device_manager.md
│       └── testing.md
├── usage/
│   ├── README.md
│   ├── FEATURES.md
│   └── TROUBLESHOOTING.md
└── archive/
    └── ... (historical docs)
```

## Task-Based Guides

**Learn DemiEngine**: [Main README](../README.md) → [Quick Reference](reference/QUICK_REFERENCE.md) → [Usage Guide](usage/README.md) → [Examples](../examples/README.md)

**Write Assembly**: [Usage Guide](usage/README.md) → [Quick Reference](reference/QUICK_REFERENCE.md) → [Example Programs](../examples/README.md) → [Test Programs](../tests/asm/)

**Understand the Codebase**: [Architecture Overview](codebase/README.md) → [API Reference](codebase/API_REFERENCE.md) → [Module Docs](codebase/modules/)

**Write Tests**: [Test Framework Design](testing/TEST_FRAMEWORK_DESIGN.md) → [Test Quick Reference](testing/TEST_QUICK_REFERENCE.md) → [Test Flags](testing/TEST_FLAGS.md)

**Future Plans**: [Roadmap](../roadmap.md) → [Future Opcodes](development/FUTURE_OPCODES.md) → [Demi Language Plan](development/DEMI_LANGUAGE_PLAN.md) → [Native Compiler](development/NATIVE_COMPILER_PLAN.md)

**Troubleshoot**: [Troubleshooting Guide](usage/TROUBLESHOOTING.md) → [Test Status](testing/TEST_FRAMEWORK_STATUS.md)

**Contribute**: [Contributing Guide](../CONTRIBUTING.md) → [Architecture Overview](codebase/README.md) → [Test Framework](testing/TEST_FRAMEWORK_DESIGN.md)

## Current Status

**Phase**: Native code generation and language frontend
**Test results**: 269 unit tests pass / 542 assembly tests (539 pass, 0 fail, 3 skip)

- Backend: 134 registers, all core opcodes, SIMD, FPU, MMX, AVX
- Native codegen: x86-64 ELF output functional for arithmetic, control flow, memory, INT 0x80 I/O bridging, and x87 FPU operations
- Demi language: Planned

## External Resources

- Repository: https://github.com/bobrossrtx/demi
- Issues: https://github.com/bobrossrtx/demi/issues
- License: MIT

---
Last updated: May 2026

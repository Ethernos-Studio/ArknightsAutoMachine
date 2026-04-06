# Third-Party Licenses

This project uses the following third-party libraries and components:

## MaaFramework

- **Repository**: https://github.com/Ethernos-Studio/MaaFramework
- **Path**: `third_party/maafw/`
- **License**: GNU Lesser General Public License v3 (LGPL-3.0)
- **License File**: `third_party/maafw/LICENSE.md`
- **Usage**: Screen capture and input control automation framework

### MaaFramework Dependencies

#### MaaAgentBinary
- **Path**: `third_party/maafw/3rdparty/MaaAgentBinary/`
- **License**: See `third_party/maafw/3rdparty/MaaAgentBinary/LICENSE`
- **Usage**: Android agent binaries for touch and screenshot operations

#### QuickJS
- **Path**: `third_party/maafw/3rdparty/quickjs/`
- **License**: MIT License
- **License File**: `third_party/maafw/3rdparty/quickjs/LICENSE`
- **Usage**: JavaScript engine for MaaFramework scripting

## vcpkg

- **Repository**: https://github.com/microsoft/vcpkg
- **Path**: `vcpkg/`
- **License**: MIT License
- **Usage**: C++ package manager for dependency management

## OpenCV

- **Repository**: https://github.com/opencv/opencv
- **License**: Apache License 2.0
- **Usage**: Computer vision and image processing (via vcpkg)

## FFmpeg

- **Repository**: https://github.com/FFmpeg/FFmpeg
- **License**: LGPL-2.1+ / GPL-2.0+ (depending on configuration)
- **Usage**: Video encoding/decoding (via vcpkg)

## fmt

- **Repository**: https://github.com/fmtlib/fmt
- **License**: MIT License
- **Usage**: String formatting library (via vcpkg)

## spdlog

- **Repository**: https://github.com/gabime/spdlog
- **License**: MIT License
- **Usage**: Logging library (via vcpkg)

## cxxopts

- **Repository**: https://github.com/jarro2783/cxxopts
- **License**: MIT License
- **Usage**: Command line argument parsing (via vcpkg)

## Protocol Buffers (protobuf)

- **Repository**: https://github.com/protocolbuffers/protobuf
- **License**: BSD-3-Clause
- **Usage**: Serialization protocol (via vcpkg)

## gRPC

- **Repository**: https://github.com/grpc/grpc
- **License**: Apache License 2.0
- **Usage**: RPC framework (via vcpkg)

## ZeroMQ

- **Repository**: https://github.com/zeromq/libzmq
- **License**: MPL-2.0
- **Usage**: Message queue library (via vcpkg)

## cppzmq

- **Repository**: https://github.com/zeromq/cppzmq
- **License**: MIT License
- **Usage**: C++ bindings for ZeroMQ (via vcpkg)

## nlohmann/json

- **Repository**: https://github.com/nlohmann/json
- **License**: MIT License
- **Usage**: JSON parsing library (via vcpkg)

## tl-expected

- **Repository**: https://github.com/TartanLlama/expected
- **License**: CC0-1.0
- **Usage**: Expected monad type for error handling (via vcpkg)

---

## License Compliance Notes

### MaaFramework (LGPL-3.0)

This project links against MaaFramework as a shared library (DLL on Windows).
According to the LGPL-3.0 license:

1. The source code of MaaFramework is available at:
   https://github.com/Ethernos-Studio/MaaFramework

2. Users can replace the MaaFramework DLL with a modified version
   that is compatible with the LGPL-3.0 license.

3. Modifications to MaaFramework itself must be released under LGPL-3.0.

### AAM Project License

The Arknights Auto Machine (AAM) project itself is licensed under:

**GNU Affero General Public License v3 (AGPL-3.0)**

See the main `LICENSE` file for the full license text.

---

## Full License Texts

For the complete license texts of all third-party components, please refer to
their respective license files in the directories mentioned above.

Last updated: 2026-04-06

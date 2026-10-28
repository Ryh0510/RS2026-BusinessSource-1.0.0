# Common local prebuilt package

Point `CMAKE_PREFIX_PATH` at the directory containing `Common/`, then call `find_package(Common CONFIG REQUIRED COMPONENTS Utility)`. Link `Common::Utility` instead of naming a `.lib` file. The imported target selects the Debug or Release library according to the consumer configuration.

The package does not bundle third-party headers or development libraries. Install or configure the required providers for the chosen component, including Eigen and glm for `Utility`. DLLs in `Common/bin` must be available to the running application. Do not mix Debug and Release DLLs in one runtime directory.

The files under `examples/UtilityQuickStart` demonstrate a small installed-package consumer. Local installation verifies package layout and linking; formal SDK release still requires the separate ABI, consumer, runtime, content, and checksum gates.

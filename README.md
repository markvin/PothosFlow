# Pothos Flow - graphical topology designer

Pothos Flow is the GUI frontend and designer tool for the Pothos framework.

This branch (`pothos2`) builds against the modernized core
(https://github.com/markvin/PothosCore, branch `pothos2`). It uses only
the public API of the core.

## Documentation

* https://github.com/pothosware/PothosFlow/wiki

## Dependencies

- an installed Pothos core (`find_package(Pothos)`);
- Qt 6.5 or higher with the Widgets and Concurrent components (for
  example the Arch Linux package `qt6-base`, or a Qt install from the Qt
  installer). The Qt Gui CMake package also needs the OpenGL headers (on
  Debian and Ubuntu, for example `libgl-dev`);
- for the smoke test: PothosBlocks in the prefix of the core;
- CMake 3.25 or higher and a C++23 compiler.

When Qt 6 is missing or too old, the configure step stops with an error.
Qt 5 is not supported.

On Windows with a current MSVC (Visual Studio 2026), use Qt 6.8 or higher:
the headers of Qt 6.5 do not compile with its standard library. The CI
uses Qt 6.5.3 on Linux and macOS, and Qt 6.8.3 on Windows.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<core prefix>
cmake --build build
ctest --test-dir build --output-on-failure
```

`<core prefix>` is the installation prefix of the Pothos core. Do not set
it when the core is installed in a default location of the system.

When Qt is not in a default location of the system, add its prefix to
`CMAKE_PREFIX_PATH` (for example
`-DCMAKE_PREFIX_PATH="<core prefix>;$HOME/Qt/6.8.0/gcc_64"`), or give
`Qt6_DIR`. On Linux and macOS, the installed binaries then have an RPATH
to the Qt library directory. On Windows, put the Qt `bin` directory on
`PATH`.

Install the application, the edit widgets module and the library
`PothosQtColorPicker`:

```sh
cmake --install build
```

The core loads modules only from its own prefix (and from
`POTHOS_PLUGIN_PATH`). Thus the default install prefix of this project is
the prefix of the core. When you give `CMAKE_INSTALL_PREFIX`, set
`POTHOS_PLUGIN_PATH` to `<prefix>/lib/Pothos/modules<ABI version>`.

## Run

Start `PothosFlow` from the `bin` directory of the prefix (on macOS, the
`PothosFlow` script starts the application bundle). You can give
topology files (`.pth`) on the command line.

## Tests

The test `flow.stage` installs flow into `build/stage`. Then `flow.smoke`
runs the smoke test `PothosFlowSmokeTest` against the core with the flow
module of the stage. The smoke test does these checks:

1. The main window comes into view.
2. The block cache gets the blocks of PothosBlocks (`/blocks/copier`).
3. Help → About shows the window "About Pothos" (modal on Linux and
   Windows; on macOS Qt shows this window without modality).
4. The message window shows a log message.
5. The window closes, and no record at Error level or worse is logged.

Install PothosBlocks into the prefix of the core before you run the
tests. Without it, `flow.smoke` stops with an error.

The test needs no display: it uses the Qt platform `offscreen`. The main
window starts a Pothos server on the fixed locator port, thus the test has
the CTest resource lock `pothos_locator_port`. Close a running PothosFlow
before you run the test: the second server cannot use the port, and the
test fails (the application has the same behavior).

`flow.smoke` stops with an error when flow is also installed in the
prefix of the core (the core would load the two copies of the flow
module). Remove the installed copy, or test against another installation
of the core.

## Licensing information

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)

# qtcolorpicker

The source for the qtcolorpicker is available in the pothos-gui source tree.
qtcolorpicker is built as a stand-alone library and used under the terms
of the GNU LESSER GENERAL PUBLIC LICENSE. See qtcolorpicker/LICENSE.LGPL

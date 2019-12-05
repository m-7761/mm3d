# mm3d
Simplified fork of Maverick/Misfit Model 3D meant for improving core data model.

# demo for Windows users
https://github.com/mick-p1982/mm3d/releases/tag/win32-demo

# build
1) Use CMake to build/install Widgets 95 (https://sourceforge.net/p/widgets-95).
2) Use CMake to build/install MM3d that depends on Widgets 95 being installed.
3) The CMake scripts are for Linux. Widgets 95 is tested with wxGTK3. They work
with Windows too but the Visual Studio project requires some tinkering afterward
in its present state, owing to inheriting some GLUI build patterns. There is a
standalone MSVC (Visual Studio) project in the source code.

# background
https://github.com/zturtleman/mm3d

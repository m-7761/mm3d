# mm3d
Simplified fork of Maverick/Misfit Model 3D meant for improving core data model.

# demo for Windows users
https://github.com/mick-p1982/mm3d/releases/tag/win32-dec-2021

![MM3D headeater smaller](https://user-images.githubusercontent.com/18209495/77861427-77816780-71da-11ea-895b-965de28f11ce.png)

# build
0) FindwxWidgets issue: https://gitlab.kitware.com/cmake/cmake/-/issues/21544
1) Use CMake to build/install Widgets 95 (https://sourceforge.net/p/widgets-95).
2) Use CMake to build/install MM3d that depends on Widgets 95 being installed.
3) The CMake scripts are for Linux. Widgets 95 is tested with wxGTK3. They work
with Windows too but the Visual Studio project requires some tinkering afterward
in its present state, owing to inheriting some GLUI build patterns. There is a
standalone MSVC (Visual Studio) project in the source code.

# background
https://github.com/zturtleman/mm3d<br>
Notes: The ZIP files are archived material from the old code that might be able
to be salvaged. Everything from MM3D is supported except for:
1) Localization is currently unimplemented in Widgets 95 as it's mid development.
2) Qt may support more OpenGL targets but desktop should work.
3) Apple desktops are unimplemented. I can't add this myself.
4) Only lightly tested on Linux (GCC builds.)
5) There are a lot of UI upgrades like hotkey bindings for nearly everything and
many convenience features that I hope will find their way into MM3D applications.

# about
<html>
<body><center><br>
<br>
<h1>Multimedia 3D</h1><br>
https://github.com/mick-p1982/mm3d<br>
Copyright &copy; 2019-2022 Mick Pearson<br><br>
<h1>Maverick Model 3D</h1>
https://clover.moe/mm3d/<br>
Copyright &copy; 2009-2019 Zack Middleton<br><br>
<h1>Misfit Model 3D</h1>		
http://www.misfitcode.com/misfitmodel3d/<br>
Copyright &copy; 2004-2008 Kevin Worcester<br><br>
</center></body></html>
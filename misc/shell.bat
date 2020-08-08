@echo off
subst w: C:\Users\andre\Documents\Progetti-GitHub\handmade-game-cpp
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set path=w:\misc;%path%
w:
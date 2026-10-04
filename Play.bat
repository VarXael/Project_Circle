@echo off
rem Plays this project with Godot 4.7. Set GODOT to use a different editor build.
if "%GODOT%"=="" set GODOT=%USERPROFILE%\Desktop\Godot_v4.7.2-stable_win64.exe
start "" "%GODOT%" --path "%~dp0."

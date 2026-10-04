@echo off
if "%GODOT%"=="" set GODOT=%USERPROFILE%\Desktop\Godot_v4.7.2-stable_win64.exe
start "" "%GODOT%" -e --path "%~dp0."

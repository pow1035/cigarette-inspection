@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

C:\Users\Administrator\anaconda3\envs\yolo26\python.exe build_engine_python.py

pause

@echo off
chcp 65001 >nul
echo ========================================
echo   OTA GUI 预览模式
echo   修改 ui_config.json 后运行此脚本预览
echo   修改 themes/*.json 可定制配色
echo ========================================
echo.

cd /d "%~dp0"
python main.py

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo 运行失败！请检查依赖安装：
    echo   pip install pyserial tkinterdnd2 Pillow
    pause
)

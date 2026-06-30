@echo off
chcp 65001 >nul
echo ========================================
echo   OTA GUI 打包脚本
echo ========================================
echo.

cd /d "%~dp0"

echo [1/3] 安装依赖...
pip install pyserial tkinterdnd2 Pillow pyinstaller -q
if %ERRORLEVEL% NEQ 0 (
    echo 依赖安装失败！请检查网络或手动安装。
    pause
    exit /b 1
)

echo [2/3] 清理旧的构建文件...
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist
if exist *.spec del /q *.spec 2>nul

echo [3/3] 打包中 (pyinstaller --onefile)...
pyinstaller --onefile --windowed --name "CarPanel_OTA_Tool" ^
    --add-data "ui_config.json;." ^
    --add-data "themes;themes" ^
    --hidden-import tkinterdnd2 ^
    --hidden-import tkinterdnd2.tkdnd ^
    --hidden-import tkinterdnd2.platform ^
    --hidden-import PIL ^
    --hidden-import PIL.Image ^
    --hidden-import PIL.ImageDraw ^
    --hidden-import PIL.ImageTk ^
    main.py

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo   打包成功！
    echo   输出: dist\CarPanel_OTA_Tool.exe
    echo ========================================
) else (
    echo.
    echo 打包失败！请检查错误信息。
)

echo.
pause

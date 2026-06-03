@echo off
REM 启动所有 Python 特征提取微服务
REM 需要先安装依赖: pip install -r requirements.txt

cd /d "%~dp0"

echo Starting Chinese-CLIP service (port 18081)...
start "CLIP Service" cmd /k "python clip_service.py"

timeout /t 2 /nobreak >nul

echo Starting WD14-tagger service (port 18082)...
start "WD14 Service" cmd /k "python wd14_service.py"

timeout /t 2 /nobreak >nul

echo Starting InsightFace service (port 18083)...
start "InsightFace Service" cmd /k "python insightface_service.py"

echo.
echo All services started!
echo   CLIP:       http://127.0.0.1:18081/health
echo   WD14:       http://127.0.0.1:18082/health
echo   InsightFace: http://127.0.0.1:18083/health

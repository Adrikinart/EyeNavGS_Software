:: SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
:: SPDX-License-Identifier: Apache-2.0
::
:: This batch script is part of the 👁️NavGS (EyeNavGS) project.
:: See LICENSE_EYENAVGS.txt and NOTICE for details.



@echo off
REM ======== User-Defined Paths ========
set viewer_exe=D:\JasonD\projects\sibr\SIBR_Gaussian_VRV\install\bin\SIBR_gaussianViewer_app_d.exe
REM Enter full path to SIBR_gaussianViewer_app_d.exe (e.g. C:\Users\You\SIBR\install\bin\SIBR_gaussianViewer_app_d.exe)
 
set gs_model_base=D:\JasonD\projects\sibr\scenes\models
REM Enter path to GaussianSplatting models folder (e.g. C:\Users\You\SIBR\models)

set zipnerf_model_base=D:\JasonD\projects\sibr\scenes\zip_nerf_3dgs
REM Enter path to ZipNeRF models folder (e.g. C:\Users\You\ZipNeRF): 

REM ======== Defined test-user folder output Path ========
set trace_output_folder=D:\JasonD\projects\sibr\trace_saving\user000

echo.

REM ======== Scene Selection ========
echo Select a scene to run:
echo   1. truck
echo   2. treehill
echo   3. train
echo   4. stump
echo   5. room
echo   6. playroom
echo   7. kitchen
echo   8. garden
echo   9. flowers
echo  10. drjohnson
echo  11. counter
echo  12. bonsai
echo  13. nyc
echo  14. london
echo  15. berlin
echo  16. alameda
echo  17. bicycle
echo.

set /p choice=Enter the number of the scene to run: 

REM ======== Scene Name Mapping ========
set scene=
if %choice%==1 set scene=truck
if %choice%==2 set scene=treehill
if %choice%==3 set scene=train
if %choice%==4 set scene=stump
if %choice%==5 set scene=room
if %choice%==6 set scene=playroom
if %choice%==7 set scene=kitchen
if %choice%==8 set scene=garden
if %choice%==9 set scene=flowers
if %choice%==10 set scene=drjohnson
if %choice%==11 set scene=counter
if %choice%==12 set scene=bonsai
if %choice%==13 set scene=nyc
if %choice%==14 set scene=london
if %choice%==15 set scene=berlin
if %choice%==16 set scene=alameda
if %choice%==17 set scene=bicycle

REM ======== Init Parameters by Scene ========
if "%scene%"=="truck" (
    set position= 0 2.1 -4
    set quaternion=-0.0896 0.0000 0.0000 0.9960
    set scale=0.76
)
if "%scene%"=="treehill" (
    set position=2 1.4 2
    set quaternion=-0.1961 0.0000 0.0000 0.9806
    set scale=1
)
if "%scene%"=="train" (
    set position=2 -1 6
    set quaternion=0.0499 0.0000 0.0100 0.9987
    set scale=0.36
)
if "%scene%"=="stump" (
    set position=-1 2.65 -2.5
    set quaternion=-0.3950 0.0000 0.0000 0.9187
    set scale=1
)
if "%scene%"=="room" (
    set position=0 1.15 0
    set quaternion=-0.2334 0.0000 0.0000 0.9724
    set scale=2
)
if "%scene%"=="playroom" (
    set position=0 0.88 0
    set quaternion=-0.1961 0.0000 0.0000 0.9806
    set scale=2.7
)
if "%scene%"=="kitchen" (
    set position=0.6 0.7 0
    set quaternion=-0.3420 0.0000 0.0000 0.9397
    set scale=5
)
if "%scene%"=="garden" (
    set position=1 1 1
    set quaternion=-0.2516 0.0000 0.0000 0.9678
    set scale=1.66
)
if "%scene%"=="flowers" (
    set position=0 0 -2
    set quaternion=0.1305 0.0000 0.0000 0.9914
    set scale=1
)
if "%scene%"=="drjohnson" (
    set position=0 1.5 0
    set quaternion=-0.3699 0.0000 0.5976 0.7114
    set scale=1
)
if "%scene%"=="counter" (
    set position=0 1 -0.3
    set quaternion=-0.3007 0.0000 0.0000 0.9537
    set scale=4
)
if "%scene%"=="bonsai" (
    set position=0.7 1 -1
    set quaternion=-0.3420 0.0000 0.0000 0.9397
    set scale=3
)
if "%scene%"=="nyc" (
    set position=-1.6 4.4 4
    set quaternion=-0.1483 0.0000 0.0000 0.9888
    set scale=0.64
)
if "%scene%"=="london" (
    set position=20 14 -9
    set quaternion=0 0 0 1
    set scale=0.53
)
if "%scene%"=="berlin" (
    set position=-1 1.8 -1.3
    set quaternion=0.0299 0.0000 -0.0599 0.9978
    set scale=0.8
)
if "%scene%"=="alameda" (
    set position=3 2.5 -1
    set quaternion=-0.1867 0.0000 0.0000 0.9824
    set scale=0.64
)
if "%scene%"=="bicycle" (
    set position=1.5 1.1 0
    set quaternion=-0.1142 0.0000 0.0000 0.9935
    set scale=1.25
)

REM ======== Dataset Folder Resolution ========
set model_base=%gs_model_base%
if "%scene%"=="nyc" set model_base=%zipnerf_model_base%
if "%scene%"=="london" set model_base=%zipnerf_model_base%
if "%scene%"=="berlin" set model_base=%zipnerf_model_base%
if "%scene%"=="alameda" set model_base=%zipnerf_model_base%

set model_path=%model_base%\%scene%

REM ======== Execute Viewer in Headset Mode (Recording) ========
echo.
echo Starting scene: %scene%
echo Model path: %model_path%
echo Initial position: %position%
echo Initial quaternion: %quaternion%
echo Scale: %scale%
echo.

REM "%viewer_exe%" -m "%model_path%" --rendering-mode 2 --initial-position %position% --initial-quaternion %quaternion% --initial-scale %scale%
pushd "%trace_output_folder%"
"%viewer_exe%" -m "%model_path%" --rendering-mode 2 --initial-position %position% --initial-quaternion %quaternion% --initial-scale %scale%
popd

pause



@echo off
chcp 65001 >nul
cd /d %~dp0
if not exist efisp mkdir efisp

echo === ABL Toolkit: AVB Disable + Fake Re-lock (Windows) ===
echo.

if not exist images\abl.img (
  echo ERROR: images\abl.img not found
  echo Place your abl partition image at images\abl.img
  exit /b 1
)

echo [1/3] Extracting ABL ELF from abl.img...
bin\extractfv.exe -o . images\abl.img
if errorlevel 1 (
  echo ERROR: extractfv failed
  exit /b 1
)

if not exist LinuxLoader.efi (
  echo ERROR: extractfv produced no LinuxLoader.efi
  exit /b 1
)

move /Y LinuxLoader.efi ABL_original.efi >nul
echo   Original ABL saved as ABL_original.efi

echo.
echo [2/3] Applying AVB verification disable patch...
bin\patch_abl_avb.exe ABL_original.efi ABL_avb.efi
if errorlevel 1 (
  echo.
  echo ERROR: patch_abl_avb AVB disable failed
  exit /b 1
)

if not exist ABL_avb.efi (
  echo ERROR: patch_abl_avb produced no output
  exit /b 1
)
echo   AVB disable applied: ABL_avb.efi

echo.
echo [3/3] Applying fake re-lock patch (gbl patch_abl)...
bin\patch_abl.exe ABL_avb.efi efisp\boot.efi
if errorlevel 1 (
  echo.
  echo ERROR: gbl patch_abl fake re-lock failed
  exit /b 1
)

if not exist efisp\boot.efi (
  echo ERROR: patch_abl produced no output
  exit /b 1
)
echo   Fake re-lock applied: efisp\boot.efi

echo.
echo ========================================
echo Done. Outputs:
echo   efisp\boot.efi   - EFISP Android loader (AVB disabled + fake re-lock)
echo   ABL_avb.efi        - after AVB disable only (intermediate)
echo   ABL_original.efi   - original unpatched ABL (backup)
echo ========================================

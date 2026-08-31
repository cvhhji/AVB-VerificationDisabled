@echo off
chcp 65001 >nul
cd /d %~dp0
if not exist efisp mkdir efisp

echo === Safe EFISP Loader + Fake Re-lock (Windows) ===
echo.

if not exist images\abl.img (
  echo ERROR: images\abl.img not found
  echo Place your abl partition image at images\abl.img
  exit /b 1
)

echo [1/2] Extracting Android LinuxLoader from abl.img...
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
echo [2/3] Disabling the AVB2 verification route...
bin\patch_abl_avb.exe ABL_original.efi ABL_avb.efi
if errorlevel 1 (
  echo ERROR: unique AVB2 route was not found; refusing unsafe output
  exit /b 1
)

echo [3/3] Applying gbl_root_canoe fake re-lock patch...
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
echo   efisp\boot.efi   - EFISP loader: AVB2 route disabled + fake re-lock
echo   ABL_original.efi   - original unpatched ABL (backup)
echo ========================================

@echo off
chcp 65001 >nul
cd /d %~dp0

echo === AVB-VerificationDisabled ABL Toolkit (Windows) ===
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
bin\patch_abl_avb.exe ABL_original.efi ABL_patched.efi
if errorlevel 1 (
  echo.
  echo ERROR: patch_abl_avb failed
  echo The ABL may use a different code layout than expected.
  exit /b 1
)

if not exist ABL_patched.efi (
  echo ERROR: patch_abl_avb produced no output
  exit /b 1
)

echo.
echo [3/3] Done.
echo.
echo ========================================
echo Outputs:
echo   ABL_patched.efi   - ABL with AVB verification disabled (BL-stage)
echo   ABL_original.efi  - original unpatched ABL (backup, do NOT flash)
echo.
echo Flash ABL_patched.efi to the abl partition (repack if needed).
echo Combine with gbl_root_canoe fake-relock for locked+green state.
echo ========================================

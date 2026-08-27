@echo off
chcp 65001 >nul
cd /d %~dp0

echo === ABL Toolkit: Fake Re-lock + AVB Disable (Windows) ===
echo.

if not exist images\abl.img (
  echo ERROR: images\abl.img not found
  echo Place your abl partition image at images\abl.img
  exit /b 1
)

echo [1/5] Extracting ABL ELF from abl.img...
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
echo [2/5] Applying fake re-lock patch (gbl patch_abl)...
bin\patch_abl.exe ABL_original.efi ABL_relocked.efi
if errorlevel 1 (
  echo.
  echo ERROR: gbl patch_abl (fake re-lock) failed
  exit /b 1
)

if not exist ABL_relocked.efi (
  echo ERROR: patch_abl produced no output
  exit /b 1
)
echo   Fake re-lock applied: ABL_relocked.efi

echo.
echo [3/5] Applying AVB verification disable patch...
bin\patch_abl_avb.exe ABL_relocked.efi ABL_patched.efi
if errorlevel 1 (
  echo.
  echo ERROR: patch_abl_avb (AVB disable) failed
  exit /b 1
)

if not exist ABL_patched.efi (
  echo ERROR: patch_abl_avb produced no output
  exit /b 1
)
echo   AVB disable applied: ABL_patched.efi

echo.
echo [4/5] Repacking into abl.img (in-place replace)...
bin\abl_pack.exe images\abl.img ABL_patched.efi abl_patched.img
if errorlevel 1 (
  echo.
  echo ERROR: abl_pack failed to repack
  exit /b 1
)

echo.
echo [5/5] Done.
echo.
echo ========================================
echo Outputs:
echo   abl_patched.img     - final abl.img (fake re-lock + AVB disabled)
echo   ABL_patched.efi     - patched ABL ELF (for analysis)
echo   ABL_relocked.efi    - after fake re-lock only (intermediate)
echo   ABL_original.efi    - original unpatched ABL (backup, do NOT flash)
echo.
echo Flash abl_patched.img to the abl partition:
echo   dd if=abl_patched.img of=/dev/block/by-name/abl
echo ========================================

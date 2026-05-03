# WinCarcerate Builder Script

$PROJ = "WinCarcerate"
$CFG  = "Release"
$PLAT = "x64"

Write-Host ""
Write-Host "
888       888 d8b           .d8888b.                                                   888            
888   o   888 Y8P          d88P  Y88b                                                  888            
888  d8b  888              888    888                                                  888            
888 d888b 888 888 88888b.  888         8888b.  888d888 .d8888b .d88b.  888d888 8888b.  888888 .d88b.  
888d88888b888 888 888 `"88b 888            `"88b 888P`"  d88P`"   d8P  Y8b 888P`"      `"88b 888   d8P  Y8b 
88888P Y88888 888 888  888 888    888 .d888888 888    888     88888888 888    .d888888 888   88888888 
8888P   Y8888 888 888  888 Y88b  d88P 888  888 888    Y88b.   Y8b.     888    888  888 Y88b. Y8b.     
888P     Y888 888 888  888  `"Y8888P`"  `"Y888888 888     `"Y8888P `"Y8888  888    `"Y888888  `"Y888 `"Y8888  
                                                                                                      
" -ForegroundColor Yellow

Write-Host ""
Write-Host "=== $PROJ Build ($CFG|$PLAT) ===" -ForegroundColor Cyan
Write-Host ""

# --- Toolset ---
$toolChoice = Read-Host "Toolset: [M]SVC or [L]LVM/Clang-cl?"
if ($toolChoice -match '^[Ll]') {
    $TOOLSET     = "ClangCL"
    $CLANG_EXTRA = "/clang:-mrdrnd /clang:-maes /clang:-msse4.1"
} else {
    $TOOLSET     = "v143"
    $CLANG_EXTRA = ""
}

# --- Locker or Decryptor ---
$exeChoice = Read-Host "Build [L]ocker or [D]ecryptor?"
if ($exeChoice -match '^[Dd]') {
    $EXE      = "Decryptor"
    $EXE_DEFS = "/DDECRYPTOR"
} else {
    $EXE      = "Locker"
    $EXE_DEFS = "/DLOCKER"
}

# --- Logging ---
$logChoice = Read-Host "Enable logging? [Y]es or [N]o?"
if ($logChoice -match '^[Yy]') {
    $LOG      = $true
    $LOG_DEFS = "/DLOG_TO_CONSOLE"
} else {
    $LOG      = $false
    $LOG_DEFS = ""
}

# --- Assemble CL extra defines ---
$CL_ARGS = "$EXE_DEFS $LOG_DEFS $CLANG_EXTRA".Trim()

Write-Host ""
Write-Host "  Toolset : $TOOLSET"        -ForegroundColor Yellow
Write-Host "  Target  : $EXE"            -ForegroundColor Yellow
Write-Host "  Logging : $LOG"            -ForegroundColor Yellow
Write-Host "  Defines : $CL_ARGS"        -ForegroundColor Yellow
Write-Host ""

# --- Save and set CL env var ---
$CL_SAVED = $env:CL
$env:CL   = "$CL_ARGS $CL_SAVED".Trim()

# --- MSBuild ---
msbuild "$PROJ.sln" /m /t:Rebuild `
    /p:Configuration=$CFG `
    /p:Platform=$PLAT `
    /p:PlatformToolset=$TOOLSET `
    /nologo

$ERR = $LASTEXITCODE

# --- Restore CL ---
$env:CL = $CL_SAVED

if ($ERR -eq 0) {
    Write-Host ""
    Write-Host "  Build succeeded." -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "  Build failed. (exit $ERR)" -ForegroundColor Red
}

Write-Host ""
exit $ERR
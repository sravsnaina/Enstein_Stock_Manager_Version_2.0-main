; Builds the single-file portable EnsteinStockManager-Portable-<version>.exe.
;
; Compiled in CI with:
;   makensis /DAPPVERSION=2.1.4 /DSTAGEDIR=<deployed app dir> \
;            /DICONFILE=<app-icon.ico> /DOUTFILE=<output exe> portable.nsi
;
; Why NSIS and not a 7-Zip SFX: 7-Zip stopped shipping the installer-type SFX
; stubs (7zS.sfx / 7zSD.sfx). The 7z.sfx that ships with 7-Zip is the plain
; extractor and has no RunProgram support, so it cannot launch the app after
; unpacking. NSIS does this natively and needs no third-party binary.
;
; $PLUGINSDIR is an NSIS-managed temp directory that is deleted automatically
; when this process exits, which is exactly the lifetime we want: unpack, run
; the app, and leave nothing behind once the user closes it.

Unicode true
Name "Enstein Stock Manager"
OutFile "${OUTFILE}"
Icon "${ICONFILE}"
; No admin rights: this build exists so it can run anywhere, including on
; locked-down machines where the installer is not an option.
RequestExecutionLevel user
SetCompressor /SOLID lzma
XPStyle on

VIProductVersion "${APPVERSION}.0"
VIAddVersionKey "ProductName"     "Enstein Stock Manager"
VIAddVersionKey "FileDescription" "Enstein Stock Manager (portable)"
VIAddVersionKey "CompanyName"     "Enstein Robots and Automations Pvt Limited"
VIAddVersionKey "LegalCopyright"  "Copyright (C) Enstein Robots and Automations Pvt Limited"
VIAddVersionKey "FileVersion"     "${APPVERSION}.0"
VIAddVersionKey "ProductVersion"  "${APPVERSION}.0"

; Show extraction progress, then get out of the way. Unpacking ~200 MB takes a
; few seconds and a silent build would look like nothing had happened.
Page instfiles
AutoCloseWindow true
ShowInstDetails hide

Section
    InitPluginsDir
    SetOutPath "$PLUGINSDIR\app"
    File /r "${STAGEDIR}\*.*"

    ; Hide the unpacking window while the app itself is on screen. ExecWait
    ; keeps this process alive for as long as the app runs, which is what keeps
    ; $PLUGINSDIR (and therefore the app's own DLLs) from being deleted early.
    HideWindow
    ExecWait '"$PLUGINSDIR\app\EnsteinStockManager.exe"'
SectionEnd

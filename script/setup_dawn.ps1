Set-Variable ROOT (Get-Item $PSScriptRoot).Parent.FullName

git submodule update --init "$ROOT/dep/depot_tools"
git submodule update --init "$ROOT/dep/dawn"

Write-Host "SETUP DAWN"
Push-Location "$ROOT/dep/dawn"
  & "$ROOT/dep/depot_tools/gclient.bat" sync
Pop-Location

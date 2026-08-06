$files = @("Top Bar.bin", "Frame.bin", "mode.bin", "ODO.bin", "Error Box.bin")
foreach ($f in $files) {
    $path = Join-Path $PSScriptRoot $f
    if (Test-Path $path) {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $size = $bytes.Length
        $h4 = ($bytes[0..3] | ForEach-Object { $_.ToString("X2") }) -join " "
        $h8 = ($bytes[4..7] | ForEach-Object { $_.ToString("X2") }) -join " "
        $dword = [BitConverter]::ToUInt32($bytes, 0)
        Write-Host "$f : size=$size, header=[$h4], dword=0x$($dword.ToString('X8')), bytes4-7=[$h8]"
    } else {
        Write-Host "$f : NOT FOUND"
    }
}

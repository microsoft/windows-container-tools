function ValidateSigning(
    [Parameter(Mandatory=$true)]
    [string] $BinaryPath
)
{
    Write-Host $LASTEXITCODE
    $signtoolCommand = "..\tools\signtool.exe verify /hash SHA256 /KP /V /ph"
    $binarySigning = Get-AuthenticodeSignature $BinaryPath;

    if($binarySigning.Status -eq "NotSigned")
    {
        Write-Host "WARNING: Base binary ($BinaryPath) is not signed. Skipping.";
        return 0;
    }

    Write-Host $LASTEXITCODE
    Invoke-Expression "$signtoolCommand $BinaryPath"

    if ($LASTEXITCODE -eq 0) 
    {
        Write-Host "Signing validation succeeded.";
    } else 
    {
        Write-Host "ERROR: Signing validation failed for LogMonitor.exe `"$BinaryPath`": location.";
    
    }

    Write-Host $LASTEXITCODE

    return $LASTEXITCODE;
}

ValidateSigning -BinaryPath "D:\LogMonitor.exe"
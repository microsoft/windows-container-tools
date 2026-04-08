
# setting variables from variable file
Write-Host "Loading variables from .\vars.txt" -ForegroundColor Yellow
foreach ($line in Get-Content .\vars.txt) {
	$line = $line.Trim()
	if ([string]::IsNullOrWhiteSpace($line)) { continue }
	if ($line.StartsWith('#')) { continue }
	$parts = $line -split '=', 2
	$name = $parts[0].Trim()
	$value = if ($parts.Count -gt 1) { $parts[1].Trim() } else { '' }
	Set-Variable -Name $name -Value $value -Scope Script
}

# ensure an Azure subscription is selected
try {
	$acct = az account show -o json 2>$null | ConvertFrom-Json
} catch {
	$acct = $null
}
if (-not $acct) {
	Write-Error "No Azure subscription selected. Run `az login` then `az account set --subscription <name|id>` and retry."
	exit 1
}

Write-Host "Using subscription: $($acct.name) ($($acct.id))" -ForegroundColor Green

Write-Host "Creating resource group: $resourceGroup" -ForegroundColor Yellow
az group create --name $resourceGroup --location $resourceGroupAZ

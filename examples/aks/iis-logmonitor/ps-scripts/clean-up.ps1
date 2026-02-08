
param(
	[switch]$Force
)

# setting variables from variable file (robust parser)
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

if (-not $Force) {
	$confirm = Read-Host "This will DELETE resource group '$resourceGroup' and all resources. Type 'DELETE' to confirm"
	if ($confirm -ne 'DELETE') {
		Write-Host "Aborted by user." -ForegroundColor Yellow
		exit 0
	}
}

Write-Host "Deleting resource group: $resourceGroup" -ForegroundColor Red
az group delete --name $resourceGroup --yes --no-wait
if ($LASTEXITCODE -ne 0) {
	Write-Error "Failed to initiate deletion of resource group '$resourceGroup' (exit code $LASTEXITCODE)"
	exit $LASTEXITCODE
}

Write-Host "Deletion initiated. Use 'az group show --name $resourceGroup' to check status." -ForegroundColor Green

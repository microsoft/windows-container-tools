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

# ensure kubectl is available
if (-not (Get-Command kubectl -ErrorAction SilentlyContinue)) {
	Write-Error "kubectl not found in PATH. Install kubectl and ensure it's on PATH."
	exit 1
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$deploymentDir = Resolve-Path -Path (Join-Path $scriptDir '..\deployment') -ErrorAction SilentlyContinue
if (-not $deploymentDir) {
	Write-Error "Deployment directory '..\deployment' not found relative to script."
	exit 1
}

Write-Host "Deploying app onto K8s cluster $clusterName" -ForegroundColor Yellow

# show current kubectl context (informational)
$currentContext = kubectl config current-context 2>$null
if ($currentContext) { Write-Host "kubectl current-context: $currentContext" -ForegroundColor Yellow } else { Write-Warning "kubectl current-context is not set. Ensure your kubeconfig is correct." }

kubectl apply -f $deploymentDir.Path
if ($LASTEXITCODE -ne 0) {
	Write-Error "kubectl apply failed with exit code $LASTEXITCODE"
	exit $LASTEXITCODE
}

Write-Host "Completed: run [kubectl get pods] or [kubectl describe pods] to see details" -ForegroundColor Green

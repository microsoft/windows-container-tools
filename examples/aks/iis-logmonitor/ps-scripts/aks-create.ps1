# ref - https://learn.microsoft.com/en-us/azure/aks/learn/quick-kubernetes-deploy-cli
# az provider register --namespace Microsoft.OperationsManagement
# az provider register --namespace Microsoft.OperationalInsights
# az provider show -n Microsoft.OperationsManagement -o table
# az provider show -n Microsoft.OperationalInsights -o table

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

$subscriptionId = $acct.id
$tenantId = $acct.tenantId
Write-Host "Using subscription: $($acct.name) ($subscriptionId), Tenant: $tenantId" -ForegroundColor Green

# az aks create `
#     --resource-group $resourceGroup `
#     --name $clusterName `
#     --node-count 2 `
#     --enable-addons monitoring `
#     --generate-ssh-keys `
#     --windows-admin-username $winUsername `
#     --windows-admin-password $winPassword `
#     --vm-set-type VirtualMachineScaleSets `
#     --network-plugin azure

# removing the "--enable-addons monitoring" // fails on our subscription
az aks create `
    --resource-group $resourceGroup `
    --name $clusterName `
    --node-count 2 `
    --generate-ssh-keys `
    --windows-admin-username $winUsername `
    --windows-admin-password $winPassword `
    --vm-set-type VirtualMachineScaleSets `
    --network-plugin azure

Write-Host "Getting cluster configuration" -ForegroundColor Yellow
az aks get-credentials --resource-group $resourceGroup --name $clusterName

# By default, an AKS cluster is created with a node pool that can run Linux containers
# add an additional node pool that can run Windows Server containers alongside the Linux node pool
Write-Host "Adding Windows Server containers node pool" -ForegroundColor Yellow

az aks nodepool add `
    --resource-group $resourceGroup `
    --cluster-name $clusterName `
    --os-type $osType `
    --name $windowsNodepoolName `
    --node-count 1 `
    --os-sku $osSku

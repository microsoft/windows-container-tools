#############################################
# Add Windows node pool on the AKS cluster
#############################################

# setting variables from variable file
Write-Host "Loading variables from .\vars.txt" -ForegroundColor Yellow
Foreach ($i in $(Get-Content vars.txt)){Set-Variable -Name $i.split("=")[0] -Value $i.split("=").split(" ")[1]}

$tenantId = (az account show | ConvertFrom-Json).tenantId
Write-Host "Subscription Id: $subscriptionId, Tenant Id: $tenantId" -ForegroundColor Green

Write-Host "Deleting Windows Server node pool: name=$windowsNodepoolName" -ForegroundColor Yellow

az aks nodepool delete `
    --cluster-name $clusterName `
    --name $windowsNodepoolName `
    --resource-group $resourceGroup `
    --no-wait

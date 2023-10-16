#############################################
# Add Windows node pool on the AKS cluster
#############################################

# setting variables from variable file
Write-Host "Loading variables from .\vars.txt" -ForegroundColor Yellow
Foreach ($i in $(Get-Content vars.txt)){Set-Variable -Name $i.split("=")[0] -Value $i.split("=").split(" ")[1]}

$tenantId = (az account show | ConvertFrom-Json).tenantId
Write-Host "Subscription Id: $subscriptionId, Tenant Id: $tenantId" -ForegroundColor Green

Write-Host "Adding Windows Server node pool: sku=$osSKU, count=$windowsNodeCount, size=$nodeVmSize" -ForegroundColor Yellow

az aks nodepool add `
    --resource-group $resourceGroup `
    --cluster-name $clusterName `
    --os-type $osType `
    --os-sku $osSKU `
    --name $windowsNodepoolName `
    --node-count $windowsNodeCount `
    --node-vm-size $nodeVmSize

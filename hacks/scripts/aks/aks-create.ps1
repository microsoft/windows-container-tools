# ref - https://learn.microsoft.com/en-us/azure/aks/learn/quick-kubernetes-deploy-cli
# az provider register --namespace Microsoft.OperationsManagement
# az provider register --namespace Microsoft.OperationalInsights
# az provider show -n Microsoft.OperationsManagement -o table
# az provider show -n Microsoft.OperationalInsights -o table

# setting variables from variable file
Write-Host "Loading variables from .\vars.txt" -ForegroundColor Yellow
Foreach ($i in $(Get-Content vars.txt)){Set-Variable -Name $i.split("=")[0] -Value $i.split("=").split(" ")[1]}

# set default subscription
az account set --subscription $subscriptionId

$tenantId = (az account show | ConvertFrom-Json).tenantId
Write-Host "Subscription Id: $subscriptionId, Tenant Id: $tenantId" -ForegroundColor Green

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
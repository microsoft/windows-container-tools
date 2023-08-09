
# setting variables from variable file
Write-Host "Loading variables from .\vars.txt" -ForegroundColor Yellow
Foreach ($i in $(Get-Content vars.txt)){Set-Variable -Name $i.split("=")[0] -Value $i.split("=").split(" ")[1]}

Write-Host "Deleting resouce group: $resourceGroup" -ForegroundColor Red
az group delete --name $resourceGroup --yes --no-wait

Write-Host "Completed."

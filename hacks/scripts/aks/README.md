# NOTES

## Setting up AKS Cluster with Windows

- Make a copy of `vars.example.txt` and rename it to `vars.txt`.
- Update the variable details: `subscriptionId`, `resourceGroup`, etc.
- To run the scripts, `cd` into `hacks\scripts\aks`.
- Run `.\rg-create.ps1` to create the _resource group_ and set default subscription Id.
- Run `.\aks-create.ps1` to create the AKS cluster.
- Run `.\aks-add-windows-nodepool.ps1` to add the Windows nodepool to the AKS cluster.

## Cleaning up the AKS Cluster

- To clean up the everything, run `rg-delete.ps1` to delete the _resource group_.

Get-ChildItem -Path "Registry::HKEY_CLASSES_ROOT\Local Settings\Software\Microsoft\Windows\CurrentVersion\AppContainer\Mappings" | ForEach-Object {
    $sid = $_.PSChildName
    $props = Get-ItemProperty $_.PsPath
    [PSCustomObject]@{
        SID = $sid
        Name = $props.AppContainerName
        DisplayName = $props.DisplayName
        Description = $props.Description
    }
}
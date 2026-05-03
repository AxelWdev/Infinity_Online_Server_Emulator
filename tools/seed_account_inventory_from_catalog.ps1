Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$cppServerRoot = Split-Path -Parent $scriptRoot
$catalogPath = Join-Path $cppServerRoot 'data\item_id_catalog.json'
$databasePaths = @(
    (Join-Path $cppServerRoot 'account_database.json'),
    (Join-Path $cppServerRoot 'account_database copy.json')
)

function Choose-StarterWeaponId {
    param(
        [object]$CharacterBucket
    )

    $weapons = @($CharacterBucket.equipment_items.Weapon)
    $preferred = @(
        $weapons |
            Where-Object {
                $_.display_name -ne 'Character set up weapon' -and
                (
                    [string]::IsNullOrWhiteSpace($_.required_level_text) -or
                    $_.required_level_text -eq 'None'
                )
            } |
            Sort-Object item_id
    )
    if ($preferred.Count -gt 0) {
        return [int]$preferred[0].item_id
    }

    $fallback = @(
        $weapons |
            Where-Object { $_.display_name -ne 'Character set up weapon' } |
            Sort-Object item_id
    )
    if ($fallback.Count -gt 0) {
        return [int]$fallback[0].item_id
    }

    return 0
}

function Choose-StarterClothesId {
    param(
        [object]$CharacterBucket
    )

    $clothes = @($CharacterBucket.equipment_items.'Main Clothes')
    $preferredBase = @(
        $clothes |
            Where-Object {
                $_.display_name -notmatch 'training suit' -and
                $_.item_id -ge 8090 -and
                $_.item_id -le 8098
            } |
            Sort-Object item_id
    )
    if ($preferredBase.Count -gt 0) {
        return [int]$preferredBase[0].item_id
    }

    $fallback = @(
        $clothes |
            Where-Object {
                $_.display_name -notmatch 'training suit' -and
                [string]::IsNullOrWhiteSpace($_.required_level_text)
            } |
            Sort-Object item_id
    )
    if ($fallback.Count -gt 0) {
        return [int]$fallback[0].item_id
    }

    return 0
}

$catalog = Get-Content -Path $catalogPath -Raw | ConvertFrom-Json

$sharedItemStacks = @(
    $catalog.starter_items |
        Where-Object {
            -not $_.is_equipment -and
            -not [string]::IsNullOrWhiteSpace($_.display_name)
        } |
        Sort-Object item_id |
        ForEach-Object {
            [ordered]@{
                item_id = [int]$_.item_id
                owned_count = 99
            }
        }
)

$starterCharacters = @(
    $catalog.equipment_by_character |
        Where-Object { $_.character_id -ge 1 -and $_.character_id -le 9 } |
        Sort-Object character_id |
        ForEach-Object {
            $weaponId = Choose-StarterWeaponId $_
            $clothesId = Choose-StarterClothesId $_
            [ordered]@{
                character_id = [int]$_.character_id
                equipped_weapon_item_id = 0
                clothes_item_id = 0
                accessory_1_item_id = 0
                accessory_2_item_id = 0
                accessory_3_item_id = 0
                owned_weapon_item_ids = @($(if ($weaponId -ne 0) { $weaponId }))
                owned_clothes_item_ids = @($(if ($clothesId -ne 0) { $clothesId }))
                owned_accessory_1_item_ids = @()
                owned_accessory_2_item_ids = @()
                owned_accessory_3_item_ids = @()
            }
        }
)

$starterInventory = [ordered]@{
    shared_item_stacks = $sharedItemStacks
    characters = $starterCharacters
}

foreach ($databasePath in $databasePaths) {
    $database = Get-Content -Path $databasePath -Raw | ConvertFrom-Json

    foreach ($account in $database.accounts) {
        if (-not $account.profile) {
            $account | Add-Member -NotePropertyName profile -NotePropertyValue ([pscustomobject]@{})
        }

        $account.profile | Add-Member -Force -NotePropertyName inventory -NotePropertyValue $starterInventory

        foreach ($propertyName in @('packet_6b_entries', 'packet_3f_entries')) {
            if ($account.profile.PSObject.Properties.Name -contains $propertyName) {
                $account.profile.PSObject.Properties.Remove($propertyName)
            }
        }
    }

    $json = $database | ConvertTo-Json -Depth 100
    [System.IO.File]::WriteAllText(
        $databasePath,
        $json,
        (New-Object System.Text.UTF8Encoding($false)))
    Write-Output "Updated $databasePath"
}

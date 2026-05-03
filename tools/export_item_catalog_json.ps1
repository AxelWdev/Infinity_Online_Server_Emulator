Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$cppServerRoot = Split-Path -Parent $scriptRoot
$repoRoot = Split-Path -Parent $cppServerRoot

$outputDirectory = Join-Path $cppServerRoot 'data'
$outputPathById = Join-Path $outputDirectory 'item_id_catalog.json'
$outputPathByType = Join-Path $outputDirectory 'item_type_catalog.json'

function Resolve-PreferredPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Could not find any of the expected source files: $($Candidates -join ', ')"
}

function Split-TabLine {
    param(
        [string]$Line
    )

    return $Line -split "`t", -1
}

function Normalize-CharacterKey {
    param(
        [string]$Text
    )

    $normalized = ($Text.ToLowerInvariant() -replace '[^a-z0-9]', '')
    if ($normalized -eq 'kirious') {
        return 'kirius'
    }
    return $normalized
}

function Parse-OptionalInt {
    param(
        [string]$Text
    )

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return $null
    }
    if ($Text -match '^[0-9]+$') {
        return [int]$Text
    }
    return $null
}

function Resolve-LocalizedPrice {
    param(
        [int[]]$CandidateValues
    )

    foreach ($value in $CandidateValues) {
        if ($null -ne $value -and $value -gt 0) {
            return $value
        }
    }

    return 0
}

function Get-InventoryBucketName {
    param(
        [string]$ItemTypeName
    )

    switch ($ItemTypeName) {
        'Weapon' { return 'owned_weapon_item_ids' }
        'Main Clothes' { return 'owned_clothes_item_ids' }
        'Accessories (1)' { return 'owned_accessory_1_item_ids' }
        'Accessories (2)' { return 'owned_accessory_2_item_ids' }
        'Accessories (3)' { return 'owned_accessory_3_item_ids' }
        default { return 'shared_item_stacks' }
    }
}

function Get-InventoryTargetName {
    param(
        [string]$InventoryBucketName
    )

    if ($InventoryBucketName -eq 'shared_item_stacks') {
        return 'shared_item_stack'
    }

    return 'character_owned'
}

function New-EquipmentCategories {
    return [ordered]@{
        'Weapon' = @()
        'Main Clothes' = @()
        'Accessories (1)' = @()
        'Accessories (2)' = @()
        'Accessories (3)' = @()
    }
}

function New-CharacterBucket {
    param(
        [int]$CharacterId,
        [string]$EnglishName,
        [string]$NormalizedKey
    )

    return [ordered]@{
        character_id = $CharacterId
        english_name = $EnglishName
        normalized_key = $NormalizedKey
        character_name_aliases = @($EnglishName)
        equipment_items = New-EquipmentCategories
    }
}

function New-ItemTypeBucket {
    param(
        [string]$ItemTypeName,
        [string]$ItemTypeNameKr,
        [string]$InventoryBucket,
        [bool]$IsEquipmentType
    )

    return [ordered]@{
        item_type_name = $ItemTypeName
        item_type_name_kr = $ItemTypeNameKr
        inventory_bucket = $InventoryBucket
        inventory_target = Get-InventoryTargetName $InventoryBucket
        is_equipment_type = $IsEquipmentType
        items = @()
        character_buckets = [ordered]@{}
    }
}

function New-ItemTypeCharacterBucket {
    param(
        [int]$CharacterId,
        [string]$CharacterName,
        [string]$CharacterNameKr,
        [string]$NormalizedKey
    )

    return [ordered]@{
        character_id = $CharacterId
        character_name = $CharacterName
        character_name_kr = $CharacterNameKr
        normalized_key = $NormalizedKey
        item_ids = @()
        items = @()
    }
}

$equipmentCategories = @(
    'Weapon',
    'Main Clothes',
    'Accessories (1)',
    'Accessories (2)',
    'Accessories (3)'
)

$characterCsvPath = Resolve-PreferredPath @(
    (Join-Path $cppServerRoot 'data\setting\character.csv'),
    (Join-Path $repoRoot 'CDirect3DEngineRecomp\GameFiles\setting\character.csv'))
$itemCsvPath = Resolve-PreferredPath @(
    (Join-Path $cppServerRoot 'data\setting\item.csv'),
    (Join-Path $repoRoot 'CDirect3DEngineRecomp\GameFiles\setting\item.csv'))
$gameItemListCsvPath = Resolve-PreferredPath @(
    (Join-Path $cppServerRoot 'data\setting\game_itemlist.csv'),
    (Join-Path $repoRoot 'CDirect3DEngineRecomp\GameFiles\setting\game_itemlist.csv'))

$characterLines = Get-Content -Path $characterCsvPath -Encoding Unicode
$itemLines = Get-Content -Path $itemCsvPath -Encoding Unicode
$gameItemListLines = Get-Content -Path $gameItemListCsvPath -Encoding Unicode

$charactersById = @{}
$charactersByNormalizedKey = @{}
$starterItemIds = @()
$starterItemIdSet = [System.Collections.Generic.HashSet[int]]::new()

for ($lineIndex = 1; $lineIndex -lt $gameItemListLines.Length; $lineIndex++) {
    $columns = Split-TabLine $gameItemListLines[$lineIndex]
    if ($columns.Length -eq 0) {
        continue
    }

    $itemId = Parse-OptionalInt $columns[0]
    if ($null -eq $itemId) {
        continue
    }

    $starterItemIds += $itemId
    [void]$starterItemIdSet.Add($itemId)
}

for ($lineIndex = 1; $lineIndex -lt $characterLines.Length; $lineIndex++) {
    $columns = Split-TabLine $characterLines[$lineIndex]
    if ($columns.Length -le 11) {
        continue
    }

    $characterId = Parse-OptionalInt $columns[1]
    $englishName = $columns[11]
    if ($null -eq $characterId -or [string]::IsNullOrWhiteSpace($englishName)) {
        continue
    }

    $normalizedKey = Normalize-CharacterKey $englishName
    $bucket = New-CharacterBucket -CharacterId $characterId -EnglishName $englishName -NormalizedKey $normalizedKey
    $charactersById[$characterId] = $bucket
    $charactersByNormalizedKey[$normalizedKey] = $bucket
}

$itemMetadataById = @{}
$equipmentItemsById = [ordered]@{}
$itemTypeBuckets = [ordered]@{}

for ($lineIndex = 1; $lineIndex -lt $itemLines.Length; $lineIndex++) {
    $columns = Split-TabLine $itemLines[$lineIndex]
    if ($columns.Length -le 18) {
        continue
    }

    $itemId = Parse-OptionalInt $columns[0]
    if ($null -eq $itemId) {
        continue
    }

    $displayNameKr = $columns[1]
    $displayName = $columns[15]
    $characterNameKr = $columns[10]
    $characterName = $columns[16]
    $itemTypeNameKr = $columns[11]
    $itemTypeName = $columns[17]
    $displayItemTypeName = if (-not [string]::IsNullOrWhiteSpace($itemTypeName)) {
        $itemTypeName
    } elseif (-not [string]::IsNullOrWhiteSpace($itemTypeNameKr)) {
        $itemTypeNameKr
    } else {
        '_blank'
    }
    $equipSlotNameKr = $columns[12]
    $equipSlotName = $columns[18]
    $maxStack = Parse-OptionalInt $columns[2]
    $durability = Parse-OptionalInt $columns[3]
    $itemTypeCode = Parse-OptionalInt $columns[4]
    $attributeCode = Parse-OptionalInt $columns[5]
    $lunaPriceKr = Parse-OptionalInt $columns[6]
    $cashPriceKr = Parse-OptionalInt $columns[7]
    $iconId = Parse-OptionalInt $columns[8]
    $meshFile = $columns[9]
    $sharedItemRaw = $columns[21]
    $saleEnabled = Parse-OptionalInt $columns[22]
    $limitedCount = Parse-OptionalInt $columns[23]
    $runeCompatibility = $columns[24]
    $runeSocketProperty = $columns[25]
    $sortKeyKr = $columns[26]
    $regionMask = if ($columns.Length -gt 28) { $columns[28] } else { '' }
    $cashPriceEn = if ($columns.Length -gt 29) { Parse-OptionalInt $columns[29] } else { $null }
    $cashPriceJp = if ($columns.Length -gt 31) { Parse-OptionalInt $columns[31] } else { $null }
    $cashPriceCn = if ($columns.Length -gt 38) { Parse-OptionalInt $columns[38] } else { $null }
    $sortKeyJp = if ($columns.Length -gt 36) { $columns[36] } else { '' }
    $sortKeyCn = if ($columns.Length -gt 43) { $columns[43] } else { '' }
    $lunaPriceEn = if ($columns.Length -gt 44) { Parse-OptionalInt $columns[44] } else { $null }
    $lunaPriceJp = if ($columns.Length -gt 45) { Parse-OptionalInt $columns[45] } else { $null }
    $lunaPriceCn = if ($columns.Length -gt 46) { Parse-OptionalInt $columns[46] } else { $null }
    $additionalAbility = if ($columns.Length -gt 47) { $columns[47] } else { '' }
    $cooltime = if ($columns.Length -gt 48) { Parse-OptionalInt $columns[48] } else { $null }
    $levelTextKr = if ($columns.Length -gt 49) { $columns[49] } else { '' }
    $requiredLevelText = if ($columns.Length -gt 50) { $columns[50] } else { '' }
    $originId = if ($columns.Length -gt 51) { Parse-OptionalInt $columns[51] } else { $null }
    $levelText = if ($columns.Length -gt 52) { $columns[52] } else { '' }
    $refundRate = if ($columns.Length -gt 54) { Parse-OptionalInt $columns[54] } else { $null }
    $isEquipment = $equipmentCategories -contains $itemTypeName
    $inventoryBucket = Get-InventoryBucketName $itemTypeName
    $normalizedCharacterKey = if ([string]::IsNullOrWhiteSpace($characterName)) { '' } else { Normalize-CharacterKey $characterName }
    $characterId = if ($normalizedCharacterKey -and $charactersByNormalizedKey.ContainsKey($normalizedCharacterKey)) {
        $charactersByNormalizedKey[$normalizedCharacterKey].character_id
    } else {
        0
    }
    $effectiveLunaPrice = Resolve-LocalizedPrice @($lunaPriceKr, $lunaPriceEn, $lunaPriceJp, $lunaPriceCn)
    $effectiveCashPrice = Resolve-LocalizedPrice @($cashPriceKr, $cashPriceEn, $cashPriceJp, $cashPriceCn)

    $itemRecord = [ordered]@{
        item_id = $itemId
        display_name_kr = $displayNameKr
        display_name = $displayName
        character_id = $characterId
        character_name_kr = $characterNameKr
        character_name = $characterName
        character_key = $normalizedCharacterKey
        item_type_name_kr = $itemTypeNameKr
        item_type_name = $displayItemTypeName
        equip_slot_name_kr = $equipSlotNameKr
        equip_slot_name = $equipSlotName
        inventory_bucket = $inventoryBucket
        inventory_target = Get-InventoryTargetName $inventoryBucket
        is_equipment = $isEquipment
        is_starter_item = $starterItemIdSet.Contains($itemId)
        max_stack = $maxStack
        durability = $durability
        item_type_code = $itemTypeCode
        attribute_code = $attributeCode
        shared_item_raw = $sharedItemRaw
        sale_enabled = $saleEnabled
        limited_quantity = $limitedCount
        usable_rune_attributes = $runeCompatibility
        rune_socket_property = $runeSocketProperty
        sort_key_kr = $sortKeyKr
        sort_key_jp = $sortKeyJp
        sort_key_cn = $sortKeyCn
        region_mask = $regionMask
        prices = [ordered]@{
            luna = [ordered]@{
                effective = $effectiveLunaPrice
                korean = $lunaPriceKr
                english = $lunaPriceEn
                japanese = $lunaPriceJp
                chinese = $lunaPriceCn
            }
            cash = [ordered]@{
                effective = $effectiveCashPrice
                korean = $cashPriceKr
                english = $cashPriceEn
                japanese = $cashPriceJp
                chinese = $cashPriceCn
            }
        }
        icon_id = $iconId
        mesh_file = $meshFile
        additional_ability = $additionalAbility
        cooltime = $cooltime
        level_text_kr = $levelTextKr
        level_text = $levelText
        required_level_text = $requiredLevelText
        origin_id = $originId
        refund_rate = $refundRate
    }

    $itemMetadataById[$itemId] = $itemRecord

    $itemTypeBucketKey = $displayItemTypeName
    if (-not $itemTypeBuckets.Contains($itemTypeBucketKey)) {
        $itemTypeBuckets[$itemTypeBucketKey] = New-ItemTypeBucket `
            -ItemTypeName $displayItemTypeName `
            -ItemTypeNameKr $itemTypeNameKr `
            -InventoryBucket $inventoryBucket `
            -IsEquipmentType $isEquipment
    }
    $itemTypeBucket = $itemTypeBuckets[$itemTypeBucketKey]
    $itemTypeBucket.items += $itemRecord

    if ($isEquipment -and -not [string]::IsNullOrWhiteSpace($normalizedCharacterKey)) {
        if (-not $itemTypeBucket.character_buckets.Contains($normalizedCharacterKey)) {
            $itemTypeBucket.character_buckets[$normalizedCharacterKey] = New-ItemTypeCharacterBucket `
                -CharacterId $characterId `
                -CharacterName $characterName `
                -CharacterNameKr $characterNameKr `
                -NormalizedKey $normalizedCharacterKey
        }
        $typeCharacterBucket = $itemTypeBucket.character_buckets[$normalizedCharacterKey]
        $typeCharacterBucket.item_ids += $itemId
        $typeCharacterBucket.items += $itemRecord
    }

    if (-not $isEquipment) {
        continue
    }

    $equipmentItemsById[[string]$itemId] = $itemRecord

    if ([string]::IsNullOrWhiteSpace($characterName)) {
        continue
    }

    $normalizedKey = Normalize-CharacterKey $characterName
    if (-not $charactersByNormalizedKey.ContainsKey($normalizedKey)) {
        $charactersByNormalizedKey[$normalizedKey] = New-CharacterBucket -CharacterId 0 -EnglishName $characterName -NormalizedKey $normalizedKey
    }

    $bucket = $charactersByNormalizedKey[$normalizedKey]
    if ($bucket.character_name_aliases -notcontains $characterName) {
        $bucket.character_name_aliases += $characterName
    }

    $equipmentEntry = [ordered]@{
        item_id = $itemId
        display_name = $displayName
        item_type_name = $itemTypeName
        equip_slot_name = $equipSlotName
        level_text = $levelText
        required_level_text = $requiredLevelText
        origin_id = $originId
    }

    $bucket.equipment_items[$itemTypeName] += $equipmentEntry
}

$starterItemsById = [ordered]@{}

foreach ($itemId in $starterItemIds) {
    if ($itemMetadataById.ContainsKey($itemId)) {
        $starterItemsById[[string]$itemId] = $itemMetadataById[$itemId]
    } else {
        $starterItemsById[[string]$itemId] = [ordered]@{
            item_id = $itemId
            display_name_kr = ''
            display_name = ''
            character_id = 0
            character_name_kr = ''
            character_name = ''
            character_key = ''
            item_type_name_kr = ''
            item_type_name = ''
            equip_slot_name_kr = ''
            equip_slot_name = ''
            inventory_bucket = 'shared_item_stacks'
            inventory_target = 'shared_item_stack'
            is_equipment = $false
            is_starter_item = $true
            max_stack = $null
            durability = $null
            item_type_code = $null
            attribute_code = $null
            shared_item_raw = ''
            sale_enabled = $null
            limited_quantity = $null
            usable_rune_attributes = ''
            rune_socket_property = ''
            sort_key_kr = ''
            sort_key_jp = ''
            sort_key_cn = ''
            region_mask = ''
            prices = [ordered]@{
                luna = [ordered]@{
                    effective = 0
                    korean = $null
                    english = $null
                    japanese = $null
                    chinese = $null
                }
                cash = [ordered]@{
                    effective = 0
                    korean = $null
                    english = $null
                    japanese = $null
                    chinese = $null
                }
            }
            icon_id = $null
            mesh_file = ''
            additional_ability = ''
            cooltime = $null
            level_text_kr = ''
            level_text = ''
            required_level_text = ''
            origin_id = $null
            refund_rate = $null
        }
    }
}

$characterBuckets = @()
foreach ($bucket in $charactersByNormalizedKey.Values | Sort-Object { $_['character_id'] }, { $_['english_name'] }) {
    foreach ($category in $equipmentCategories) {
        $bucket.equipment_items[$category] = @($bucket.equipment_items[$category] | Sort-Object { $_['item_id'] })
    }

    $bucket['equipment_item_ids'] = [ordered]@{
        'Weapon' = @($bucket.equipment_items['Weapon'] | ForEach-Object { $_.item_id })
        'Main Clothes' = @($bucket.equipment_items['Main Clothes'] | ForEach-Object { $_.item_id })
        'Accessories (1)' = @($bucket.equipment_items['Accessories (1)'] | ForEach-Object { $_.item_id })
        'Accessories (2)' = @($bucket.equipment_items['Accessories (2)'] | ForEach-Object { $_.item_id })
        'Accessories (3)' = @($bucket.equipment_items['Accessories (3)'] | ForEach-Object { $_.item_id })
    }

    $characterBuckets += $bucket
}

foreach ($bucket in $itemTypeBuckets.Values) {
    $bucket.items = @($bucket.items | Sort-Object { $_['item_id'] })
    foreach ($characterBucket in $bucket.character_buckets.Values) {
        $characterBucket.item_ids = @($characterBucket.item_ids | Sort-Object -Unique)
        $characterBucket.items = @($characterBucket.items | Sort-Object { $_['item_id'] })
    }
}

$itemTypesByName = [ordered]@{}
foreach ($entry in $itemTypeBuckets.GetEnumerator() | Sort-Object { $_.Value.item_type_name }, { $_.Key }) {
    $bucket = $entry.Value

    $characterBucketsForType = @()
    foreach ($characterBucket in $bucket.character_buckets.Values | Sort-Object { $_['character_id'] }, { $_['character_name'] }) {
        $characterBucketsForType += $characterBucket
    }

    $typeRecord = [ordered]@{
        item_type_name = $bucket.item_type_name
        item_type_name_kr = $bucket.item_type_name_kr
        inventory_bucket = $bucket.inventory_bucket
        inventory_target = $bucket.inventory_target
        is_equipment_type = $bucket.is_equipment_type
        item_count = $bucket.items.Count
        item_ids = @($bucket.items | ForEach-Object { $_.item_id })
        starter_item_ids = @($bucket.items | Where-Object { $_.is_starter_item } | ForEach-Object { $_.item_id })
        items = $bucket.items
    }

    if ($characterBucketsForType.Count -gt 0) {
        $typeRecord['characters'] = $characterBucketsForType
    }

    $itemTypesByName[$entry.Key] = $typeRecord
}

$outputById = [ordered]@{
    generated_at_utc = [DateTime]::UtcNow.ToString('o')
    source_files = [ordered]@{
        character_csv = $characterCsvPath
        item_csv = $itemCsvPath
        game_itemlist_csv = $gameItemListCsvPath
    }
    counts = [ordered]@{
        starter_item_count = $starterItemsById.Count
        equipment_item_count = $equipmentItemsById.Count
        character_bucket_count = $characterBuckets.Count
    }
    starter_item_ids = @($starterItemIds | Sort-Object -Unique)
    starter_items = @($starterItemsById.Values | Sort-Object { $_['item_id'] })
    equipment_by_character = $characterBuckets
    equipment_by_id = $equipmentItemsById
}

$outputByType = [ordered]@{
    generated_at_utc = [DateTime]::UtcNow.ToString('o')
    source_files = [ordered]@{
        character_csv = $characterCsvPath
        item_csv = $itemCsvPath
        game_itemlist_csv = $gameItemListCsvPath
    }
    counts = [ordered]@{
        starter_item_count = ($starterItemIds | Sort-Object -Unique).Count
        item_type_count = $itemTypesByName.Count
        total_item_count = $itemMetadataById.Count
    }
    inventory_seed_layout = [ordered]@{
        shared_item_stacks = [ordered]@{
            item_types = @($itemTypesByName.Values |
                Where-Object { $_.inventory_bucket -eq 'shared_item_stacks' } |
                ForEach-Object { $_.item_type_name } |
                Sort-Object -Unique)
        }
        owned_weapon_item_ids = @('Weapon')
        owned_clothes_item_ids = @('Main Clothes')
        owned_accessory_1_item_ids = @('Accessories (1)')
        owned_accessory_2_item_ids = @('Accessories (2)')
        owned_accessory_3_item_ids = @('Accessories (3)')
    }
    item_types = $itemTypesByName
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$jsonById = $outputById | ConvertTo-Json -Depth 12
[System.IO.File]::WriteAllText(
    $outputPathById,
    $jsonById,
    (New-Object System.Text.UTF8Encoding($false)))

$jsonByType = $outputByType | ConvertTo-Json -Depth 12
[System.IO.File]::WriteAllText(
    $outputPathByType,
    $jsonByType,
    (New-Object System.Text.UTF8Encoding($false)))

Write-Output "Wrote $outputPathById"
Write-Output "Wrote $outputPathByType"

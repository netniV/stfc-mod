$csvDir = "C:\Games\Star Trek Fleet Command\Star Trek Fleet Command\default\game\prime_Data"
$testCsv = Join-Path $csvDir "2026-04-06 13-00-00.csv"

$tab = "`t"

$lines = @(
    "Player Name${tab}Player Level${tab}Outcome${tab}Ship Name${tab}Ship Level${tab}Ship Strength${tab}Ship XP${tab}Officer One${tab}Officer Two${tab}Officer Three${tab}Hull Health${tab}Hull Health Remaining${tab}Shield Health${tab}Shield Health Remaining${tab}Location${tab}Timestamp",
    "SpotTheSpaceCat${tab}41${tab}WIN${tab}Defiant${tab}35${tab}5000000${tab}1234${tab}Spock${tab}Kirk${tab}Bones${tab}9000000${tab}4500000${tab}0${tab}0${tab}Romulan Space${tab}4/6/2026 1:00:00 PM",
    "",
    "Reward Name${tab}Count",
    "Parsteel${tab}50000",
    "Tritanium${tab}25000",
    "",
    "Fleet Type${tab}Attack${tab}Defense${tab}Hull Health${tab}Shield Health",
    "Player${tab}450000${tab}380000${tab}9000000${tab}0",
    "",
    "Round${tab}Battle Event${tab}Attacker${tab}Target${tab}Damage${tab}Mitigated",
    "1${tab}Hit${tab}SpotTheSpaceCat${tab}Exchange Bank${tab}1250000${tab}120000",
    "2${tab}Hit${tab}Exchange Bank${tab}SpotTheSpaceCat${tab}980000${tab}95000",
    "3${tab}Hit${tab}SpotTheSpaceCat${tab}Exchange Bank${tab}1320000${tab}130000"
)

[System.IO.File]::WriteAllLines($testCsv, $lines, [System.Text.Encoding]::UTF8)
Write-Host "Dropped test CSV: $testCsv"

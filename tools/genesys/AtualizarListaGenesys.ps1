$ErrorActionPreference = 'Stop'
$url = 'https://www.yugioh-card.com/en/genesys/'
$target = Join-Path (Get-Location) 'config\genesys_points.csv'
Write-Host 'Baixando a lista oficial Genesys da Konami...'
$response = Invoke-WebRequest -Uri $url -UseBasicParsing -Headers @{ 'User-Agent' = 'Mozilla/5.0 EDOPro-Genesys-List-Updater' }
$html = [string]$response.Content
if ([string]::IsNullOrWhiteSpace($html)) { throw 'A pagina oficial retornou conteudo vazio.' }

function Convert-HtmlText([string]$value) {
    $value = [regex]::Replace($value, '(?is)<br\s*/?>', ' ')
    $value = [regex]::Replace($value, '(?is)<[^>]+>', '')
    $value = [System.Net.WebUtility]::HtmlDecode($value)
    return ([regex]::Replace($value, '\s+', ' ')).Trim()
}

$rows = New-Object System.Collections.Generic.List[object]
$foundHeader = $false
foreach ($rowMatch in [regex]::Matches($html, '(?is)<tr\b[^>]*>(.*?)</tr>')) {
    $cells = New-Object System.Collections.Generic.List[string]
    foreach ($cellMatch in [regex]::Matches($rowMatch.Groups[1].Value, '(?is)<t[dh]\b[^>]*>(.*?)</t[dh]>')) {
        $cells.Add((Convert-HtmlText $cellMatch.Groups[1].Value))
    }
    if ($cells.Count -lt 2) { continue }
    if ($cells[0] -match '^Card Name$' -and $cells[1] -match '^Points$') {
        $foundHeader = $true
        continue
    }
    if ($foundHeader -and $cells[1] -match '^\d+$' -and -not [string]::IsNullOrWhiteSpace($cells[0])) {
        $rows.Add([pscustomobject]@{ card_name = $cells[0]; points = [int]$cells[1] })
    }
}
if (-not $foundHeader -or $rows.Count -lt 100) {
    throw "Nao consegui reconhecer a tabela oficial com seguranca (linhas encontradas: $($rows.Count)). O CSV existente foi preservado."
}

$temp = "$target.tmp"
$rows | Export-Csv -Path $temp -NoTypeInformation -Encoding UTF8
Move-Item -Force $temp $target
Write-Host "Concluido: $($rows.Count) cartas/pontuacoes salvas em config\genesys_points.csv"
Write-Host 'Fonte: https://www.yugioh-card.com/en/genesys/'

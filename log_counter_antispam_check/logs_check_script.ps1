$today = Get-Date -Format "yyyyMMdd"

$logFolder = "C:\Users\szymo\AppData\Roaming\MetaQuotes\Terminal\1DAFD9A7C67DC84FE37EAA1FC1E5CF75\logs\"
$fileName = "$today.log"
$filePath = Join-Path -Path $logFolder -ChildPath $fileName

if (Test-Path $filePath) {
    # Patterns specific to MT4 broker communication logs
    $pattern = 'order was opened|was modified|was deleted|pending order|close order|modify order|delete pending|failed|rejected|too frequent'

    # Using default encoding first; add -Encoding Unicode if results are 0
    $content = Get-Content -Path $filePath -ErrorAction SilentlyContinue
    
    if ($null -eq $content) {
        Write-Host "Error: File is empty or locked." -ForegroundColor Red
    } else {
        $foundMatches = $content | Select-String -Pattern $pattern -AllMatches
        $count = ($foundMatches.Matches | Measure-Object).Count
        
        Write-Host "`n--- MT4 STATISTICS FOR DATE $today ---" -ForegroundColor Yellow
        Write-Host "Total broker interactions: $count" -ForegroundColor Cyan
        
        Write-Host "`nBreakdown by type:" -ForegroundColor Gray
        $foundMatches.Matches | Group-Object Value | Select-Object Name, Count | Sort-Object Count -Descending | Format-Table -HideTableHeaders

        if ($count -gt 3000 -and $count -le 10000) {
            Write-Host "WARNING: High activity detected ($count)!" -ForegroundColor Red
        }
        elseif ($count -gt 10000) {
            Write-Host "CRITICAL: Activity ($count) exceeds safety limits!" -ForegroundColor DarkRed
        }
        else {
            Write-Host "Status: OK. Activity within safe limits." -ForegroundColor Green
        }
    }
} else {
    Write-Host "Error: Could not find log file: $filePath" -ForegroundColor Red
}

Write-Host ""
Read-Host -Prompt "Press Enter to quit"
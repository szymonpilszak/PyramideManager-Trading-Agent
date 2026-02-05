# 1. Get current date in YYYYMMDD format
$today = Get-Date -Format "yyyyMMdd"

# 2. Dynamic path to logs
$logFolder = "C:\Users\szymo\AppData\Roaming\MetaQuotes\Terminal\1DAFD9A7C67DC84FE37EAA1FC1E5CF75\logs\"
$fileName = "$today.log"
$filePath = Join-Path -Path $logFolder -ChildPath $fileName

if (Test-Path $filePath) {
    # Search for patterns that increase Hyperactive counter: modify, opened, order #, delete, failed
    $matches = Select-String -Path $filePath -Pattern 'modify|opened|order #|delete|failed' -AllMatches
    $count = $matches.Matches.Count
    
    Write-Host "--- MT4 STATISTICS FOR DATE $today ---" -ForegroundColor Yellow
    Write-Host "Server messages count: $count" -ForegroundColor Cyan
    
    # Warnings based on broker limits
    if ($count -gt 3000) {
        Write-Host "WARNING: You have exceeded the warning threshold (3000)!" -ForegroundColor Red
    }
    if ($count -gt 10000) {
        Write-Host "CRITICAL: Very close to account disable limit (10000)!" -ForegroundColor DarkRed
    }
} else {
    Write-Host "Error: Could not find log file for today: $filePath" -ForegroundColor Red
}

# Stop window
Read-Host -Prompt "Press Enter to quit"
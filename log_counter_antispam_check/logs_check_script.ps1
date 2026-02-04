# Replace 'XXXXXXXXXXXXXXXXXXXXXXX' with path to your MetaTrader4 lates logs file
# Example of path : 'C:\Users\YourUserName\AppData\Roaming\MetaQuotes\Terminal\1AFD9A7C67DC12FI17ECCAC1E55\logs'
$filePath = "XXXXXXXXXXXXXXXXXXXX"

if (Test-Path $filePath) {
    $count = (Select-String -Path $filePath -Pattern 'modified|opened|pending' -AllMatches).Matches.Count
    Write-Host "Logs count: $count" -ForegroundColor Cyan
} else {
    Write-Host "Error: Coulnd find file $filePath" -ForegroundColor Red
}

# Stop window
Read-Host -Prompt "Press Enter, to quit"
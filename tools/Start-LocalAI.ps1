param(
    [string]$Backend = "ollama",
    [string]$Model = "gemma3:4b",
    [string]$LlamaServerPath = ".\\third_party\\llama-bin\\llama-server.exe",
    [string]$ModelPath = ".\\models\\npc-brain.gguf",
    [int]$Port = 8080
)

if ($Backend -eq "ollama") {
    $ollama = Get-Command ollama -ErrorAction SilentlyContinue
    if (-not $ollama) {
        Write-Error "Ollama is not installed yet."
        exit 1
    }

    Start-Process -FilePath $ollama.Source -ArgumentList "serve" -WindowStyle Hidden
    Write-Host "Started Ollama local server. Pull model with: ollama pull $Model"
    exit 0
}

if (-not (Test-Path $LlamaServerPath)) {
    Write-Error "llama-server.exe not found at $LlamaServerPath"
    exit 1
}

if (-not (Test-Path $ModelPath)) {
    Write-Error "Model file not found at $ModelPath"
    exit 1
}

Start-Process -FilePath $LlamaServerPath -ArgumentList "-m `"$ModelPath`" --port $Port --ctx-size 4096" -WindowStyle Hidden
Write-Host "Started llama-server on port $Port"

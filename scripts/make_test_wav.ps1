Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(16000, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
$s.SetOutputToWaveFile("$PSScriptRoot\..\models\test_fr.wav", $fmt)
$frVoice = ($s.GetInstalledVoices() | Where-Object { $_.VoiceInfo.Culture.Name -like 'fr*' } | Select-Object -First 1)
if ($frVoice) { $s.SelectVoice($frVoice.VoiceInfo.Name) } else { Write-Host 'aucune voix fr' }
$s.Speak('Bonjour, ceci est un test de dictee vocale locale.')
$s.Dispose()
Write-Host 'wav ok'

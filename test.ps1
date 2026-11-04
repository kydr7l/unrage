# Round-trip test for unrage. It uses OPEN archives, so it needs no keys.
# Run it with: powershell -NoProfile -ExecutionPolicy Bypass -File test.ps1
$U = Join-Path $PSScriptRoot 'unrage.exe'
$T = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())

function Check($ok, $msg) { if (-not $ok) { throw $msg } }

function Run {
	$out = & $U @args
	Check ($LASTEXITCODE -eq 0) "unrage $args failed with exit code $LASTEXITCODE"
	$out
}

function Same($a, $b) {
	# Get-FileHash does not load when pwsh starts powershell.exe, so compare the bytes.
	$x = [Convert]::ToBase64String([IO.File]::ReadAllBytes((Resolve-Path $a).Path))
	$y = [Convert]::ToBase64String([IO.File]::ReadAllBytes((Resolve-Path $b).Path))
	Check ($x -eq $y) "$a and $b differ"
}

function SameTree($a, $b) {
	$ra = (Resolve-Path $a).Path
	$rb = (Resolve-Path $b).Path
	$fa = @(Get-ChildItem $ra -Recurse -File | ForEach-Object { $_.FullName.Substring($ra.Length) } | Sort-Object)
	$fb = @(Get-ChildItem $rb -Recurse -File | ForEach-Object { $_.FullName.Substring($rb.Length) } | Sort-Object)
	Check ($null -eq (Compare-Object $fa $fb)) "$a and $b have different files"
	foreach ($f in $fa) { Same "$ra$f" "$rb$f" }
}

New-Item -ItemType Directory $T | Out-Null
Push-Location $T
try {
	New-Item -ItemType Directory src\data\sub, src\inner.rpf\deep | Out-Null
	Set-Content src\a.txt hello
	Set-Content src\data\b.txt world
	$big = New-Object byte[] 100000
	(New-Object Random 1).NextBytes($big)
	[IO.File]::WriteAllBytes("$T\src\data\sub\big.bin", $big)
	Set-Content src\inner.rpf\deep\c.txt nested

	Run a test.rpf src\a.txt src\data src\inner.rpf | Out-Null
	Check ((Run l -r test.rpf) -match 'inner.rpf/deep/c.txt') "l -r does not show the nested file"
	Check ((Run t -r test.rpf) -match 'Everything is Ok') "t -r failed"

	# x with no paths and -r gives back the source tree.
	Run x -r -oout test.rpf | Out-Null
	SameTree src out

	# x and e with paths.
	Run x -oone test.rpf DATA/sub/big.bin | Out-Null
	Same src\data\sub\big.bin one\data\sub\big.bin
	Run e -r -oflat test.rpf data inner.rpf | Out-Null
	Same src\data\sub\big.bin flat\big.bin
	Same src\inner.rpf\deep\c.txt flat\c.txt

	# a on an existing archive replaces a file and keeps the others.
	Set-Content src\a.txt changed
	Run a test.rpf src\a.txt | Out-Null
	Run x -oout2 test.rpf a.txt data/b.txt | Out-Null
	Same src\a.txt out2\a.txt
	Same src\data\b.txt out2\data\b.txt

	# d removes an entry. A missing entry fails and changes nothing.
	Run d test.rpf data/sub | Out-Null
	Check (-not ((Run l test.rpf) -match 'big.bin')) "d did not remove data/sub"
	& $U d test.rpf nothere 2>$null | Out-Null
	Check ($LASTEXITCODE -ne 0) "d of a missing entry did not fail"
	Check ((Run l test.rpf) -match 'b.txt') "d of a missing entry changed the archive"
	Check (-not (Test-Path test.rpf.unrage-tmp)) "the temporary directory is still there"

	Write-Output "all tests passed"
} catch {
	Write-Output "FAIL: $_"
	exit 1
} finally {
	Pop-Location
	Remove-Item -Recurse -Force $T
}

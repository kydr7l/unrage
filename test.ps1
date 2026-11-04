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

# Run fix with the switches sw on a copy of test.rpf. The archive and its nested
# archive inner.rpf must get the encryption enc, and the files must not change.
function Fixed($name, $enc, [string[]]$sw) {
	Copy-Item test.rpf $name
	Run fix @sw $name | Out-Null
	Check ((Run l $name) -match "Encryption: $enc") "fix $sw $name did not write an $enc archive"
	Run x "-o$name.n" $name inner.rpf | Out-Null
	Check ((Run l "$name.n\inner.rpf") -match "Encryption: $enc") "fix $sw did not convert the nested archive"
	Run x -r "-o$name.ref" test.rpf | Out-Null
	Run x -r "-o$name.all" $name | Out-Null
	SameTree "$name.ref" "$name.all"
}

New-Item -ItemType Directory $T | Out-Null
Push-Location $T
try {
	# Isolate the key cache. GetTempPathA reads TMP first, then TEMP.
	$env:TMP = $env:TEMP = "$T\tmp"
	New-Item -ItemType Directory $env:TMP | Out-Null
	$C = "$T\tmp\unrage-keys"
	$none = "$T\none.exe"

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

	# With no GTA5.exe and no cache, an OPEN archive works and makes no cache.
	Remove-Item -Recurse -Force $C -ErrorAction SilentlyContinue
	$err = & $U l "-g$none" test.rpf 2>&1 | Where-Object { $_ -is [Management.Automation.ErrorRecord] }
	Check ($LASTEXITCODE -eq 0) "l of an OPEN archive with no keys failed"
	Check ($null -eq $err) "l of an OPEN archive with no keys wrote to stderr"
	Check (-not (Test-Path $C)) "the cache was made with no GTA5.exe"

	# Find GTA5.exe as unrage does. Skip the key tests if it is not installed.
	$gta = $null
	$reg = Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Rockstar Games\Grand Theft Auto V' -ErrorAction SilentlyContinue
	foreach ($v in 'InstallFolder', 'InstallFolderSteam', 'InstallFolderEpic') {
		if (-not $gta -and $reg.$v -and (Test-Path (Join-Path $reg.$v 'GTA5.exe'))) { $gta = $reg.$v }
	}
	if ($gta) {
		$ng = Join-Path $gta 'update\update.rpf'

		# The first NG list fetches the keys into the cache.
		$out = Run l "-g$(Join-Path $gta 'GTA5.exe')" $ng
		Check ($out[0] -eq "Archive: $ng") "the fetch changed stdout"
		Check ($out -match 'Encryption: NG') "l of the NG archive failed"
		foreach ($f in @{ 'gtav_aes_key.dat' = 32; 'gtav_ng_key.dat' = 27472;
		    'gtav_ng_decrypt_tables.dat' = 278528; 'gtav_hash_lut.dat' = 256 }.GetEnumerator()) {
			Check ((Get-Item "$C\$($f.Key)").Length -eq $f.Value) "$($f.Key) has the wrong size"
		}

		# The second list uses only the cache.
		Check ((Run l "-g$none" $ng) -match 'Encryption: NG') "the cache did not work"

		# A cache file with a wrong size makes a new fetch. The walk up finds GTA5.exe.
		[IO.File]::WriteAllBytes("$C\gtav_aes_key.dat", [byte[]](1, 2, 3, 4, 5))
		Run l $ng | Out-Null
		Check ((Get-Item "$C\gtav_aes_key.dat").Length -eq 32) "a bad cache was not fetched again"

		# The walk up from $T finds no GTA5.exe, so only the registry can find it.
		Remove-Item -Recurse -Force $C
		Run l test.rpf | Out-Null
		Check (Test-Path "$C\gtav_ng_key.dat") "the registry did not find GTA5.exe"

		# The derive of the encrypt tables takes some minutes, so it runs only on request.
		if ($env:UNRAGE_SLOW) {
			Run a -tng ng.rpf src\a.txt | Out-Null
			Check ((Run l ng.rpf) -match 'Encryption: NG') "a -tng did not write an NG archive"
			Run x -ong ng.rpf a.txt | Out-Null
			Same src\a.txt ng\a.txt
			Check ((Get-Item "$C\gtav_ng_encrypt_luts.dat").Length -eq 53477376) "the LUTs are not in the cache"

			# With no -t, fix writes NG.
			Fixed fixng.rpf NG
		}

		# A bad encrypt file in the cache makes one new fetch, not a fetch on each run.
		[IO.File]::WriteAllBytes("$C\gtav_ng_encrypt_luts.dat", [byte[]](1, 2, 3))
		Run l test.rpf | Out-Null
		$err = & $U l test.rpf 2>&1 | Where-Object { $_ -is [Management.Automation.ErrorRecord] }
		Check ($null -eq $err) "a bad encrypt file in the cache makes a fetch on each run"

		# A cache with a missing part makes a new fetch.
		Remove-Item "$C\gtav_hash_lut.dat"
		Check ((Run l $ng) -match 'Encryption: NG') "a cache with a missing part was not fetched again"

		# fix converts the nested archives too. AES needs only the AES key, so this test is fast.
		Fixed fix.rpf AES '-taes'

		# With no GTA5.exe and no cache, an NG archive fails and names GTA5.exe.
		Remove-Item -Recurse -Force $C
		$msg = & $U l "-g$none" $ng 2>&1 | Out-String
		Check ($LASTEXITCODE -eq 1) "l of an NG archive with no keys did not fail"
		Check ($msg -match 'GTA5\.exe' -and $msg -match '-g') "the key error does not name GTA5.exe and -g"
	} else {
		Write-Output "skip: GTA5.exe is not installed, so the key tests did not run"
	}

	Write-Output "all tests passed"
} catch {
	Write-Output "FAIL: $_"
	exit 1
} finally {
	Pop-Location
	Remove-Item -Recurse -Force $T
}

#!/bin/sh
# Round-trip test for unrage. It uses OPEN archives, so it needs no keys.
set -e
U="$PWD/unrage"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cd "$T"

mkdir -p src/data/sub src/inner.rpf/deep
echo hello > src/a.txt
echo world > src/data/b.txt
head -c 100000 /dev/urandom > src/data/sub/big.bin
echo nested > src/inner.rpf/deep/c.txt

"$U" a test.rpf src/a.txt src/data src/inner.rpf
"$U" l -r test.rpf | grep -q 'inner.rpf/deep/c.txt'
"$U" t -r test.rpf | grep -q 'Everything is Ok'

# x with no paths and -r gives back the source tree.
"$U" x -r -oout test.rpf
diff -r src out

# x and e with paths.
"$U" x -oone test.rpf DATA/sub/big.bin
cmp src/data/sub/big.bin one/data/sub/big.bin
"$U" e -r -oflat test.rpf data inner.rpf
cmp src/data/sub/big.bin flat/big.bin
cmp src/inner.rpf/deep/c.txt flat/c.txt

# a on an existing archive replaces a file and keeps the others.
echo changed > src/a.txt
"$U" a test.rpf src/a.txt
"$U" x -oout2 test.rpf a.txt data/b.txt
diff src/a.txt out2/a.txt
diff src/data/b.txt out2/data/b.txt

# d removes an entry. A missing entry fails and changes nothing.
"$U" d test.rpf data/sub
! "$U" l test.rpf | grep -q big.bin
! "$U" d test.rpf nothere 2>/dev/null
"$U" l test.rpf | grep -q b.txt
test ! -e test.rpf.unrage-tmp

echo "all tests passed"

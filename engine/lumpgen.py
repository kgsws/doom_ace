#!/usr/bin/python3
import sys
import subprocess
from struct import unpack_from
from struct import pack_into

# check arguments
if len(sys.argv) != 4:
	print("usage:", sys.argv[0], "code relocations output")
	exit(1)

# fancy version check
git_hash = subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode('ascii').strip()
git_hash = int(git_hash[:8], 16)
sp = subprocess.run(['git', 'diff', '--quiet'])
if sp.returncode != 0:
	# dirty!
	git_hash ^= 1

# load main binary
with open(sys.argv[1], "rb") as f:
	code = bytearray()
	code += f.read()

# padding
if len(code) % 32:
	code += b"\x00" * (32 - (len(code) % 32))

# load relocations
with open(sys.argv[2], "rb") as f:
	reloc = f.read()

# get .bss size
bss_size = unpack_from("<I", code, 0)[0]

# info
print("version %08X" % git_hash)
print("code size", len(code))
print(".bss size", bss_size)

# process relocations
rel_count = 0
while True:
	value = unpack_from("<II", reloc, 0)
	if value[0] == 0:
		break;
	if value[1] != 8:
		raise Exception("Unknown relocation type '%u'!" % value[1])
	code = code + reloc[0:4]
	rel_count += 1
	reloc = reloc[8:]
	if len(reloc) == 0:
		break

# relocation terminator
code = code + b"\x00\x00\x00\x00"

print("reloc count", rel_count)

# change .bss size
rel_count = rel_count * 4 + 4
if bss_size < rel_count:
	bss_size = 0
else:
	bss_size -= rel_count
pack_into("<I", code, 0, bss_size)

# change version
pack_into("<I", code, 4, git_hash)

# save new file
with open(sys.argv[3], "wb") as f:
	f.write(code)

print("lump size", len(code))


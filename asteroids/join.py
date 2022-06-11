#!/usr/bin/python3
import sys
import struct

if len(sys.argv) < 2:
	print("Missing argument!")
	sys.exit(1)

with open("savebase.bin", "rb") as f:
	savebase = bytearray(f.read())
	print("save base:", len(savebase))

with open("loader.bin", "rb") as f:
	loader = f.read()
	print("loader:", len(loader))

with open("game.bin", "rb") as f:
	game = f.read()
	print("game:", len(game))

with open("image.jpg", "rb") as f:
	jpgtest = f.read(2)
	image = f.read()
	print("image:", len(image))

# process data
if jpgtest != b"\xFF\xD8" or image[:2] != b"\xFF\xE0":
	print("Invalid source JPG!")
	sys.exit(1)

struct.pack_into(">HHH", savebase, 0, 0xFFD8, 0xFFFE, len(savebase) + len(loader) + len(game))

# write file
with open(sys.argv[1], "wb") as f:
	f.write(savebase)
	f.write(loader)
	f.write(struct.pack("<I", len(game)))
	f.write(game)
	f.write(image)


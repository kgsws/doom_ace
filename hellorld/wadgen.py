#!/usr/bin/python3
import sys
from collections import OrderedDict
from struct import pack
from struct import unpack_from
from struct import pack_into

# special lump
data_pnames = b"\x01\x00\x00\x00" + b"HELLORLD"

# primitive WAD file stuff
class kgWadFile:
	lump = OrderedDict()
	verbose = True

	def lumpFromBuff(self, lumpname, data):
		size = len(data)
		lmp = {}
		lmp["size"] = size
		padsize = size + 3
		padsize &= ~3
		lmp["padsize"] = padsize
		lmp["data"] = bytes(data) + b"\x00" * (padsize - size)
		self.lump[lumpname] = lmp
		if self.verbose:
			print("added", lumpname, ",", size, "B")

	def lumpFromFile(self, lumpname, filename):
		with open(filename, "rb") as f:
			contents = f.read()
		self.lumpFromBuff(lumpname, contents)

	def saveFile(self, filename):
		if self.verbose:
			print("saving", len(self.lump), "entries to", filename)
		offset = 12
		for key in self.lump:
			offset += self.lump[key]["padsize"]
		with open(filename, "wb") as f:
			# header
			data = pack("<III", 0x44415750, len(self.lump), offset)
			f.write(data)
			# data
			for key in self.lump:
				f.write(self.lump[key]["data"])
			# directory
			offset = 12
			for key in self.lump:
				data = pack("<II", offset, self.lump[key]["size"])
				f.write(data)
				data = bytearray(8)
				pack_into("8s", data, 0, key.encode('utf-8'))
				f.write(data)
				offset += self.lump[key]["padsize"]

# convert binary to special Doom 'patch'
def convert_code(filename):
	# load the file
	with open(filename, "rb") as f:
		code = bytearray()
		code += f.read()

	print("code size", len(code), "B")

	if len(code) > 256:
		raise Exception("Loader 0 size is too big!")

	# generate output
	ret = bytearray(688)
	index = 0

	# header
	pack_into("<Q", ret, 0, 0x80)
	index += 8

	# contents
	for i in range(0,len(code)//2):
		value, = unpack_from("<H", code, i * 2)
		value += 0x10000
		value -= 3
		value &= 0xFFFF
		pack_into("<I", ret, index, value)
		index += 4

	return ret

# check arguments
if len(sys.argv) != 3:
	print("usage:", sys.argv[0], "input.bin output.wad")
	exit(1)

hellorld = convert_code(sys.argv[1])

# create WAD file
wadfile = kgWadFile()
wadfile.lumpFromBuff("HELLORLD", hellorld)
wadfile.lumpFromBuff("PNAMES", data_pnames)
wadfile.lumpFromFile("TEXTURE1", "texture1.lmp")

# done
wadfile.saveFile(sys.argv[2])


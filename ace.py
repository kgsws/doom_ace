#!/usr/bin/python3
import struct

with open("exploit/exploit.bin", "rb") as f:
	bin_e = f.read()
	len_e = len(bin_e)

with open("code/code.bin", "rb") as f:
	bin_c = f.read() + b'\0\0'
	len_c = int(len(bin_c) / 3)
	size = struct.pack("<H", len_c) + b'\0'
	bin_c = size + bin_c[:len_c * 3]
	len_c += 1

if len_c > 65535:
	print("Code is too large!")
	exit(1)

output = bytearray(len_e + len_c * 4)
output[0:len_e] = bin_e

print("Exploit length:", len(bin_e))
print("Code length:", (len_c - 1) * 3)
print("ACE length:", len(output))

for i in range(len_c):
	temp = bin_c[i * 3:i * 3 + 3]
	output[len_e + i * 4 + 0] = temp[0] | 0x80
	output[len_e + i * 4 + 1] = temp[1] | 0x80
	output[len_e + i * 4 + 2] = temp[2] | 0x80
	output[len_e + i * 4 + 3] = 0x80 | ((temp[0] & 0x80) >> 7) | ((temp[1] & 0x80) >> 6) | ((temp[2] & 0x80) >> 5)

with open("ACE", "wb") as f:
	f.write(output)


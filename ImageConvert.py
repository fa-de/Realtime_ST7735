#Usage: ImageConvert.py myimage.raw
# where myimage.raw is a 8x8 (by default) 32 bit interleaved RAW image
# The output can be directly copied into your sprite array

import sys

def color565(r,g,b):
	lsb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
	msb = (lsb >> 8) | (lsb << 8);
	mask = ~(~0 << 16)
	return msb & mask

with open(sys.argv[1], 'rb') as f:
	data = bytearray(f.read())
	i = 0
	assert len(data) == 8 * 8 * 3
	
	print("{"),

	for i in range(0,len(data),3):
		r = data[i]
		g = data[i+1]
		b = data[i+2]
		v = color565(r,g,b)

		if i != 0 : print(", "),
		print("{0:04X}".format(v)),

	print("}")

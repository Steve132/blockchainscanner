import pybitcointools as bc
import sys

curaddresses=set()

for l in sys.stdin:
	if(len(l) > 44):
		addr=bc.pubtoaddr(l.strip())
	else:
		addr=bc.hex_to_b58check(l.strip())
	#if(addr not in curaddresses):
	#	curaddresses.add(addr)
	print(addr)


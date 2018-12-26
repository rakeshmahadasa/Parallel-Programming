import os
e,t,binary=[x for x in raw_input().split()]
e=int(e)
t=int(t)
#binary=raw_input()
for tr in range(t,0,-1):
	print("Running for "+str(tr)+" threads")
	for i in range(0,3):
		os.system("./" +binary+" "+str(e)+" "+str(tr))


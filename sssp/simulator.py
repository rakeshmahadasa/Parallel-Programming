import os
binary = raw_input()
vertexCountList = [1000000,1500000,2000000,2100000]
threadCountList = [1,2,3,4,5,6,7,8,9,10]
for i in vertexCountList:
    for j in threadCountList:
        for k in range(0,3):
            command = "./" + binary + " " + str(i) + " " + str(j) + " >> ./output/" + binary + ".out"
            os.system(command)

#!/bin/bash

OUT="Results/strong_scale.csv"
echo "n,p,time" > "$OUT"

P=(1 2 4 8)

for p in "${P[@]}"; do
	result=$(mpirun --bind-to none -np "$p" ./stencil "Data/input1000000.txt" "output.txt" 100)
	echo "$result" >> "$OUT"
done

OUT="Results/weak_scale.csv"
echo "n,p,time" > "$OUT"

N=("Data/input1000000.txt" "Data/input2000000.txt" "Data/input4000000.txt" "Data/input8000000.txt")
P=(1 2 4 8)

for i in "${!N[@]}"; do
	result=$(mpirun --bind-to none -n "${P[i]}" ./stencil "${N[i]}" "output.txt" 100)
	echo "$result" >> "$OUT"
done

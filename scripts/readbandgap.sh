#!/bin/bash
# Modified VASP bandgap calculation script

if [ -z $1 ] ; then
    outcar="OUTCAR"
    echo 'Default OUTCAR'
else
    outcar=$1
fi

# Detect if the calculation is spin-polarized
ispin=$(awk '/ISPIN/ {print $3}' $outcar)
ispin=$(echo $ispin | awk '{print $1}')

homo=$(awk '/NELECT/ {print int($3/2)}' $outcar)

lumo=$(awk '/NELECT/ {print int($3/2+1)}' $outcar)
nkpt=$(awk '/NKPTS/ {print $4}' $outcar)

if [ "$ispin" -eq "2" ]; then
    # For spin-polarized calculations, separate spin-up and spin-down channels
    e1_up=$(grep -a "     $homo     " $outcar | head -$nkpt | sort -n -k 2 | tail -1 | awk '{print $2}')
    e2_up=$(grep -a "     $lumo     " $outcar | head -$nkpt | sort -n -k 2 | head -1 | awk '{print $2}')
    
    e1_down=$(grep -a "     $homo     " $outcar | head -$nkpt | sort -n -k 2 | tail -1 | awk '{print $3}')
    e2_down=$(grep -a "     $lumo     " $outcar | head -$nkpt | sort -n -k 2 | head -1 | awk '{print $3}')
    
    e1=$(echo "$e1_up $e1_down" | awk '{print ($1 > $2) ? $1 : $2}')
    e2=$(echo "$e2_up $e2_down" | awk '{print ($1 < $2) ? $1 : $2}')
else
    e1=$(grep -a "     $homo     " $outcar | head -$nkpt | sort -n -k 2 | tail -1 | awk '{print $2}')
    e2=$(grep -a "     $lumo     " $outcar | head -$nkpt | sort -n -k 2 | head -1 | awk '{print $2}')
fi

# echo "HOMO: band:" $homo " E=" $e1
# echo "LUMO: band:" $lumo " E=" $e2
bandgap=$(echo "scale=4;${e2}-${e1}" | bc)
# echo "Bandgap: E=${bandgap} eV"
echo "${bandgap}"


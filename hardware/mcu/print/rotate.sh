#!/usr/bin/env bash

#These are on the pick and place machine rotated by by some amount, fix here

sed -i '/"J1",/ s/,180.000000,top/,0.000000,top/;' "$1" #USB-C
sed -i '/"J4",/ s/,0.000000,top/,180.000000,top/;' "$1" #JST-PH

sed -i '/"Q2",/ s/,180.000000,top/,0.000000,top/;' "$1"
sed -i '/"Q3",/ s/,0.000000,top/,180.000000,top/;' "$1"
sed -i '/"Q4",/ s/,180.000000,top/,0.000000,top/;' "$1"
sed -i '/"Q5",/ s/,0.000000,top/,180.000000,top/;' "$1"
sed -i '/"SW2",/ s/,0.000000,top/,180.000000,top/;' "$1"

sed -i '/"U1",/ s/,315.000000,top/,225.000000,top/;' "$1" #ESP
sed -i '/"U2",/ s/,270.000000,top/,180.000000,top/;' "$1" #CP210x
sed -i '/"U3",/ s/,90.000000,top/,270.000000,top/;' "$1" #USB ESD
sed -i '/"U4",/ s/,0.000000,top/,180.000000,top/;' "$1" #TP4065
sed -i '/"U7",/ s/,180.000000,top/,90.000000,top/;' "$1" #HX711
sed -i '/"U8",/ s/,90.000000,top/,0.000000,top/;' "$1" #TM1638

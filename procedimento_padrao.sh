fusermount -u temp
make clean
make
./mount_fat16 temp/
rm *.pdf
rm *.jpg

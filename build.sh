rm ./compress2
gcc -o compress2 -O3 -m64 -mavx512f -march=skylake-avx512 compress2.c

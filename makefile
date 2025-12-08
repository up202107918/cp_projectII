run-ecosystem:
	gcc -fopenmp -O3 -o ecosystem ecosystem.c && ./ecosystem (1,2,4,8,16) < ecosystem_examples/input(5x5, 10x10, 20x20, 
	100x100, 100x100_unbal(01,02), input200x200)

run-ecosystem_seq:
	gcc -O3 -o ecosystem ecosystem_seq.c && ./ecosystem < ecosystem_examples/input(5x5, 10x10, 20x20, 
	100x100, 100x100_unbal(01,02), input200x200)

Caution: If you cannot run the first gcc command above (gcc -fopenmp ... ecosystem.c), try running the following command instead: 
	gcc -Xclang -fopenmp -L/opt/homebrew/opt/libomp/lib -I/opt/homebrew/opt/libomp/include -lomp -O3 -o ecosystem ecosystem(_seq).c



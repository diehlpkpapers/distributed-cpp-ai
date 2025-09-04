all:
	g++ -std=c++20 -O3 -pthread coroutines.cpp -o coroutines
	g++ -std=c++17 -O3 -ltbb -pthread parallel_algorithm.cpp -o parallel_algorithm
	g++ -std=c++17 -O3 -pthread async.cpp -o async
	g++ -std=c++11 -O3 -fopenmp openmp.cpp -o openmp


# Coroutines

```bash
g++ -std=c++20 -O3 -pthread coroutines.cpp -o coroutines
coroutines.cpp: In function ‘Task generateMandelbrot(Config, std::vector<unsigned char>&, int, int)’:
coroutines.cpp:59:5: error: no member named ‘return_void’ in ‘std::__n4861::__coroutine_traits_impl<Task, void>::promise_type’ {aka ‘Task::promise_type’}
   59 |     co_return;
      |     ^~~~~~~~~
coroutines.cpp: In lambda function:
coroutines.cpp:103:13: error: no member named ‘await_ready’ in ‘Task’
  103 |             co_await generateMandelbrot(config, pixels, startRow, endRow);
      |             ^~~~~~~~
make: *** [Makefile:2: all] Error 1
```

# Asynchronous programming

```bash
 g++ -std=c++17 -O3 -ltbb -pthread parallel_algorithm.cpp -o parallel_algorithm
parallel_algorithm.cpp: In function ‘std::vector<Pixel> generateMandelbrot(int, int, int, int)’:
parallel_algorithm.cpp:21:69: error: too many initializers for ‘__pstl::execution::v1::parallel_policy’
   21 |     std::execution::parallel_policy policy{std::execution::par_unseq};
      |                                                                     ^
parallel_algorithm.cpp:22:12: error: ‘__pstl::execution::v1::parallel_policy::execution_policy_tag’ has not been declared
   22 |     policy.execution_policy_tag::concurrency_hint = numThreads;
      |            ^~~~~~~~~~~~~~~~~~~~
parallel_algorithm.cpp: In lambda function:
parallel_algorithm.cpp:26:40: error: no matching function for call to ‘distance(std::vector<Pixel>::iterator, Pixel*)’
   26 |         int x = pixel.x = std::distance(std::begin(pixels), &pixel) % width;
      |                           ~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~
In file included from /usr/include/c++/11/bits/stl_algobase.h:66,
                 from /usr/include/c++/11/bits/char_traits.h:39,
                 from /usr/include/c++/11/ios:40,
                 from /usr/include/c++/11/ostream:38,
                 from /usr/include/c++/11/iostream:39,
                 from parallel_algorithm.cpp:1:
/usr/include/c++/11/bits/stl_iterator_base_funcs.h:138:5: note: candidate: ‘template<class _InputIterator> constexpr typename std::iterator_traits< <template-parameter-1-1> >::difference_type std::distance(_InputIterator, _InputIterator)’
  138 |     distance(_InputIterator __first, _InputIterator __last)
      |     ^~~~~~~~
/usr/include/c++/11/bits/stl_iterator_base_funcs.h:138:5: note:   template argument deduction/substitution failed:
parallel_algorithm.cpp:26:40: note:   deduced conflicting types for parameter ‘_InputIterator’ (‘__gnu_cxx::__normal_iterator<Pixel*, std::vector<Pixel> >’ and ‘Pixel*’)
   26 |         int x = pixel.x = std::distance(std::begin(pixels), &pixel) % width;
      |                           ~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~
parallel_algorithm.cpp:27:40: error: no matching function for call to ‘distance(std::vector<Pixel>::iterator, Pixel*)’
   27 |         int y = pixel.y = std::distance(std::begin(pixels), &pixel) / width;
      |                           ~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~
In file included from /usr/include/c++/11/bits/stl_algobase.h:66,
                 from /usr/include/c++/11/bits/char_traits.h:39,
                 from /usr/include/c++/11/ios:40,
                 from /usr/include/c++/11/ostream:38,
                 from /usr/include/c++/11/iostream:39,
                 from parallel_algorithm.cpp:1:
/usr/include/c++/11/bits/stl_iterator_base_funcs.h:138:5: note: candidate: ‘template<class _InputIterator> constexpr typename std::iterator_traits< <template-parameter-1-1> >::difference_type std::distance(_InputIterator, _InputIterator)’
  138 |     distance(_InputIterator __first, _InputIterator __last)
      |     ^~~~~~~~
/usr/include/c++/11/bits/stl_iterator_base_funcs.h:138:5: note:   template argument deduction/substitution failed:
parallel_algorithm.cpp:27:40: note:   deduced conflicting types for parameter ‘_InputIterator’ (‘__gnu_cxx::__normal_iterator<Pixel*, std::vector<Pixel> >’ and ‘Pixel*’)
   27 |         int y = pixel.y = std::distance(std::begin(pixels), &pixel) / width;
      |                           ~~~
```

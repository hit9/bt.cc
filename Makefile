defalut: build

install-release:
	conan install . --output-folder=build --build=missing -s compiler.cppstd=20 -s build_type=Release

install-debug:
	conan install . --output-folder=build --build=missing -s compiler.cppstd=20 -s build_type=Debug

cmake-build:
	cd build && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1

cmake-build-test:
	cd build && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
		-DBT_TEST=1

cmake-build-test-release-mode:
	cd build && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
		-DBT_TEST=1

build: cmake-build
	cd build && make

build-example:
	cd build/example && make

build-example-onsignal:
	cd build/example/onsignal && make

build-test:
	cd build/tests && make bt_tests

build-benchmark:
	cd build/tests && make bt_benchmark

run-example: build-example
	./build/example/bt_example

run-example-onsignal: build-example-onsignal
	./build/example/onsignal/bt_example_onsignal

run-tests: build-test
	ctest --test-dir ./build/tests --output-on-failure

run-benchmark: build-benchmark
	./build/tests/bt_benchmark

clean:
	make -C build clean

.PHONY: build build-test

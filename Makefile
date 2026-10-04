CXX = g++
MPICC = mpicc
EIGEN_INC ?= $(mkEigenInc)
LIS_INC ?= $(mkLisInc)
LIS_LIB ?= $(mkLisLib)
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic
CPPFLAGS += -I$(EIGEN_INC) -isystem third_party

.PHONY: all test clean
all: build/challenge1 build/test_challenge1 build/lis_test1

build:
	mkdir -p build

build/challenge1: challenge1.cpp image_filters.hpp third_party/stb_image.h third_party/stb_image_write.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

build/test_challenge1: test_challenge1.cpp image_filters.hpp third_party/stb_image.h third_party/stb_image_write.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

build/lis_test1: third_party/lis_test1.c | build
	$(MPICC) -O2 -DUSE_MPI -fopenmp -I$(LIS_INC) $< -L$(LIS_LIB) -Wl,-rpath,$(LIS_LIB) -llis -lm -o $@

test: build/test_challenge1
	./build/test_challenge1 build/test_scratch
	python3 -m unittest -v test_reports

clean:
	rm -rf build

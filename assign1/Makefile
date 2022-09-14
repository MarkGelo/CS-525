all: test

test: test_assign1_1.c storage_mgr.c dberror.c
	gcc -o test_assign1 test_assign1_1.c storage_mgr.c dberror.c

clean:
	rm -f test_assign1 test_pagefile.bin
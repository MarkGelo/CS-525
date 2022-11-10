all: clean test_expr test_assign3_1 test_assign3_1_V2 run_test

test_expr: test_expr.c buffer_mgr_stat.c buffer_mgr.c dberror.c expr.c record_mgr.c rm_serializer.c storage_mgr.c
	gcc -o test_expr test_expr.c buffer_mgr_stat.c buffer_mgr.c dberror.c expr.c record_mgr.c rm_serializer.c storage_mgr.c

test_assign3_1: test_assign3_1.c buffer_mgr_stat.c buffer_mgr.c dberror.c expr.c record_mgr.c rm_serializer.c storage_mgr.c
	gcc -o test_assign3_1 test_assign3_1.c buffer_mgr_stat.c buffer_mgr.c dberror.c expr.c record_mgr.c rm_serializer.c storage_mgr.c

test_assign3_1_V2: test_assign3_1_V2.c buffer_mgr_stat.c buffer_mgr.c dberror.c expr.c record_mgr.c rm_serializer.c storage_mgr.c
	gcc -o test_assign3_1_V2 test_assign3_1_V2.c buffer_mgr_stat.c buffer_mgr.c dberror.c expr.c record_mgr.c rm_serializer.c storage_mgr.c

clean:
	rm -f test_assign3_1 test_assign3_1_V2 test_expr *.bin *.o *.out test_table*

run_test: test_assign3_1_V2
	./test_assign3_1_V2
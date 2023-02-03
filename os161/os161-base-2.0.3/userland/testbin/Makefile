#
# Makefile for src/testbin (sources for programs installed in /testbin)
#

TOP=../..
.include "$(TOP)/mk/os161.config.mk"

SUBDIRS=add argtest badcall bigexec bigfile bigfork bigseek bloat conman \
	crash ctest dirconc dirseek dirtest f_test factorial farm faulter \
	filetest forkbomb forktest frack hash hog huge \
	malloctest matmult multiexec palin parallelvm poisondisk psort \
	randcall redirect rmdirtest rmtest \
	sbrktest schedpong sort sparsefile tail tictac triplehuge \
	triplemat triplesort usemtest zero \
	helloworld argument_test rw_filetest lseek_test dup2_test \
	dir_test proc_test execv_test test_program fork_test multifork_test

# But not:
#    userthreads    (no support in kernel API in base system)

.include "$(TOP)/mk/os161.subdir.mk"

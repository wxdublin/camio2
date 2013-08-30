#! /bin/bash

#use --variant=release to build a release version

#There are lots of options passed to the compiler here, so I'll disect a couple
#-Ideps means we can use library notation for everything in the deps directory which cleans up the code a head
#-D_XOPEN_SOURCE=700 tells us we are using the 2008 POSIX standard which a few functions need
#-D_BSD_SOURCE tells us we can use some BSD specific functions
#-Werror -Wall -Wextra -pedantic  treats all warnings as errors and is very picky about them
#-Wno-missing-field-initializers in my opinion this is more often than not a spurious error.
#  see http://gcc.gnu.org/ml/gcc-bugs/1998-07/msg00031.html
#      http://gcc.gnu.org/ml/gcc-bugs/1998-07/msg00059.html
#      http://gcc.gnu.org/ml/gcc-bugs/1998-07/msg00128.html
#-std=c11 We use anonymous unions and alligned_alloc

buil/cake/cake src/camio2.c --append-CFLAGS="-Ideps -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -std=c11 -Werror -Wall -Wextra -pedantic -Wno-missing-field-initializers" $@ --begintests tests/*.c --endtests

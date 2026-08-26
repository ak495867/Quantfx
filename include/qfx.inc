%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_OPEN 2
%define SYS_CLOSE 3
%define SYS_LSEEK 8
%define SYS_EXIT 60
%define SYS_FSTAT 5
%define SYS_MMAP 9
%define SYS_MUNMAP 11
%define SYS_SOCKET 41
%define SYS_CONNECT 42
%define SYS_ACCEPT 43
%define SYS_BIND 49
%define SYS_LISTEN 50
%define SYS_GETPID 39
%define SYS_CLOCK_GETTIME 228
%define SYS_UNLINK 87
%define SYS_FSYNC 74

%define STDIN 0
%define STDOUT 1
%define STDERR 2

%define O_RDONLY 0
%define O_WRONLY 1
%define O_CREAT 64
%define O_TRUNC 512
%define O_APPEND 1024
%define PROT_READ 1
%define PROT_WRITE 2
%define MAP_PRIVATE 2
%define MAP_ANONYMOUS 32
%define CLOCK_MONOTONIC 1

%define MAX_SYMBOLS 64
%define MAX_EQUITY 4096
%define MAX_LINE 4096
%define MAX_EVENTS 10000000

%define EV_TS 0
%define EV_SYMBOL 8
%define EV_BID 16
%define EV_ASK 24
%define EV_LAST 32
%define EV_VOLUME 40
%define EV_BID_SIZE 48
%define EV_ASK_SIZE 56
%define EV_SEQ 64
%define EV_FLAGS 72
%define EVENT_SIZE 80

%define ORD_SIDE 0
%define ORD_QTY 8
%define ORD_PRICE 16
%define ORD_STOP 24
%define ORD_TAKE 32
%define ORD_SYMBOL 40
%define ORD_ID 48
%define ORDER_SIZE 56

%define QFX_MAGIC 0x31584651
%define QFX_VERSION 1
%define QFX_HEADER_SIZE 64
%define QFX_BLOCK_SIZE 65536

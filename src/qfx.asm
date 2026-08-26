%include "qfx.inc"

global _start

global qfx_parse_num

global qfx_parse_line

global qfx_process_event

global qfx_risk_check

global qfx_write_qfx_header

global qfx_write_qfx_event

global qfx_print_u64

global qfx_print_cstr

global qfx_open_input

global qfx_close_input

global qfx_mmap_input

global qfx_munmap_input

section .data
usage db "qfx validate <file.csv>",10,"qfx ingest <file.csv> <out.qfx>",10,"qfx backtest <file.csv>",10,"qfx replay <file.csv>",10,"qfx live <market-adapter> <broker-adapter>",10,0
invalid_command db "invalid command",10,0
open_error db "input open failed",10,0
map_error db "input map failed",10,0
parse_error db "parse failure",10,0
write_error db "write failure",10,0
validation_label db "valid_events=",0
invalid_label db "invalid_lines=",0
events_label db "events=",0
trades_label db "trades=",0
fills_label db "fills=",0
wins_label db "wins=",0
losses_label db "losses=",0
net_label db "net_pnl=",0
peak_label db "peak_equity=",0
drawdown_label db "max_drawdown=",0
position_label db "position=",0
cash_label db "cash=",0
route_broker_label db "broker=",0
route_score_label db "score=",0
route_price_label db "price=",0
route_none db "no_eligible_venue",10,0
newline db 10,0
csv_header db "timestamp_ns,symbol,bid,ask,last,volume,bid_size,ask_size,sequence,flags",10,0
qfx_magic dq QFX_MAGIC
qfx_version dq QFX_VERSION
qfx_scale dq 100000

section .bss
input_fd resq 1
output_fd resq 1
journal_fd resq 1
input_addr resq 1
input_len resq 1
input_end resq 1
iter_ptr resq 1
current_event resb EVENT_SIZE
statbuf resb 256
num_buf resb 32
write_buf resb 512
line_buf resb MAX_LINE
prev_price resq 1
has_prev resq 1
position resq 1
cash resq 1
equity resq 1
peak_equity resq 1
max_drawdown resq 1
trades resq 1
fills resq 1
wins resq 1
losses resq 1
net_pnl resq 1
last_trade_pnl resq 1
valid_events resq 1
invalid_lines resq 1
sequence_counter resq 1
risk_max_position resq 1
risk_max_order resq 1
risk_max_spread resq 1
risk_max_drawdown resq 1
risk_min_cash resq 1
risk_threshold resq 1
live_mode resq 1
record_count resq 1
num_len resq 1
input_format resq 1
stream_mode resq 1
order_counter resq 1
route_side resq 1
route_quantity resq 1
route_best_score resq 1
route_best_start resq 1
route_best_end resq 1
route_bid resq 1
route_ask resq 1
route_latency resq 1
route_slippage resq 1
route_fee resq 1
route_reject resq 1
route_age resq 1
route_capacity resq 1
route_broker_start resq 1
route_broker_len resq 1

section .text

_start:
    mov r12,[rsp]
    lea r13,[rsp+8]
    cmp r12,2
    jb .usage
    mov rdi,[r13+8]
    lea rsi,[rel cmd_validate]
    call qfx_streq
    test eax,eax
    jnz .validate
    mov rdi,[r13+8]
    lea rsi,[rel cmd_ingest]
    call qfx_streq
    test eax,eax
    jnz .ingest
    mov rdi,[r13+8]
    lea rsi,[rel cmd_backtest]
    call qfx_streq
    test eax,eax
    jnz .backtest
    mov rdi,[r13+8]
    lea rsi,[rel cmd_replay]
    call qfx_streq
    test eax,eax
    jnz .replay
    mov rdi,[r13+8]
    lea rsi,[rel cmd_live]
    call qfx_streq
    test eax,eax
    jnz .live
    mov rdi,[r13+8]
    lea rsi,[rel cmd_route]
    call qfx_streq
    test eax,eax
    jnz .route
    lea rdi,[rel invalid_command]
    call qfx_print_cstr
    jmp .exit_fail

.validate:
    cmp r12,3
    jb .usage
    mov rdi,[r13+16]
    call qfx_open_input
    test eax,eax
    js .open_fail
    call qfx_mmap_input
    test rax,rax
    js .map_fail
    call qfx_reset_state
    call qfx_scan_input
    lea rdi,[rel validation_label]
    call qfx_print_cstr
    mov rdi,[valid_events]
    call qfx_print_u64
    lea rdi,[rel invalid_label]
    call qfx_print_cstr
    mov rdi,[invalid_lines]
    call qfx_print_u64
    call qfx_munmap_input
    call qfx_close_input
    jmp .exit_ok

.ingest:
    cmp r12,4
    jb .usage
    mov rdi,[r13+16]
    call qfx_open_input
    test eax,eax
    js .open_fail
    call qfx_mmap_input
    test rax,rax
    js .map_fail
    mov rdi,[r13+24]
    call qfx_open_output
    test eax,eax
    js .write_fail
    call qfx_write_qfx_header
    call qfx_scan_and_write
    call qfx_finalize_qfx
    call qfx_close_output
    call qfx_munmap_input
    call qfx_close_input
    jmp .exit_ok

.backtest:
    cmp r12,3
    jb .usage
    mov rdi,[r13+16]
    call qfx_open_input
    test eax,eax
    js .open_fail
    call qfx_mmap_input
    test rax,rax
    js .map_fail
    call qfx_reset_state
    mov qword [risk_max_position],1000000
    mov qword [risk_max_order],100000
    mov qword [risk_max_spread],500
    mov qword [risk_max_drawdown],20000000
    mov qword [risk_min_cash],1000
    mov qword [risk_threshold],10
    call qfx_scan_and_backtest
    call qfx_print_report
    call qfx_munmap_input
    call qfx_close_input
    jmp .exit_ok

.replay:
    cmp r12,3
    jb .usage
    mov rdi,[r13+16]
    call qfx_open_input
    test eax,eax
    js .open_fail
    call qfx_mmap_input
    test rax,rax
    js .map_fail
    call qfx_reset_state
    mov qword [risk_max_position],1000000
    mov qword [risk_max_order],100000
    mov qword [risk_max_spread],500
    mov qword [risk_max_drawdown],20000000
    mov qword [risk_min_cash],1000
    mov qword [risk_threshold],10
    call qfx_scan_and_backtest
    call qfx_print_report
    call qfx_munmap_input
    call qfx_close_input
    jmp .exit_ok

.route:
    cmp r12,4
    jb .usage
    mov rdi,[r13+16]
    call qfx_open_input
    test eax,eax
    js .open_fail
    call qfx_mmap_input
    test rax,rax
    js .map_fail
    mov qword [route_side],1
    mov rdi,[r13+24]
    lea rsi,[rel cmd_sell]
    call qfx_streq
    test eax,eax
    jz .route_buy
    mov qword [route_side],-1
.route_buy:
    mov qword [route_quantity],1000
    mov qword [route_best_score],-1
    call qfx_scan_route
    call qfx_print_route
    call qfx_munmap_input
    call qfx_close_input
    jmp .exit_ok

.live:
    cmp r12,4
    jb .usage
    lea rdi,[rel live_banner]
    call qfx_print_cstr
    mov qword [live_mode],1
    mov rdi,[r13+16]
    call qfx_open_input
    test eax,eax
    js .open_fail
    cmp qword [input_fd],STDIN
    jne .live_file
    mov qword [stream_mode],1
    jmp .live_broker
.live_file:
    call qfx_mmap_input
    test rax,rax
    js .map_fail
.live_broker:
    mov rdi,[r13+24]
    call qfx_open_live_broker
    test eax,eax
    js .broker_fail
    call qfx_open_journal
    test eax,eax
    js .journal_fail
    call qfx_reset_state
    call qfx_load_checkpoint
    mov qword [risk_max_position],1000000
    mov qword [risk_max_order],100000
    mov qword [risk_max_spread],500
    mov qword [risk_max_drawdown],20000000
    mov qword [risk_min_cash],1000
    mov qword [risk_threshold],10
    mov qword [live_mode],1
    cmp qword [stream_mode],1
    jne .live_mapped
    call qfx_scan_stream_live
    jmp .live_report
.live_mapped:
    call qfx_scan_and_live
.live_report:
    lea rdi,[rel live_ready]
    call qfx_print_cstr
    call qfx_write_checkpoint
    call qfx_close_journal
    call qfx_close_output
    cmp qword [stream_mode],1
    je .live_stream_done
    call qfx_munmap_input
.live_stream_done:
    call qfx_close_input
    jmp .exit_ok

.usage:
    lea rdi,[rel usage]
    call qfx_print_cstr
.exit_ok:
    mov eax,SYS_EXIT
    xor edi,edi
    syscall
.exit_fail:
    mov eax,SYS_EXIT
    mov edi,1
    syscall
.open_fail:
    lea rdi,[rel open_error]
    call qfx_print_cstr
    jmp .exit_fail
.map_fail:
    lea rdi,[rel map_error]
    call qfx_print_cstr
    jmp .exit_fail
.write_fail:
    lea rdi,[rel write_error]
    call qfx_print_cstr
    jmp .exit_fail
.broker_fail:
    lea rdi,[rel broker_error]
    call qfx_print_cstr
    jmp .exit_fail
.journal_fail:
    lea rdi,[rel journal_error]
    call qfx_print_cstr
    jmp .exit_fail

qfx_streq:
    push rbx
    mov rbx,rdi
.loop:
    mov al,[rbx]
    mov dl,[rsi]
    cmp al,dl
    jne .no
    test al,al
    jz .yes
    inc rbx
    inc rsi
    jmp .loop
.yes:
    mov eax,1
    pop rbx
    ret
.no:
    xor eax,eax
    pop rbx
    ret

qfx_print_cstr:
    push rbx
    mov rbx,rdi
    xor edx,edx
.len:
    cmp byte [rbx+rdx],0
    je .write
    inc rdx
    jmp .len
.write:
    mov eax,SYS_WRITE
    mov edi,STDOUT
    mov rsi,rbx
    syscall
    pop rbx
    ret

qfx_print_u64:
    push rbx
    push r12
    mov r12,rdi
    cmp r12,0
    jge .positive_value
    mov byte [num_buf],'-'
    mov eax,SYS_WRITE
    mov edi,STDOUT
    lea rsi,[num_buf]
    mov edx,1
    syscall
    neg r12
.positive_value:
    lea rbx,[num_buf+31]
    mov byte [rbx],10
    dec rbx
    mov rax,r12
    test rax,rax
    jnz .digits
    mov byte [rbx],'0'
    jmp .emit
.digits:
    xor edx,edx
    mov ecx,10
    div rcx
    add dl,'0'
    mov [rbx],dl
    dec rbx
    test rax,rax
    jnz .digits
    inc rbx
.emit:
    mov rdx,num_buf+32
    sub rdx,rbx
    mov eax,SYS_WRITE
    mov edi,STDOUT
    mov rsi,rbx
    syscall
    pop r12
    pop rbx
    ret

qfx_open_input:
    cmp byte [rdi],'-'
    jne .open_path
    cmp byte [rdi+1],0
    jne .open_path
    mov rax,STDIN
    mov [input_fd],rax
    ret
.open_path:
    mov eax,SYS_OPEN
    mov rsi,O_RDONLY
    xor edx,edx
    syscall
    mov [input_fd],rax
    ret

qfx_open_output:
    mov eax,SYS_OPEN
    mov rsi,O_WRONLY|O_CREAT|O_TRUNC
    mov edx,0644o
    syscall
    mov [output_fd],rax
    ret

qfx_open_journal:
    mov eax,SYS_OPEN
    lea rdi,[rel journal_path]
    mov rsi,O_WRONLY|O_CREAT|O_APPEND
    mov edx,0644o
    syscall
    mov [journal_fd],rax
    ret

qfx_close_journal:
    mov eax,SYS_CLOSE
    mov rdi,[journal_fd]
    syscall
    ret

qfx_write_journal:
    mov eax,SYS_WRITE
    mov rdi,[journal_fd]
    lea rsi,[current_event]
    mov edx,EVENT_SIZE
    syscall
    mov eax,SYS_FSYNC
    mov rdi,[journal_fd]
    syscall
    ret

qfx_load_checkpoint:
    mov eax,SYS_OPEN
    lea rdi,[rel checkpoint_path]
    mov esi,O_RDONLY
    xor edx,edx
    syscall
    test rax,rax
    js .missing
    mov r15,rax
    mov eax,SYS_READ
    mov rdi,r15
    lea rsi,[write_buf]
    mov edx,48
    syscall
    cmp rax,48
    jne .close
    mov rax,[write_buf]
    mov [position],rax
    mov rax,[write_buf+8]
    mov [cash],rax
    mov rax,[write_buf+16]
    mov [equity],rax
    mov rax,[write_buf+24]
    mov [peak_equity],rax
    mov rax,[write_buf+32]
    mov [max_drawdown],rax
    mov rax,[write_buf+40]
    mov [sequence_counter],rax
.close:
    mov eax,SYS_CLOSE
    mov rdi,r15
    syscall
.missing:
    ret

qfx_write_checkpoint:
    mov eax,SYS_OPEN
    lea rdi,[rel checkpoint_path]
    mov rsi,O_WRONLY|O_CREAT|O_TRUNC
    mov edx,0644o
    syscall
    mov r15,rax
    mov rax,[position]
    mov [write_buf],rax
    mov rax,[cash]
    mov [write_buf+8],rax
    mov rax,[equity]
    mov [write_buf+16],rax
    mov rax,[peak_equity]
    mov [write_buf+24],rax
    mov rax,[max_drawdown]
    mov [write_buf+32],rax
    mov rax,[sequence_counter]
    mov [write_buf+40],rax
    mov eax,SYS_WRITE
    mov rdi,r15
    lea rsi,[write_buf]
    mov edx,48
    syscall
    mov eax,SYS_FSYNC
    mov rdi,r15
    syscall
    mov eax,SYS_CLOSE
    mov rdi,r15
    syscall
    ret

qfx_open_live_broker:
    cmp byte [rdi],'-'
    jne .broker_path
    cmp byte [rdi+1],0
    jne .broker_path
    mov rax,STDOUT
    mov [output_fd],rax
    ret
.broker_path:
    mov eax,SYS_OPEN
    mov rsi,O_WRONLY|O_CREAT|O_TRUNC
    mov edx,0644o
    syscall
    mov [output_fd],rax
    ret

qfx_close_input:
    mov eax,SYS_CLOSE
    mov rdi,[input_fd]
    syscall
    ret

qfx_close_output:
    mov eax,SYS_CLOSE
    mov rdi,[output_fd]
    syscall
    ret

qfx_mmap_input:
    mov eax,SYS_FSTAT
    mov rdi,[input_fd]
    lea rsi,[statbuf]
    syscall
    test rax,rax
    js .bad
    mov r14,[statbuf+48]
    mov [input_len],r14
    mov eax,SYS_MMAP
    xor edi,edi
    mov rsi,r14
    mov edx,PROT_READ
    mov r10,MAP_PRIVATE
    mov r8,[input_fd]
    xor r9d,r9d
    syscall
    test rax,rax
    js .bad
    mov [input_addr],rax
    cmp dword [rax],QFX_MAGIC
    jne .csv_format
    mov qword [input_format],1
    add rax,QFX_HEADER_SIZE
    mov [iter_ptr],rax
    jmp .format_done
.csv_format:
    mov qword [input_format],0
    mov rax,[input_addr]
    mov [iter_ptr],rax
.format_done:
    add r14,[input_addr]
    mov [input_end],r14
    mov rax,[input_addr]
    ret
.bad:
    mov rax,-1
    ret

qfx_munmap_input:
    mov eax,SYS_MUNMAP
    mov rdi,[input_addr]
    mov rsi,[input_len]
    syscall
    ret

qfx_reset_state:
    mov qword [prev_price],0
    mov qword [has_prev],0
    mov qword [position],0
    mov qword [cash],100000000
    mov qword [equity],100000000
    mov qword [peak_equity],100000000
    mov qword [max_drawdown],0
    mov qword [trades],0
    mov qword [fills],0
    mov qword [wins],0
    mov qword [losses],0
    mov qword [net_pnl],0
    mov qword [last_trade_pnl],0
    mov qword [valid_events],0
    mov qword [invalid_lines],0
    mov qword [sequence_counter],0
    mov qword [order_counter],0
    ret

qfx_read_live_line:
    push r12
    lea rbx,[line_buf]
    xor r12d,r12d
.read:
    mov eax,SYS_READ
    mov rdi,[input_fd]
    lea rsi,[rbx+r12]
    mov edx,1
    syscall
    test rax,rax
    jle .eof
    cmp byte [rbx+r12],10
    je .done
    inc r12
    cmp r12,MAX_LINE-2
    jb .read
    mov byte [rbx+r12],10
.done:
    mov rax,rbx
    add rax,r12
    inc rax
    pop r12
    ret
.eof:
    test r12,r12
    jz .empty
    mov byte [rbx+r12],10
    mov rax,rbx
    add rax,r12
    inc rax
    pop r12
    ret
.empty:
    xor eax,eax
    pop r12
    ret

qfx_scan_stream_live:
.loop:
    call qfx_read_live_line
    test rax,rax
    jz .done
    mov qword [input_addr],line_buf
    mov [input_end],rax
    mov qword [input_format],0
    mov qword [iter_ptr],line_buf
    mov rdi,line_buf
    call qfx_parse_line
    test rax,rax
    jz .done
    test edx,edx
    jz .loop
    inc qword [valid_events]
    lea rdi,[current_event]
    call qfx_process_event
    jmp .loop
.done:
    ret

qfx_parse_route_line:
    push rbx
    mov rbx,rdi
    mov rsi,[input_end]
    cmp rbx,rsi
    jae .end
    mov rdi,rbx
    call qfx_parse_raw
    jc .bad
    mov [route_broker_start],rax
    mov rsi,rax
.broker_scan:
    cmp byte [rsi],','
    je .broker_done
    cmp byte [rsi],10
    je .broker_done
    inc rsi
    jmp .broker_scan
.broker_done:
    mov rdx,rsi
    sub rdx,[route_broker_start]
    mov [route_broker_len],rdx
    mov rdi,rax
    call qfx_skip_field
    mov rdi,rax
    call qfx_parse_num
    jc .bad
    mov [route_bid],rdx
    mov rdi,rax
    call qfx_parse_num
    jc .bad
    mov [route_ask],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .bad
    mov [route_latency],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .bad
    mov [route_slippage],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .bad
    mov [route_fee],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .bad
    mov [route_reject],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .bad
    mov [route_age],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .bad
    mov [route_capacity],rdx
    mov rax,rdi
    mov rsi,[input_end]
.seek:
    cmp rax,rsi
    jae .good
    cmp byte [rax],10
    je .inc
    inc rax
    jmp .seek
.inc:
    inc rax
.good:
    mov edx,1
    pop rbx
    ret
.bad:
    mov rax,rbx
    mov rsi,[input_end]
.skip:
    cmp rax,rsi
    jae .bad_done
    cmp byte [rax],10
    je .bad_inc
    inc rax
    jmp .skip
.bad_inc:
    inc rax
.bad_done:
    xor edx,edx
    pop rbx
    ret
.end:
    xor eax,eax
    xor edx,edx
    pop rbx
    ret

qfx_scan_route:
    mov r12,[input_addr]
    mov [iter_ptr],r12
.loop:
    mov rdi,[iter_ptr]
    call qfx_parse_route_line
    test rax,rax
    jz .done
    mov [iter_ptr],rax
    test edx,edx
    jz .loop
    call qfx_score_route
    jmp .loop
.done:
    ret

qfx_score_route:
    mov rax,[route_age]
    mov rcx,5000000000
    cmp rax,rcx
    ja .return
    mov rax,[route_capacity]
    cmp rax,[route_quantity]
    jb .return
    mov rax,[route_ask]
    sub rax,[route_bid]
    mov rcx,[route_latency]
    imul rcx,100
    add rax,rcx
    add rax,[route_slippage]
    add rax,[route_fee]
    mov rcx,[route_reject]
    xor rdx,rdx
    mov r8,1000000
    mov rax,rcx
    div r8
    add rax,[route_ask]
    mov rcx,[route_age]
    mov r8,100000000
    xor rdx,rdx
    mov rax,rcx
    div r8
    add rax,[route_latency]
    cmp rax,[route_best_score]
    jae .return
    mov [route_best_score],rax
    mov rax,[route_side]
    cmp rax,0
    jg .buy
    mov rax,[route_bid]
    jmp .price
.buy:
    mov rax,[route_ask]
.price:
    mov [route_best_start],rax
.return:
    ret

qfx_print_route:
    mov rax,[route_best_score]
    cmp rax,-1
    jne .show
    lea rdi,[rel route_none]
    call qfx_print_cstr
    ret
.show:
    lea rdi,[rel route_broker_label]
    call qfx_print_cstr
    mov eax,SYS_WRITE
    mov edi,STDOUT
    mov rsi,[route_broker_start]
    mov rdx,[route_broker_len]
    syscall
    mov eax,SYS_WRITE
    mov edi,STDOUT
    lea rsi,[newline]
    mov edx,1
    syscall
    lea rdi,[rel route_score_label]
    call qfx_print_cstr
    mov rdi,[route_best_score]
    call qfx_print_u64
    lea rdi,[rel route_price_label]
    call qfx_print_cstr
    mov rdi,[route_best_start]
    call qfx_print_u64
    ret

qfx_scan_input:
    mov r12,[iter_ptr]
.loop:
    mov rdi,[iter_ptr]
    call qfx_parse_line
    test rax,rax
    jz .done
    mov [iter_ptr],rax
    test edx,edx
    jz .bad
    inc qword [valid_events]
    jmp .loop
.bad:
    inc qword [invalid_lines]
    jmp .loop
.done:
    ret

qfx_scan_and_backtest:
    mov r12,[iter_ptr]
.loop:
    mov rdi,[iter_ptr]
    call qfx_parse_line
    test rax,rax
    jz .done
    mov [iter_ptr],rax
    test edx,edx
    jz .bad
    inc qword [valid_events]
    lea rdi,[current_event]
    call qfx_process_event
    jmp .loop
.bad:
    inc qword [invalid_lines]
    jmp .loop
.done:
    ret

qfx_scan_and_live:
    mov r12,[iter_ptr]
.loop:
    mov rdi,[iter_ptr]
    call qfx_parse_line
    test rax,rax
    jz .done
    mov [iter_ptr],rax
    test edx,edx
    jz .loop
    inc qword [valid_events]
    lea rdi,[current_event]
    call qfx_process_event
    jmp .loop
.done:
    ret

qfx_scan_and_write:
    mov r12,[iter_ptr]
.loop:
    mov rdi,[iter_ptr]
    call qfx_parse_line
    test rax,rax
    jz .done
    mov [iter_ptr],rax
    test edx,edx
    jz .loop
    inc qword [record_count]
    call qfx_write_qfx_event
    jmp .loop
.done:
    ret

qfx_parse_num:
    push rbx
    push r8
    push r9
    push r10
    push r11
    mov rbx,rdi
    xor r8d,r8d
    xor r9d,r9d
    xor r10d,r10d
    xor r11d,r11d
    mov rsi,[input_end]
    cmp byte [rbx],'-'
    jne .scan
    mov r9d,1
    inc rbx
.scan:
    cmp rbx,rsi
    jae .bad
    mov al,[rbx]
    cmp al,','
    je .finish
    cmp al,10
    je .finish
    cmp al,13
    je .finish
    cmp al,'.'
    je .dot
    cmp al,'0'
    jb .bad
    cmp al,'9'
    ja .bad
    imul r8, r8, 10
    movzx rax,al
    sub rax,'0'
    add r8,rax
    cmp r10d,0
    je .advance
    inc r10d
    cmp r10d,5
    ja .bad
.advance:
    inc rbx
    jmp .scan
.dot:
    cmp r10d,0
    jne .bad
    mov r10d,1
    inc rbx
    jmp .scan
.finish:
    cmp r10d,0
    je .sign
    dec r10d
.pad:
    cmp r10d,5
    jae .sign
    imul r8,r8,10
    inc r10d
    jmp .pad
.sign:
    test r9d,r9d
    jz .positive
    neg r8
.positive:
    mov rdx,r8
    mov rax,rbx
    cmp byte [rbx],','
    jne .return
    inc rax
.return:
    clc
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbx
    ret
.bad:
    mov rax,rbx
    .seek:
    cmp rax,rsi
    jae .bad_done
    cmp byte [rax],10
    je .bad_done
    inc rax
    jmp .seek
.bad_done:
    cmp rax,rsi
    jae .bad_end
    inc rax
.bad_end:
    stc
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbx
    ret

qfx_parse_raw:
    push rbx
    push r8
    push r9
    mov rbx,rdi
    xor r8d,r8d
    xor r9d,r9d
    mov rsi,[input_end]
    cmp byte [rbx],'-'
    jne .scan
    mov r9d,1
    inc rbx
.scan:
    cmp rbx,rsi
    jae .bad
    mov al,[rbx]
    cmp al,','
    je .finish
    cmp al,10
    je .finish
    cmp al,13
    je .finish
    cmp al,'0'
    jb .bad
    cmp al,'9'
    ja .bad
    imul r8,r8,10
    movzx rax,al
    sub rax,'0'
    add r8,rax
    inc rbx
    jmp .scan
.finish:
    test r9d,r9d
    jz .positive
    neg r8
.positive:
    mov rdx,r8
    mov rax,rbx
    cmp byte [rbx],','
    jne .return
    inc rax
.return:
    clc
    pop r9
    pop r8
    pop rbx
    ret
.bad:
    mov rax,rbx
.seek:
    cmp rax,rsi
    jae .bad_done
    cmp byte [rax],10
    je .bad_done
    inc rax
    jmp .seek
.bad_done:
    cmp rax,rsi
    jae .bad_end
    inc rax
.bad_end:
    stc
    pop r9
    pop r8
    pop rbx
    ret

qfx_skip_field:
    mov rax,rdi
    mov rsi,[input_end]
.loop:
    cmp rax,rsi
    jae .done
    mov dl,[rax]
    cmp dl,','
    je .comma
    cmp dl,10
    je .newline
    inc rax
    jmp .loop
.comma:
    inc rax
    ret
.newline:
    inc rax
.done:
    ret

qfx_parse_line:
    cmp qword [input_format],1
    je qfx_parse_qfx
    push rbx
    push r12
    mov rbx,rdi
    mov rsi,[input_end]
    cmp rbx,rsi
    jae .end
    mov rdi,rbx
    call qfx_parse_raw
    jc .invalid
    mov [current_event+EV_TS],rdx
    mov rdi,rax
    call qfx_skip_field
    mov qword [current_event+EV_SYMBOL],0
    mov rdi,rax
    call qfx_parse_num
    jc .invalid
    mov [current_event+EV_BID],rdx
    mov rdi,rax
    call qfx_parse_num
    jc .invalid
    mov [current_event+EV_ASK],rdx
    mov rdi,rax
    call qfx_parse_num
    jc .invalid
    mov [current_event+EV_LAST],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .invalid
    mov [current_event+EV_VOLUME],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .invalid
    mov [current_event+EV_BID_SIZE],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .invalid
    mov [current_event+EV_ASK_SIZE],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .invalid
    mov [current_event+EV_SEQ],rdx
    mov rdi,rax
    call qfx_parse_raw
    jc .invalid
    mov [current_event+EV_FLAGS],rdx
    mov rax,[current_event+EV_BID]
    cmp rax,0
    jle .invalid
    mov rax,[current_event+EV_ASK]
    cmp rax,0
    jle .invalid
    mov rax,[current_event+EV_ASK]
    cmp rax,[current_event+EV_BID]
    jb .invalid
    mov rax,rdi
    mov rsi,[input_end]
.seek_end:
    cmp rax,rsi
    jae .valid
    cmp byte [rax],10
    je .valid_inc
    inc rax
    jmp .seek_end
.valid_inc:
    inc rax
.valid:
    mov edx,1
    pop r12
    pop rbx
    ret
.invalid:
    mov rax,rbx
    mov rsi,[input_end]
.seek_bad:
    cmp rax,rsi
    jae .invalid_end
    cmp byte [rax],10
    je .invalid_inc
    inc rax
    jmp .seek_bad
.invalid_inc:
    inc rax
.invalid_end:
    xor edx,edx
    pop r12
    pop rbx
    ret
.end:
    xor eax,eax
    xor edx,edx
    pop r12
    pop rbx
    ret

qfx_parse_qfx:
    mov rax,rdi
    cmp rax,[input_end]
    jae .qfx_end
    mov rdx,rax
    add rdx,EVENT_SIZE
    cmp rdx,[input_end]
    ja .qfx_end
    mov rsi,rax
    mov rdi,current_event
    mov rcx,EVENT_SIZE
    cld
    rep movsb
    mov rax,rsi
    mov edx,1
    ret
.qfx_end:
    xor eax,eax
    xor edx,edx
    ret

qfx_process_event:
    push rbx
    mov rbx,rdi
    mov rax,[rbx+EV_TS]
    mov rdx,[sequence_counter]
    cmp rax,rdx
    jb .reject
    mov [sequence_counter],rax
    mov rax,[rbx+EV_ASK]
    sub rax,[rbx+EV_BID]
    cmp rax,[risk_max_spread]
    ja .reject
    cmp qword [has_prev],0
    jne .signal
    mov rax,[rbx+EV_LAST]
    mov [prev_price],rax
    mov qword [has_prev],1
    call qfx_mark_equity
    pop rbx
    ret
.signal:
    mov rax,[rbx+EV_LAST]
    mov rdx,[prev_price]
    mov rcx,[risk_threshold]
    add rdx,rcx
    cmp rax,rdx
    ja .buy
    mov rdx,[prev_price]
    sub rdx,rcx
    cmp rax,rdx
    jb .sell
    call qfx_mark_equity
    pop rbx
    ret
.buy:
    mov rdi,1
    call qfx_submit_order
    jmp .finish
.sell:
    mov rdi,-1
    call qfx_submit_order
.finish:
    mov rax,[rbx+EV_LAST]
    mov [prev_price],rax
    call qfx_mark_equity
.reject:
    pop rbx
    ret

qfx_submit_order:
    push rbx
    mov rbx,rdi
    mov rdi,1000
    cmp rdi,[risk_max_order]
    ja .done
    mov rax,[position]
    mov rcx,rbx
    imul rcx,rdi
    add rcx,rax
    cmp rcx,[risk_max_position]
    jg .done
    cmp rcx,0
    jl .negative
    jmp .check
.negative:
    mov rdx,rcx
    neg rdx
    cmp rdx,[risk_max_position]
    ja .done
.check:
    mov rax,[current_event+EV_ASK]
    test rbx,rbx
    jg .long_price
    mov rax,[current_event+EV_BID]
.long_price:
    test qword [live_mode],1
    jz .paper_fill
    call qfx_emit_order
    call qfx_write_journal
    inc qword [trades]
    pop rbx
    ret
.paper_fill:
    mov rdx,rdi
    imul rdx,rax
    cqo
    mov r8,100000
    idiv r8
    test rbx,rbx
    jg .debit
    add [cash],rax
    jmp .book
.debit:
    sub [cash],rax
.book:
    mov [position],rcx
    mov rax,[cash]
    sub rax,100000000
    mov [net_pnl],rax
    cmp rcx,0
    jne .trade_done
    cmp rax,0
    jg .count_win
    jl .count_loss
    jmp .trade_done
.count_win:
    inc qword [wins]
    jmp .trade_done
.count_loss:
    inc qword [losses]
.trade_done:
    inc qword [trades]
    inc qword [fills]
.done:
    pop rbx
    ret

qfx_mark_equity:
    mov rax,[position]
    imul rax,[current_event+EV_LAST]
    cqo
    mov rcx,100000
    idiv rcx
    add rax,[cash]
    mov [equity],rax
    cmp rax,[peak_equity]
    jbe .draw
    mov [peak_equity],rax
.draw:
    mov rdx,[peak_equity]
    sub rdx,rax
    cmp rdx,[max_drawdown]
    jbe .risk
    mov [max_drawdown],rdx
.risk:
    cmp rdx,[risk_max_drawdown]
    jbe .return
    mov qword [live_mode],2
.return:
    ret

qfx_risk_check:
    mov rax,[position]
    test rax,rax
    jns .abs
    neg rax
.abs:
    cmp rax,[risk_max_position]
    ja .fail
    mov rax,[cash]
    cmp rax,[risk_min_cash]
    jb .fail
    xor eax,eax
    ret
.fail:
    mov eax,1
    ret

qfx_print_report:
    lea rdi,[rel events_label]
    call qfx_print_cstr
    mov rdi,[valid_events]
    call qfx_print_u64
    lea rdi,[rel trades_label]
    call qfx_print_cstr
    mov rdi,[trades]
    call qfx_print_u64
    lea rdi,[rel fills_label]
    call qfx_print_cstr
    mov rdi,[fills]
    call qfx_print_u64
    lea rdi,[rel wins_label]
    call qfx_print_cstr
    mov rdi,[wins]
    call qfx_print_u64
    lea rdi,[rel losses_label]
    call qfx_print_cstr
    mov rdi,[losses]
    call qfx_print_u64
    lea rdi,[rel net_label]
    call qfx_print_cstr
    mov rdi,[net_pnl]
    call qfx_print_u64
    lea rdi,[rel peak_label]
    call qfx_print_cstr
    mov rdi,[peak_equity]
    call qfx_print_u64
    lea rdi,[rel drawdown_label]
    call qfx_print_cstr
    mov rdi,[max_drawdown]
    call qfx_print_u64
    lea rdi,[rel position_label]
    call qfx_print_cstr
    mov rdi,[position]
    call qfx_print_u64
    lea rdi,[rel cash_label]
    call qfx_print_cstr
    mov rdi,[cash]
    call qfx_print_u64
    ret

qfx_write_qfx_header:
    lea rdi,[write_buf]
    mov qword [rdi],QFX_MAGIC
    mov qword [rdi+8],QFX_VERSION
    mov qword [rdi+16],EVENT_SIZE
    mov qword [rdi+24],100000
    mov qword [rdi+32],0
    mov qword [rdi+40],0
    mov qword [rdi+48],0
    mov qword [rdi+56],0
    mov eax,SYS_WRITE
    mov rdi,[output_fd]
    lea rsi,[write_buf]
    mov edx,QFX_HEADER_SIZE
    syscall
    ret

qfx_finalize_qfx:
    mov eax,SYS_LSEEK
    mov rdi,[output_fd]
    mov esi,40
    xor edx,edx
    syscall
    mov rax,[record_count]
    mov [write_buf],rax
    mov eax,SYS_WRITE
    mov rdi,[output_fd]
    lea rsi,[write_buf]
    mov edx,8
    syscall
    ret

qfx_append_u64:
    push rbx
    push r12
    push r13
    mov r12,rdi
    mov rax,rsi
    lea rbx,[num_buf+32]
    xor r13d,r13d
    test rax,rax
    jnz .digits
    dec rbx
    mov byte [rbx],'0'
    inc r13
    jmp .copy
.digits:
    xor edx,edx
    mov ecx,10
    div rcx
    add dl,'0'
    dec rbx
    mov [rbx],dl
    inc r13
    test rax,rax
    jnz .digits
.copy:
    mov rsi,rbx
    mov rcx,r13
    rep movsb
    mov rax,rdi
    pop r13
    pop r12
    pop rbx
    ret

qfx_emit_order:
    lea rdi,[write_buf]
    cmp rbx,0
    jg .buy
    lea rsi,[rel sell_order]
    jmp .side_done
.buy:
    lea rsi,[rel buy_order]
.side_done:
    mov rcx,4
.copy_side:
    mov al,[rsi]
    mov [rdi],al
    inc rdi
    inc rsi
    loop .copy_side
    mov byte [rdi],','
    inc rdi
    inc qword [order_counter]
    mov rsi,[order_counter]
    call qfx_append_u64
    mov rdi,rax
    mov byte [rdi],','
    inc rdi
    mov rsi,1000
    call qfx_append_u64
    mov rdi,rax
    mov byte [rdi],','
    inc rdi
    cmp rbx,0
    jg .buy_price
    mov rsi,[current_event+EV_BID]
    jmp .price_done
.buy_price:
    mov rsi,[current_event+EV_ASK]
.price_done:
    call qfx_append_u64
    mov rdi,rax
    mov byte [rdi],','
    inc rdi
    mov rsi,[current_event+EV_TS]
    call qfx_append_u64
    mov rdi,rax
    mov byte [rdi],','
    inc rdi
    mov rsi,[current_event+EV_SEQ]
    call qfx_append_u64
    mov rdi,rax
    mov byte [rdi],10
    inc rdi
    mov rax,rdi
    sub rax,write_buf
    mov edx,eax
    mov eax,SYS_WRITE
    mov rdi,[output_fd]
    lea rsi,[write_buf]
    syscall
    ret

qfx_write_qfx_event:
    mov eax,SYS_WRITE
    mov rdi,[output_fd]
    lea rsi,[current_event]
    mov edx,EVENT_SIZE
    syscall
    ret

cmd_validate db "validate",0
cmd_ingest db "ingest",0
cmd_backtest db "backtest",0
cmd_replay db "replay",0
cmd_live db "live",0
cmd_route db "route",0
cmd_sell db "sell",0
live_banner db "live mode requires paper or armed broker adapters",10,0
live_ready db "adapters opened; live loop is ready for external feed",10,0
broker_error db "broker adapter open failed",10,0
journal_error db "journal open failed",10,0
journal_path db "data/live.journal",0
checkpoint_path db "data/live.checkpoint",0
buy_order db "BUY ",0
sell_order db "SELL",0

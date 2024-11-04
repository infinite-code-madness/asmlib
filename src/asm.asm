asmcode SEGMENT PARA EXECUTE READ

EXTERN memcpy: PROC
EXTERN EnterCriticalSection: PROC
EXTERN LeaveCriticalSection: PROC

dyncode_begin PROC PRIVATE FRAME
;r10-Format: Argumentflags, Funktionsadresse +8, Argument1 +10h, 
;Argumentenanzahl der Zielfunktion +18h; Argument2 +20h, Freigabeadresse(hier irrelevant), Code(hier auch irrelevant)
;r10 wird von dynamisch generiertem code gesetzt

;Argumente auf Stack sichern
mov r11, [r10]
test r11, 1h
jz push4
movq r9, xmm3
push4:
mov [rsp+20h], r9

test r11, 2h
jz push3
movq r8, xmm2
push3:
mov [rsp+18h], r8

test r11, 4h
jz push2
movq rdx, xmm1
push2:
mov [rsp+10h], rdx

test r11, 8h
jz push1
movq rcx, xmm0
push1:
mov [rsp+8], rcx

lea rax, [rsp+8]

;Speichern
push rbp;belegt
.pushreg rbp
push r12
.pushreg r12
push r13;belegt
.pushreg r13
push r14
.pushreg r14
push r15
.pushreg r15
push rsi
.pushreg rsi
push rdi
.pushreg rdi

;rax: Stackanfang
mov rbp, rsp ; Ursprüngliche Stackgröße
.setframe rbp, 0
.endprolog
mov r13, r10 ; Das ist später auch wichtig

;Überprüfen, ob kritische Abschnitte betreten werden müssen
mov dil, [r13+70]
test dil, dil
jz no_critical_sections
push rax
sub rsp, 28h;Schattenbereich für EnterCriticalSection und alignment
lea rcx, [r13+72]
call EnterCriticalSection
add rsp, 28h
pop rax
mov rcx, [rax]
mov rdx, [rax+8h]
mov r8, [rax+10h]
mov r9, [rax+18h]
no_critical_sections:

mov r10, 20h; tatsächliche Stackerweiterungsgröße
mov r11, [r13+18h]; Argumentenzahl
cmp r11, 4h
jle skipreadjustment
mov r10, r11
test r10, 1
jz skipincr
add r10, 1
skipincr:
mov rsi, rax
mov rdi, rdx
xor rdx, rdx
mov rax, r10
mov r14, 8h
mul r14
mov r15, rax
mov rax, rsi
mov rdx, rdi
mov r10, r15
skipreadjustment:
sub rsp, r10

mov r12, rsp; Speicherposition
;Argumente kopieren
sub r11, 2
nextarg:
test r11, r11
jz endloop
mov r14, [rax]
mov [r12], r14
add r12, 8
add rax, 8
sub r11, 1
jmp nextarg;

endloop:
mov r11, [r13+20h]
push r11
mov r11, [r13+10h]
push r11

;argumente in register laden
mov rcx, [rsp]
movq xmm0, rcx

mov rdx, [rsp+8]
movq xmm1, rdx

mov r8, [rsp+10h]
movq xmm2, r8

mov r9, [rsp+18h]
movq xmm3, r9

mov rax, [r13+8]

mov dil, [r13+70];müssen Kritische Abschnitte verlassen werden?
test dil, dil
jz leave_no_critical_arg_sections
mov dil, [r13+71]
test dil, dil
jz leave_no_critical_arg_sections
push rcx
push rdx
push r8
push r9
push rax
sub rsp,28h
lea rcx, [r13+72]
call LeaveCriticalSection
add rsp,28h
pop rax
pop r9
pop r8
pop rdx
pop rcx
movq xmm0, rcx
movq xmm1, rdx
movq xmm2, r8
movq xmm3, r9
leave_no_critical_arg_sections:

call rax

mov dil, [r13+70]
test dil, dil
jz leave_no_critical_call_sections
mov dil, [r13+71]
test dil, dil
jnz leave_no_critical_call_sections
push rax
sub rsp, 18h
movdqa [rsp+10h], xmm0
sub rsp, 20h
lea rcx, [r13+72]
call LeaveCriticalSection
add rsp, 20h
movdqa xmm0, [rsp+10h]
add rsp, 18h
pop rax
leave_no_critical_call_sections:

lea rsp, [rbp]
pop rdi
pop rsi
pop r15
pop r14
pop r13
pop r12
pop rbp
ret
dyncode_begin ENDP

redirect_code:
mov rax, [r10]
jmp rax

GetRedirectableAddr PROC PUBLIC
lea rax, redirect_code
ret
GetRedirectableAddr ENDP

GetArgpushAddr PROC PUBLIC
lea rax, dyncode_begin
ret
GetArgpushAddr ENDP

VAListCall PROC EXPORT FRAME
mov [rsp+8], rcx
mov [rsp+10h], rdx
mov [rsp+18h], r8
test r8, r8
jnz continue; bei keinen Argumenten direkt zur Zielfunktion springen
jmp rcx
continue:
mov r10, rsp
push r12
.pushreg r12
push r13
.pushreg r13
push r14; temporärer Frame-Pointer zum kopieren
.pushreg r14
push r15; Eigenes Parameter-Offset
.pushreg r15
push rbp; Stack-Wiederherstellung
.pushreg rbp
mov rbp, rsp
.setframe rbp, 0
.endprolog
mov r15, r10

;Stackframegröße berechnen(in r10)
mov r10, 20h
cmp r8, 4h
jle skipincr_valist
mov rax, r8
test rax, 1
jz skip_single_incr
add rax, 1
skip_single_incr:
xor rdx, rdx
mov r11, 8h
mul r11
mov r10, rax
mov rdx, [r15+10h]; rdx wiederherstellen(wurde für mul zerstört)
skipincr_valist:
sub rsp, r10

;va_list ist in rdx
mov r14, rsp
mov rax, r8
next_arg:
test rax, rax
jz endloop_valist
mov r9, [rdx]
mov [r14], r9
add rdx, 8
add r14, 8
sub rax, 1
jmp next_arg
endloop_valist:

mov rcx, [rsp]
movq xmm0, rcx
mov rdx, [rsp+8]
movq xmm1, rdx
mov r8, [rsp+10h]
movq xmm2, r8
mov r9, [rsp+18h]
movq xmm3, r9

mov rax, [r15+8]
call rax

lea rsp, [rbp]
pop rbp
pop r15
pop r14
pop r13
pop r12
ret
VAListCall ENDP

asmcode ENDS

END
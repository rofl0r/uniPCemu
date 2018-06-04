	cpu 386
	section .text

	bits 16
; NULL memory pointer in binary code
%define NULLMEMORY 0x06,0x00,0x00
%define IS32BIT 0x66
%define TESTREG8 0xff
%define TESTREG16 0xff,0xff
%define TESTREG32 0xff,0xff,0xff,0xff

; Insert instructions here to test!
cpuTest:
; Initialize indicator!
mov al,0x00
out 0xe9,al
cli ; Prevent any interrupts while testing!

; Now start the tests!

; Set 00
db 0x00,NULLMEMORY ; ADD [0000],al
db 0x01,NULLMEMORY ; ADD [0000],ax
db IS32BIT,0x01,NULLMEMORY ; ADD [0000],eax
db 0x02,NULLMEMORY ; ADD al,[0000]
db 0x03,NULLMEMORY ; ADD ax,[0000]
db IS32BIT,0x03,NULLMEMORY ; ADD eax,[0000]
db 0x04,TESTREG8 ; ADD al,0xff
db 0x05,TESTREG16 ; ADD ax,0xffff
db IS32BIT,0x05,TESTREG32 ; ADD eax,0xffffffff

; Set 08
db 0x08,NULLMEMORY ; ADD [0000],al
db 0x09,NULLMEMORY ; ADD [0000],ax
db IS32BIT,0x09,NULLMEMORY ; ADD [0000],eax
db 0x0A,NULLMEMORY ; ADD al,[0000]
db 0x0B,NULLMEMORY ; ADD ax,[0000]
db IS32BIT,0x0B,NULLMEMORY ; ADD eax,[0000]
db 0x0C,TESTREG8 ; ADD al,0xff
db 0x0D,TESTREG16 ; ADD ax,0xffff
db IS32BIT,0x0D,TESTREG32 ; ADD eax,0xffffffff

; Set 10
db 0x10,NULLMEMORY ; ADD [0000],al
db 0x11,NULLMEMORY ; ADD [0000],ax
db IS32BIT,0x11,NULLMEMORY ; ADD [0000],eax
db 0x12,NULLMEMORY ; ADD al,[0000]
db 0x13,NULLMEMORY ; ADD ax,[0000]
db IS32BIT,0x13,NULLMEMORY ; ADD eax,[0000]
db 0x14,TESTREG8 ; ADD al,0xff
db 0x15,TESTREG16 ; ADD ax,0xffff
db IS32BIT,0x15,TESTREG32 ; ADD eax,0xffffffff

; Set 18
db 0x18,NULLMEMORY ; ADD [0000],al
db 0x19,NULLMEMORY ; ADD [0000],ax
db IS32BIT,0x19,NULLMEMORY ; ADD [0000],eax
db 0x1A,NULLMEMORY ; ADD al,[0000]
db 0x1B,NULLMEMORY ; ADD ax,[0000]
db IS32BIT,0x1B,NULLMEMORY ; ADD eax,[0000]
db 0x1C,TESTREG8 ; ADD al,0xff
db 0x1D,TESTREG16 ; ADD ax,0xffff
db IS32BIT,0x1D,TESTREG32 ; ADD eax,0xffffffff

; Set 20
db 0x20,NULLMEMORY ; ADD [0000],al
db 0x21,NULLMEMORY ; ADD [0000],ax
db IS32BIT,0x21,NULLMEMORY ; ADD [0000],eax
db 0x22,NULLMEMORY ; ADD al,[0000]
db 0x23,NULLMEMORY ; ADD ax,[0000]
db IS32BIT,0x23,NULLMEMORY ; ADD eax,[0000]
db 0x24,TESTREG8 ; ADD al,0xff
db 0x25,TESTREG16 ; ADD ax,0xffff
db IS32BIT,0x25,TESTREG32 ; ADD eax,0xffffffff

; Set 28
db 0x28,NULLMEMORY ; ADD [0000],al
db 0x29,NULLMEMORY ; ADD [0000],ax
db IS32BIT,0x29,NULLMEMORY ; ADD [0000],eax
db 0x2A,NULLMEMORY ; ADD al,[0000]
db 0x2B,NULLMEMORY ; ADD ax,[0000]
db IS32BIT,0x2B,NULLMEMORY ; ADD eax,[0000]
db 0x2C,TESTREG8 ; ADD al,0xff
db 0x2D,TESTREG16 ; ADD ax,0xffff
db IS32BIT,0x2D,TESTREG32 ; ADD eax,0xffffffff

; Set 30
db 0x30,NULLMEMORY ; ADD [0000],al
db 0x31,NULLMEMORY ; ADD [0000],ax
db IS32BIT,0x31,NULLMEMORY ; ADD [0000],eax
db 0x32,NULLMEMORY ; ADD al,[0000]
db 0x33,NULLMEMORY ; ADD ax,[0000]
db IS32BIT,0x33,NULLMEMORY ; ADD eax,[0000]
db 0x34,TESTREG8 ; ADD al,0xff
db 0x35,TESTREG16 ; ADD ax,0xffff
db IS32BIT,0x35,TESTREG32 ; ADD eax,0xffffffff

; Set 38
db 0x38,NULLMEMORY ; ADD [0000],al
db 0x39,NULLMEMORY ; ADD [0000],ax
db IS32BIT,0x39,NULLMEMORY ; ADD [0000],eax
db 0x3A,NULLMEMORY ; ADD al,[0000]
db 0x3B,NULLMEMORY ; ADD ax,[0000]
db IS32BIT,0x3B,NULLMEMORY ; ADD eax,[0000]
db 0x3C,TESTREG8 ; ADD al,0xff
db 0x3D,TESTREG16 ; ADD ax,0xffff
db IS32BIT,0x3D,TESTREG32 ; ADD eax,0xffffffff

; TODO: 0x80-0x83 range

; INC reg
inc ax
inc cx
inc dx
inc bx
inc sp
inc bp
inc si
inc di
inc eax
inc ecx
inc edx
inc ebx
inc esp
inc ebp
inc esi
inc edi

; DEC reg
dec ax
dec cx
dec dx
dec bx
dec sp
dec bp
dec si
dec di
dec eax
dec ecx
dec edx
dec ebx
dec esp
dec ebp
dec esi
dec edi

; MOV immediate offset
mov al,0xaa
mov cl,0xaa
mov dl,0xaa
mov bl,0xaa
mov ah,0x55
mov ch,0x55
mov dh,0x55
mov bh,0x55
mov ax,0xaa55
mov cx,0xaa55
mov dx,0xaa55
mov bx,0xaa55
mov sp,0xaa55
mov bp,0xaa55
mov si,0xaa55
mov di,0xaa55
mov eax,0x12345678
mov ecx,0x12345678
mov edx,0x12345678
mov ebx,0x12345678
mov esp,0x12345678
mov ebp,0x12345678
mov esi,0x12345678
mov edi,0x12345678

; TEST AL/AX/EAX,imm (opcodes A8/A9)
mov al,0xaa
test al,0xaa
test al,0x55
mov ax,0x55aa
test ax,0x55aa
test ax,0xaa55
mov eax,0x12345678
test eax,0x12345678
test eax,0x00000000

; TEST [0000],reg
mov eax,0x87654321
mov [0],eax
test [0],al
test word [0],ax
test dword [0],eax
test byte [0],0x21
test byte [0],0x00
test word [0],0x4321
test word [0],0x0000
test dword [0],0x87654321
test dword [0],0x00000000

mov [0],ax
mov bx,0
mov bx,[0]

mov [0],eax
mov ebx,0
mov ebx,[0]

; MOV [addr],AL/AX/EAX and reversed.
mov EAX,0x12345678
mov [0],al
mov [0],ax
mov [0],eax
mov EAX,0
mov al,[0]
mov ax,[0]
mov eax,[0]


mov ecx,0x100
mov eax,0
repeating: dec eax
loopnz repeating

mov ecx,0x10000
mov eax,0
repeating2:
dec eax
a32 loopnz repeating2

; xchg instructions
mov eax,0x12345678
xchg [0],eax
mov eax,0
xchg eax,[0]
xchg al,ah
xchg ah,al
nop
xchg cx,ax
xchg cx,ax
xchg dx,ax
xchg dx,ax
xchg bx,ax
xchg bx,ax
xchg sp,ax
xchg sp,ax
xchg bp,ax
xchg bp,ax
xchg si,ax
xchg si,ax
xchg di,ax
xchg di,ax
xchg ecx,eax
xchg ecx,eax
xchg edx,eax
xchg edx,eax
xchg ebx,eax
xchg ebx,eax
xchg esp,eax
xchg esp,eax
xchg ebp,eax
xchg ebp,eax
xchg esi,eax
xchg esi,eax
xchg edi,eax
xchg edi,eax

mov esp,0
call subfunc
cmp esp,0
jne failvector
sub esp,32
call subfunc32
cmp esp,0
jne failvector
call subfuncn
jmp succeedretfar
nop
nop
nop
nop
jmp failvector

subfunc:
ret 0

subfunc32:
ret 32

subfuncn:
ret

succeedretfar:

jmp finishup 

failvector:
cli
mov al,0xfe
out 0xe9,al
hlt

; Finish up, make sure we stop running now!
finishup:
cli
mov al,0xff
out 0xe9,al
hlt

;
;   Fill the remaining space with NOPs until we get to target offset 0xFFF0.
;
	times 0xfff0-($-$$) nop

; Startup vector!
resetVector:
	jmp   0xf000:cpuTest ; 0000FFF0

release:
	db    "00/00/00",0       ; 0000FFF5  release date
	db    0xFC            ; 0000FFFE  FC (Model ID byte)
	db    0x00            ; 0000FFFF  00 (checksum byte, unused)

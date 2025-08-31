; cv_hw.asm  VGA, Keyboard, MIDI helpers for Borland PowerPack (32-bit)
; Build: tasm32 /ml /m3 cv_hw.asm

.386
.model flat, C
public  vga_set_palette256
public  vsync_wait
public  kbd_get_byte
public  kbd_bda_pop_word
public  mpu_wait_ready
public  mpu_write_data
public  mpu_write_cmd
public  mpu_init_uart

.code
; void vga_set_palette256(unsigned char* pDac);
; Sets the 256-color VGA palette from a 768-byte RGB array at pDac.
vga_set_palette256 proc pDac:dword
    pushad
    mov     esi, [pDac]
    mov     dx, 03C8h
    xor     al, al
    out     dx, al
    inc     dx            ; 03C9h
    mov     ecx, 256*3
@@L: lodsb
    out     dx, al
    loop    @@L
    popad
    ret
vga_set_palette256 endp

; void vsync_wait(void);
; Polls VGA status (port 3DAh bit 3) wait not-in-retrace, then retrace start.
vsync_wait proc
    push edx
    mov  dx,03DAh
; wait for end of current retrace
nr: 
     in al,dx
     test al,08h
     jnz  nr
; wait for next retrace to begin
r:  
     in   al,dx
     test al,08h
     jz   r
    pop  edx
    ret
vsync_wait endp

; int kbd_get_byte(unsigned char* outByte);
; Returns 1 if a byte was read from port 0x60 (keyboard), else 0.
kbd_get_byte proc outp:dword
    push edx
    push eax

    ; Check status port 0x64 bit0 = 1 means output buffer full
    mov  dx,064h
    in   al,dx
    test al,01h
    jz   no_data

    ; Read data byte from 0x60
    mov  dx,060h
    in   al,dx

    ; Store to *outp
    mov  edx,[outp]
    mov  [edx],al

    pop  eax
    pop  edx
    mov  eax,1
    ret

no_data:
    pop  eax
    pop  edx
    xor  eax,eax
    ret
kbd_get_byte endp


; int kbd_bda_pop_word(unsigned short* outWord);
; Returns 1 if a key was popped from the BIOS ring buffer, else 0.
; outWord receives (scan<<8) | ascii.

kbd_bda_pop_word proc pOut:dword
    pushad
    push    es

    ; Create an alias selector for RM segment 0x0040 (BDA)
    mov  bx, 0040h
    mov  ax, 0002h          ; DPMI: Create descriptor alias for RM seg in BX
    int  31h
    jc   short no_key_exit  ; if it fails, say "no key"
    mov  dx, ax             ; keep selector in DX

    ; ES = BDA selector
    mov  ax, dx
    mov  es, ax

    ; Read head and tail pointers (words) at 40:001A / 40:001C
    mov  bx, 001Ah
    mov  ax, es:[bx]        ; AX = head
    mov  bx, 001Ch
    mov  cx, es:[bx]        ; CX = tail

    ; If head == tail, buffer empty
    cmp  ax, cx
    je   short free_and_no_key

    ; Buffer runs from 001Eh .. 003Eh (32 bytes = 16 words)
    ; Read word at current head
    mov  bx, ax             ; BX = head offset
    mov  si, es:[bx]        ; SI = key word (low=ASCII, high=scan)

    ; Advance head by 2, with wrap back to 001Eh after 003Eh
    add  ax, 2
    cmp  ax, 003Eh+2
    jb   short store_head
    mov  ax, 001Eh
store_head:
    mov  bx, 001Ah
    mov  es:[bx], ax        ; store new head

    ; Write result to *pOut
    mov  edi, [pOut]
    mov  [edi], si

    ; Free selector
    mov  ax, 0001h
    mov  cx, dx
    int  31h

    ; success
    pop  es
    popad
    mov  eax, 1
    ret

free_and_no_key:
    ; Free selector
    mov  ax, 0001h
    mov  cx, dx
    int  31h

no_key_exit:
    pop  es
    popad
    xor  eax, eax
    ret
kbd_bda_pop_word endp

; void mpu_wait_ready(unsigned short base);
; Wait until status bit7 == 0 (write-ready) on base+1
mpu_wait_ready proc uses dx, base:word
    mov     dx, WORD PTR [base]
    add     dx, 1                   ; status/command port
w: in      al, dx
     test    al, 80h
     jnz     w
    ret
mpu_wait_ready endp

; void mpu_write_data(unsigned short base, unsigned char val);
mpu_write_data proc uses dx, base:word, val:byte
    ; wait ready
    mov     dx, WORD PTR [base]
    add     dx, 1
w1:  in      al, dx
     test    al, 80h
     jnz     w1
    ; write data to base+0
    mov     dx, WORD PTR [base]
    mov     al, BYTE PTR [val]
    out     dx, al
    ret
mpu_write_data endp

; void mpu_write_cmd(unsigned short base, unsigned char val);
mpu_write_cmd proc uses dx, base:word, val:byte
    ; wait ready
    mov     dx, WORD PTR [base]
    add     dx, 1
w2:  in      al, dx
     test    al, 80h
     jnz     w2
    ; write command to base+1
    mov     dx, WORD PTR [base]
    add     dx, 1
    mov     al, BYTE PTR [val]
    out     dx, al
    ret
mpu_write_cmd endp

; void mpu_init_uart(unsigned short base);
; Reset (FF) then enter UART mode (3F) on base+1
mpu_init_uart proc uses dx, base:word
    ; wait & send 0xFF (reset)
    mov     dx, WORD PTR [base]
    add     dx, 1
w3:  in      al, dx
     test    al, 80h
     jnz     w3
    mov     al, 0FFh
    out     dx, al

    ; wait & send 0x3F (UART mode)
w4:  in      al, dx
     test    al, 80h
     jnz     w4
    mov     al, 03Fh
    out     dx, al
    ret
mpu_init_uart endp

end
; cv_blit.asm � Borland PowerPack 32-bit, TASM32-friendly
; Build:  tasm32 /ml /m3 cv_blit.asm

.386
.model flat, C

public  dpmi_rm_int
public  vga_blit

; -------- Real-mode register frame (DPMI 0.9) --------
RMFRAME_WORDS EQU 25

.data
rm_regs  label byte
rm_edi   dd ?
rm_esi   dd ?
rm_ebp   dd ?
rm_res   dd ?
rm_ebx   dd ?
rm_edx   dd ?
rm_ecx   dd ?
rm_eax   dd ?
rm_flags dw ?
rm_es    dw ?
rm_ds    dw ?
rm_fs    dw ?
rm_gs    dw ?
rm_ip    dw ?
rm_cs    dw ?
rm_sp    dw ?
rm_ss    dw ?

.code
; cv_dpmi.asm  � DPMI helpers for Borland C++ 4.5 PowerPack
; Build:  tasm32 /ml /m3 cv_dpmi.asm

; void dpmi_rm_int(
;   unsigned short intno,
;   unsigned long *pEAX,*pEBX,*pECX,*pEDX,*pESI,*pEDI,
;   unsigned short *pDS, unsigned short *pES, unsigned short *pFLAGS);
dpmi_rm_int proc uses esi edi ebx ecx edx eax, \
  intno:word, pEAX:dword, pEBX:dword, pECX:dword, pEDX:dword, \
  pESI:dword, pEDI:dword, pDS:dword, pES:dword, pFL:dword

    ; zero the frame (25 words)
    lea edi, rm_regs
    mov ecx, RMFRAME_WORDS
    xor eax, eax
    rep stosw

    ; load GP regs from pointers
    mov esi, [pEAX]   
    mov eax, [esi]   
    mov rm_eax, eax
    mov esi, [pEBX]   
    mov eax, [esi]   
    mov rm_ebx, eax
    mov esi, [pECX]   
    mov eax, [esi]   
    mov rm_ecx, eax
    mov esi, [pEDX]   
    mov eax, [esi]   
    mov rm_edx, eax
    mov esi, [pESI]   
    mov eax, [esi]   
    mov rm_esi, eax
    mov esi, [pEDI]   
    mov eax, [esi]   
    mov rm_edi, eax

    ; 16-bit ones
    mov esi, [pDS]    
    mov ax,  [esi]   
    mov rm_ds, ax
    mov esi, [pES]    
    mov ax,  [esi]   
    mov rm_es, ax
    mov esi, [pFL]    
    mov ax,  [esi]   
    mov rm_flags, ax

    ; INT 31h AX=0300h  simulate real-mode interrupt
    mov ax, 0300h
    mov bl, BYTE PTR [intno]
    xor bh, bh
    lea edi, rm_regs
    int 31h

    ; write back GP regs
    mov esi, [pEAX]   
    mov eax, [esi]   
    mov rm_eax, eax
    mov esi, [pEBX]   
    mov eax, [esi]   
    mov rm_ebx, eax
    mov esi, [pECX]   
    mov eax, [esi]   
    mov rm_ecx, eax
    mov esi, [pEDX]   
    mov eax, [esi]   
    mov rm_edx, eax
    mov esi, [pESI]   
    mov eax, [esi]   
    mov rm_esi, eax
    mov esi, [pEDI]   
    mov eax, [esi]   
    mov rm_edi, eax

    ; and DS/ES/FLAGS
    mov esi, [pDS]    
    mov ax, rm_ds    
    mov [esi], ax
    mov esi, [pES]    
    mov ax, rm_es    
    mov [esi], ax
    mov esi, [pFL]    
    mov ax, rm_flags 
    mov [esi], ax

    ret
dpmi_rm_int endp

; void vga_blit(const void* src, unsigned long nbytes);
vga_blit proc src:dword, nbytes:dword
    pushad
    push    es

    mov     esi, [src]          ; source ptr
    mov     ecx, [nbytes]       ; bytes to copy (e.g., 64000)

    ; make an alias selector for RM seg A000h
    mov     bx, 0A000h
    mov     ax, 0002h
    int     31h
    jc      short .done         ; if it fails, exit quietly
    mov     dx, ax

    mov     ax, dx
    mov     es, ax              ; ES = VGA selector
    xor     edi, edi            ; ES:EDI = A000:0000

    ; copy ECX bytes (rep movsd/movsb split)
    mov     edx, ecx
    shr     ecx, 2
    rep     movsd
    mov     ecx, edx
    and     ecx, 3
    rep     movsb

    ; free selector
    mov     ax, 0001h
    mov     cx, dx
    int     31h

.done:
    pop     es
    popad
    ret
vga_blit endp

end

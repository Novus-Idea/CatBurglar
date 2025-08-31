; cv_glue.asm — TASM32 glue for Borland PowerPack (32-bit DPMI)
; Build:  tasm32 /ml /m3 cv_glue.asm

.386
.model flat, C

public  dpmi_rm_int
public  dpmi_map_physical
public  dpmi_unmap_physical
public  dpmi_make_phys_selector
public  dpmi_memcpy_to_sel

; ----- DPMI RM register frame -----
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

; map physical -> linear
dpmi_map_physical proc uses ebx ecx esi edi eax, phys:dword, size:dword
    mov ax,0800h
    xor ecx,ecx
    mov ebx,[phys]
    xor edi,edi
    mov esi,[size]
    int 31h
    jc  mp_fail
    ret
mp_fail: xor eax,eax
    ret
dpmi_map_physical endp

; unmap linear
dpmi_unmap_physical proc uses eax, linear:dword
    mov ax,0801h
    mov eax,[linear]
    int 31h
    ret
dpmi_unmap_physical endp

; void dpmi_unmap_physical(void* linear)
; AX=0801h  EAX = linear address returned by 0800h
;dpmi_unmap_physical proc uses eax, linear:dword
;    mov ax, 0801h
;    mov eax, [linear]
;    int 31h
;    ret
;dpmi_unmap_physical endp

; void dpmi_rm_int(u16 intno, u32*EAX,u32*EBX,u32*ECX,u32*EDX,u32*ESI,u32*EDI,u16*DS,u16*ES,u16*FLAGS)
dpmi_rm_int proc uses esi edi ebx ecx edx eax, \
  intno:word, pEAX:dword, pEBX:dword, pECX:dword, pEDX:dword, \
  pESI:dword, pEDI:dword, pDS:dword, pES:dword, pFL:dword

    ; zero frame (25 words)
    lea edi, rm_regs
    mov ecx, RMFRAME_WORDS
    xor eax, eax
    rep stosw

    ; load regs
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

    mov esi, [pDS]    
    mov ax,  [esi]    
    mov rm_ds, ax
    mov esi, [pES]    
    mov ax,  [esi]   
    mov rm_es, ax
    mov esi, [pFL]    
    mov ax,  [esi]   
    mov rm_flags, ax

    ; simulate RM int
    mov ax, 0300h
    mov bl, BYTE PTR [intno]
    xor bh, bh
    lea edi, rm_regs
    int 31h

    ; write back
    mov esi, [pEAX]   
    mov eax, rm_eax  
    mov [esi], eax
    mov esi, [pEBX]   
    mov eax, rm_ebx  
    mov [esi], eax
    mov esi, [pECX]   
    mov eax, rm_ecx  
    mov [esi], eax
    mov esi, [pEDX]   
    mov eax, rm_edx  
    mov [esi], eax
    mov esi, [pESI]   
    mov eax, rm_esi  
    mov [esi], eax
    mov esi, [pEDI]   
    mov eax, rm_edi  
    mov [esi], eax

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

; u16 dpmi_make_phys_selector(u32 physBase, u32 sizeBytes)
; Creates a selector aliasing the real-mode segment at physBase (must be paragraph aligned).
; Works with VGA: physBase=A0000h, sizeBytes=65536.
dpmi_make_phys_selector proc uses eax ebx ecx edx, physBase:dword, sizeBytes:dword

    ; require paragraph alignment (physBase % 16 == 0)
    mov eax, [physBase]
    test eax, 0Fh
    jnz  failed

    ; BX = real-mode segment = physBase >> 4
    shr eax, 4
    mov bx, ax

    ; AX=0002h  Create alias descriptor for real-mode segment in BX
    mov ax, 0002h
    int 31h
    jc  failed
    mov bx, ax                 ; BX = selector we just got

    ; Set limit to sizeBytes-1 (byte granularity)
    mov ax, 0008h
    mov cx, bx
    mov eax, [sizeBytes]
    dec eax
    int 31h
    jc  free_and_fail

    ; Optional: make it 32-bit RW data, present
    mov ax, 0009h
    mov cx, bx
    mov dx, 0C093h             ; present | DPL0 | 32-bit | RW data
    int 31h
    ; (ignore CF here—many hosts already set suitable AR)

    mov ax, bx                 ; return selector
    ret

free_and_fail:
    mov ax, 0001h              ; free descriptor
    mov cx, bx
    int 31h
failed:
    xor ax, ax
    ret
dpmi_make_phys_selector endp

; void dpmi_memcpy_to_sel(u16 sel, u32 dstOfs, const void* src, u32 nbytes)
; NOTE: sel is passed as a 32-bit stack arg; we take its low 16 bits.
dpmi_memcpy_to_sel proc uses esi edi ecx edx eax, sel32:dword, dstOfs:dword, src:dword, nbytes:dword
    push es
    mov ax, WORD PTR [sel32]
    mov es, ax
    mov edi, [dstOfs]
    mov esi, [src]
    mov ecx, [nbytes]

    mov edx, ecx
    shr ecx, 2
    rep movsd
    mov ecx, edx
    and ecx, 3
    rep movsb

    pop es
    ret
dpmi_memcpy_to_sel endp

end

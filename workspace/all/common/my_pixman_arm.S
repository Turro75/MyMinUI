


.text
.global pixman_composite_src_8888_0565_asm_neon
.type pixman_composite_src_8888_0565_asm_neon, %function

pixman_composite_src_8888_0565_asm_neon:
    PUSH {r4-r11, lr}

    // Carica i parametri
    MOV r4, r0            // width
    MOV r5, r1            // height
    MOV r6, r2            // puntatore dst
    MOV r7, r3            // stride dst (in pixel)
    LDR r8, [sp, #32]     // puntatore src
    LDR r9, [sp, #36]     // stride src (in pixel)

row_loop:
    CMP r5, #0
    BEQ done

    MOV r10, r4           // contatore pixel
    MOV r11, r6           // puntatore riga dst
    MOV r12, r8           // puntatore riga src

pixel_loop:
    CMP r10, #0
    BEQ next_row

    // Carica 8 pixel ARGB8888 da src (32 bit)
    VLD4.8 {d0, d1, d2, d3}, [r12]!   // d0=B, d1=G, d2=R, d3=A

    // Riduci la profondità di colore
    VSHR.U8 d4, d2, #3     // R >> 3
    VSHR.U8 d5, d1, #2     // G >> 2
    VSHR.U8 d6, d0, #3     // B >> 3

    // Converti a 16 bit
    VMOVL.U8 q3, d4        // R a 16 bit
    VMOVL.U8 q4, d5        // G a 16 bit
    VMOVL.U8 q5, d6        // B a 16 bit

    // Combina i componenti
    VSHL.U16 q3, q3, #11   // R << 11
    VSHL.U16 q4, q4, #5    // G << 5
    // B rimane invariato

    VORR q6, q3, q4        // R | G
    VORR q6, q6, q5        // (R | G) | B

    // Salva 8 pixel RGB565 (16 bit)
    VST1.16 {q6}, [r11]!

    SUB r10, r10, #8
    B pixel_loop

next_row:
    ADD r6, r6, r7, LSL #1    // dst += dst_stride * 2
    ADD r8, r8, r9, LSL #2    // src += src_stride * 4
    SUB r5, r5, #1
    B row_loop

done:
    POP {r4-r11, pc}


/* Handwritten baseline-68000 hot paths. C arguments occupy 32-bit stack
 * slots after the return address; save/restore offsets include pushed registers.
 * Byte counts, alignment and positive loop bounds are caller contracts, not
 * checked here: DBRA uses count-minus-one and zero is not an empty loop.
 * Keep C/reference implementations for identical-workload comparisons.
 * See docs/ARCHITECTURE.md for renderer ownership and timing constraints.
 */
/* Baseline 68000 only. C ABI: 32-bit arguments, stack passed. */
.text
.even
.globl copy_move
/* Copy non-overlapping buffers in longwords; byte count must be a multiple of four. */
copy_move:
 move.l 4(%sp),%a0
 move.l 8(%sp),%a1
 move.l 12(%sp),%d0
 lsr.l #2,%d0
 beq 2f
 subq.l #1,%d0
1: move.l (%a1)+,(%a0)+
 dbra %d0,1b
2: rts
.globl copy_movem
/* 32-byte register batches amortise loop overhead. Saved registers shift stack arguments; preserve that accounting when editing. */
copy_movem:
 movem.l %d2-%d7/%a2-%a3,-(%sp)
 move.l 36(%sp),%a0
 move.l 40(%sp),%a1
 move.l 44(%sp),%d0
 move.l %d0,%d1
 lsr.l #5,%d0
 beq 2f
 subq.l #1,%d0
1: movem.l (%a1)+,%d2-%d7/%a2-%a3
 movem.l %d2-%d7/%a2-%a3,(%a0)
 lea 32(%a0),%a0
 dbra %d0,1b
2: andi.l #31,%d1
 lsr.l #2,%d1
 beq 4f
 subq.l #1,%d1
3: move.l (%a1)+,(%a0)+
 dbra %d1,3b
4: movem.l (%sp)+,%d2-%d7/%a2-%a3
 rts
.globl clear_asm
/* Zero longword-aligned work in 32-byte groups, then whole longwords. Caller supplies a multiple-of-four count. */
clear_asm:
 move.l 4(%sp),%a0
 move.l 8(%sp),%d1
 lsr.l #5,%d1
 moveq #0,%d0
 tst.l %d1
 beq 2f
 subq.l #1,%d1
1: move.l %d0,(%a0)+
 move.l %d0,(%a0)+
 move.l %d0,(%a0)+
 move.l %d0,(%a0)+
 move.l %d0,(%a0)+
 move.l %d0,(%a0)+
 move.l %d0,(%a0)+
 move.l %d0,(%a0)+
 dbra %d1,1b
2: move.l 8(%sp),%d1
 andi.l #31,%d1
 lsr.l #2,%d1
 beq 4f
 subq.l #1,%d1
3: move.l %d0,(%a0)+
 dbra %d1,3b
4: rts
.globl masked_asm
/* Interleaved keep-mask and ink words implement (destination & keep) | ink. Rows and words must be positive. */
masked_asm:
 movem.l %d2-%d3,-(%sp)
 move.l 12(%sp),%a0
 move.l 16(%sp),%a1
 move.l 20(%sp),%d0
 move.l 28(%sp),%d2
 move.l 24(%sp),%d1
 add.l %d1,%d1
 sub.l %d1,%d2
 tst.l %d0
 beq 3f
 subq.l #1,%d0
1: move.l 24(%sp),%d1
 subq.l #1,%d1
2: move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 dbra %d1,2b
 adda.l %d2,%a0
 dbra %d0,1b
3: movem.l (%sp)+,%d2-%d3
 rts
/* Clipped preshifted rows. Arguments: dst, pairs, rows, words, stride, sourceSkip. */
.globl masked_clip_asm
/* Clipped sprite fallback: advance source by its skipped bytes after each row. Performance selector retains the comparison path. */
masked_clip_asm:
 btst #5,perfKernel+3
 bne masked_clip_unrolled
 movem.l %d2-%d4,-(%sp)
 move.l 16(%sp),%a0
 move.l 20(%sp),%a1
 move.l 24(%sp),%d0
 move.l 32(%sp),%d2
 move.l 36(%sp),%d4
 move.l 28(%sp),%d1
 add.l %d1,%d1
 sub.l %d1,%d2
 subq.l #1,%d0
1: move.l 28(%sp),%d1
 subq.l #1,%d1
2: move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 dbra %d1,2b
 adda.l %d2,%a0
 adda.l %d4,%a1
 dbra %d0,1b
 movem.l (%sp)+,%d2-%d4
 rts
/* Eight-pixel-wide, twelve-row HUD glyph; destination row stride is 64 bytes. */
.globl hud_glyph_asm
hud_glyph_asm:
 move.l 4(%sp),%a0
 move.l 8(%sp),%a1
 move.b (%a1)+,(%a0)
 move.b (%a1)+,64(%a0)
 move.b (%a1)+,128(%a0)
 move.b (%a1)+,192(%a0)
 move.b (%a1)+,256(%a0)
 move.b (%a1)+,320(%a0)
 move.b (%a1)+,384(%a0)
 move.b (%a1)+,448(%a0)
 move.b (%a1)+,512(%a0)
 move.b (%a1)+,576(%a0)
 move.b (%a1)+,640(%a0)
 move.b (%a1)+,704(%a0)
 rts
/* Copy a completed tile run. Even width/addresses; positive height; equal stride. */
.globl present_rect_asm
/* Transfer only damaged rectangles to the screen. Even widths/alignment and positive height are established by tile clipping. */
present_rect_asm:
 btst #0,perfKernel+3
 bne present_rect_asm_unrolled
present_rect_asm_loop:
 movem.l %d2-%d4,-(%sp)
 move.l 16(%sp),%a0
 move.l 20(%sp),%a1
 move.l 24(%sp),%d2
 move.l %d2,%d3
 andi.l #2,%d3
 move.l 32(%sp),%d4
 sub.l %d2,%d4
 lsr.l #2,%d2
 move.l 28(%sp),%d0
 subq.l #1,%d0
1: move.l %d2,%d1
 beq 3f
 subq.l #1,%d1
2: move.l (%a1)+,(%a0)+
 dbra %d1,2b
3: tst.l %d3
 beq 4f
 move.w (%a1)+,(%a0)+
4: adda.l %d4,%a0
 adda.l %d4,%a1
 dbra %d0,1b
 movem.l (%sp)+,%d2-%d4
 rts

/* Clear even-width completed-composition regions, positive height. */
.globl clear_rect_asm
/* Erase an even-width rectangle in the persistent canvas; surrounding tiles are owned by the compositor. */
clear_rect_asm:
 btst #0,perfKernel+3
 bne clear_rect_asm_unrolled
clear_rect_asm_loop:
 movem.l %d2-%d4,-(%sp)
 move.l 16(%sp),%a0
 move.l 20(%sp),%d2
 move.l %d2,%d3
 andi.l #2,%d3
 move.l 28(%sp),%d4
 sub.l %d2,%d4
 lsr.l #2,%d2
 move.l 24(%sp),%d0
 subq.l #1,%d0
1: move.l %d2,%d1
 beq 3f
 subq.l #1,%d1
2: clr.l (%a0)+
 dbra %d1,2b
3: tst.l %d3
 beq 4f
 clr.w (%a0)+
4: adda.l %d4,%a0
 dbra %d0,1b
 movem.l (%sp)+,%d2-%d4
 rts

/* Same subtraction formatter as C, baseline 68000, returns end pointer. */
.globl hud_u32_asm
hud_u32_asm:
 movem.l %d2-%d4/%a2,-(%sp)
 move.l 20(%sp),%a0
 move.l 24(%sp),%d0
 lea hud_powers,%a2
 moveq #9,%d3
 moveq #0,%d4
1: move.l (%a2)+,%d1
 moveq #48,%d2
2: cmp.l %d1,%d0
 bcs 3f
 sub.l %d1,%d0
 addq.w #1,%d2
 bra 2b
3: cmpi.w #48,%d2
 bne 4f
 tst.w %d4
 bne 4f
 tst.w %d3
 bne 5f
4: move.b %d2,(%a0)+
 moveq #1,%d4
5: dbra %d3,1b
 move.l %a0,%d0
 movem.l (%sp)+,%d2-%d4/%a2
 rts
.even
hud_powers:
 .long 1000000000,100000000,10000000,1000000,100000,10000,1000,100,10,1

/* Build interleaved shifted keep-mask/bits. No 68020 instructions. */
.globl phase_build_asm
/* Pre-shift mask/ink pairs once for a horizontal phase. Reset carry at every scanline to avoid wrapping pixels into the next row. */
phase_build_asm:
 movem.l %d2-%d7/%a2,-(%sp)
 move.l 32(%sp),%a0
 move.l 36(%sp),%a1
 move.l 40(%sp),%a2
 move.l 44(%sp),%d0
 move.l 48(%sp),%d1
 move.l 52(%sp),%d2
 subq.l #1,%d0
1: moveq #0,%d3
 moveq #0,%d4
 move.l %d1,%d6
 subq.l #1,%d6
2: swap %d3
 move.w (%a1)+,%d3
 move.l %d3,%d5
 lsr.l %d2,%d5
 not.w %d5
 move.w %d5,(%a0)+
 swap %d4
 move.w (%a2)+,%d4
 move.l %d4,%d5
 lsr.l %d2,%d5
 move.w %d5,(%a0)+
 dbra %d6,2b
 dbra %d0,1b
 movem.l (%sp)+,%d2-%d7/%a2
 rts

.globl present_rect_asm_unrolled
/* Computed entry into a fixed instruction sled. Instruction sizes are part of the algorithm: do not insert instructions inside the sled. */
present_rect_asm_unrolled:
 movem.l %d2-%d3/%a2,-(%sp)
 move.l 16(%sp),%a0
 move.l 20(%sp),%a1
 move.l 24(%sp),%d1
 move.l 28(%sp),%d0
 move.l 32(%sp),%d2
 sub.w %d1,%d2
 subq.w #1,%d0
 move.w %d1,%d3
 andi.w #2,%d3
 lsr.w #2,%d1
 add.w %d1,%d1
 lea present_rect_asm_tail(%pc),%a2
 suba.w %d1,%a2
present_rect_asm_row:
 jmp (%a2)
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
present_rect_asm_tail:
 tst.w %d3
 beq present_rect_asm_skip
 move.w (%a1)+,(%a0)+
present_rect_asm_skip:
 adda.w %d2,%a0
 adda.w %d2,%a1
 dbra %d0,present_rect_asm_row
 movem.l (%sp)+,%d2-%d3/%a2
 rts

.globl clear_rect_asm_unrolled
/* Computed-entry clear sled; retain opcode sizes and table arithmetic when modifying it. */
clear_rect_asm_unrolled:
 movem.l %d2-%d3/%a2,-(%sp)
 move.l 16(%sp),%a0
 move.l 20(%sp),%d1
 move.l 24(%sp),%d0
 move.l 28(%sp),%d2
 sub.w %d1,%d2
 subq.w #1,%d0
 move.w %d1,%d3
 andi.w #2,%d3
 lsr.w #2,%d1
 add.w %d1,%d1
 lea clear_rect_asm_tail(%pc),%a2
 suba.w %d1,%a2
 moveq #0,%d1
clear_rect_asm_row:
 jmp (%a2)
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
 move.l %d1,(%a0)+
clear_rect_asm_tail:
 tst.w %d3
 beq clear_rect_asm_skip
 move.w %d1,(%a0)+
clear_rect_asm_skip:
 adda.w %d2,%a0
 dbra %d0,clear_rect_asm_row
 movem.l (%sp)+,%d2-%d3/%a2
 rts

.globl hud_packed_asm
hud_packed_asm:
 move.l 4(%sp),%a0
 move.l 8(%sp),%a1
 move.b (%a1)+,%d0
 or.b %d0,0(%a0)
 move.b (%a1)+,%d0
 or.b %d0,1(%a0)
 move.b (%a1)+,%d0
 or.b %d0,64(%a0)
 move.b (%a1)+,%d0
 or.b %d0,65(%a0)
 move.b (%a1)+,%d0
 or.b %d0,128(%a0)
 move.b (%a1)+,%d0
 or.b %d0,129(%a0)
 move.b (%a1)+,%d0
 or.b %d0,192(%a0)
 move.b (%a1)+,%d0
 or.b %d0,193(%a0)
 move.b (%a1)+,%d0
 or.b %d0,256(%a0)
 move.b (%a1)+,%d0
 or.b %d0,257(%a0)
 move.b (%a1)+,%d0
 or.b %d0,320(%a0)
 move.b (%a1)+,%d0
 or.b %d0,321(%a0)
 move.b (%a1)+,%d0
 or.b %d0,384(%a0)
 move.b (%a1)+,%d0
 or.b %d0,385(%a0)
 move.b (%a1)+,%d0
 or.b %d0,448(%a0)
 move.b (%a1)+,%d0
 or.b %d0,449(%a0)
 move.b (%a1)+,%d0
 or.b %d0,512(%a0)
 move.b (%a1)+,%d0
 or.b %d0,513(%a0)
 move.b (%a1)+,%d0
 or.b %d0,576(%a0)
 move.b (%a1)+,%d0
 or.b %d0,577(%a0)
 move.b (%a1)+,%d0
 or.b %d0,640(%a0)
 move.b (%a1)+,%d0
 or.b %d0,641(%a0)
 move.b (%a1)+,%d0
 or.b %d0,704(%a0)
 move.b (%a1)+,%d0
 or.b %d0,705(%a0)
 rts

masked_clip_unrolled:
 movem.l %d2-%d4/%a2,-(%sp)
 move.l 20(%sp),%a0
 move.l 24(%sp),%a1
 move.l 28(%sp),%d0
 move.l 32(%sp),%d1
 move.l 36(%sp),%d2
 move.l 40(%sp),%d4
 add.w %d1,%d1
 sub.w %d1,%d2
 lsl.w #2,%d1
 lea masked_clip_tail(%pc),%a2
 suba.w %d1,%a2
 subq.w #1,%d0
masked_clip_row:
 jmp (%a2)
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
 move.w (%a0),%d3
 and.w (%a1)+,%d3
 or.w (%a1)+,%d3
 move.w %d3,(%a0)+
masked_clip_tail:
 adda.w %d2,%a0
 adda.w %d4,%a1
 dbra %d0,masked_clip_row
 movem.l (%sp)+,%d2-%d4/%a2
 rts

/* Port-compatible 32-bit LCG, returning its 15-bit value modulo positive max.
   Two unsigned 16-bit products preserve all low 32 product bits. */
.globl ski_random_asm
/* Equivalent to the port C LCG modulo a positive limit. This proves C/assembly parity, not an original Windows binary RNG trace. */
ski_random_asm:
 move.l 4(%sp),%a0
 move.l (%a0),%d0
 move.l %d0,%d1
 swap %d1
 mulu.w #17405,%d1
 add.w %d0,%d1
 add.w %d0,%d1
 add.w %d0,%d1
 swap %d1
 clr.w %d1
 mulu.w #17405,%d0
 add.l %d1,%d0
 addi.l #2531011,%d0
 move.l %d0,(%a0)
 swap %d0
 andi.l #32767,%d0
 divu.w 10(%sp),%d0
 swap %d0
 andi.l #65535,%d0
 rts

/* System 6.0.3+ revised Time Manager: A1 addresses our nonmovable task.
   Position-independent callback copied to resident data; no A5 world access. */
.globl ski_timer_begin
.globl ski_timer_end
/* Resident interrupt callback: no allocation or Toolbox drawing, no A5 globals. Offsets are the packed task layout; end label bounds copied code. */
ski_timer_begin:
 movem.l %d0-%d1/%a0-%a1,-(%sp)
 move.l 22(%a1),%a0
 addq.l #1,(%a0)
 move.l 26(%a1),%d0
 move.l %a1,%a0
 .word 0xa05a
 movem.l (%sp)+,%d0-%d1/%a0-%a1
 rts
ski_timer_end:

/* Replace one native monospaced glyph cell; preserve neighbouring cells. */
.globl hud_cell_asm
hud_cell_asm:
 move.l %d2,-(%sp)
 move.l 8(%sp),%a0
 move.l 12(%sp),%a1
 move.l 16(%sp),%d2
 move.w %d2,%d1
 lsr.w #8,%d1
 cmpi.b #255,%d2
 beq hud_cell_single
 move.b 0(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,0(%a0)
 move.b 1(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,1(%a0)
 move.b 64(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,64(%a0)
 move.b 65(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,65(%a0)
 move.b 128(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,128(%a0)
 move.b 129(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,129(%a0)
 move.b 192(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,192(%a0)
 move.b 193(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,193(%a0)
 move.b 256(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,256(%a0)
 move.b 257(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,257(%a0)
 move.b 320(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,320(%a0)
 move.b 321(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,321(%a0)
 move.b 384(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,384(%a0)
 move.b 385(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,385(%a0)
 move.b 448(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,448(%a0)
 move.b 449(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,449(%a0)
 move.b 512(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,512(%a0)
 move.b 513(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,513(%a0)
 move.b 576(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,576(%a0)
 move.b 577(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,577(%a0)
 move.b 640(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,640(%a0)
 move.b 641(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,641(%a0)
 move.b 704(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,704(%a0)
 move.b 705(%a0),%d0
 and.b %d2,%d0
 or.b (%a1)+,%d0
 move.b %d0,705(%a0)
 move.l (%sp)+,%d2
 rts
hud_cell_single:
 move.b 0(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,0(%a0)
 addq.l #1,%a1
 move.b 64(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,64(%a0)
 addq.l #1,%a1
 move.b 128(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,128(%a0)
 addq.l #1,%a1
 move.b 192(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,192(%a0)
 addq.l #1,%a1
 move.b 256(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,256(%a0)
 addq.l #1,%a1
 move.b 320(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,320(%a0)
 addq.l #1,%a1
 move.b 384(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,384(%a0)
 addq.l #1,%a1
 move.b 448(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,448(%a0)
 addq.l #1,%a1
 move.b 512(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,512(%a0)
 addq.l #1,%a1
 move.b 576(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,576(%a0)
 addq.l #1,%a1
 move.b 640(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,640(%a0)
 addq.l #1,%a1
 move.b 704(%a0),%d0
 and.b %d1,%d0
 or.b (%a1)+,%d0
 move.b %d0,704(%a0)
 addq.l #1,%a1
 move.l (%sp)+,%d2
 rts

.globl shadow_stamp_asm
shadow_stamp_asm:
 move.l 4(%sp),%a0
 move.l 8(%sp),%a1
 move.l (%a1)+,%d0
 or.l %d0,0(%a0)
 move.l (%a1)+,%d0
 or.l %d0,64(%a0)
 move.l (%a1)+,%d0
 or.l %d0,128(%a0)
 move.l (%a1)+,%d0
 or.l %d0,192(%a0)
 move.l (%a1)+,%d0
 or.l %d0,256(%a0)
 rts

/* Fixed 80x80 inset: packed source rows (10 bytes), screen stride 64. */
.even
.globl inset_copy_asm
/* Jumpcam copies 80 rows of 10 bytes from its startup-built cache into the 64-byte-stride canvas. No per-frame rescaling or allocation. */
inset_copy_asm:
 move.l 4(%sp),%a0
 move.l 8(%sp),%a1
 moveq #79,%d0
1: move.l (%a1)+,(%a0)+
 move.l (%a1)+,(%a0)+
 move.w (%a1)+,(%a0)+
 lea 54(%a0),%a0
 dbra %d0,1b
 rts

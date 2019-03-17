        xdef _ioredirect_trap_install
        xdef _ioredirect_trap_restore

        xdef _ioredirect_cconws

        xdef _ioredirect_Bconin
        xdef _ioredirect_Bconout
        xdef _ioredirect_Bconstat
        xdef _ioredirect_Bcostat

TRAP1_EXCEPTION: equ $84
TRAP13_EXCEPTION: equ $B4
LONGFRAME: equ $59e

; ------------------------------------------------------------------
    section text

_ioredirect_trap_install:
        move.l  TRAP1_EXCEPTION.w,ioredirect_trap_store
        move.l  #ioredirect_trap1_handler,TRAP1_EXCEPTION.w

        move.l  TRAP13_EXCEPTION.w,ioredirect_trap13_store
        move.l  #ioredirect_trap13_handler,TRAP13_EXCEPTION.w
        rts

_ioredirect_trap_restore:
        move.l  ioredirect_trap_store,TRAP1_EXCEPTION.w
        move.l  ioredirect_trap13_store,TRAP13_EXCEPTION.w
        rts

; ------------------------------------------------------------------
ioredirect_trap13_handler:
        btst.b  #5,(sp)              ; supervisor?
        bne.b   .supervisor
        move.l  usp,a0
        bra.b   .got_frame
.supervisor:
        tst.w   LONGFRAME.w
        beq.b   .short_frame
        lea     8(sp),a0
        bra.b   .got_frame
.short_frame:
        lea     6(sp),a0
.got_frame:
        cmp.w   #$8000,(a0)
        bls.b   .normal_trap
        ; passthrough trap
        and.w   #$7fff,(a0)
        move.l  ioredirect_trap13_store,a0
        jmp     (a0)
.normal_trap
        cmp.w   #1,(a0)
        beq.b   .Bconstat

        cmp.w   #2,(a0)
        beq.b   .Bconin

        cmp.w   #3,(a0)
        beq.b   .Bconout

        cmp.w   #8,(a0)
        beq.b   .Bcostat

        move.l  ioredirect_trap13_store,a0
        jmp     (a0)

.Bconin:
        moveq   #0,d0
        move.w  2(a0),d0
        move.l  d0,-(sp)
        jsr     _ioredirect_Bconin
        addq    #4,sp
        rte

.Bconstat:
        moveq   #0,d0
        move.w  2(a0),d0
        move.l  d0,-(sp)
        jsr     _ioredirect_Bconstat
        addq    #4,sp
        rte

.Bconout:
        moveq   #0,d0
        move.w  4(a0),d0
        move.l  d0,-(sp)
        move.w  2(a0),d0
        move.l  d0,-(sp)
        jsr     _ioredirect_Bconout
        addq    #8,sp
        rte

.Bcostat:
        moveq   #0,d0
        move.w  2(a0),d0
        move.l  d0,-(sp)
        jsr     _ioredirect_Bcostat
        addq    #4,sp
        rte
; ------------------------------------------------------------------

ioredirect_trap1_handler:
        btst.b  #5,(sp)              ; supervisor?
        bne.b   .supervisor
        move.l  usp,a0
        bra.b   .got_frame
.supervisor:
        tst.w   LONGFRAME.w
        beq.b   .short_frame
        lea     8(sp),a0
        bra.b   .got_frame
.short_frame:
        lea     6(sp),a0
.got_frame:
        cmp.w   #$8000,(a0)
        bls.b   .normal_trap
        ; passthrough trap
        and.w   #$7fff,(a0)
        move.l  ioredirect_trap_store,a0
        jmp     (a0)

.normal_trap:

        cmp.w   #9,(a0)
        beq.b   .cconws

.old_trap:
        move.l  ioredirect_trap_store,a0
        jmp     (a0)

; ------------------------------------------------------------------

.cconws:
        move.l   2(a0),-(sp)
        jsr     _ioredirect_cconws
        addq    #4,sp
        rte

; ------------------------------------------------------------------

        section bss
ioredirect_trap_store: ds.l 1
ioredirect_trap13_store: ds.l 1


	xdef	_interruptsOff
	xdef	_interruptsOn

	TEXT    
; Saves SR reg to a variable and turns off interrupts
_interruptsOff:
    move.w	sr,save_SR
	move.w	#$2700,sr
	rts

;Turns on interrupts by restoring SR. Only call this after having called int_off!
_interruptsOn:
	move.w	save_SR,sr
	rts

 	BSS   
save_SR:    ds.w	1

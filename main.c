#include <ez8.h>

#define WEAPONTYPE 1		/* 	1: Machinegun
								2: Sniper
							*/

#if WEAPONTYPE==1
	//Machinegun
	#define DOUBLETAPS 0
	#define AMMO 15
	#define BURSTSIZE 15
	#define REFIRERATE 16
	#define LOADSFX 14
	#define FIRESFX 11
	#define RELOADMIDCLIP 1
#endif

#if WEAPONTYPE==2
	//Sniper
	#define DOUBLETAPS 1
	#define AMMO 15
	#define BURSTSIZE 5
	#define REFIRERATE 16
	#define LOADSFX 14
	#define FIRESFX 13
	#define RELOADMIDCLIP 0
#endif

struct ser_rx {
	volatile unsigned char hasrxed;
	volatile unsigned char buffer[16];
	volatile unsigned char size;
	volatile unsigned char checksum;
} captureBuffer;

volatile unsigned char ser_rx_eob_counter;

volatile unsigned char ser_tx_buffer[16];
volatile unsigned char ser_tx_size;
volatile unsigned char ser_tx_cooldown;
volatile unsigned char ser_tx_currentbyte;

volatile unsigned char ir_rx_counter;
volatile unsigned char ir_rx_bits;
volatile unsigned char ir_loaded_tags;

volatile unsigned char btn_counter;
volatile unsigned char btn_prelim;
volatile unsigned char btn_pressed;

volatile unsigned char tempIO;
volatile unsigned char lastIO;

volatile unsigned char temp_tx_buffer[16];
volatile unsigned char temp_tx_size;

volatile unsigned char sendBarrelReply;
volatile unsigned char sendLoadSpecial;
volatile unsigned char sendArmSpecial;
volatile unsigned char weAreRegistered;
volatile unsigned char weAreReloading;

volatile unsigned int reloadCooldown;

void interrupt isr_uartrx(void) {
	unsigned char temp;
	
	temp = U0RXD;
	//Byte received
	if((U0STAT0 & 0x78) == 0x78) {
		//Error. Do nothing.
		return;
	} else {
		//If the interrupt was because we received data and there were no errors...
		if(!(captureBuffer.size & 0x80)) {
			//If we're not waiting for a block to be processed already...
			captureBuffer.buffer[captureBuffer.size++] = temp;
			captureBuffer.checksum -= temp;
			//captureBuffer[captureBufferHead].size++;
			ser_rx_eob_counter = 0;
			captureBuffer.hasrxed = 1;
		}
	}
	//Clear the interrupt bit
	IRQ0 &= 0xEF;
}

void interrupt isr_uarttx(void) {
	//Ready to transmit a byte
	if(ser_tx_size && !ser_tx_cooldown) {
        //If there's something in the transmit buffer
		if(ser_tx_currentbyte == ser_tx_size) {
			//That was the last byte that we just sent
			ser_tx_currentbyte = 0;
			ser_tx_size = 0;
			ser_tx_cooldown = 116; //Since this interrupt occurs after the first bit has been shifted
									//, and protocol requires we wait 10ms after the end of the block
									//, we need to wait for 7 data bits, 2 stop bits, and then 10ms.
									//This works out to 58 1/4ms counts, or 14.5ms.
									//This works out to 116 1/8ms counts.
		} else {
			//There's still bytes to send
			U0TXD = ser_tx_buffer[ser_tx_currentbyte];
			ser_tx_currentbyte++;
		}
	}
	//Clear the interrupt bit.
	IRQ0 &= 0xF7;
}

void interrupt isr_timer0(void) {
	//4KHz interrupt.
	
	lastIO = tempIO;
	tempIO = PAIN;
	
	//Various 1/8ms timers
	ser_rx_eob_counter += 1;
	if(ser_tx_cooldown) {
		ser_tx_cooldown -= 1;
	}
	if(reloadCooldown) {
		reloadCooldown -= 1;
	}
	
	//Serial RX end-of-block stuff.
	//if((ser_rx_eob_counter == 30) && ser_rx_hasrxed) {
	if((ser_rx_eob_counter == 60) && captureBuffer.hasrxed) {
        captureBuffer.hasrxed = 0;
        if(!captureBuffer.checksum) {
            captureBuffer.size |= 0x80;
        } else {
            captureBuffer.size = 0;
        }
        captureBuffer.checksum = 0xFF;
    }
	
	//Button stuff
	if((tempIO & 0x01) != (lastIO & 0x01)) {
        //Button state changed, clear the counter.
        btn_counter = 0;
        if(!(tempIO & 0x01)) {
            //Was just pressed
            //Set a flag.
            btn_prelim = 1;
        } else {
			btn_prelim = 0;
		}
    } else {
        btn_counter++;
        if(btn_prelim && (btn_counter > 3)) {
            btn_prelim = 0;
            btn_pressed = 1;
        }
    }
	
	//IR stuff
	if((tempIO & 0x08) != (lastIO & 0x08)){
		//LAZERMOD's state has changed
		 if(ir_rx_bits == 1) {
			//if((ir_rx_counter > 22) && (ir_rx_counter < 26)) {
			 if((ir_rx_counter > 45) && (ir_rx_counter < 51)) {
				//If it's between 5.75ms and 6.25ms...
				ir_rx_bits++;
			}
		 } else if(ir_rx_bits < 3) {
			//if((ir_rx_counter > 10) && (ir_rx_counter < 14)) {
			 if((ir_rx_counter > 21) && (ir_rx_counter < 27)) {
				//If it's between 2.75ms and 3.25ms, move on to the next stage
				ir_rx_bits++;
			}
		} else {
			//if((ir_rx_counter > 2) && (ir_rx_counter < 10)) {
			if((ir_rx_counter > 5) && (ir_rx_counter < 19)) {
				//If it's between 0.75ms and 2.25ms...
				ir_rx_bits++;
			}
		}
		ir_rx_counter = 0;
	} else {
		ir_rx_counter++;
		//if(ir_rx_counter > 40) {
		if(ir_rx_counter > 64) {
			//End of IR packet
			if(ir_rx_bits == 17) {
				//Tag of some sort.
				if(ir_loaded_tags) {
					ir_loaded_tags--;
				}
				if(!ir_loaded_tags) {
					//Turn off the INHIBIT line
					PAOUT &= 0xFD;
				}
			}
			ir_rx_bits = 0;
			ir_rx_counter = 0;
		}
	}
}

void init_cpu(void) {
	DI();
	
	//PA0: RELOAD/DBG. Input, external 10k pullup.
	//PA1: INHIBIT. Output.
	//PA2: 38KHz. T1OUT, output.
	//PA3: LAZERMOD. Input.
	//PA4: RXD. UART, input.
	//PA5: TXD. UART, output.
	
	//Set up Port A.
	PADD = 0x19;	//00011001
	PAAF = 0x34;	//00110100
	PAAFS1 = 0x04;	//00000100
	PAAFS2 = 0x00;	//00000000
	//PAPUE = 0x10;	//00010000
	
	//Set up UART for 2000 baud, 8 data bits, 1 start bit, 2 stop bits, interrupt driven, no parity.
	//Set baud rate
	U0BRH = 0x00;
	U0BRL = 0xAD;
	//Set up interrupt
	SET_VECTOR(UART0_RX_IVECT, isr_uartrx);
	SET_VECTOR(UART0_TX_IVECT, isr_uarttx);
	IRQ0ENH |= 0x18;
	IRQ0ENL |= 0x18;
	IRQ0 &= ~0x18;
	//Enable transmitter and receiver, disable parity, 2 stop bits.
	U0CTL0 = 0xC2; 	//11000010
	//Enable interrupts on received data
	U0CTL1 = 0x00;	//00000000
	
	//Set up Timer0 to be an 4KHz heartbeat interrupt.
	//8KHz now.
	//Disable timer, set mode to Continuous, set prescale to 1:1
	T0CTL1 = 0x01;	//00000001
	T0CTL0 = 0x00;	//00000000
	//Reset count
	T0H = 0x00; T0L = 0x01;
	//Set the reload value
	//T0RH = 0x05; T0RL = 0x5F;
	T0RH = 0x02; T0RL = 0xB3;
	//Set the interrupt vector and priority
	SET_VECTOR(TIMER0, isr_timer0);
	IRQ0ENH |= 0x20;
	IRQ0ENL |= 0x20;
	//Enable timer
	T0CTL1 |= 0x80;
	
	//Set up Timer1 to be 38KHz, 50% duty cycle PWM.
	//Turn off timer, set mode to single PWM, set prescale to 1:1, set initial logic level to 1
	T1CTL1 = 0x43;	//01000011
	T1CTL0 = 0x00;	//00000000
	//Reset count
	T1H = 0x00; T1L = 0x01;
	//Set PWM value(Duty cycle)
	T1PWMH = 0x00; T1PWML = 0x48;
	//Set the reload value(Period)
	T1RH = 0x00; T1RL = 0x90;
	//Enable timer
	T1CTL1 |= 0x80;
	
	//Turn off the inhibit line
	PAOUT &= 0xFD;
	
	//lastIO = PAIN;
	//tempIO = PAIN;
	
	EI();
}

void sendQueue(void) {
	int i;
	
	//Copy the data from the temporary queue to the real one.
	for(i = 0;i < temp_tx_size;i++) {
		ser_tx_buffer[i] = temp_tx_buffer[i];
	}
	ser_tx_size = temp_tx_size;
	
	//Clear the temporary queue.
	temp_tx_size = 0;
	
	//Kickstart the UART.
	IRQ0 |= 0x08;
}

void queueByte(unsigned char input) {
	//Add a byte to the temporary queue.
	temp_tx_buffer[temp_tx_size] = input;
	temp_tx_size += 1;
}

void queueChecksum(void) {
	int i;
	unsigned char checksum = 0xFF;
	
	//Calculate a checksum for all the data that's currently in the temporary queue.
	for(i = 0; i < temp_tx_size;i++) {
		checksum -= temp_tx_buffer[i];
	}
	
	//Add the checksum to the queue.
	queueByte(checksum);
}

void loadAmmo(unsigned char howMuch) {
	ir_loaded_tags = howMuch;
	//INHIBIT should be high to generate IR.
	PAOUT |= 0x02;
}

void rxedResetBarrel(void) {
	//This will reset any variables used for keeping track of barrel/tagger interaction status.
	
	ir_loaded_tags = 0;
	
	sendArmSpecial = 0;
	sendLoadSpecial = 0;
	sendBarrelReply = 0;
	
	weAreRegistered = 0;
	weAreReloading = 0;
	
	reloadCooldown = 800;
	
	PAOUT &= 0xFD;
}

void rxedRollCall(void) {
	/*	Format:		BType		$00
					BData0		Blaster code revision
					BSum		$cs
	*/
	//We don't actually care about anything in this block, but we do need to reply to it.
	//Set a flag that will be seen by sendSomething()
	sendBarrelReply = 1;
}

void rxedBarrelSeen(void) {
	/*	Format:		BType		$40
					BSum		$cs
	*/
	//No data, no cares! This does mean that we're registered, though.
	rxedResetBarrel();
	weAreRegistered = 1;
}

void rxedBarrelAck(void) {
	/*	Format:		BType		$41
					BSum		$cs
	*/
	//This one we actually care about, if we were trying to reload.
	if(weAreReloading == 1) {
		//Because we still need to actually arm the secondary weapon!
		//Flag that we need to send this when possible.
		sendArmSpecial = 1;
	} else if(weAreReloading == 2) {
		weAreReloading = 0;
		//We're now in control of sending IR. Set everything up for that.
		loadAmmo(AMMO);
		//Slow down reloading a bit...
		reloadCooldown = 800;
	}
}

void BarrelReply(void) {
	/*	BARREL-REPLY
		Format:		BType		$40
					BData0		Barrel Type
					BSum		$cs
		Barrels send this block in response to $00 ROLL-CALL blocks.
		BData0 should be 0x01 for a shotgun, 0x02 for a machinegun, 0x03 for a sniper, 0x04 for a grenade launcher.
		Other values are undefined, but can be used.
	*/
	
	//BType
	queueByte(0x40);
	//BData0
	queueByte(0x02);
	//BSum
	queueChecksum();
	
	//Send it!
	sendQueue();
}

void LoadSpecial(void) {
	/*	LOAD-SPECIAL
		Format:		BType		$41
					BData0		Ammo Needed (1-10)
					BData1		DoubleTaps
					BData2		LoadSFX
					BData3		FireSFX
					BData4		DisplayMode
					BSum		$cs
		Unloads any ready ammo and loads special shot.
	*/
	
	//BType
	queueByte(0x41);
	//BData0
	queueByte(AMMO);
	//BData1
	queueByte(DOUBLETAPS);
	//BData2
	queueByte(LOADSFX);
	//BData3
	queueByte(FIRESFX);
	//BData4
	queueByte(0);
	//BSum
	queueChecksum();
	
	//We also need to mark that we're trying to load something and are expecting a response from the tagger.
	weAreReloading = 1;
	
	//Send it!
	sendQueue();
}

void ArmSpecial(void) {
	/*	ARM-SPECIAL
		Format:		BType		$42
					BData0	BurstSize (1-15)
					BData1	Ammo (1-15)
					BData2	TrigRate (13-255)
					BSum		$cs
		BurstSize is in the range 1 to 15, 15 meaning unlimited firing.
		Ammo is the amount of ready ammo to indicate in the Ammo Power Bar and should normally be in the range 1-10.
		TrigRate is the number of milliseconds between shots in increments of 10msec.
	*/
	
	//BType
	queueByte(0x42);
	//BData0
	queueByte(BURSTSIZE);
	//BData1
	queueByte(AMMO);
	//BData2
	queueByte(REFIRERATE);
	//BSum
	queueChecksum();
	
	//Send it!
	sendQueue();
	
	//We're expecting another ACK from this.
	weAreReloading = 2;
}

void sendSomething(void) {
	if(!ser_tx_size && !ser_tx_cooldown) {		
		if(sendBarrelReply) {
			sendBarrelReply = 0;
			BarrelReply();
			return;
		}
		if(sendArmSpecial) {
			sendArmSpecial = 0;
			ArmSpecial();
			return;
		}
		if(sendLoadSpecial) {
			sendLoadSpecial = 0;
			if(RELOADMIDCLIP) {
				if(!weAreReloading && !reloadCooldown /*&& !ir_loaded_tags*/) {
					if(ir_loaded_tags != AMMO) {
						LoadSpecial();
					}
				}
			} else {
				if(!weAreReloading && !reloadCooldown && !ir_loaded_tags) {
					LoadSpecial();
				}
			}
			return;
		}
	}
}

void receiveSomething(void) {
	if(captureBuffer.size & 0x80) {
		//There's a block in the buffer
		
		//Figure out what type it is and do stuff accordingly.
		switch(captureBuffer.buffer[0]) { //Look at the Block Type byte.
			case 0x00:
				//ROLL-CALL
				rxedRollCall();
				break;
			case 0x1E: //NO-ACCY
			case 0x1F: //RESET-ALL
			case 0x20: //ROLL-REPLY		//From the TV Game.
			case 0x5F: //RESET-BARREL
			case 0x60: //POWERUP-REPLY
			case 0x80: //RADAR-REPLY
			case 0xA0: //MASTER-REPLY
			case 0xC0: //RHOST-REPLY
				//Any of these results in the barrel resetting and going idle until it sees a ROLL-CALL.
				rxedResetBarrel();
				break;
			case 0x40:
				//BARREL-SEEN
				rxedBarrelSeen();
				break;
			case 0x41:
				//BARREL-ACK
				rxedBarrelAck();
				break;
			default:
				//Nothing we know how to handle. Ignore it.
				break;
		}
		captureBuffer.size = 0;
	}
}

void main(void) {
	unsigned char i;
	init_cpu();
	
	//Initialize the checksum.
	captureBuffer.checksum = 0xFF;
	
	while(1) {
		//Do stuff here. Not sure what yet...
		
		//Debugging thingy.
		/*if(!ser_tx_size) {
            //No block ready to TX. Stuff it.
            ser_tx_buffer[0] = 0xAA;
			ser_tx_buffer[1] = 0x55;
			ser_tx_buffer[2] = 0xAA;
			ser_tx_buffer[3] = 0x55;
            ser_tx_size = 4;
			IRQ0 |= 0x08;
        } else {
			if(!(U0STAT0 ^ 0x06)) {
				IRQ0 |= 0x08;
			}
		}*/
		
		//More debugging stuff.
		//Echo everything back to the tagger.
		//Should put it in factory test mode.
		/*if((ser_rx_size & 0x80) && !ser_tx_size) {
            //Block received.
            //Echo it back for testing.
            unsigned char rx_size = (ser_rx_size & 0x7F);
			int i;
			
            for(i = 0; i < rx_size;i++) {
                ser_tx_buffer[i] = ser_rx_buffer[i];
            }
			
            ser_tx_size = rx_size;
            ser_rx_size = 0;
			IRQ0 |= 0x08;
        } else {
			if(!(U0STAT0 ^ 0x06)) {
				IRQ0 |= 0x08;
			}
		}*/
		
		receiveSomething();
		sendSomething();
		if(btn_pressed && weAreRegistered) {
			btn_pressed = 0;
			if(!weAreReloading && !reloadCooldown) {
				sendLoadSpecial = 1;
			}
		}
	}
}
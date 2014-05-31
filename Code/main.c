/*
*   DIY LTX RapidFire
*   Copyright (C) 2013  Ryan L. "Izzy84075" Bales
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License along
*    with this program; if not, write to the Free Software Foundation, Inc.,
*    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

//Beyond this is the actual code. Unless you're making something that's not a weapon-like attachment, you
// probably don't need to go in here. Go look in config.h!
/********************************************************************************************************
*********************************************************************************************************
*                                                                                                       *
*        DANGER                              HIGH VOLTAGE                                 DANGER        *
*                                                                                                       *
*********************************************************************************************************
********************************************************************************************************/


#include <ez8.h>

#include "main.h"

void interrupt isr_uartrx(void) {
	unsigned char temp;
	
	temp = U0RXD;
	//Byte received
	if((U0STAT0 & 0x78) == 0x78) {
		//Error during reception. Do nothing.
		return;
	} else {
		//If the interrupt was because we received data and there were no errors...
		if(!(captureBuffer.size & 0x80)) { //If we're not waiting for a block to be processed already...
			captureBuffer.buffer[captureBuffer.size++] = temp; //Store the byte into the buffer
			captureBuffer.checksum -= temp; //Subtract it from the running checksum
			ser_rx_eob_counter = 0; //Clear the End-Of-Block counter
			captureBuffer.hasrxed = 1; //Mark that we've received at least one byte.
		}
	}
	//Clear the interrupt bit
	IRQ0 &= 0xEF;
}

void interrupt isr_uarttx(void) {
	//Ready to transmit a byte
	if(ser_tx_size && !ser_tx_cooldown) { //If there's something in the transmit buffer
		if(ser_tx_currentbyte == ser_tx_size) { //If there are no more bytes to send
			ser_tx_currentbyte = 0; //Reset our position in the buffer
			ser_tx_size = 0; 		//Clear the old message out of the buffer
			ser_tx_cooldown = 116; 	//Add the End-Of-Block delay.
									//Since this interrupt occurs after the first bit has been shifted
									//, and protocol requires we wait 10ms after the end of the block
									//, we need to wait for 7 data bits, 2 stop bits, and then 10ms.
									//This works out to 116 1/8ms counts, or 14.5ms.
		} else { //There's still bytes to send
			U0TXD = ser_tx_buffer[ser_tx_currentbyte]; //Put a byte in the UART
			ser_tx_currentbyte++; //Advance our position in the buffer
		}
	}
	//Clear the interrupt bit.
	IRQ0 &= 0xF7;
}

void interrupt isr_timer0(void) {
	//8KHz interrupt.
	
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
	//When the RX line has been idle for 7.5ms(60 1/8ms counts) after we have received at least one byte, it is safe to assume that
	// The block has finished transmitting
	if((ser_rx_eob_counter == 60) && captureBuffer.hasrxed) {
        captureBuffer.hasrxed = 0;
        if(!captureBuffer.checksum) {
			//The easiest way to check the checksum is to initialize it to 0xFF, then subtract every received byte from that 0xFF,
			// including the checksum byte we receive, allowing it to roll over to 0xFF if possible. If we end up with 0x00,
			// the block was received correctly.
            captureBuffer.size |= 0x80; //Flag that the block is valid for processing.
        } else {
            captureBuffer.size = 0; //Clear the buffer.
        }
        captureBuffer.checksum = 0xFF; //Reinitialize the checksum.
    }
	
	//Button stuff
	//If the button is not in the state it was last time we went through the ISR, we either just let go of the button, or
	// just pressed the button.
	if((tempIO & 0x01) != (lastIO & 0x01)) {
        //Button state changed, clear the counter.
        btn_counter = 0;
        if(!(tempIO & 0x01)) {
            //Was just pressed
            //Set a flag that it's been pressed, but might be noise and needs debounced.
            btn_prelim = 1;
        } else {
			//Was just released
			btn_prelim = 0;
		}
    } else {
		//Button is still in whatever state it was last time
        btn_counter++; //Count how long it's been in that state
        if(btn_prelim && (btn_counter > 3)) { //Button has been held for at least 1ms, call it good.
            btn_prelim = 0;
            btn_pressed = 1; //Mark that the button was pressed and debounced
        }
    }
	
	//IR stuff
	//If we have ammo loaded in the attachment, we need to count how many shots have been fired so that we know when to
	// switch back to pistol mode. This code doesn't care what team/player the shot signal is, just that it has the
	// right header and length.
	if((tempIO & 0x08) != (lastIO & 0x08)){
		//LAZERMOD's state has changed
		 if(ir_rx_halfbits == 1) {
			//if((ir_rx_counter > 22) && (ir_rx_counter < 26)) {
			 if((ir_rx_counter > 45) && (ir_rx_counter < 51)) {
				//If it's between 5.75ms and 6.25ms, we probably have the second part of a shot signal.
				//This is actually the second step of the shot signal, but needs to be at the beginning for the if/else if/else
				// to work.
				ir_rx_halfbits++;
			}
		 } else if(ir_rx_halfbits < 3) {
			//if((ir_rx_counter > 10) && (ir_rx_counter < 14)) {
			 if((ir_rx_counter > 21) && (ir_rx_counter < 27)) {
				//If it's between 2.75ms and 3.25ms, move on to the next stage. We should reach this point twice for a shot signal.
				ir_rx_halfbits++;
			}
		} else {
			//if((ir_rx_counter > 2) && (ir_rx_counter < 10)) {
			if((ir_rx_counter > 5) && (ir_rx_counter < 19)) {
				//If it's between 0.75ms and 2.25ms, this is part of a data bit.
				//If we reached this point, we have a shot header, so we should get 14 half-bits.
				ir_rx_halfbits++;
			}
		}
		ir_rx_counter = 0;
	} else {
		ir_rx_counter++;
		//if(ir_rx_counter > 40) {
		if(ir_rx_counter > 64) {
			//End of IR packet
			if(ir_rx_halfbits == 17) {
				//Tag of some sort.
				if(ir_loaded_tags) { //If we have ammo loaded in the attachment
					ir_loaded_tags--; //Take one away!
				}
				if(!ir_loaded_tags) { //If we are now out of ammo
					//Turn off the INHIBIT line
					PAOUT &= 0xFD; //Disable the attachment barrel and go back to the main tagger barrel.
				}
			}
			//Reset for the next tag.
			ir_rx_halfbits = 0;
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
	
	//Set up Timer0 to be an 8KHz heartbeat interrupt.
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
	
	//Put the count of how many bytes are in this block in the real place.
	ser_tx_size = temp_tx_size;
	
	//Clear the temporary counter.
	temp_tx_size = 0;
	
	//Kickstart the UART.
	IRQ0 |= 0x08;
}

void queueByte(unsigned char input) {
	//Add a byte to the queue.
	ser_tx_buffer[temp_tx_size] = input;
	temp_tx_size += 1;
}

void queueChecksum(void) {
	int i;
	unsigned char checksum = 0xFF;
	
	//Calculate a checksum for all the data that's currently in the queue.
	for(i = 0; i < temp_tx_size;i++) {
		checksum -= ser_tx_buffer[i];
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
	queueByte(BARRELTYPE);
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
		Ammo Needed is how much ammo will be deducted from the available total.
		DoubleTaps is (UNKNOWN).
		LoadSFX is what sound effect will be played when this block is RX'd by the tagger.
		FireSFX is what sound effect will be played when the trigger is pulled after this
			special ammo is loaded/armed.
		DisplayMode is what will be shown on the ammo display of the tagger.
	*/
	
	//BType
	queueByte(0x41);
	//BData0
	queueByte(AMMONEEDED);
	//BData1
	queueByte(DOUBLETAPS);
	//BData2
	queueByte(LOADSFX);
	//BData3
	queueByte(FIRESFX);
	//BData4
	queueByte(DISPLAYMODE);
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
	queueByte(TRIGRATE);
	//BSum
	queueChecksum();
	
	//Send it!
	sendQueue();
	
	//We're expecting another ACK from this.
	weAreReloading = 2;
}

void sendSomething(void) {
	if(!ser_tx_size && !ser_tx_cooldown) { //If there's nothing in the queue currently
		if(sendBarrelReply) { //If we need to send a BarrelReply
			sendBarrelReply = 0; //Clear the flag
			BarrelReply(); //Send a BarrelReply.
			return;
		}
		if(sendArmSpecial) { //If we're in the middle of reloading and have received the ACK from loading.
			sendArmSpecial = 0; //Clear the flag
			ArmSpecial(); //Send an ArmSpecial so we can start firing!
			return;
		}
		if(sendLoadSpecial) { //If the button has been pressed and debounced and we're registered with the tagger
			sendLoadSpecial = 0; //Clear the flag
			if(RELOADMIDCLIP) { //If we can reload mid-clip
				if(!weAreReloading && !reloadCooldown) { //If we're not already reloading and are not delaying because we /just/ reloaded
					if(ir_loaded_tags != AMMO) { //If we aren't already full
						LoadSpecial(); //Start reloading.
					}
				}
			} else { //If we can't reload mid-clip
				if(!weAreReloading && !reloadCooldown && !ir_loaded_tags) { //If we're not reloading, not delaying, and aren't loaded.
					LoadSpecial(); //Start reloading.
				}
			}
			return;
		}
	}
}

void receiveSomething(void) {
	if(captureBuffer.size & 0x80) { //There's a block in the buffer!
		
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
		/*
		//Echo everything back to the tagger.
		//Should put it in factory test mode.
		if(captureBuffer.size & 0x80 && !ser_tx_size && !ser_tx_cooldown) {
            //Block received.
            //Echo it back for testing.
            unsigned char rx_size = (captureBuffer.size & 0x7F);
			int i;
			
            for(i = 0; i < rx_size;i++) {
                queueByte(captureBuffer.buffer[i]);
            }
			
            sendQueue();
            captureBuffer.size = 0;
        } else {
			if(!(U0STAT0 ^ 0x06)) {
				IRQ0 |= 0x08;
			}
		}
		*/
		
		//The actual main loop if you want it to work like a weapon accessory.
		receiveSomething(); //Go check if there's anything to process in the receive buffer
		sendSomething(); //Go check if there's anything we need to transmit
		if(btn_pressed) { //If the button has been pressed and debounced
			btn_pressed = 0; //Clear the flag.
			if(weAreRegistered) { //If the tagger has acknowledged our presence
				sendLoadSpecial = 1; //Flag that we should start loading the attachment
			}
		}
	}
}

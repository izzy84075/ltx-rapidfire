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

#include "config.h"

//Global variables

struct ser_rx {
	volatile unsigned char hasrxed; 		//Has at least one byte been received? Used to skip over stuff unless we have something to worry about.
	volatile unsigned char buffer[16];		//The actual serial RX buffer.
	volatile unsigned char size; 			//How many bytes have been received.
	volatile unsigned char checksum; 		//Running checksum counter. Initialized to 0xFF, and every received byte is subtracted from that
											// and allowed to roll over back to 0xFF. When we reach the end of a serial block, this should
											// equal 0x00 if the block was received successfully.
} captureBuffer;

volatile unsigned char ser_rx_eob_counter;	//A counter used to catch the 10ms delay at the end of a block.

volatile unsigned char ser_tx_buffer[16];	//Serial TX buffer
volatile unsigned char ser_tx_size;			//How many bytes are in the TX buffer.
volatile unsigned char ser_tx_cooldown;		//A counter used to insert the 10ms delay at the end of a block.
volatile unsigned char ser_tx_currentbyte;	//Used to keep track of where in the buffer we are.

volatile unsigned char ir_rx_counter;		//Used to count how many cycles the IR RX pin has been in a particular state
volatile unsigned char ir_rx_halfbits;		//Used to count how many high/low pulses have been captured.
volatile unsigned char ir_loaded_tags;		//How much ammo is loaded in the attachment.
volatile unsigned char ir_doubletap_state;	//Used for handling when to switch over to the doubletap INHIBIT state.

volatile unsigned char btn_counter;			//Used to count how many cycles the button pin has been in a particular state
volatile unsigned char btn_prelim;			//What state the button is in
volatile unsigned char btn_pressed;			//Flag to say that the button has been pressed and has passed the debounce period.

volatile unsigned char tempIO;				//GPIO buffer. Current state as of the beginning of this timer ISR cycle.
volatile unsigned char lastIO;				//GPIO buffer. State as of the beginning of the last ISR cycle.

volatile unsigned char temp_tx_size;		//How many bytes are in the queue to be transmitted, but haven't been marked as a complete block..

volatile unsigned char sendBarrelReply;		//Flag saying we need to send a BARREL-REPLY packet.
volatile unsigned char sendLoadSpecial;		//Flag saying we need to send a LOAD-SPECIAL packet.
volatile unsigned char sendArmSpecial;		//Flag saying we need to send an ARM-SPECIAL packet.
volatile unsigned char weAreRegistered;		//Flag that we have been registered with the tagger, and can now load ammo into the attachment.
volatile unsigned char weAreReloading;		//Keeps track of where we are in a reload sequence.

volatile unsigned int reloadCooldown;		//Counter used to delay reloading, if the button gets pressed just after a reload sequence completes.

//Function prototypes!

//ISRs
void interrupt isr_uartrx(void);
void interrupt isr_uarttx(void);
void interrupt isr_timer0(void);
//Initialization
void init_cpu(void);
//TX buffer stuffing functions
void sendQueue(void);
void queueByte(unsigned char input);
void queueChecksum(void);
//Handle loading ammo into the appropriate variables and toggling the right pins
void loadAmmo(unsigned char howMuch);
//Handle receiving various types of packets
void rxedResetBarrel(void);
void rxedRollCall(void);
void rxedBarrelSeen(void);
void rxedBarrelAck(void);
//Handle sending various types of packets
void BarrelReply(void);
void LoadSpecial(void);
void ArmSpecial(void);
//Check what needs to be sent and send it
void sendSomething(void);
//Check what's in the receive buffer
void receiveSomething(void);
//Main!
void main(void);

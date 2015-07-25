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

//This file has all the configuration settings that most people should need.

//WEAPONPROFILE controls which of the profiles below actually gets compiled into the program.
//You can add more profiles below, and not worry about losing the previous set of settings.
//Just change the number to match whatever profile you add!
#define WEAPONPROFILE 1		/* 	1: Machinegun
								2: Sniper
							*/

//This is a weapon profile. There's a lot of comments in this one, but if you're making your own, you don't need to copy those.
//This first "#if" checks against the number defined above to see which one gets included.
// When you make your own profile, make sure to pick a unique number!
#if WEAPONPROFILE==1
	//Machinegun
	#define BARRELTYPE 2		//BarrelType should be 0x01 for a shotgun, 0x02 for a machinegun, 0x03 for a sniper, 0x04 for a grenade launcher.
								// Other types are undefined, but can be used.
	#define AMMONEEDED 15		//Ammo Needed is how much ammo will be deducted from the available total when loaded.
								// Ranges from 1-10 officially, but other values seem to work, as seen here.
	#define AMMO 15				//Ammo is the amount of ready ammo to indicate in the Ammo Power Bar.
								// Should normally be in the range 1-10, but accepted values range from 1-15.
	#define DOUBLETAPS 0x00		//DoubleTaps is complicated. It's a data structure specifically set up to take advantage of the processor in the LTX.
								//When expanded, it looks something like ABB0CDD0, in binary.
								//A and DD are paired, and affect the first shot that the tagger fires after loading/arming.
								//A is a flag saying that there is data in DD.
								//DD is the megatag value for the first shot.
								//B and CC are also a pair.
								//If B is set, you'll get a "free" shot immediately after the first.
								//CC is the megatag value for this free shot.

	#define BURSTSIZE 15		//BurstSize is how many shots will be fired if the trigger is held down.
								// Ranges from 1 to 15, with 15 meaning "until we run out of ammo".
	#define TRIGRATE 16			//TrigRate is the number of milliseconds between shots in increments of 10msec.
								// Ranges from 13 to 255.
	#define LOADSFX 14			//LoadSFX is what sound effect will be played when this block is RX'd by the tagger.
	#define FIRESFX 11			//FireSFX is what sound effect will be played when the trigger is pulled after this
								// special ammo is loaded/armed.
								//SFX for both of the above are as follows:
								// 0: No sound(Silence)
								// 1: Standard clip-eject
								// 2: Empty chamber
								// 3: Tag Taken (hit) (Will override other playing sounds.)
								// 4: Accessory detected
								// 5: Accessory removed
								// 6: Standard reload
								// 7: Standard fire
								// 8: Shotgun reload
								// 9: Shotgun fire
								// 10: Machinegun load
								// 11: Machinegun fire (single shot)
								// 12: Sniper load
								// 13: Sniper fire
								// 14: Grenade charge
	#define DISPLAYMODE	0		//DisplayMode is what will be shown on the ammo display of the tagger.
								//Known modes are as follows:
								// 0x00: Normal (Shows how much ammo remains. Anything above 10 will just show as 10.)
								// 0x20: All flash (If any ammo is left, all ammo display segments will flash.)
								// 0x40: All solid (If any ammo is left, all ammo display segments will be lit solid.)
								// 0x80: Show nothing (Ammo display will be blank.), weapon cannot fire in this mode
								// Using other values is not recommended.
	#define RELOADMIDCLIP 1		//Whether or not you can reload the attachment while it still has ammo loaded.
	#define USEACCESSORYFORTAGS 1		//Do we use the accessory's barrel for the normal tags that the accessory fires?
	#define USEACCESSORYFORDOUBLETAP 0	//Do we use the accessory's barrel for the DoubleTap "Free" shot?
#endif	

//And this is a profile without all the comments. This is probably what most of them will end up looking like.
//Copy this one if you want to make your own! Use the machinegun one above for documentation.
#if WEAPONPROFILE==2
	//Sniper
	#define BARRELTYPE 3
	#define AMMONEEDED 4
	#define AMMO 1
	#define DOUBLETAPS 0
	#define BURSTSIZE 1
	#define TRIGRATE 200
	#define LOADSFX 14
	#define FIRESFX 13
	#define DISPLAYMODE 0
	#define RELOADMIDCLIP 0
	#define USEACCESSORYFORTAGS 0
	#define USEACCESSORYFORDOUBLETAP 0
#endif

#endif
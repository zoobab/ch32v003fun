// The "bootloader" blob is (C) WCH.
// The rest of the code, Copyright 2023 Charles Lohr
// Freely licensable under the MIT/x11, NewBSD Licenses, or
// public domain where applicable. 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "minichlink.h"
#include "../ch32v003fun/ch32v003fun.h"

static int64_t SimpleReadNumberInt( const char * number, int64_t defaultNumber );
void TestFunction(void * v );
struct MiniChlinkFunctions MCF;

int main( int argc, char ** argv )
{
	void * dev = 0;
	if( (dev = TryInit_WCHLinkE()) )
	{
		fprintf( stderr, "Found WCH LinkE\n" );
	}
	else if( (dev = TryInit_ESP32S2CHFUN()) )
	{
		fprintf( stderr, "Found ESP32S2 Programmer\n" );
	}
	else
	{
		fprintf( stderr, "Error: Could not initialize any supported programmers\n" );
		return -32;
	}
	
	SetupAutomaticHighLevelFunctions( dev );

	int status;
	int must_be_end = 0;
	uint8_t rbuff[1024];

	if( MCF.SetupInterface )
	{
		if( MCF.SetupInterface( dev ) < 0 )
		{
			fprintf( stderr, "Could not setup interface.\n" );
			return -33;
		}
	}

	TestFunction( dev );

	int iarg = 1;
	const char * lastcommand = 0;
	for( ; iarg < argc; iarg++ )
	{
		char * argchar = argv[iarg];

		lastcommand = argchar;
		if( argchar[0] != '-' )
		{
			fprintf( stderr, "Error: Need prefixing - before commands\n" );
			goto help;
		}
		if( must_be_end )
		{
			fprintf( stderr, "Error: the command '%c' cannot be followed by other commands.\n", must_be_end );
			return -1;
		}
		
keep_going:
		switch( argchar[1] )
		{
			default:
				fprintf( stderr, "Error: Unknown command %c\n", argchar[1] );
				goto help;
			case '3':
				if( MCF.Control3v3 )
					MCF.Control3v3( dev, 1 );
				else
					goto unimplemented;
				break;
			case '5':
				if( MCF.Control5v )
					MCF.Control5v( dev, 1 );
				else
					goto unimplemented;
				break;
			case 't':
				if( MCF.Control3v3 )
					MCF.Control3v3( dev, 0 );
				else
					goto unimplemented;
				break;
			case 'f':
				if( MCF.Control5v )
					MCF.Control5v( dev, 0 );
				else
					goto unimplemented;
				break;
			case 'u':
				if( MCF.Unbrick )
					MCF.Unbrick( dev );
				else
					goto unimplemented;
				break;
			case 'r': 
				if( MCF.HaltMode )
					MCF.HaltMode( dev, 0 );
				else
					goto unimplemented;
				must_be_end = 'r';
				break;
			case 'R':
				if( MCF.HaltMode )
					MCF.HaltMode( dev, 1 );
				else
					goto unimplemented;
				must_be_end = 'R';
				break;

			// disable NRST pin (turn it into a GPIO)
			case 'd':  // see "RSTMODE" in datasheet
				if( MCF.ConfigureNRSTAsGPIO )
					MCF.ConfigureNRSTAsGPIO( dev, 0 );
				else
					goto unimplemented;
				break;
			case 'D': // see "RSTMODE" in datasheet
				if( MCF.ConfigureNRSTAsGPIO )
					MCF.ConfigureNRSTAsGPIO( dev, 1 );
				else
					goto unimplemented;
				break;
			// PROTECTION UNTESTED!
			/*
			case 'p':
				wch_link_multicommands( devh, 8,
					11, "\x81\x06\x08\x02\xf7\xff\xff\xff\xff\xff\xff",
					4, "\x81\x0b\x01\x01",
					4, "\x81\x0d\x01\xff",
					4, "\x81\x0d\x01\x01",
					5, "\x81\x0c\x02\x09\x01",
					4, "\x81\x0d\x01\x02",
					4, "\x81\x06\x01\x01",
					4, "\x81\x0d\x01\xff" );
				break;
			case 'P':
				wch_link_multicommands( devh, 7,
					11, "\x81\x06\x08\x03\xf7\xff\xff\xff\xff\xff\xff",
					4, "\x81\x0b\x01\x01",
					4, "\x81\x0d\x01\xff",
					4, "\x81\x0d\x01\x01",
					5, "\x81\x0c\x02\x09\x01",
					4, "\x81\x0d\x01\x02",
					4, "\x81\x06\x01\x01" );
				break;
			*/
			case 'o':
			{
				int i;
				int transferred;
				if( argchar[2] != 0 )
				{
					fprintf( stderr, "Error: can't have char after paramter field\n" ); 
					goto help;
				}
				iarg++;
				argchar = 0; // Stop advancing
				if( iarg + 2 >= argc )
				{
					fprintf( stderr, "Error: missing file for -o.\n" ); 
					goto help;
				}
				uint64_t offset = SimpleReadNumberInt( argv[iarg++], -1 );
				uint64_t amount = SimpleReadNumberInt( argv[iarg++], -1 );
				if( offset > 0xffffffff || amount > 0xffffffff )
				{
					fprintf( stderr, "Error: memory value request out of range.\n" );
					return -9;
				}

				// Round up amount.
				amount = ( amount + 3 ) & 0xfffffffc;
				FILE * f = fopen( argv[iarg], "wb" );
				if( !f )
				{
					fprintf( stderr, "Error: can't open write file \"%s\"\n", argv[iarg] );
					return -9;
				}
				uint8_t * readbuff = malloc( amount );
				int readbuffplace = 0;

				if( MCF.ReadBinaryBlob )
				{
					if( MCF.ReadBinaryBlob( dev, offset, amount, readbuff ) < 0 )
					{
						fprintf( stderr, "Fault reading device\n" );
						return -12;
					}
				}				
				else
				{
					goto unimplemented;
				}

				fwrite( readbuff, amount, 1, f );

				free( readbuff );

				fclose( f );
				break;
			}
			case 'w':
			{
				if( argchar[2] != 0 ) goto help;
				iarg++;
				argchar = 0; // Stop advancing
				if( iarg >= argc ) goto help;
				// Write binary.
				int i;
				FILE * f = fopen( argv[iarg], "rb" );
				fseek( f, 0, SEEK_END );
				int len = ftell( f );
				fseek( f, 0, SEEK_SET );
				char * image = malloc( len );
				status = fread( image, len, 1, f );
				fclose( f );
	
				if( status != 1 )
				{
					fprintf( stderr, "Error: File I/O Fault.\n" );
					exit( -10 );
				}
				if( len > 16384 )
				{
					fprintf( stderr, "Error: Image for CH32V003 too large (%d)\n", len );
					exit( -9 );
				}

				if( MCF.WriteBinaryBlob )
				{
					if( MCF.WriteBinaryBlob( dev, 0x08000000, len, image ) )
					{
						fprintf( stderr, "Error: Fault writing image.\n" );
						return -13;
					}
				}
				else
				{
					goto unimplemented;
				}

				// Waiting or something on 2.46.2???????  I don't know why the main system does this.
				//	WCHCHECK( libusb_bulk_transfer( devh, 0x82, rbuff, 1024, &transferred, 2000 ) ); // Ignore respone.
				free( image );
				break;
			}
			
		}
		if( argchar && argchar[2] != 0 ) { argchar++; goto keep_going; }
	}

	if( MCF.Exit )
		MCF.Exit( dev );

	return 0;

help:
	fprintf( stderr, "Usage: minichlink [args]\n" );
	fprintf( stderr, " single-letter args may be combined, i.e. -3r\n" );
	fprintf( stderr, " multi-part args cannot.\n" );
	fprintf( stderr, " -3 Enable 3.3V\n" );
	fprintf( stderr, " -5 Enable 5V\n" );
	fprintf( stderr, " -t Disable 3.3V\n" );
	fprintf( stderr, " -f Disable 5V\n" );
	fprintf( stderr, " -u Clear all code flash - by power off (also can unbrick)\n" );
	fprintf( stderr, " -r Release from Reset\n" );
	fprintf( stderr, " -R Place into Reset\n" );
	fprintf( stderr, " -D Configure NRST as GPIO **WARNING** If you do this and you reconfig\n" );
	fprintf( stderr, "      the SWIO pin (PD1) on boot, your part can never again be programmed!\n" );
	fprintf( stderr, " -d Configure NRST as NRST\n" );
//	fprintf( stderr, " -P Enable Read Protection (UNTESTED)\n" );
//	fprintf( stderr, " -p Disable Read Protection (UNTESTED)\n" );
	fprintf( stderr, " -w [binary image to write]\n" );
	fprintf( stderr, " -o [memory address, decimal or 0x, try 0x08000000] [size, decimal or 0x, try 16384] [output binary image]\n" );
	return -1;	

unimplemented:
	fprintf( stderr, "Error: Command '%s' unimplemented on this programmer.\n", lastcommand );
	return -1;
}


#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)
#define strtoll _strtoi64
#endif

static int64_t SimpleReadNumberInt( const char * number, int64_t defaultNumber )
{
	if( !number || !number[0] ) return defaultNumber;
	int radix = 10;
	if( number[0] == '0' )
	{
		char nc = number[1];
		number+=2;
		if( nc == 0 ) return 0;
		else if( nc == 'x' ) radix = 16;
		else if( nc == 'b' ) radix = 2;
		else { number--; radix = 8; }
	}
	char * endptr;
	uint64_t ret = strtoll( number, &endptr, radix );
	if( endptr == number )
	{
		return defaultNumber;
	}
	else
	{
		return ret;
	}
}


int DefaultSetupInterface( void * dev )
{
	struct InternalState * iss = (struct InternalState*)(((struct ProgrammerStructBase*)dev)->internal);

	if( MCF.Control3v3 ) MCF.Control3v3( dev, 1 );
	if( MCF.DelayUS ) MCF.DelayUS( dev, 16000 );
	MCF.WriteReg32( dev, DMSHDWCFGR, 0x5aa50000 | (1<<10) ); // Shadow Config Reg
	MCF.WriteReg32( dev, DMCFGR, 0x5aa50000 | (1<<10) ); // CFGR (1<<10 == Allow output from slave)
	MCF.WriteReg32( dev, DMCFGR, 0x5aa50000 | (1<<10) ); // Bug in silicon?  If coming out of cold boot, and we don't do our little "song and dance" this has to be called.
	MCF.WriteReg32( dev, DMCONTROL, 0x80000001 ); // Make the debug module work properly.
	MCF.WriteReg32( dev, DMCONTROL, 0x80000001 ); // Initiate a halt request.

	// Read back chip ID.
	uint32_t reg;
	int r = MCF.ReadReg32( dev, DMSTATUS, &reg );
	if( r >= 0 )
	{
		// Valid R.
		if( reg == 0x00000000 || reg == 0xffffffff )
		{
			fprintf( stderr, "Error: Setup chip failed. Got code %08x\n", reg );
			return -9;
		}
		return 0;
	}
	else
	{
		fprintf( stderr, "Error: Could not read chip code.\n" );
		return r;
	}

	iss->statetag = STTAG( "HALT" );
}

static int DefaultWriteWord( void * dev, uint32_t address_to_write, uint32_t data )
{
	printf( "Write Word %08x => %08x\n", data, address_to_write );
	struct InternalState * iss = (struct InternalState*)(((struct ProgrammerStructBase*)dev)->internal);
	uint32_t rrv;
	int r;
	int first = 0;

	if( iss->statetag != STTAG( "WRSQ" ) || address_to_write != iss->currentstateval )
	{
		MCF.WriteReg32( dev, DMABSTRACTAUTO, 0x00000000 ); // Disable Autoexec.
		if(  iss->statetag != STTAG( "WRSQ" ) )
		{
			// Different address, so we don't need to re-write all the program regs.
			MCF.WriteReg32( dev, DMPROGBUF0, 0x0072a023 ); // sw x7,0(x5)
			MCF.WriteReg32( dev, DMPROGBUF1, 0x00428293 ); // addi x5, x5, 4
			MCF.WriteReg32( dev, DMPROGBUF2, 0x00100073 ); // ebreak

			// TODO: This code could also read from DATA1, and then that would go MUCH faster for random writes.
		}

		MCF.WriteReg32( dev, DMDATA0, address_to_write );
		MCF.WriteReg32( dev, DMCOMMAND, 0x00231005 ); // Copy data to x5

		iss->statetag = STTAG( "WRSQ" );
		iss->currentstateval = address_to_write;

		MCF.WriteReg32( dev, DMDATA0, data );
		MCF.WriteReg32( dev, DMCOMMAND, 0x00271007 ); // Copy data to x7, and execute program.
		MCF.WriteReg32( dev, DMABSTRACTAUTO, 1 ); // Enable Autoexec.

		do
		{
			r = MCF.ReadReg32( dev, DMABSTRACTCS, &rrv );
			if( r ) return r;
		}
		while( rrv & (1<<12) );
		if( (rrv >> 8 ) & 7 )
		{
			fprintf( stderr, "Fault writing memory (DMABSTRACTS = %08x)\n", rrv );
		}
	}
	else
	{
		MCF.WriteReg32( dev, DMDATA0, data );
	}


	iss->currentstateval += 4;

	return 0;
}

int DefaultWriteBinaryBlob( void * dev, uint32_t address_to_write, uint32_t blob_size, uint8_t * blob )
{
	int timeout = 0;
	struct InternalState * iss = (struct InternalState*)(((struct ProgrammerStructBase*)dev)->internal);
	int is_flash = 0;
	uint32_t lastgroup = -1;

	if( (address_to_write & 0xff000000) == 0x08000000 || (address_to_write & 0xff000000) == 0x00000000 ) 
	{
		// Need to unlock flash.
		// Flash reg base = 0x40022000,
		// FLASH_MODEKEYR => 0x40022024
		// FLASH_KEYR => 0x40022004
		printf( "UNLOCKING\n" );
		MCF.WriteWord( dev, 0x40022004, 0x45670123 );
		MCF.WriteWord( dev, 0x40022004, 0xCDEF89AB );
		MCF.WriteWord( dev, 0x40022024, 0x45670123 );
		MCF.WriteWord( dev, 0x40022024, 0xCDEF89AB );
		uint32_t rv = 0xffffffff;

		MCF.ReadWord( dev, 0x40022010, &rv );
		printf( "FLASH CTR: %08x\n", rv );
		is_flash = 1;
	}
/*
	MCF.WriteReg32( dev, DMDATA0, address_to_write );
	MCF.WriteReg32( dev, DMCOMMAND, 0x00231003 ); // Copy data to x3 (DATAADDRESS)
	MCF.WriteReg32( dev, DMDATA0, FLASH_R_BASE + 0x10 );
	MCF.WriteReg32( dev, DMCOMMAND, 0x00231004 ); // Copy data to x4 (CTLR)
	MCF.WriteReg32( dev, DMDATA0, FLASH_R_BASE + 0x14 );
	MCF.WriteReg32( dev, DMCOMMAND, 0x00231005 ); // Copy data to x5 (ADDR)

	MCF.WriteReg32( dev, DMDATA0, CR_PAGE_ER );
	MCF.WriteReg32( dev, DMCOMMAND, 0x00231006 ); // Copy flag 1 to x6 (CR_PAGE_ER)
	MCF.WriteReg32( dev, DMDATA0, CR_STRT_Set | CR_PAGE_ER );
	MCF.WriteReg32( dev, DMCOMMAND, 0x00231007 ); // Copy flag 2 to x7 (CR_STRT_Set)

	MCF.WriteReg32( dev, DMPROGBUF0, 0x00022403 ); // lw x8,0(x4)
	MCF.WriteReg32( dev, DMPROGBUF1, 0x00646433 ); // or x8, x8, x6
	MCF.WriteReg32( dev, DMPROGBUF2, 0x00822023 ); // sw x8,0(x4)
	MCF.WriteReg32( dev, DMPROGBUF3, 0x0032a023 ); // sw x3,0(x5)
//	MCF.WriteReg32( dev, DMPROGBUF4, 0x00022023 ); // sw x0,0(x4)
	MCF.WriteReg32( dev, DMPROGBUF4, 0x00100073 ); // ebreak
	MCF.WriteReg32( dev, DMCOMMAND, 0x00250000 ); // Copy data to x7, and execute program.
	MCF.WriteReg32( dev, DMABSTRACTAUTO, 1 ); // Enable Autoexec.
	return 0;


*/
	uint32_t wp = address_to_write;
	uint32_t ew = wp + blob_size;
	int group = -1;
	lastgroup = -1;

	while( wp <= ew )
	{
		if( is_flash )
		{
			group = (wp & 0xffffffc0);
			if( group != lastgroup )
			{
				uint32_t rw = 0xffffffff;
				// 16.4.7, Steps 1+2  (Make sure flash isn't locked)
				MCF.ReadWord( dev, 0x40022010, &rw ); 
				if( rw & 0x8080 ) 
				{
					fprintf( stderr, "Error: Flash is not unlocked\n" );
					return -9;
				}

				// 16.4.7, Step 3: Check the BSY bit of the FLASH_STATR register to confirm that there are no other programming operations in progress.
				// skip (we make sure at the end)

				// Step 4:  set PAGE_ER of FLASH_CTLR(0x40022010)
				MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, CR_PAGE_ER ); // Actually FTER

				// Step 5: Write the first address of the fast erase page to the FLASH_ADDR register.
				MCF.WriteWord( dev, (intptr_t)&FLASH->ADDR, group );

				// Step 6: Set the STAT bit of FLASH_CTLR register to '1' to initiate a fast page erase (64 bytes) action.
				MCF.ReadWord( dev, (intptr_t)&FLASH->CTLR, &rw ); 
				printf( "FLASH_CTLR = %08x\n", rw );
				MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, CR_STRT_Set|CR_PAGE_ER );
				do
				{
					rw = 0;
					MCF.ReadWord( dev, (intptr_t)&FLASH->STATR, &rw ); // FLASH_STATR => 0x4002200C
					printf( "FLASH_STATR %08x %d\n", rw,__LINE__ );
					if( timeout++ > 100 ) goto timedout;
				} while(rw & 1);  // BSY flag.

				MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, 0 );

				// Step 7: Wait for the BSY bit to become '0' or the EOP bit of FLASH_STATR register to be '1' to indicate the endof erase, and clear the EOP bit to 0.
				do
				{
					rw = 0;
					MCF.ReadWord( dev, (intptr_t)&FLASH->STATR, &rw ); // FLASH_STATR => 0x4002200C
					printf( "FLASH_STATR %08x\n", rw );
					if( timeout++ > 100 ) goto timedout;
				} while(rw & 1);  // BSY flag.
				if( rw & FLASH_STATR_WRPRTERR )
				{
					fprintf( stderr, "Memory Protection Error\n" );
					return -44;
				}

				MCF.ReadWord( dev, (intptr_t)&FLASH->CTLR, &rw );
				printf( "FLASH_CTLR = %08x\n", rw );


				MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, CR_BUF_RST | CR_PAGE_PG );
				do
				{
					rw = 0;
					MCF.ReadWord( dev, (intptr_t)&FLASH->STATR, &rw ); // FLASH_STATR => 0x4002200C
					printf( "FLASH_STATR %08x\n", rw );
					if( timeout++ > 100 ) goto timedout;
				} while(rw & 1);  // BSY flag.

				int j;
				for( j = 0; j < 16; j++ )
				{
					MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, CR_PAGE_PG );
					MCF.WriteWord( dev, wp, 0x0000ffff );
					wp += 4; 

					MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, CR_BUF_LOAD );
					do
					{
						rw = 0;
						MCF.ReadWord( dev, (intptr_t)&FLASH->STATR, &rw ); // FLASH_STATR => 0x4002200C
						printf( "FLASH_STATR %08x\n", rw );
						if( timeout++ > 100 ) goto timedout;
					} while(rw & 1);  // BSY flag.
				}
				printf( "Group written.\n" );

				MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, CR_PAGE_PG );
				MCF.WriteWord( dev, (intptr_t)&FLASH->ADDR, group );
				MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, CR_PAGE_PG|CR_STRT_Set );

				do
				{
					rw = 0;
					MCF.ReadWord( dev, (intptr_t)&FLASH->STATR, &rw ); // FLASH_STATR => 0x4002200C
					printf( "FLASH_STATR %08x\n", rw );
					if( timeout++ > 100 ) goto timedout;
				} while(rw & 1);  // BSY flag.
				MCF.WriteWord( dev, (intptr_t)&FLASH->CTLR, 0 );
				do
				{
					rw = 0;
					MCF.ReadWord( dev, (intptr_t)&FLASH->STATR, &rw ); // FLASH_STATR => 0x4002200C
					printf( "FLASH_STATR %08x\n", rw );
					if( timeout++ > 100 ) goto timedout;
				} while(rw & 1);  // BSY flag.

				lastgroup = group;

				// Step 4:  set PAGE_ER of FLASH_CTLR(0x40022010)
//				MCF.WriteWord( dev, FLASH_R_BASE + 0x10, 0 );
			}
		}
		//MCF.WriteWord( dev, wp, *(((uint32_t*)blob)+4) ); //Write data.
		//MCF.WriteWord( dev, 0x40022010, (1<<16)|(1<<18) ); //Enable BUFLOAD.
		wp+=4;
	}
	
//	MCF.WriteReg32( dev, DMCONTROL, 0x80000001 ); // Make the debug module work properly.
//	MCF.WriteReg32( dev, DMCONTROL, 0x80000001 ); // Initiate a halt request.
//	MCF.WriteReg32( dev, DMCONTROL, 0x00000001 ); // Clear Halt Request.
	return 0;
timedout:
	fprintf( stderr, "Timed out\n" );
	return -5;
}

static int DefaultReadWord( void * dev, uint32_t address_to_read, uint32_t * data )
{
	struct InternalState * iss = (struct InternalState*)(((struct ProgrammerStructBase*)dev)->internal);
	uint32_t rrv;
	int r;
	int first = 0;

	if( iss->statetag != STTAG( "RDSQ" ) || address_to_read != iss->currentstateval )
	{
		MCF.WriteReg32( dev, DMABSTRACTAUTO, 0 ); // Disable Autoexec.
		MCF.WriteReg32( dev, DMPROGBUF0, 0x0002a303 ); // lw x6,0(x5)
		MCF.WriteReg32( dev, DMPROGBUF1, 0x00428293 ); // addi x5, x5, 4
		MCF.WriteReg32( dev, DMPROGBUF2, 0x0065a023 ); // sw x6,0(x11) // Write
		MCF.WriteReg32( dev, DMPROGBUF3, 0x00100073 ); // ebreak

		MCF.WriteReg32( dev, DMDATA0, address_to_read );
		MCF.WriteReg32( dev, DMCOMMAND, 0x00231005 ); // Copy data to x5

		MCF.WriteReg32( dev, DMDATA0, 0xe00000f4 );   // DATA0's location in memory.
		MCF.WriteReg32( dev, DMCOMMAND, 0x0023100b ); // Copy data to x11

		MCF.WriteReg32( dev, DMCOMMAND, 0x00241006 ); // Copy x6 to data0 Then execute.  This will be what is repeated.

		MCF.WriteReg32( dev, DMABSTRACTAUTO, 1 ); // Disable Autoexec.
		do
		{
			r = MCF.ReadReg32( dev, DMABSTRACTCS, &rrv );
			if( r ) return r;
		}
		while( rrv & (1<<12) );
		first = 1;
		iss->statetag = STTAG( "RDSQ" );
		iss->currentstateval = address_to_read;

		do
		{
			r = MCF.ReadReg32( dev, DMABSTRACTCS, &rrv );
			if( r ) return r;
		}
		while( rrv & (1<<12) );
	}

	iss->currentstateval += 4;

	if( (rrv >> 8 ) & 7 )
	{
		fprintf( stderr, "Fault reading memory (DMABSTRACTS = %08x)\n", rrv );
	}
	return MCF.ReadReg32( dev, DMDATA0, data );
}

int DefaultReadBinaryBlob( void * dev, uint32_t address_to_read_from, uint32_t read_size, uint8_t * blob )
{
	struct InternalState * iss = (struct InternalState*)(((struct ProgrammerStructBase*)dev)->internal);

	MCF.WriteReg32( dev, DMCONTROL, 0x80000001 ); // Make the debug module work properly.
	MCF.WriteReg32( dev, DMCONTROL, 0x80000001 ); // Initiate a halt request.
	MCF.WriteReg32( dev, DMCONTROL, 0x00000001 ); // Clear Halt Request.
	
	
}

void TestFunction(void * dev )
{
	struct InternalState * iss = (struct InternalState*)(((struct ProgrammerStructBase*)dev)->internal);

	uint32_t rv;
	int r;
	MCF.WriteReg32( dev, DMCONTROL, 0x80000001 ); // Make the debug module work properly.
	MCF.WriteReg32( dev, DMCONTROL, 0x80000001 ); // Initiate a halt request.
	MCF.WriteReg32( dev, DMCONTROL, 0x00000001 ); // Clear Halt Request.


	r = MCF.WriteWord( dev, 0x20000100, 0xdeadbeef );
	printf( "%d\n", r );
	r = MCF.WriteWord( dev, 0x20000104, 0xcafed0de );
	printf( "%d\n", r );
	r = MCF.WriteWord( dev, 0x20000108, 0x12345678 );
	printf( "%d\n", r );
	r = MCF.WriteWord( dev, 0x20000108, 0x00b00d00 );
	printf( "%d\n", r );
	r = MCF.WriteWord( dev, 0x20000104, 0x33334444 );
	printf( "%d\n", r );

	r = MCF.ReadWord( dev, 0x20000100, &rv );
	printf( "%d %08x\n", r, rv );
	r = MCF.ReadWord( dev, 0x20000104, &rv );
	printf( "%d %08x\n", r, rv );
	r = MCF.ReadWord( dev, 0x20000108, &rv );
	printf( "%d %08x\n", r, rv );

	
	MCF.WriteBinaryBlob( dev, 0x08000300, 16, "hello, world!!\n" );
	r = MCF.ReadWord( dev, 0x00000300, &rv );
	printf( "%d %08x\n", r, rv );
	r = MCF.ReadWord( dev, 0x00000304, &rv );
	printf( "%d %08x\n", r, rv );
	r = MCF.ReadWord( dev, 0x00000308, &rv );
	printf( "%d %08x\n", r, rv );

}



int SetupAutomaticHighLevelFunctions( void * dev )
{
	// Will populate high-level functions from low-level functions.
	if( MCF.WriteReg32 == 0 || MCF.ReadReg32 == 0 ) return -5;

	// Else, TODO: Build the high level functions from low level functions.
	// If a high-level function alrady exists, don't override.
	
	if( !MCF.SetupInterface )
		MCF.SetupInterface = DefaultSetupInterface;
	if( !MCF.WriteBinaryBlob )
		MCF.WriteBinaryBlob = DefaultWriteBinaryBlob;
	if( !MCF.ReadBinaryBlob )
		MCF.ReadBinaryBlob = DefaultReadBinaryBlob;
	if( !MCF.WriteWord )
		MCF.WriteWord = DefaultWriteWord;
	if( !MCF.ReadWord )
		MCF.ReadWord = DefaultReadWord;

	struct InternalState * iss = malloc( sizeof( struct InternalState ) );
	iss->statetag = 0;
	iss->currentstateval = 0;

	((struct ProgrammerStructBase*)dev)->internal = iss;
}


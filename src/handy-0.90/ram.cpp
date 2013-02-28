//
// Copyright (c) 2004 K. Wilkins
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//

//////////////////////////////////////////////////////////////////////////////
//                       Handy - An Atari Lynx Emulator                     //
//                          Copyright (c) 1996,1997                         //
//                                 K. Wilkins                               //
//////////////////////////////////////////////////////////////////////////////
// RAM emulation class                                                      //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This class emulates the system RAM (64KB), the interface is pretty       //
// simple: constructor, reset, peek, poke.                                  //
//                                                                          //
//    K. Wilkins                                                            //
// August 1997                                                              //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
// Revision History:                                                        //
// -----------------                                                        //
//                                                                          //
// 01Aug1997 KW Document header added & class documented.                   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////


#define RAM_CPP

//#include <crtdbg.h>
//#define   TRACE_RAM

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "system.h"
#include "ram.h"

CRam::CRam(UBYTE *filememory,ULONG filesize)
{
	HOME_HEADER	header;

	// Take a copy into the backup buffer for restore on reset
	mFileSize=filesize;

	if(filesize)
	{
		// Take a copy of the ram data
		mFileData = new UBYTE[mFileSize];
		memcpy(mFileData,filememory,mFileSize);

		// Sanity checks on the header
		memcpy(&header,mFileData,sizeof(HOME_HEADER));

		if(header.magic[0]!='B' || header.magic[1]!='S' || header.magic[2]!='9' || header.magic[3]!='3')
		{
			CLynxException lynxerr;
			lynxerr.Message() << "Handy Error: File format invalid (Magic No)";
			lynxerr.Description()
				<< "The image you selected was not a recognised homebrew format." << endl
				<< "(see the Handy User Guide for more information).";
			throw(lynxerr);
		}
	}
	else
	{
		filememory=NULL;
	}
	// Reset will cause the loadup

	Reset();
}

CRam::~CRam()
{
	if(mFileSize)
	{
		delete[] mFileData;
		mFileData=NULL;
	}
}

void CRam::Reset(void)
{
	for(int loop=0;loop<RAM_SIZE;loop++)
		mRamData[loop]=DEFAULT_RAM_CONTENTS;

	// Open up the file

	if(mFileSize)
	{
		HOME_HEADER	header;
		UBYTE tmp;

		// Zero the RAM
		for(int loop=0;loop<RAM_SIZE;loop++) mRamData[loop]=0x00;

		// Reverse the bytes in the header words
		memcpy(&header,mFileData,sizeof(HOME_HEADER));
		tmp=(header.load_address&0xff00)>>8;
		header.load_address=(header.load_address<<8)+tmp;
		tmp=(header.size&0xff00)>>8;
		header.size=(header.size<<8)+tmp;

		// Now we can safely read/manipulate the data
		header.load_address-=10;

		memcpy(mRamData+header.load_address,mFileData,header.size);
		gCPUBootAddress=header.load_address;
	}
}

bool CRam::ContextSave(FILE *fp)
{	
	if(!fprintf(fp,"CRam::ContextSave")) return 0;
	if(!fwrite(mRamData,sizeof(UBYTE),RAM_SIZE,fp)) return 0;
	return 1;
}

bool CRam::ContextLoad(LSS_FILE *fp)
{
	char teststr[100]="XXXXXXXXXXXXXXXXX";
	if(!lss_read(teststr,sizeof(char),17,fp)) return 0;
	if(strcmp(teststr,"CRam::ContextSave")!=0) return 0;

	if(!lss_read(mRamData,sizeof(UBYTE),RAM_SIZE,fp)) return 0;
	mFileSize=0;
	return 1;
}

//END OF FILE
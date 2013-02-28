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
// Generic error handler class                                              //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This class provides error handler facilities for the Lynx emulator, I    //
// shamelessly lifted most of the code from Stella by Brad Mott.            //
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

#ifndef ERROR_H
#define ERROR_H

#include <sstream>
#include <iosfwd>
using namespace std;

#define MAX_ERROR_MSG	512
#define MAX_ERROR_DESC	2048

//class CLynxException : public CException
class CLynxException
{
	public:
		// Constructor
		CLynxException() {}
 
		// Copy Constructor
		CLynxException(CLynxException& err)
		{
			mMsg.str("");
			mMsg << err.Message().str ();
			mDesc.str("");
			mDesc << err.Description().str ();
		}
 
		// Destructor
		virtual ~CLynxException()
		{
		}

  public:
		// Answer stream which should contain the one line error message
		std::stringstream& Message() { return mMsgStream; }

		// Answer stream which should contain the multiple line description
		std::stringstream& Description() { return mDescStream; }


  private:
		// Contains the one line error code message
		std::stringstream mMsgStream;

		// Contains a multiple line description of the error and ways to 
		// solve the problem
		std::stringstream mDescStream;

  public:
		// strings to hold the data after its been thrown

		std::stringstream mMsg;
		std::stringstream mDesc;
};
#endif



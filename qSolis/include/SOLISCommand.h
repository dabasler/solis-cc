#ifndef SOLISCOMMAND_H
#define SOLISCOMMAND_H

//##########################################################################
//#                                                                        #
//#                      CLOUDCOMPARE PLUGIN                               #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: CloudCompare project                               #
//#                                                                        #
//##########################################################################

#include "ccCommandLineInterface.h"

class ccProgressDialog;
class ccMainAppInterface;

//qCC_db
#include <ccHObject.h>

class SOLISCommand : public ccCommandLineInterface::Command
{
public:
	SOLISCommand();

	~SOLISCommand() override = default;

	static bool Process(	const ccHObject::Container& candidates,
							const std::vector<CCVector3>& rays,
							const std::vector<double>& irradiance,
							bool modeDirect,
							double conversion,
							bool meshIsClosed,
							unsigned resolution,
							ccProgressDialog* progressDlg = nullptr,
							ccMainAppInterface* app = nullptr);

	bool process(ccCommandLineInterface& cmd) override;
};

#endif

//##########################################################################
//#                                                                        #
//#                       CLOUDCOMPARE PLUGIN: qSOLIS                      #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                  COPYRIGHT: David Basler                               #
//#                                                                        #
//##########################################################################

#ifndef CC_SOLIS_DLG_HEADER
#define CC_SOLIS_DLG_HEADER

#include <ui_solisDlg.h>

//! Dialog for the SOLIS plugin
class ccSolisDlg : public QDialog, public Ui::SOLISDialog
{
public:
	explicit ccSolisDlg(QWidget* parent = nullptr);
};

#endif

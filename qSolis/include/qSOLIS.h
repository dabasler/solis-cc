//##########################################################################
//#                                                                        #
//#                       CLOUDCOMPARE PLUGIN: qSOLIS                      #
//#  The SOLIS pplugin is built upon the qPCV plugin for CC  modified by   #
//#                         David Basler 2020                              #
//#                The modified code is released under                     #
//#               the same conditions as the oroginal source:              #
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
//#                  COPYRIGHT: Daniel Girardeau-Montaut (qPCV)            #
//#                  with modifications by David Basler  (qSOLIS)          #
//##########################################################################

#ifndef Q_SOLIS_PLUGIN_HEADER
#define Q_SOLIS_PLUGIN_HEADER

#include "ccStdPluginInterface.h"
#include "SOLISCommand.h"

//! Wrapper to the ShadeVis algorithm for computing Ambient Occlusion on meshes and point clouds
/** "Visibility based methods and assessment for detail-recovery", M. Tarini, P. Cignoni, R. Scopigno
	Proc. of Visualization 2003, October 19-24, Seattle, USA.
	http://vcg.sourceforge.net/index.php/ShadeVis
	
	modified to calculate solar Irradiance
**/
class qSOLIS : public QObject, public ccStdPluginInterface
{
	Q_OBJECT
	Q_INTERFACES( ccPluginInterface ccStdPluginInterface )
	
	Q_PLUGIN_METADATA( IID "cccorp.cloudcompare.plugin.qSOLIS" FILE "../info.json" )

public:
	//! Default constructor
	explicit qSOLIS(QObject* parent = nullptr);
	
	~qSOLIS()override  = default;

	//inherited from ccStdPluginInterface
	void onNewSelection(const ccHObject::Container& selectedEntities) override;
	QList<QAction *> getActions() override;
	void registerCommands(ccCommandLineInterface *cmd) override;

private:
	//! Slot called when associated ation is triggered
	void doAction();

	//! Associated action
	QAction* m_action;
};

#endif

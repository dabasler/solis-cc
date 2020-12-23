// GUI INTERFACE

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
//#                  COPYRIGHT: Daniel Girardeau-Montaut  (PCV code)       #
//#                   with modifications by  David Basler (SOLIS)          #
//##########################################################################

#include "qSOLIS.h"
#include "ccSolisDlg.h"
#include "SOLISCommand.h"

//CCCoreLib
#include <SOLIS.h>
#include <ScalarField.h>

//qCC_db
#include <ccGenericMesh.h>
#include <ccGenericPointCloud.h>
#include <ccHObjectCaster.h>
#include <ccPointCloud.h>
#include <ccProgressDialog.h>
#include <ccScalarField.h>

//Qt
#include <QMainWindow>
#include <QProgressBar>

#include <math.h>
extern "C" {
    #include <solrad.h>
}

//persistent settings during a single session
static bool s_firstLaunch				= true;
static int  s_doySpinBoxValue           = 1;
static int  s_timeEditHour              = 0;
static int  s_timeEditMinute            = 0;
static double  s_integrationSpinBoxValue= 24;
static double s_timestepSpinBoxValue	= 1.0;
static double s_latSpinBoxValue			= 45.0;
static double s_lonSpinBoxValue			= 10.0;
static int s_raysSpinBoxValue			= 256;
static int s_resSpinBoxValue			= 1024;
static bool s_closedMeshCheckBoxState	= false;
static bool s_integrateCheckBoxState	= true;


qSOLIS::qSOLIS(QObject* parent/*=0*/)
	: QObject(parent)
	, ccStdPluginInterface(":/CC/plugin/qSOLIS/info.json")
	, m_action(nullptr)
{
}

void qSOLIS::onNewSelection(const ccHObject::Container& selectedEntities)
{
	if (m_action)
	{
		bool elligibleEntitiies = false;
		for (ccHObject* obj : selectedEntities)
		{
			if (obj && (obj->isKindOf(CC_TYPES::POINT_CLOUD) || obj->isKindOf(CC_TYPES::MESH)))
			{
				elligibleEntitiies = true;
				break;
			}
		}
		m_action->setEnabled(elligibleEntitiies);
	}
}

QList<QAction *> qSOLIS::getActions()
{
	//default action
	if (!m_action)
	{
		m_action = new QAction(getName(),this);
		m_action->setToolTip(getDescription());
		m_action->setIcon(getIcon());

		connect(m_action, &QAction::triggered, this, &qSOLIS::doAction);
	}

	return QList<QAction *>{ m_action };
}

void qSOLIS::doAction()
{
	assert(m_app);
	if (!m_app)
		return;

	const ccHObject::Container& selectedEntities = m_app->getSelectedEntities();

	ccHObject::Container candidates;
	bool hasMeshes = false;
	for (ccHObject* obj : selectedEntities)
	{
		if (!obj)
		{
			assert(false);
			continue;
		}
		
		if (obj->isA(CC_TYPES::POINT_CLOUD))
		{
			//we need a real point cloud
			candidates.push_back(obj);
		}
		else if (obj->isKindOf(CC_TYPES::MESH))
		{
			ccGenericMesh* mesh = ccHObjectCaster::ToGenericMesh(obj);
			if (mesh->getAssociatedCloud() && mesh->getAssociatedCloud()->isA(CC_TYPES::POINT_CLOUD))
			{
				//we need a mesh with a real point cloud
				candidates.push_back(obj);
				hasMeshes = true;
			}
		}
	}

	ccSolisDlg dlg(m_app->getMainWindow());

	//restore previous dialog state
	if (!s_firstLaunch)
	{
	dlg.doySpinBox->setValue(s_doySpinBoxValue);
	QTime s_time(s_timeEditHour, s_timeEditMinute, 0);
	dlg.timeEdit->setTime(s_time);
	dlg.integrationDoubleSpinBox->setValue(s_integrationSpinBoxValue);
	dlg.integrationDoubleSpinBox->setValue(s_integrationSpinBoxValue);
    dlg.timestepDoubleSpinBox->setValue(s_timestepSpinBoxValue);
	dlg.integrateCheckBox->setChecked(s_integrateCheckBoxState);
	dlg.latDoubleSpinBox->setValue(s_latSpinBoxValue);
	dlg.lonDoubleSpinBox->setValue(s_lonSpinBoxValue);	
	dlg.raysSpinBox->setValue(s_raysSpinBoxValue);
	dlg.resSpinBox->setValue(s_resSpinBoxValue);
	dlg.closedMeshCheckBox->setChecked(s_closedMeshCheckBoxState);
	}

	dlg.closedMeshCheckBox->setEnabled(hasMeshes); //for meshes only

	if (!dlg.exec())
	{
		return;
	}

	//save dialog state
	
	s_firstLaunch				= false;
	s_doySpinBoxValue           = dlg.doySpinBox->value();
	s_timeEditHour              = dlg.timeEdit->time().hour();
	s_timeEditMinute            = dlg.timeEdit->time().minute();
	s_integrationSpinBoxValue   = dlg.integrationDoubleSpinBox->value();
    s_timestepSpinBoxValue      = dlg.timestepDoubleSpinBox->value();
	s_latSpinBoxValue           = dlg.latDoubleSpinBox->value();
	s_lonSpinBoxValue           = dlg.lonDoubleSpinBox->value();	
	s_raysSpinBoxValue			= dlg.raysSpinBox->value();
	s_resSpinBoxValue			= dlg.resSpinBox->value();
	s_closedMeshCheckBoxState	= dlg.closedMeshCheckBox->isChecked();
	s_integrateCheckBoxState    = dlg.integrateCheckBox->isChecked();

    unsigned doyField    = dlg.doySpinBox->value();
	unsigned hour        = dlg.timeEdit->time().hour();
	unsigned minute      = dlg.timeEdit->time().minute();
	double integration   = dlg.integrationDoubleSpinBox->value();
    double timestep      = dlg.timestepDoubleSpinBox->value();
	double lat           = dlg.latDoubleSpinBox->value();
	double lon           = dlg.lonDoubleSpinBox->value();	
	double elevation     = dlg.elevationSpinBox->value();	
	unsigned rayCount    = dlg.raysSpinBox->value();
	unsigned resolution  = dlg.resSpinBox->value();
	bool meshIsClosed    = (hasMeshes ? dlg.closedMeshCheckBox->isChecked() : false);
	
	double doyFrom       = doyField + (1.0 * hour/24) + (1.0*minute)/60/24;
	
	bool modeDirect;
	double conversion;
	//char buf[100];
	
	
	//PCV type ShadeVis & Irradiance
	std::vector<CCVector3> rays;
	std::vector<double>  irradiance;
	
	if (! dlg.integrateCheckBox->isChecked()) {
		integration   = 0.02;
		timestep      = 1;
	}
	
	conversion = timestep/60/integration;
	
	if (dlg.calcDirectCheckBox->isChecked())
	{
		// Generate direct sunrays & irradiance
		if (!SOLIS::GenerateSunRays(doyFrom,doyFrom+integration/24.0,timestep,lat,lon,elevation,rays, irradiance))
		{
			m_app->dispToConsole("Failed to generate the set of sun rays", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}
		m_app->dispToConsole("Direct Illumination", ccMainAppInterface::WRN_CONSOLE_MESSAGE);		
		modeDirect=true;
		
		/*	
			char buf[100];
			sprintf(buf,"len: %i",irradiance.size());
			for (int i=0;i<irradiance.size();i++){
				sprintf(buf,"[%i] %f",i,irradiance[i]);
				m_app->dispToConsole(buf, ccMainAppInterface::WRN_CONSOLE_MESSAGE);
			}
		*/		
		if (rays.empty())
		{
			m_app->dispToConsole("No ray was generated. Sun always below horizon in selected timerange", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
			return;
		}

		ccProgressDialog pcvProgressCb(true, m_app->getMainWindow());
		pcvProgressCb.setAutoClose(false);
		SOLISCommand::Process(candidates, rays,  irradiance, modeDirect, conversion , meshIsClosed, resolution, &pcvProgressCb, m_app);
		pcvProgressCb.close();
	}

	if (dlg.calcDiffCheckBox->isChecked())
	{
        // SOLISThe mormal PVC::GenereateRays in hemishperical mode is used for the diffusive component of solar radiation
		//generates light directions
		if (!SOLIS::GenerateDiffRays(rayCount, rays))
		{
			m_app->dispToConsole("Failed to generate the set of rays", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}
		irradiance.resize(1);
		irradiance[0] = SOLIS::totalDiffIrradiance (doyFrom,doyFrom+integration/24.0,timestep,lat,lon,elevation);
		m_app->dispToConsole("Diffuse Illumination", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
		modeDirect=false;
		
		
		if (rays.empty())
		{
			m_app->dispToConsole("No ray was generated. Sun always below horizon in selected timerange", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
			return;
		}

		ccProgressDialog pcvProgressCb(true, m_app->getMainWindow());
		pcvProgressCb.setAutoClose(false);

		SOLISCommand::Process(candidates, rays,  irradiance, modeDirect, conversion, meshIsClosed, resolution, &pcvProgressCb, m_app);
		pcvProgressCb.close();
	}

	//currently selected entities parameters may have changed!
	m_app->updateUI();
	//currently selected entities appearance may have changed!
	m_app->refreshAll();
}

void qSOLIS::registerCommands(ccCommandLineInterface* cmd)
{
	cmd->registerCommand(ccCommandLineInterface::Command::Shared(new SOLISCommand));
}
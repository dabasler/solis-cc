// COMAND LINE INTERFACE

#include "SOLISCommand.h"
#include "SOLIS.h"
#include "qSOLIS.h"

//qCC_db
#include <ccColorScalesManager.h>
#include <ccGenericMesh.h>
#include <ccHObjectCaster.h>
#include <ccPointCloud.h>
#include <ccProgressDialog.h>
#include <ccScalarField.h>

constexpr char CC_SOLIS_FIELD_LABEL_NAME_DIRECT[]  = "direct_Irradiance";
constexpr char CC_SOLIS_FIELD_LABEL_NAME_DIFFUSE[] = "diffuse_Irradiance";

constexpr char COMMAND_SOLIS[]        = "SOLIS";
constexpr char COMMAND_SOLIS_TYPE[]   = "TYPE";

constexpr char COMMAND_SOLIS_LAT[]    = "LAT";
constexpr char COMMAND_SOLIS_LON[]    = "LON";
constexpr char COMMAND_SOLIS_ELV[]    = "ELV";

constexpr char COMMAND_SOLIS_DOY[]    = "DOY";
constexpr char COMMAND_SOLIS_INT[]    = "INT";
constexpr char COMMAND_SOLIS_TS[]     = "TS";

constexpr char COMMAND_SOLIS_N_RAYS[] = "NRAYS";
constexpr char COMMAND_SOLIS_IS_CLOSED[] = "IS_CLOSED";
constexpr char COMMAND_SOLIS_RESOLUTION[] = "RESOLUTION";

#define SOLIS_DIRECT 0
#define SOLIS_DIFFUSE 1
#define SOLIS_BOTH 2

SOLISCommand::SOLISCommand()
	: Command("SOLIS", COMMAND_SOLIS)
{
}

bool SOLISCommand::Process(	const ccHObject::Container& candidates,
							const std::vector<CCVector3>& rays,
							const std::vector<double>& irradiance,
							bool modeDirect,
							double conversion,
							bool meshIsClosed,
							unsigned resolution,
							ccProgressDialog* progressDlg/*=nullptr*/,
							ccMainAppInterface* app/*=nullptr*/)
{
	size_t count = 0;
	size_t errorCount = 0;

	for (ccHObject* obj : candidates)
	{
		ccPointCloud* cloud = nullptr;
		ccGenericMesh* mesh = nullptr;
		QString objName("unknown");

		assert(obj);
		if (obj->isA(CC_TYPES::POINT_CLOUD))
		{
			//we need a real point cloud
			cloud = ccHObjectCaster::ToPointCloud(obj);
			objName = cloud->getName();
		}
		else if (obj->isKindOf(CC_TYPES::MESH))
		{
			mesh = ccHObjectCaster::ToGenericMesh(obj);
			cloud = ccHObjectCaster::ToPointCloud(mesh->getAssociatedCloud());
			objName = mesh->getName();
		}

		if (cloud == nullptr)
		{
			assert(false);
			if (app)
				app->dispToConsole(QObject::tr("Invalid object type"), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			++errorCount;
			continue;
		}
		
		int sfIdx;
		if (modeDirect){
			//we get the SOLIS field if it already exists
			sfIdx = cloud->getScalarFieldIndexByName(CC_SOLIS_FIELD_LABEL_NAME_DIRECT);
			//otherwise we create it
			if (sfIdx < 0)
			{
				sfIdx = cloud->addScalarField(CC_SOLIS_FIELD_LABEL_NAME_DIRECT);
			}
		} else {
			//we get the SOLIS field if it already exists
			sfIdx = cloud->getScalarFieldIndexByName(CC_SOLIS_FIELD_LABEL_NAME_DIFFUSE);
			//otherwise we create it
			if (sfIdx < 0)
			{
				sfIdx = cloud->addScalarField(CC_SOLIS_FIELD_LABEL_NAME_DIFFUSE);
			}
		}
		
		if (sfIdx < 0)
		{
			if (app)
				app->dispToConsole("Couldn't allocate a new scalar field for computing SOLIS field! Try to free some memory...", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return false;
		}
		cloud->setCurrentScalarField(sfIdx);

		QString objNameForPorgressDialog = objName;
		if (candidates.size() > 1)
		{
			objNameForPorgressDialog += QStringLiteral("(%1/%2)").arg(++count).arg(candidates.size());
		}

		bool wasEnabled = obj->isEnabled();
		bool wasVisible = obj->isVisible();
		obj->setEnabled(true);
		obj->setVisible(true);
		
		bool success = SOLIS::Launch(rays,irradiance, modeDirect ,conversion ,cloud, mesh, meshIsClosed, resolution, resolution, progressDlg, objNameForPorgressDialog);

		obj->setEnabled(wasEnabled);
		obj->setVisible(wasVisible);

		if (!success)
		{
			cloud->deleteScalarField(sfIdx);
			if (app)
				app->dispToConsole(QObject::tr("An error occurred during entity '%1' illumination!").arg(objName), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			++errorCount;
		}
		else
		{
			ccScalarField* sf = static_cast<ccScalarField*>(cloud->getScalarField(sfIdx));
			if (sf)
			{
				sf->computeMinAndMax();
				cloud->setCurrentDisplayedScalarField(sfIdx);
				sf->setColorScale(ccColorScalesManager::GetDefaultScale(ccColorScalesManager::GREY));
				if (obj->hasNormals() && obj->normalsShown())
				{
					if (app)
						app->dispToConsole(QObject::tr("Entity '%1' normals have been automatically disabled").arg(objName), ccMainAppInterface::WRN_CONSOLE_MESSAGE);
				}
				obj->showNormals(false);
				obj->showSF(true);
				if (obj != cloud)
				{
					cloud->showSF(true);
				}
				obj->prepareDisplayForRefresh_recursive();
			}
			else
			{
				assert(false);
			}
		}

		if (progressDlg && progressDlg->wasCanceled())
		{
			if (app)
				app->dispToConsole(QObject::tr("Process has been cancelled by the user"), ccMainAppInterface::WRN_CONSOLE_MESSAGE);
			++errorCount;
			break;
		}
	}

	return (errorCount == 0);
}
// COMMANDLINE COMMANDS
bool SOLISCommand::process(ccCommandLineInterface& cmd)
{
	cmd.print("[SOLIS]");

	if (cmd.meshes().empty() && cmd.clouds().empty())
	{
		return cmd.error(qSOLIS::tr("No entity is loaded."));
	}

	// Initialize to match SOLIS::Launch defaults
	double latitude=45;
	double longitude=0;
	int elevation=0;
	
	double doyFrom = 172.0; //Summer equinox
	double timestep=1;
	double integration=24;
		
	unsigned rayCount = 256;
	bool meshIsClosed = false;
	unsigned resolution = 1024;
	
	unsigned mode=SOLIS_BOTH;
	double conversion;
	bool modeDirect;
	char buf[100];
	
	while (!cmd.arguments().empty())
	{
		const QString& arg = cmd.arguments().front();
		
		if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_IS_CLOSED))
		{
			cmd.arguments().pop_front();
			meshIsClosed = true;
		}
		
		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_TYPE))
		{
			cmd.arguments().pop_front();
			QString rtype = cmd.arguments().takeFirst().toUpper();
			if (!QString::compare(rtype,"DIRECT"))  mode = SOLIS_DIRECT;
			else if (!QString::compare(rtype,"DIFFUSE"))  mode = SOLIS_DIFFUSE;
			else if (!QString::compare(rtype,"ALL")) mode = SOLIS_BOTH;
			else {
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_TYPE));
			}
		}
		
		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_DOY))
		{
			cmd.arguments().pop_front();
			bool conversionOk = false;
			doyFrom = cmd.arguments().takeFirst().toDouble(&conversionOk);
			if (!conversionOk)
			{
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_DOY));
			}
		}
		
		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_TS))
		{
			cmd.arguments().pop_front();
			bool conversionOk = false;
			timestep = cmd.arguments().takeFirst().toDouble(&conversionOk);
			if (!conversionOk)
			{
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_TS));
			}
		}

		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_INT))
		{
			cmd.arguments().pop_front();
			bool conversionOk = false;
			integration = cmd.arguments().takeFirst().toDouble(&conversionOk);
			if (!conversionOk)
			{
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_INT));
			}
		}
		
		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_LAT))
		{
			cmd.arguments().pop_front();
			bool conversionOk = false;
			latitude = cmd.arguments().takeFirst().toDouble(&conversionOk);
			if (!conversionOk)
			{
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_LAT));
			}
		}

		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_LON))
		{
			cmd.arguments().pop_front();
			bool conversionOk = false;
			longitude = cmd.arguments().takeFirst().toDouble(&conversionOk);
			if (!conversionOk)
			{
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_LON));
			}
		}
		
		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_ELV))
		{
			cmd.arguments().pop_front();
			bool conversionOk = false;
			elevation = cmd.arguments().takeFirst().toInt(&conversionOk);
			if (!conversionOk)
			{
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_ELV));
			}
		}
		
		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_N_RAYS))
		{
			cmd.arguments().pop_front();
			bool conversionOk = false;
			rayCount = cmd.arguments().takeFirst().toUInt(&conversionOk);
			if (!conversionOk)
			{
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_N_RAYS));
			}
		}
		
		else if (ccCommandLineInterface::IsCommand(arg, COMMAND_SOLIS_RESOLUTION))
		{
			cmd.arguments().pop_front();
			bool conversionOk = false;
			resolution = cmd.arguments().takeFirst().toUInt(&conversionOk);
			if (!conversionOk)
			{
				return cmd.error(QObject::tr("Invalid parameter: value after \"-%1\"").arg(COMMAND_SOLIS_RESOLUTION));
			}
		}
		else
		{
			cmd.warning(arg);
			break;
		}
	}

	if (integration<0) { // Single direct ray
		integration=0.02;
		timestep=1.0;
	}
	
	conversion = timestep/60/integration;
	
	//generates light directions
	std::vector<CCVector3> rays;
	std::vector<double> irradiance;

	


	if (mode==SOLIS_DIRECT || mode ==SOLIS_BOTH)
	{
		sprintf(buf, "Direct irradiance: LAT %0.3f LON %0.3f ELV %i DOY %0.3f INT %0.3f TS %0.3f",latitude,longitude,elevation,doyFrom, integration, timestep);
		cmd.warning(buf);
		// Generate direct sunrays & irradiance
		if (!SOLIS::GenerateSunRays(doyFrom,doyFrom+integration/24.0,timestep,latitude,longitude,elevation, rays, irradiance))
		{
			return cmd.error(QObject::tr("Failed to generate the set of rays"));
		}
		if (rays.empty())
		{
			return cmd.error(QObject::tr("No ray was generated. Sun always below horizon in selected timerange"));
		}

		ccHObject::Container candidates;
		try
		{
			candidates.reserve(cmd.clouds().size() + cmd.meshes().size());
		}
		catch (const std::bad_alloc)
		{
			return cmd.error(QObject::tr("Not enough memory"));
		}

		for (CLCloudDesc& desc : cmd.clouds())
			candidates.push_back(desc.pc);
		for (CLMeshDesc& desc : cmd.meshes())
			candidates.push_back(desc.mesh);
		
		modeDirect=true;
		ccProgressDialog pcvProgressCb(true);
		pcvProgressCb.setAutoClose(false);
		if (!SOLISCommand::Process(candidates, rays,  irradiance, modeDirect, conversion, meshIsClosed, resolution, &pcvProgressCb, nullptr))
		{
			return cmd.error(QObject::tr("Process failed"));
		}
		pcvProgressCb.close();
		// Save output
		for (CLCloudDesc& desc : cmd.clouds())
		{
			desc.basename += QString("_SOLISDIR");
			//save output
			if (cmd.autoSaveMode())
			{
				QString errorStr = cmd.exportEntity(desc);
				if (!errorStr.isEmpty())
				{
					return cmd.error(errorStr);
				}
			}
		}

		for (CLMeshDesc& desc : cmd.meshes())
		{
			desc.basename += QString("_SOLISDIR");
			//save output
			if (cmd.autoSaveMode())
			{
				QString errorStr = cmd.exportEntity(desc);
				if (!errorStr.isEmpty())
				{
					return cmd.error(errorStr);
				}
			}
		}		
	}
    // END DIRECT

	if (mode==SOLIS_DIFFUSE || mode == SOLIS_BOTH)
	{
		sprintf(buf, "Diffuse irradiance: LAT %0.3f LON %0.3f ELV %i DOY %0.3f INT %0.3f TS %0.3f",latitude,longitude,elevation,doyFrom, integration, timestep);
		cmd.warning(buf);

		if (!SOLIS::GenerateDiffRays(rayCount, rays))
		{
			return cmd.error(QObject::tr("Failed to generate the set of rays"));
		}
		irradiance.resize(1);
		irradiance[0] = SOLIS::totalDiffIrradiance (doyFrom,doyFrom+integration/24.0,timestep,latitude,longitude,elevation);
		modeDirect=false;
		
		
		if (rays.empty())
		{
			return cmd.error(QObject::tr("No ray was generated. Sun always below horizon in selected timerange"));
		}
		
		ccHObject::Container candidates;
		try
		{
			candidates.reserve(cmd.clouds().size() + cmd.meshes().size());
		}
		catch (const std::bad_alloc)
		{
			return cmd.error(QObject::tr("Not enough memory"));
		}

		for (CLCloudDesc& desc : cmd.clouds())
			candidates.push_back(desc.pc);
		for (CLMeshDesc& desc : cmd.meshes())
			candidates.push_back(desc.mesh);

		ccProgressDialog pcvProgressCb(true);
		pcvProgressCb.setAutoClose(false);
		if (!SOLISCommand::Process(candidates, rays,  irradiance, modeDirect, conversion, meshIsClosed, resolution, &pcvProgressCb, nullptr))
		{
			return cmd.error(QObject::tr("Process failed"));
		}

		for (CLCloudDesc& desc : cmd.clouds())
		{
			desc.basename += QString("_SOLISDIF");
			//save output
			if (cmd.autoSaveMode())
			{
				QString errorStr = cmd.exportEntity(desc);
				if (!errorStr.isEmpty())
				{
					return cmd.error(errorStr);
				}
			}
		}

		for (CLMeshDesc& desc : cmd.meshes())
		{
			desc.basename += QString("_SOLISDIF");
			//save output
			if (cmd.autoSaveMode())
			{
				QString errorStr = cmd.exportEntity(desc);
				if (!errorStr.isEmpty())
				{
					return cmd.error(errorStr);
				}
			}
		}
		pcvProgressCb.close();
	}
	return true;
}

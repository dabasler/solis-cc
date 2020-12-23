[![CloudCompare Plugin](https://img.shields.io/badge/plugin-CloudCompare-brightgreen.svg)](https://github.com/CloudCompare/CloudCompare)

# <img src="https://github.com/dabasler/solis-cc/raw/main/qSolis/images/SOLISIcon.png"/> Solis

Solis (latin : sun) is a plugin for [CloudCompare](https://github.com/CloudCompare/CloudCompare) to calculate solar irradiance on pointcloud and meshes.
It is based on the qPCV plugin in CloudCompare and uses the a C implementation of the [solrad](https://github.com/bnasr/solrad) functions to calculate solar radiation and related variables based on location and time.

## Install Solis in CloudCompare
The plugin is not (yet) made available as compiled plugin .dll that can be added to already installed versions of CloudCompare. To compile it, 
- clone CloudCompare from [https://github.com/CloudCompare/CloudCompare](https://github.com/CloudCompare/CloudCompare)
- follow the instructions to compile CloudCompare [here](https://github.com/CloudCompare/CloudCompare/blob/master/BUILD.md)
- clone this repository in the `CloudCompare/plugins/3rdparty` directory
- re-run CloudCompare's cmake
- turn on `PLUGIN_3RDPARTY_QSOLIS` in your cmake options
- build CloudCompare

## Use Solis in CloudCompare

The plugin is available through the GUI in >Plugins 

The plugin can also be accessed through the [CloudCompare commandline](https://www.cloudcompare.org/doc/wiki/index.php?title=Command_line_mode): 

Command |	Description
------------ | -------------
`-SOLIS`     | *Runs the SOLIS plugin* <br /> Optional settings are:<br /> `-TYPE`  [value]: one of 'DIRECT' 'DIFFUSE' 'ALL' <br /> `-LAT`  [value]: latitude (degree N)<br />`-LON`  [value]: longitude (degree E) <br /> `-ELV`  [value]: elevation (m)<br /> `-DOY`  [value]: Doy of Year (with fractional time) <br /> `-INT`  [value]: Integrate over x hours (hours). Set to -1 for single timepoint <br /> `-TS`    [value]: Timestep for calculation of sun position (minutes) <br /> `-NRAYS`    [value]: number of rays for diffuse light calculations <br /> `-TYPE`  [value]: one of 'DIRECT', 'DIFFUSE' or 'ALL' <br /> `-IS_CLOSED`: Tells SOLIS that the mesh is watertight. This accelerates processing. <br /> `-RESOLUTION` [value]: OpenGL context resolution <br /> 


[![CloudCompare Plugin](https://img.shields.io/badge/plugin-CloudCompare-brightgreen.svg)](https://github.com/CloudCompare/CloudCompare)

# <img src="https://github.com/dabasler/solis-cc/blob/master/qSolis/images/SOLISIcon.png"/> Solis

Solis (latin : sun) is a plugin for [CloudCompare](https://github.com/CloudCompare/CloudCompare) to calculate solar irradiance on pointcloud and meshes.
It is based on the qPCV plugin in CloudCompare and uses the a C implementation of the [solrad](https://github.com/bnasr/solrad) functions to calculate solar radiation and related variables based on location and time.

## Use qSolis in CloudCompare
The plugin is not yet made available as compiled plugin .dll that can be added to already installed versions of CloudCompare. To compile it, 

- clone this repository in the `CloudCompare/plugins/3rdparty` directory
- re-run CloudCompare's cmake
- turn on `PLUGIN_3RDPARTY_QSOLIS` in your cmake options
- build CloudCompare

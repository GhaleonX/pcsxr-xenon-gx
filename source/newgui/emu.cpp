/****************************************************************************
 * PCSXR Xenon
 * Based on
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2010
 * Ced2911 2013
 *
 * emu.cpp
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/
#include "config.h"
#include "r3000a.h"
#include "psxcommon.h"
#include "debug.h"
#include "sio.h"
#include "misc.h"

#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <string>
#include <vector>
#include "emu.h"

extern "C" int pcsx_run_gui;

static int emulationRunning = 0;
extern "C" int use_experimental_dr;
extern "C" void cdrcimg_set_fname(const char *fname);

char *basename(char *path)
{
    static char base[PATH_MAX];
    char *end;

    strcpy(base,".");

    if (path == NULL || *path == '\0')
        return base;

    if ((end = strrchr(path, '/')) != NULL);
    else if ((end = strrchr(path, '\\')) != NULL);
    else if ((end = strrchr(path, ':')) != NULL);
    else { strcpy(base,path); return base; }

    end++;

    if (*end != '\0')
        strcpy(base,end);

    return base;
}

static void _SetIso(const char * fname) {
	FILE *fd = fopen(fname, "rb");
	if (fd == NULL) {
		printf("Error loading %s\r\n", fname);
		return;
	}
	uint8_t header[0x10];
	int n = fread(header, 0x10, 1, fd);

	if (header[0] == 0x78 && header[1] == 0xDA) {
		printf("Use CDRCIMG for  %s\r\n", fname);
		strcpy(Config.Cdr, "CDRCIMG");
		cdrcimg_set_fname(fname);
	} else {
		SetIsoFile(fname);
	}
	SetIsoFile(fname);
	fclose(fd);
}

SEMUInterface::SEMUInterface() {
	// Default dir
	strcpy(rootdir, "sda0:/pcsxr/");
}

int SEMUInterface::Reset() {
	cpuRunning = 1;
	EmuReset();
	return 1;
}

int SEMUInterface::ConfigRequested() {
	int need_config = pcsx_run_gui;
	pcsx_run_gui = 0;
	if (need_config) {
		// disable scissor ! will be renabled by the gpu
		g_pVideoDevice->scissor_enable = 0;
	}
	return need_config;
}

int SEMUInterface::ResetRequested() {
	return -1;
}

void SEMUInterface::Step() {
	cpuRunning = 1;
	emulationRunning = 1;
	psxCpu->Execute();
}

int SEMUInterface::Start(const char * filename) {
	memset(&Config, 0, sizeof (PcsxConfig));
	
	printf("sizeof (PcsxConfig) : %d\n", sizeof(PcsxConfig));
	printf("MAXPATHLEN : %d\n", MAXPATHLEN);
	
	strcpy(Config.Net, "Disabled");
	strcpy(Config.Cdr, "CDR");
	strcpy(Config.Spu, "SPU");
	strcpy(Config.Gpu, "GPUHW");
	strcpy(Config.Pad1, "PAD1");
	strcpy(Config.Pad2, "PAD2");
	strcpy(Config.Bios, "SCPH1001.BIN"); // Use actual BIOS
	
	sprintf(Config.BiosDir, "%s/bios/", rootdir);
	sprintf(Config.PatchesDir, "%s/patches/", rootdir);
	sprintf(Config.Mcd1, "%s/memcards/card1.mcd", rootdir);
	sprintf(Config.Mcd2, "%s/memcards/card2.mcd", rootdir);

	// Config.SlowBoot = 1;
	Config.PsxAuto = 1; // autodetect system
	
	if (EMUSettings.use_interpreter) {
		Config.Cpu = CPU_INTERPRETER;
	} else {
		Config.Cpu = CPU_DYNAREC;
	}
	
	// Gpu plugin options	
	Config.UseFrameLimit = EMUSettings.framelimit;
	if (EMUSettings.use_gpu_soft_plugin) {
		strcpy(Config.Gpu, "GPUSW");
		Config.GpuFilter = EMUSettings.sw_filter;
	} else {	
		strcpy(Config.Gpu, "GPUHW");
		Config.GpuFilter = EMUSettings.hw_filter;
	}

	cpuRunning = 1;
	emulationRunning = 1;
	
	// strcpy(ROMInfo.filename, filename);
	// strcpy(ROMInfo.diplayname, basename((char*) filename));
	
	printf("Loading %s!\n", filename);
	
	_SetIso(filename);
	if (LoadPlugins() == 0) {
		if (OpenPlugins() == 0) {
			if (SysInit() == -1) {
				printf("SysInit() Error!\n");
				return -1;
			}

			SysReset();
			// Check for hle ...
			if (Config.HLE == 1) {
				printf("Can't continue ... bios not found ...\r\n");
				//exit(0);
			}

			CheckCdrom();
			LoadCdrom();

			psxCpu->Execute();
			//SysReset();
			return 1;
		}
	}
	printf("Plugin error !\n");
	return -1;
}

int SEMUInterface::Running() {
	return emulationRunning;
}

int SEMUInterface::PowerOff() {
	cpuRunning = 0;
	emulationRunning = 0;
	return emulationRunning;
}

int SEMUInterface::LoadStates(const char * filename) {
	printf("LoadStates: %s\n", filename);
	return LoadState(filename)==0;
}

int SEMUInterface::SaveStates(const char * filename) {
	printf("SaveStates: %s\n", filename);
	return SaveState(filename)==0;
}

int SEMUInterface::LoadMCD(const char * filename) {
	strcpy(Config.Mcd1, filename);
	LoadMcd(1, Config.Mcd1);

	return 0;
}

int SEMUInterface::SaveMCD(const char * filename) {
	//strcpy(Config.Mcd1, filename);
	//LoadMcd(1, filename)
	return -1;
}

void SEMUInterface::SetRootdir(const char * dirname) {
	strcpy(rootdir, dirname);
}
int SEMUInterface::ScanRootdir() {
	// parse each device and get try to get rootdir
	int found = 0;
	char path[MAXPATHLEN];
	std::vector<std::string> available_dir;
	
	// @todo parse from config !
	available_dir.push_back("/pcsxr/");
//	available_dir.push_back("/devkit/pcsxr/");
	
	for (int idev = 3; idev < STD_MAX; idev++) {
		if (devoptab_list[idev]->structSize) {
			for (int p=0; p<available_dir.size(); p++) {
				sprintf(path, "%s:%s", devoptab_list[idev]->name, available_dir[p].c_str());
				printf("look for found: %s\n", path);
				DIR * dir = opendir (path);
				if (dir) {				
					closedir(dir);
					found = 1;
					printf("use %s as rootdir\n", path);
					break;
				}
				closedir(dir);				
			}
		}
		if (found) {
			break;
		}
	}
	if (found) {
		SetRootdir(path);
	}
	return found;
}
#ifdef USE_HTTP
extern "C" void network_poll(void);
#endif
extern "C" void systemPoll() {
#ifdef USE_HTTP
	network_poll();
#endif
}

class SEMUInterface EMUInterface;
struct SROMInfo ROMInfo;


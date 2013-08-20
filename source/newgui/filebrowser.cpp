/****************************************************************************
 * PCSXR Xenon
 * Based on
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * svpe June 2007
 * crunchy2 May-July 2007
 * Michniewski 2008
 * Tantric 2008-2010
 * Ced2911 2013
 *
 * filebrowser.cpp
 *
 * Generic file routines - reading, writing, browsing
 ***************************************************************************/
#include <unistd.h>
#include <xetypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <debug.h>
#include <sys/iosupport.h>
#include "emu.h"
#include "filebrowser.h"
#include "menu.h"
#include "video.h"
#include "fileop.h"
#include "input.h"
#include "sram.h"
#include "freeze.h"

#include "w_input.h"

BROWSERINFO browser;
BROWSERENTRY * browserList = NULL; // list of files/folders in browser
DEVICES_INFO devsinfo;

char pathPrefix[STD_MAX][8];

unsigned long SNESROMSize = 0;
bool loadingFile = false;

/****************************************************************************
* autoLoadMethod()
* Auto-determines and sets the load device
* Returns device set
****************************************************************************/
int autoLoadMethod()
{
	int device  = 0;
	return device;
}

/****************************************************************************
* autoSaveMethod()
* Auto-determines and sets the save device
* Returns device set
****************************************************************************/
int autoSaveMethod(bool silent)
{
	int device  = 0;
	return device;
}

/****************************************************************************
 * ResetBrowser()
 * Clears the file browser memory, and allocates one initial entry
 ***************************************************************************/
void ResetBrowser()
{
	browser.numEntries = 0;
	browser.selIndex = 0;
	browser.pageIndex = 0;
	browser.size = 0;
}

bool AddBrowserEntry()
{
	if(browser.size >= MAX_BROWSER_SIZE)
	{
		ErrorPrompt("Out of memory: too many files!");
		return false; // out of space
	}

	memset(&(browserList[browser.size]), 0, sizeof(BROWSERENTRY)); // clear the new entry
	browser.size++;
	return true;
}

/****************************************************************************
 * CleanupPath()
 * Cleans up the filepath, removing double // and replacing \ with /
 ***************************************************************************/
static void CleanupPath(char * path)
{
	if(!path || path[0] == 0)
		return;
	
	int pathlen = strlen(path);
	int j = 0;
	for(int i=0; i < pathlen && i < MAXPATHLEN; i++)
	{
		if(path[i] == '\\')
			path[i] = '/';

		if(j == 0 || !(path[j-1] == '/' && path[i] == '/'))
			path[j++] = path[i];
	}
	path[j] = 0;
}

bool IsDeviceRoot(char * path)
{
	char * s;
	if(path == NULL || path[0] == 0)
		return false;

	s = strchr(path, ':');
	
	if (s == NULL) {
		return false;
	}
	
	if ( strcmp(s, ":/") == 0) {
		return true;
	}
	return false;
}

/****************************************************************************
 * UpdateDirName()
 * Update curent directory name for file browser
 ***************************************************************************/
int UpdateDirName()
{
	int size=0;
	char * test;
	char temp[1024];
	int device = 0;

	if(browser.numEntries == 0)
		return 1;

	FindDevice(browser.dir, &device);

	/* current directory doesn't change */
	if (strcmp(browserList[browser.selIndex].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(browserList[browser.selIndex].filename,"..") == 0)
	{
		// already at the top level
		if(IsDeviceRoot(browser.dir))
		{
			browser.dir[0] = 0; // remove device - we are going to the device listing screen
		}
		else
		{
			/* determine last subdirectory namelength */
			sprintf(temp,"%s",browser.dir);
			test = strtok(temp,"/");
			while (test != NULL)
			{
				size = strlen(test);
				test = strtok(NULL,"/");
			}
	
			/* remove last subdirectory name */
			size = strlen(browser.dir) - size - 1;
			browser.dir[size] = 0;
		}

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) < MAXPATHLEN)
		{
			/* update current directory name */
			sprintf(browser.dir, "%s%s/",browser.dir, browserList[browser.selIndex].filename);
			return 1;
		}
		else
		{
			ErrorPrompt("Directory name is too long!");
			return -1;
		}
	}
}

bool MakeFilePath(char filepath[], int type, char * filename, int filenum)
{
	char file[512];
	char folder[1024];
	char ext[4];
	char temppath[MAXPATHLEN];
	
	if (filepath == NULL) {
		return false;
	}
	
	printf("Makefile path for [filename]%s - [type]%d - [filenum]%d\n", filename, type,  filenum);

	if(type == FILE_ROM)
	{
		// Check path length
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) >= MAXPATHLEN)
		{
			ErrorPrompt("Maximum filepath length reached!");
			filepath[0] = 0;
			return false;
		}
		else
		{
			sprintf(temppath, "%s%s",browser.dir,browserList[browser.selIndex].filename);
		}
	}
	else
	{
		// char  * savename = strchr(filename, '/') + 1;
		char  * savename = filename;
		switch(type)
		{
			case FILE_SRAM:
			case FILE_SNAPSHOT:
				sprintf(folder, "%s%s", devsinfo.save.path[EMUSettings.SaveMethod], EMUSettings.SaveFolder);

				if(type == FILE_SRAM) sprintf(ext, "srm");
				else sprintf(ext, "frz");

				if(filenum >= -1)
				{
					if(filenum == -1)
						sprintf(temppath, "%s%s.%s", folder, savename, ext);
					else if(filenum == 0)
						sprintf(temppath, "%s%s Auto.%s", folder, savename, ext);
					else
						sprintf(temppath, "%s%s %i.%s", folder, savename, filenum, ext);
				}
				else
				{
					sprintf(temppath, "%s%s", folder, savename);
				}
				printf("temppath:%s\n", temppath);
				printf("folder:%s\n", folder);
				break;
			case FILE_CHEAT:
				sprintf(temppath, "%s%s.cht", folder, savename);
				break;
		}
	}
	CleanupPath(temppath); // cleanup path
	snprintf(filepath, MAXPATHLEN, "%s", temppath);
	return true;
}

/****************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
int FileSortCallback(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((BROWSERENTRY *)f1)->filename[0] == '.' || ((BROWSERENTRY *)f2)->filename[0] == '.')
	{
		if(strcmp(((BROWSERENTRY *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((BROWSERENTRY *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((BROWSERENTRY *)f1)->isdir && !(((BROWSERENTRY *)f2)->isdir)) return -1;
	if(!(((BROWSERENTRY *)f1)->isdir) && ((BROWSERENTRY *)f2)->isdir) return 1;

	return stricmp(((BROWSERENTRY *)f1)->filename, ((BROWSERENTRY *)f2)->filename);
}

/****************************************************************************
 * IsValidROM
 *
 * Checks if the specified file is a valid ROM
 * For now we will just check the file extension and file size
 * If the file is a zip, we will check the file extension / file size of the
 * first file inside
 ***************************************************************************/
static bool IsValidROM()
{
	if (strlen(browserList[browser.selIndex].filename) > 4)
	{
		char * p = strrchr(browserList[browser.selIndex].filename, '.');

		if (p != NULL)
		{
			if(p != NULL)
			{
				if (stricmp(p, ".iso") == 0 ||
					stricmp(p, ".bin") == 0 ||
					stricmp(p, ".Z") == 0 ||
					stricmp(p, ".table") == 0 ||
					stricmp(p, ".ccd") == 0 ||
					stricmp(p, ".cue") == 0)
				{
					return true;
				}
			}
		}
	}
	ErrorPrompt("Unknown file type!");
	return false;
}

/****************************************************************************
 * StripExt
 *
 * Strips an extension from a filename
 ***************************************************************************/
void StripExt(char* returnstring, char * inputstring)
{
	char* loc_dot;

	snprintf (returnstring, MAXJOLIET, "%s", inputstring);

	if(inputstring == NULL || strlen(inputstring) < 4)
		return;

	loc_dot = strrchr(returnstring,'.');
	if (loc_dot != NULL)
		*loc_dot = 0; // strip file extension
}

/****************************************************************************
 * BrowserLoadFile
 *
 * Loads the selected ROM
 ***************************************************************************/
int BrowserLoadFile()
{
	char loadingfile[2048];

	int loaded = 0;
	int device;

	if(!FindDevice(browser.dir, &device)) {
		printf("Device not found !\n");
		return 0;
	}

	// check that this is a valid ROM
	if(!IsValidROM())
		goto done;

	if (MakeFilePath(loadingfile, FILE_ROM, NULL, 0)) {	
		// load SRAM or snapshot
		if (EMUSettings.AutoLoad == 1)
			LoadSRAMAuto(SILENT);
		else if (EMUSettings.AutoLoad == 2)
			LoadSnapshotAuto(SILENT);
		
		if (EMUInterface.Start(loadingfile)) {
			ResetBrowser();
			loaded = 1;
		}
	}
	
	if (loaded == 0) {
		ErrorPrompt("Error loading game!");
	}
done:
	CancelAction();
	return loaded;
}

/****************************************************************************
 * BrowserChangeFolder
 *
 * Update current directory and set new entry list if directory has changed
 ***************************************************************************/
int BrowserChangeFolder()
{
	int device = 0;
	FindDevice(browser.dir, &device);

	if(!UpdateDirName()){
		return -1;
	}

	HaltParseThread(); // halt parsing
	CleanupPath(browser.dir);
	ResetBrowser(); // reset browser

	if(browser.dir[0] != 0){
		ParseDirectory();
	}

	if(browser.numEntries == 0)
	{
		browser.dir[0] = 0;
		int i=0;
		
		devsinfo.save.nbr = 0;
		
		for (int idev = 3; idev < STD_MAX; idev++) {
			if (devoptab_list[idev]->structSize) {
				printf("Device found: %s:/\n", devoptab_list[idev]->name);
				AddBrowserEntry();
				sprintf(browserList[i].filename, "%s:/", devoptab_list[idev]->name);
				sprintf(browserList[i].displayname, "TODO: %s", devoptab_list[idev]->name);
				browserList[i].length = 0;
				browserList[i].isdir = 1;
				if (browserList[i].filename[0] == 'd') {
					browserList[i].icon = ICON_DVD;
				} else if (browserList[i].filename[0] == 's') {
					browserList[i].icon = ICON_SD;
				} else if (browserList[i].filename[0] == 'u') {
					browserList[i].icon = ICON_USB;
				} else {
					browserList[i].icon = ICON_USB;
				}
				sprintf(pathPrefix[i], "%s:/", devoptab_list[idev]->name);
				
				// add the device to save dev list
				if (devoptab_list[idev]->write_r != NULL) {
					sprintf(devsinfo.save.path[devsinfo.save.nbr], "%s:/", devoptab_list[idev]->name);				
					devsinfo.save.nbr++;
				}				
				i++;
			}
		}
		
		browser.numEntries = i;
	}
	
	if(browser.dir[0] == 0)
	{
		//EMUSettings.LoadFolder[0] = 0;
		//EMUSettings.LoadMethod = 0;
	}
	else
	{
		char * path = StripDevice(browser.dir);
		if(path != NULL)
			strcpy(EMUSettings.SaveFolder, path);
		FindDevice(browser.dir, &EMUSettings.SaveMethod);
	}

	return browser.numEntries;
}

/****************************************************************************
 * OpenROM
 * Displays a list of ROMS on load device
 ***************************************************************************/
int
OpenGameList ()
{
	int device = EMUSettings.SaveMethod;
	
	//device = DEVICE_USB;
//
	if(device == DEVICE_AUTO && strlen(EMUSettings.SaveFolder) > 0)
		device = autoLoadMethod();

	// change current dir to roms directory
	if(device > 0)
		sprintf(browser.dir, "%s%s/", pathPrefix[device], EMUSettings.SaveFolder);
	else
		browser.dir[0] = 0;
		
	BrowserChangeFolder();
	return browser.numEntries;
}

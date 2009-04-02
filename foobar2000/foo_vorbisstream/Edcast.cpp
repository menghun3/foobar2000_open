#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include <time.h>
#include <stdarg.h>
#include <vorbis/vorbisenc.h>
#include "Encoders.h"
#include "Edcast.h"
#include "Socket.h"

#define UNICODE
#include "../SDK/foobar2000.h"
#include "../shared/shared.h"

#define FILE_SEPARATOR	"\\"
#define INT32	__int32
#define MAX_ENCODERS 10

extern advconfig_radio_factory acfg_logerror;
extern advconfig_radio_factory acfg_loginfo;
extern advconfig_radio_factory acfg_logdebug;
extern edcastGlobals			*g[MAX_ENCODERS];
extern edcastGlobals			gMain;


long	dwDataBlockPos = 0;
long	TotalWritten = 0;
int buffer_blocksize = 0;
//critical_section mutex;

typedef struct tagConfigFileValue
{
	char_t	Variable[256];
	char_t	Value[256];
	char_t	Description[1024];
} configFileValue;

static configFileValue	configFileValues[100];
static int				numConfigValues = 0;

static int				greconnectFlag = 0;

int getReconnectFlag(edcastGlobals *g) {
	return g->gAutoReconnect;
}

int getReconnectSecs(edcastGlobals *g) {
	return g->gReconnectSec;
}

void addConfigVariable(edcastGlobals *g, char_t *variable) {
	g->configVariables[g->numConfigVariables] = _strdup(variable);
	g->numConfigVariables++;
}

long getWritten(edcastGlobals *g) {
	return g->written;
}

void setWritten(edcastGlobals *g, long writ) {
	g->written = writ;
}

char_t *getServerDesc(edcastGlobals *g) {
	return g->gServDesc;
}

char_t *getSourceURL(edcastGlobals *g) {
	return g->gSourceURL;
}

void setSourceURL(edcastGlobals *g, char_t *url) {
	strcpy(g->gSourceURL, url);
}

int getIsConnected(edcastGlobals *g) {
	return g->weareconnected;
}

long getCurrentSamplerate(edcastGlobals *g) {
	return g->currentSamplerate;
}

int getCurrentBitrate(edcastGlobals *g) {
	return g->currentBitrate;
}

int getCurrentChannels(edcastGlobals *g) {
	return g->currentChannels;
}

void setSourceDescription(edcastGlobals *g, char_t *desc) {
	strcpy(g->sourceDescription, desc);
}

long getLastXWindow(edcastGlobals *g) {
	return g->lastX;
}

long getLastYWindow(edcastGlobals *g) {
	return g->lastY;
}

void setLastXWindow(edcastGlobals *g, long x) {
	g->lastX = x;
}

void setLastYWindow(edcastGlobals *g, long y) {
	g->lastY = y;
}

long getLastDummyXWindow(edcastGlobals *g) {
	return g->lastDummyX;
}

long getLastDummyYWindow(edcastGlobals *g) {
	return g->lastDummyY;
}

void setLastDummyXWindow(edcastGlobals *g, long x) {
	g->lastDummyX = x;
}

void setLastDummyYWindow(edcastGlobals *g, long y) {
	g->lastDummyY = y;
}

char_t *getLockedMetadata(edcastGlobals *g) {
	return g->gManualSongTitle;
}

void setLockedMetadata(edcastGlobals *g, char_t *buf) {
	memset(g->gManualSongTitle, '\000', sizeof(g->gManualSongTitle));
	strncpy(g->gManualSongTitle, buf, sizeof(g->gManualSongTitle) - 1);
}

int getLockedMetadataFlag(edcastGlobals *g) {
	return g->gLockSongTitle;
}

void setLockedMetadataFlag(edcastGlobals *g, int flag) {
	g->gLockSongTitle = flag;
}

int resetResampler(edcastGlobals *g) {
	if(g->initializedResampler) {
		res_clear(&(g->resampler));
	}

	g->initializedResampler = 0;
	return 1;
}

/* Gratuitously ripped from util.c */
static char_t			base64table[64] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' };

static signed char_t	base64decode[256] = { -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, 62, -2, -2, -2, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -2, -2, -2, -1, -2, -2, -2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -2, -2, -2, -2, -2, -2, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2 };

/*
 =======================================================================================================================
    This isn't efficient, but it doesn't need to be
 =======================================================================================================================
 */
char_t *util_base64_encode(char_t *data) {
	int		len = strlen(data);
	char_t	*out = (char_t *) malloc(len * 4 / 3 + 4);
	char_t	*result = out;
	int		chunk;

	while(len > 0) {
		chunk = (len > 3) ? 3 : len;
		*out++ = base64table[(*data & 0xFC) >> 2];
		*out++ = base64table[((*data & 0x03) << 4) | ((*(data + 1) & 0xF0) >> 4)];
		switch(chunk) {
			case 3:
				*out++ = base64table[((*(data + 1) & 0x0F) << 2) | ((*(data + 2) & 0xC0) >> 6)];
				*out++ = base64table[(*(data + 2)) & 0x3F];
				break;

			case 2:
				*out++ = base64table[((*(data + 1) & 0x0F) << 2)];
				*out++ = '=';
				break;

			case 1:
				*out++ = '=';
				*out++ = '=';
				break;
		}

		data += chunk;
		len -= chunk;
	}

	*out = 0;

	return result;
}

char_t *util_base64_decode(unsigned char_t *input) {
	int			len = strlen((char_t *) input);
	char_t		*out = (char_t *) malloc(len * 3 / 4 + 5);
	char_t		*result = out;
	signed char_t vals[4];

	while(len > 0) {
		if(len < 4) {
			free(result);
			return NULL;	/* Invalid Base64 data */
		}

		vals[0] = base64decode[*input++];
		vals[1] = base64decode[*input++];
		vals[2] = base64decode[*input++];
		vals[3] = base64decode[*input++];

		if(vals[0] < 0 || vals[1] < 0 || vals[2] < -1 || vals[3] < -1) {
			continue;
		}

		*out++ = vals[0] << 2 | vals[1] >> 4;
		if(vals[2] >= 0)
			*out++ = ((vals[1] & 0x0F) << 4) | (vals[2] >> 2);
		else
			*out++ = 0;

		if(vals[3] >= 0)
			*out++ = ((vals[2] & 0x03) << 6) | (vals[3]);
		else
			*out++ = 0;

		len -= 4;
	}

	*out = 0;

	return result;
}

#define HEADER_TYPE 1
#define CODEC_TYPE	2

int sendToServer(edcastGlobals *g, int sd, char_t *data, int length, int type) {
	int ret = 0;
	int sendflags = 0;

	switch(type) {
		case HEADER_TYPE:
			ret = send(sd, data, length, sendflags);
			break;

		case CODEC_TYPE:
			ret = send(sd, data, length, sendflags);
			break;
	}

	if(ret > 0) {
		if(g->writeBytesCallback) {
			g->writeBytesCallback((void *) g, (void *) ret);
		}
	}

	return ret;
}

int readConfigFile(edcastGlobals *g, int readOnly) {
	FILE	*filep;
	char_t	buffer[1024];
	char_t	configFile[1024] = "";
	char_t	defaultConfigName[] = "edcast";


	numConfigValues = 0;
	memset(&configFileValues, '\000', sizeof(configFileValues));

	if(readOnly) {
		sprintf(configFile, "%s", g->gConfigFileName);
	}
	else {
		if(strlen(g->gConfigFileName) == 0) {
			sprintf(configFile, "%s_%d.cfg", defaultConfigName, g->encoderNumber);
		}
		else {
			sprintf(configFile, "%s_%d.cfg", g->gConfigFileName, g->encoderNumber);
		}
	}

	filep = fopen(configFile, "r");
	if(filep != 0){
		while(!feof(filep)) {
			char_t	*p2;

			memset(buffer, '\000', sizeof(buffer));
			fgets(buffer, sizeof(buffer) - 1, filep);
			p2 = strchr(buffer, '\r');
			if(p2) {
				*p2 = '\000';
			}

			p2 = strchr(buffer, '\n');
			if(p2) {
				*p2 = '\000';
			}

			if(buffer[0] != '#') {
				char_t	*p1 = strchr(buffer, '=');

				if(p1) {
					strncpy(configFileValues[numConfigValues].Variable, buffer, p1 - buffer);
					p1++;	/* Get past the = */
					strcpy(configFileValues[numConfigValues].Value, p1);
					numConfigValues++;
				}
			}
		}

		if(filep) {
			fclose(filep);
		}
	}

	config_read(g);

	if(!readOnly) {
		writeConfigFile(g);
	}

	return 1;
}

int deleteConfigFile(edcastGlobals *g) {
	char_t	configFile[1024] = "";
	char_t	defaultConfigName[] = "edcast";

	if(strlen(g->gConfigFileName) == 0) {
		sprintf(configFile, "%s_%d.cfg", defaultConfigName, g->encoderNumber);
	}
	else {
		sprintf(configFile, "%s_%d.cfg", g->gConfigFileName, g->encoderNumber);
	}

	_unlink(configFile);

	return 1;
}

void setConfigFileName(edcastGlobals *g, char_t *configFile) {
	strcpy(g->gConfigFileName, configFile);
}

char_t *getConfigFileName(edcastGlobals *g) {
	return g->gConfigFileName;
}

int writeConfigFile(edcastGlobals *g) {
	char_t	configFile[1024] = "";
	char_t	defaultConfigName[] = "edcast";

	config_write(g);

	if(strlen(g->gConfigFileName) == 0) {
		sprintf(configFile, "%s_%d.cfg", defaultConfigName, g->encoderNumber);
	}
	else {
		sprintf(configFile, "%s_%d.cfg", g->gConfigFileName, g->encoderNumber);
	}

	FILE	*filep = fopen(configFile, "w");

	if(filep == 0) {
		LogMessage(g,LOG_ERROR, "Cannot open config file %s\n", configFile);
		return 0;
	}

	for(int i = 0; i < numConfigValues; i++) {
		int ok = 1;
		if (g->configVariables) {
			ok = 0;
			for (int j=0;j<g->numConfigVariables;j++) {
				if (!strcmp(g->configVariables[j], configFileValues[i].Variable)) {
					ok = 1;
					break;
				}
			}
		}

		if (ok) {
			if (strlen(configFileValues[i].Description) > 0) {
				fprintf(filep, "# %s\n", configFileValues[i].Description);
			}
			fprintf(filep, "%s=%s\n", configFileValues[i].Variable, configFileValues[i].Value);
		}
	}

	fclose(filep);

	return 1;
}

void GetConfigVariable(edcastGlobals *g, char_t *appName, char_t *paramName, char_t *defaultvalue, char_t *destValue, int destSize, char_t *desc) {
	
	if (g->configVariables) {
		int ok = 0;
		for (int j=0;j<g->numConfigVariables;j++) {
			if (!strcmp(g->configVariables[j], paramName)) {
				ok = 1;
				break;
			}
		}
		if (!ok) {
			strcpy(destValue, defaultvalue);
			return;
		}
	}
	for(int i = 0; i < numConfigValues; i++) {
		if(!strcmp(paramName, configFileValues[i].Variable)) {
			strcpy(destValue, configFileValues[i].Value);
			if (desc) {
				strcpy(configFileValues[i].Description, desc);
			}
			return;
		}
	}

	strcpy(configFileValues[numConfigValues].Variable, paramName);
	strcpy(configFileValues[numConfigValues].Value, defaultvalue);
	if (desc) {
		strcpy(configFileValues[numConfigValues].Description, desc);
	}

	strcpy(destValue, configFileValues[numConfigValues].Value);
	numConfigValues++;
	return;
}

long GetConfigVariableLong(edcastGlobals *g, char_t *appName, char_t *paramName, long defaultvalue, char_t *desc) {
	char_t	buf[1024] = "";
	char_t	defaultbuf[1024] = "";

	sprintf(defaultbuf, "%d", defaultvalue);

	GetConfigVariable(g, appName, paramName, defaultbuf, buf, sizeof(buf), desc);

	return atol(buf);
}

void PutConfigVariable(edcastGlobals *g, char_t *appName, char_t *paramName, char_t *destValue) {
	if (g->configVariables) {
		int ok = 0;
		for (int j=0;j<g->numConfigVariables;j++) {
			if (!strcmp(g->configVariables[j], paramName)) {
				ok = 1;
				break;
			}
		}
		if (!ok) {
			return;
		}
	}

	for(int i = 0; i < numConfigValues; i++) {
		if(!strcmp(paramName, configFileValues[i].Variable)) {
			strcpy(configFileValues[i].Value, destValue);
			return;
		}
	}

	strcpy(configFileValues[numConfigValues].Variable, paramName);
	strcpy(configFileValues[numConfigValues].Value, destValue);
	strcpy(configFileValues[numConfigValues].Description, "");
	numConfigValues++;
	return;
}

void PutConfigVariableLong(edcastGlobals *g, char_t *appName, char_t *paramName, long value) {
	char_t	buf[1024] = "";

	sprintf(buf, "%d", value);

	PutConfigVariable(g, appName, paramName, buf);

	return;
}

int trimVariable(char_t *variable) {
	char_t	*p1;

	/* Trim off the back */
	for(p1 = variable + strlen(variable) - 1; p1 > variable; p1--) {
		if((*p1 == ' ') || (*p1 == '	')) {
			*p1 = '\000';
		}
		else {
			break;
		}
	}

	/* Trim off the front */
	char_t	tempVariable[1024] = "";

	memset(tempVariable, '\000', sizeof(tempVariable));
	for(p1 = variable; p1 < variable + strlen(variable) - 1; p1++) {
		if((*p1 == ' ') || (*p1 == '	')) {
			;
		}
		else {
			break;
		}
	}

	strcpy(tempVariable, p1);
	strcpy(variable, tempVariable);
	return 1;
}

void setDestURLCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->destURLCallback = pCallback;
}

void setSourceURLCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->sourceURLCallback = pCallback;
}

void setServerStatusCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->serverStatusCallback = pCallback;
}

void setGeneralStatusCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->generalStatusCallback = pCallback;
}

void setWriteBytesCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->writeBytesCallback = pCallback;
}

void setServerTypeCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->serverTypeCallback = pCallback;
}

void setServerNameCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->serverNameCallback = pCallback;
}

void setStreamTypeCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->streamTypeCallback = pCallback;
}

void setBitrateCallback(edcastGlobals *g, void (*pCallback) (void *, void *)) {
	g->bitrateCallback = pCallback;
}

void setOggEncoderText(edcastGlobals *g, char_t *text) {
	strcpy(g->gOggEncoderText, text);
}

void setVUCallback(edcastGlobals *g, void (*pCallback) (int, int)) {
	g->VUCallback = pCallback;
}

void setForceStop(edcastGlobals *g, int forceStop) {
	g->gForceStop = forceStop;
}

void initializeGlobals(edcastGlobals *g) {

	/* Global variables....gotta love em... */
	g->gSCSocket = 0;
	g->gSCSocket2 = 0;
	g->gSCSocketControl = 0;
	g->gSCFlag = 0;
	g->gReconnectSec = 10;
	g->gAutoCountdown = 10;
	g->automaticconnect = 1;
	memset(g->gServer, '\000', sizeof(g->gServer));
	memset(g->gPort, '\000', sizeof(g->gPort));
	memset(g->gPassword, '\000', sizeof(g->gPassword));
	memset(g->gIniFile, '\000', sizeof(g->gIniFile));
	memset(g->gAppName, '\000', sizeof(g->gAppName));
	memset(g->gCurrentSong, '\000', sizeof(g->gCurrentSong));
	g->gPubServ = 1;
	memset(g->gServIRC, '\000', sizeof(g->gServIRC));
	memset(g->gServAIM, '\000', sizeof(g->gServAIM));
	memset(g->gServICQ, '\000', sizeof(g->gServICQ));
	memset(g->gServURL, '\000', sizeof(g->gServURL));
	memset(g->gServDesc, '\000', sizeof(g->gServDesc));
	memset(g->gMountpoint, '\000', sizeof(g->gMountpoint));
	g->gAutoReconnect = 0;
	memset(g->gAutoStart, '\000', sizeof(g->gAutoStart));
	memset(g->gAutoStartSec, '\000', sizeof(g->gAutoStartSec));
	memset(g->gQuality, '\000', sizeof(g->gQuality));
	memset(g->gIceFlag, '\000', sizeof(g->gIceFlag));
	memset(g->gSongTitle, '\000', sizeof(g->gSongTitle));
	memset(g->gManualSongTitle, '\000', sizeof(g->gManualSongTitle));
	g->gLockSongTitle = 0;
	g->startTime = 0;
	g->endTime = 0;

	g->weareconnected = 0;

	g->gCurrentlyEncoding = 0;
	g->gShoutcastFlag = 0;
	g->gIcecastFlag = 0;
	g->destURLCallback = NULL;
	g->sourceURLCallback = NULL;
	g->serverStatusCallback = NULL;
	g->generalStatusCallback = NULL;
	g->writeBytesCallback = NULL;
	g->serverTypeCallback = NULL;
	g->serverNameCallback = NULL;
	g->streamTypeCallback = NULL;
	g->bitrateCallback = NULL;
	g->VUCallback = NULL;

	memset(g->sourceDescription, '\000', sizeof(g->sourceDescription));

	/* OGG Stuff */
	g->oggflag = 1;
	g->ice2songChange = false;
	g->in_header = 0;

	/* Resampler stuff */
	g->initializedResampler = 0;

	memset(g->gOggEncoderText, '\000', sizeof(g->gOggEncoderText));
	g->gForceStop = 0;
	g->currentBitrate=128;
	g->currentSamplerate=44100;
	g->currentChannels=2;
	strcpy(g->gOggQuality,"3");
	strcpy(g->gMountpoint,"/stream.ogg");
	strcpy(g->gPort,"8000");
	strcpy(g->gServer,"localhost");
	strcpy(g->gServName,"foo_vorbisstream");
	strcpy(g->gServDesc,"An audio stream.");

	memset(&(g->vi), '\000', sizeof(g->vi));
}


int setCurrentSongTitleURL(edcastGlobals *g, char_t *song) {
	char_t	*pCurrent;

	if(g->gLockSongTitle) {
		pCurrent = g->gManualSongTitle;
	}
	else {
		pCurrent = song;
	}

	if(strcmp(g->gSongTitle, pCurrent)) {
		strcpy(g->gSongTitle, pCurrent);
		updateSongTitle(g, 0);
		return 1;
	}

	return 0;
}

void getCurrentSongTitle(edcastGlobals *g, char_t *song, char_t *artist, char_t *full) {
	char_t	songTitle[1024] = "";
	char_t	songTitle2[1024] = "";

	memset(songTitle2, '\000', sizeof(songTitle2));

	char_t	*pCurrent;

	if(g->gLockSongTitle) {
		pCurrent = g->gManualSongTitle;
	}
	else {
		pCurrent = g->gSongTitle;
	}

	strcpy(songTitle, pCurrent);

	strcpy(full, songTitle);

	char_t	*p1 = strchr(songTitle, '-');

	if(p1) {
		if(*(p1 - 1) == ' ') {
			p1--;
		}

		strncpy(artist, songTitle, p1 - songTitle);
		p1 = strchr(songTitle, '-');
		p1++;
		if(*p1 == ' ') {
			p1++;
		}

		strcpy(song, p1);
	}
	else {
		strcpy(artist, "");
		strcpy(song, songTitle);
	}
}

void ReplaceString(char_t *source, char_t *dest, char_t *from, char_t *to) {
	int		loop = 1;
	char_t	*p2 = (char_t *) 1;
	char_t	*p1 = source;

	while(p2) {
		p2 = strstr(p1, from);
		if(p2) {
			strncat(dest, p1, p2 - p1);
			strcat(dest, to);
			p1 = p2 + strlen(from);
		}
		else {
			strcat(dest, p1);
		}
	}
}

/*
 =======================================================================================================================
    This function URLencodes strings for use in sending them thru ;
    the Shoutcast admin.cgi interface to update song titles..
 =======================================================================================================================
 */
void URLize(char_t *input, char_t *output, int inputlen, int outputlen) {
	ReplaceString(input, output, "%", "%25");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, ";", "%3B");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, "/", "%2F");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, "?", "%3F");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, ":", "%3A");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, "@", "%40");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, "&", "%26");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, "=", "%3D");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, "+", "%2B");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, " ", "%20");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, "\"", "%22");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, "#", "%23");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, "<", "%3C");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, ">", "%3E");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, "!", "%21");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, "*", "%2A");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, "'", "%27");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, "(", "%28");
	memset(output, '\000', outputlen);
	ReplaceString(input, output, ")", "%29");
	memset(input, '\000', inputlen);
	ReplaceString(output, input, ",", "%2C");

	memset(output, '\000', outputlen);
	strcpy(output, input);
}

int updateSongTitle(edcastGlobals *g, int forceURL) {
	char_t	contentString[2056] = "";
	char_t	URLPassword[255] = "";
	char_t	URLSong[1024] = "";
	char_t	Song[1024] = "";

	if(getIsConnected(g)) {
		if(forceURL) {
			if((g->gSCFlag) || (g->gIcecastFlag) || (g->gIcecast2Flag) || forceURL) {

				URLize(g->gPassword, URLPassword, sizeof(g->gPassword), sizeof(URLPassword));
				strcpy(g->gCurrentSong, g->gSongTitle);
				URLize(g->gCurrentSong, URLSong, sizeof(g->gSongTitle), sizeof(URLSong));

				if(g->gIcecast2Flag) {
					char_t	userAuth[1024] = "";
					sprintf(userAuth, "source:%s", g->gPassword);
					char_t	*puserAuthbase64 = util_base64_encode(userAuth);

					if(puserAuthbase64) {
						sprintf(contentString,
								"GET /admin/metadata?pass=%s&mode=updinfo&mount=%s&song=%s HTTP/1.0\r\nAuthorization: Basic %s\r\nUser-Agent: (Mozilla Compatible)\r\n\r\n",
							URLPassword,
								g->gMountpoint,
								URLSong,
								puserAuthbase64);
						free(puserAuthbase64);
					}
				}

				if(g->gIcecastFlag) {
					sprintf(contentString,
							"GET /admin.cgi?pass=%s&mode=updinfo&mount=%s&song=%s HTTP/1.0\r\nUser-Agent: (Mozilla Compatible)\r\n\r\n",
						URLPassword,
							g->gMountpoint,
							URLSong);
				}

				if(g->gSCFlag) {
					sprintf(contentString,
							"GET /admin.cgi?pass=%s&mode=updinfo&song=%s HTTP/1.0\r\nUser-Agent: (Mozilla Compatible)\r\n\r\n",
						URLPassword,
							URLSong);
				}

				g->gSCSocketControl = g->controlChannel.DoSocketConnect(g->gServer, atoi(g->gPort));
				if(g->gSCSocketControl != -1) {
					int sent = send(g->gSCSocketControl, contentString, strlen(contentString), 0);
					closesocket(g->gSCSocketControl);
				}
				else {
					LogMessage(g,LOG_ERROR, "Cannot connect to server");
				}
			}
		}
		return 1;
	}
	return 0;
}


/*
 =======================================================================================================================
    This function will disconnect the DSP from the server (duh)
 =======================================================================================================================
 */
int disconnectFromServer(edcastGlobals *g) {
	console::info("Disconnecting from server...");
	g->weareconnected = 0;

	if(g->gCurrentlyEncoding)
	{
		Sleep(1000);
	}

	/* Close all open sockets */
	closesocket(g->gSCSocket);
	closesocket(g->gSCSocketControl);

	/*
	 * Reset the Status to Disconnected, and reenable the config ;
	 * button
	 */
	g->gSCSocket = 0;
	g->gSCSocketControl = 0;

	ogg_stream_clear(&g->os);
	vorbis_block_clear(&g->vb);
	vorbis_dsp_clear(&g->vd);
	vorbis_info_clear(&g->vi);
	memset(&(g->vi), '\000', sizeof(g->vi));

	if(g->serverStatusCallback) {
		g->serverStatusCallback(g, (void *) "Disconnected");
	}

	return 1;
}

/*
 =======================================================================================================================
    This funciton will connect to a server (Shoutcast/Icecast/Icecast2) ;
    and send the appropriate password info and check to make sure things ;
    are connected....
 =======================================================================================================================
 */
int connectToServer(edcastGlobals *g) {
	int		s_socket = 0;
	char_t	buffer[1024] = "";
	char_t	contentString[1024] = "";
	char_t	brate[25] = "";
	char_t	ypbrate[25] = "";

	LogMessage(g,LOG_DEBUG, "Connecting encoder %d", g->encoderNumber);

	sprintf(brate, "%d", g->currentBitrate);

	if(!g->gOggBitQualFlag) {
		sprintf(ypbrate, "Quality %s", g->gOggQuality);
	}
	else {
		strcpy(ypbrate, brate);
	}

	g->gSCFlag = 0;

	greconnectFlag = 0;

	if(g->serverStatusCallback) 
		g->serverStatusCallback(g, (void *) "Connecting");

	g->dataChannel.initWinsockLib();

	/* If we are Icecast/Icecast2, then connect to specified port */
	if(g->gIcecastFlag || g->gIcecast2Flag) {
		g->gSCSocket = g->dataChannel.DoSocketConnect(g->gServer, atoi(g->gPort));
	}
	else {

		/*
		 * If we are Shoutcast, then the control socket (used for password) ;
		 * is port+1.
		 */
		g->gSCSocket = g->dataChannel.DoSocketConnect(g->gServer, atoi(g->gPort) + 1);
	}

	/* Check to see if we connected okay */
	if(g->gSCSocket == -1) {
		if(g->serverStatusCallback) {
			g->serverStatusCallback(g, (void *) "Unable to connect to socket");
		}

		return 0;
	}

	int pswdok = 1;

	/* Yup, we did. */
	if(g->serverStatusCallback) {
		g->serverStatusCallback(g, (void *) "Socket connected");
	}

	char_t	contentType[255] = "";

	strcpy(contentType, "application/ogg");

	/*
	 * Here are all the variations of sending the password to ;
	 * a server..This if statement really is ugly...must fix.
	 */
	if(g->gIcecastFlag || g->gIcecast2Flag) {

		/* The Icecast/Icecast2 Way */
		if(g->gIcecastFlag) {
			sprintf(contentString,
					"SOURCE %s %s\r\ncontent-type: %s\r\nx-audiocast-name: %s\r\nx-audiocast-url: %s\r\nx-audiocast-genre: %s\r\nx-audiocast-bitrate: %s\r\nx-audiocast-public: %d\r\nx-audiocast-description: %s\r\n\r\n",
				g->gPassword,
					g->gMountpoint,
					contentType,
					g->gServDesc,
					g->gServURL,
					g->gServGenre,
					brate,
					g->gPubServ,
					g->gServDesc);
		}

		if(g->gIcecast2Flag) {
			char_t	audioInfo[1024] = "";

			sprintf(audioInfo,
					"ice-samplerate=%d;ice-bitrate=%s;ice-channels=%d",
					getCurrentSamplerate(g),
					ypbrate,
					getCurrentChannels(g));

			char_t	userAuth[1024] = "";

			sprintf(userAuth, "source:%s", g->gPassword);

			char_t	*puserAuthbase64 = util_base64_encode(userAuth);

			if(puserAuthbase64) {
				sprintf(contentString,
						"SOURCE %s ICE/1.0\ncontent-type: %s\nAuthorization: Basic %s\nice-name: %s\nice-url: %s\nice-genre: %s\nice-bitrate: %s\nice-private: %d\nice-public: %d\nice-description: %s\nice-audio-info: %s\n\n",
					g->gMountpoint,
						contentType,
						puserAuthbase64,
						g->gServName,
						g->gServURL,
						g->gServGenre,
						ypbrate,
						!g->gPubServ,
						g->gPubServ,
						g->gServDesc,
						audioInfo);
				free(puserAuthbase64);
			}
		}
	}
	else {

		/* The Shoutcast way */
		sendToServer(g, g->gSCSocket, g->gPassword, strlen(g->gPassword), HEADER_TYPE);
		sendToServer(g, g->gSCSocket, "\r\n", strlen("\r\n"), HEADER_TYPE);

		recv(g->gSCSocket, buffer, sizeof(buffer), (int) 0);

		/*
		 * if we get an OK, then we are not a Shoutcast server ;
		 * (could be live365 or other variant)..And OK2 means it's ;
		 * Shoutcast and we can safely send in metadata via the ;
		 * admin.cgi interface.
		 */
		if(!strncmp(buffer, "OK", strlen("OK"))) {
			if(!strncmp(buffer, "OK2", strlen("OK2"))) {
				g->gSCFlag = 1;
			}
			else {
				g->gSCFlag = 0;
			}

			if(g->serverStatusCallback) {
				g->serverStatusCallback(g, (void *) "Password OK");
			}
		}
		else {
			if(g->serverStatusCallback) {
				g->serverStatusCallback(g, (void *) "Password Failed");
			}

			closesocket(g->gSCSocket);
			return 0;
		}

		memset(contentString, '\000', sizeof(contentString));
		if(strlen(g->gServICQ) == 0) {
			strcpy(g->gServICQ, "N/A");
		}

		if(strlen(g->gServAIM) == 0) {
			strcpy(g->gServAIM, "N/A");
		}

		if(strlen(g->gServIRC) == 0) {
			strcpy(g->gServIRC, "N/A");
		}

		sprintf(contentString,
				"content-type:%s\r\nicy-name:%s\r\nicy-genre:%s\r\nicy-url:%s\r\nicy-pub:%d\r\nicy-irc:%s\r\nicy-icq:%s\r\nicy-aim:%s\r\nicy-br:%s\r\n\r\n",
			contentType,
				g->gServName,
				g->gServGenre,
				g->gServURL,
				g->gPubServ,
				g->gServIRC,
				g->gServICQ,
				g->gServAIM,
				brate);
	}

	sendToServer(g, g->gSCSocket, contentString, strlen(contentString), HEADER_TYPE);

	if(g->gIcecastFlag) {

		/*
		 * Here we are checking the response from Icecast/Icecast2 ;
		 * from when we sent in the password...OK means we are good..if the ;
		 * password is bad, Icecast just disconnects the socket.
		 */
		
		recv(g->gSCSocket, buffer, sizeof(buffer), 0);
		if(!strncmp(buffer, "OK", strlen("OK"))) {

			/* I don't think this check is needed.. */
			if(!strncmp(buffer, "OK2", strlen("OK2"))) {
				g->gSCFlag = 1;
			}
			else {
				g->gSCFlag = 0;
			}

			if(g->serverStatusCallback)g->serverStatusCallback(g, (void *) "Password OK");
		}
		else {
			if(g->serverStatusCallback)g->serverStatusCallback(g, (void *) "Password Failed");
			closesocket(g->gSCSocket);
			return 0;
		}
	}

	/* We are connected */
	char_t		outFilename[1024] = "";
	char_t		outputFile[1024] = "";
	struct tm	*newtime;
	time_t		aclock;

	time(&aclock);
	newtime = localtime(&aclock);

	int ret = 0;

	ret = initializeencoder(g);
	g->forcedDisconnect = false;
	if(ret) {
		g->weareconnected = 1;
		g->automaticconnect = 1;
		if(g->serverStatusCallback)g->serverStatusCallback(g, (void *) "Success");
	}
	else {
		disconnectFromServer(g);
		if(g->serverStatusCallback)g->serverStatusCallback(g, (void *) "Encoder init failed");
		return 0;
	}
	if(g->serverStatusCallback)g->serverStatusCallback(g, (void *) "Connected");
//	setCurrentSongTitle(g, g->gSongTitle);
	updateSongTitle(g, 0);
	return 1;
}

int ogg_encode_dataout(edcastGlobals *g)
{
	ogg_packet	op;
	ogg_page	og;
	int			result;
	int			sentbytes = 0;

	if(g->in_header) {
		result = ogg_stream_flush(&g->os, &og);
		g->in_header = 0;
	}

	while(vorbis_analysis_blockout(&g->vd, &g->vb) == 1) {
		vorbis_analysis(&g->vb, NULL);
		vorbis_bitrate_addblock(&g->vb);

		int packetsdone = 0;

		while(vorbis_bitrate_flushpacket(&g->vd, &op)) {

			/* Add packet to bitstream */
			ogg_stream_packetin(&g->os, &op);
			packetsdone++;

			/*
			 * If we've gone over a page boundary, we can do actual output, so do so (for
			 * however many pages are available)
			 */
			int eos = 0;

			while(!eos) {
				int result = ogg_stream_pageout(&g->os, &og);

				if(!result) break;

				int ret = 0;

				sentbytes = sendToServer(g, g->gSCSocket, (char_t *) og.header, og.header_len, CODEC_TYPE);
				if(sentbytes < 0) {
					return sentbytes;
				}

				sentbytes += sendToServer(g, g->gSCSocket, (char_t *) og.body, og.body_len, CODEC_TYPE);
				if(sentbytes < 0) {
					return sentbytes;
				}

				if(ogg_page_eos(&og)) {
					eos = 1;
				}
			}
		}
	}

	return sentbytes;
}

int initializeResampler(edcastGlobals *g, long inSampleRate, long inNCH) {
	if(!g->initializedResampler) {
		long	in_samplerate = inSampleRate;
		long	out_samplerate = getCurrentSamplerate(g);
		long	in_nch = inNCH;
		long	out_nch = 2;

		if(res_init(&(g->resampler), out_nch, out_samplerate, in_samplerate, RES_END)) {
			LogMessage(g,LOG_ERROR, "Error initializing resampler");
			return 0;
		}

		g->initializedResampler = 1;
	}

	return 1;
}

int ocConvertAudio(edcastGlobals *g, float *in_samples, float *out_samples, int num_in_samples, int num_out_samples) {
	int max_num_samples = res_push_max_input(&(g->resampler), num_out_samples);
	int ret_samples = res_push_interleaved(&(g->resampler),
										   (SAMPLE *) out_samples,
										   (const SAMPLE *) in_samples,
										   max_num_samples);

	return ret_samples;
}

int initializeencoder(edcastGlobals *g) {
	int		ret = 0;
	char_t	outFilename[1024] = "";
	char_t	message[1024] = "";

	resetResampler(g);

	/* Ogg Vorbis Initialization */
	ogg_stream_clear(&g->os);
	vorbis_block_clear(&g->vb);
	vorbis_dsp_clear(&g->vd);
	vorbis_info_clear(&g->vi);

	int bitrate = 0;

	vorbis_info_init(&g->vi);

	int encode_ret = 0;

	if(!g->gOggBitQualFlag) {
		encode_ret = vorbis_encode_setup_vbr(&g->vi,
											 g->currentChannels,
											 g->currentSamplerate,
											 ((float) atof(g->gOggQuality) * (float) .1));
		if(encode_ret) {
			vorbis_info_clear(&g->vi);
		}
	}
	else {
		int maxbit = -1;
		int minbit = -1;

		if(g->currentBitrateMax > 0) {
			maxbit = g->currentBitrateMax;
		}

		if(g->currentBitrateMin > 0) {
			minbit = g->currentBitrateMin;
		}

		encode_ret = vorbis_encode_setup_managed(&g->vi,
												 g->currentChannels,
												 g->currentSamplerate,
												 g->currentBitrate * 1000,
												 g->currentBitrate * 1000,
												 g->currentBitrate * 1000);

		if(encode_ret) {
			vorbis_info_clear(&g->vi);
		}
	}

	if(encode_ret == OV_EIMPL) {
		LogMessage(g,LOG_ERROR, "Sorry, but this vorbis mode is not supported currently...");
		return 0;
	}

	if(encode_ret == OV_EINVAL) {
		LogMessage(g,LOG_ERROR, "Sorry, but this is an illegal vorbis mode...");
		return 0;
	}

	ret = vorbis_encode_setup_init(&g->vi);

	/*
	 * Now, set up the analysis engine, stream encoder, and other preparation before
	 * the encoding begins
	 */
	ret = vorbis_analysis_init(&g->vd, &g->vi);
	ret = vorbis_block_init(&g->vd, &g->vb);

	g->serialno = 0;
	srand((unsigned int)time(0));
	ret = ogg_stream_init(&g->os, rand());

	/*
	 * Now, build the three header packets and send through to the stream output stage
	 * (but defer actual file output until the main encode loop)
	 */
	ogg_packet		header_main;
	ogg_packet		header_comments;
	ogg_packet		header_codebooks;
	vorbis_comment	vc;
	char_t			title[1024] = "";
	char_t			artist[1024] = "";
	char_t			FullTitle[1024] = "";
	char_t			SongTitle[1024] = "";
	char_t			Artist[1024] = "";
	char_t			Streamed[1024] = "";

	memset(Artist, '\000', sizeof(Artist));
	memset(SongTitle, '\000', sizeof(SongTitle));
	memset(FullTitle, '\000', sizeof(FullTitle));
	memset(Streamed, '\000', sizeof(Streamed));

	vorbis_comment_init(&vc);

	for(unsigned i=0;i<encoders.metadata.get_count();++i)
		vorbis_comment_add(&vc,(char*)encoders.metadata.get_item(i).ptr());

	sprintf(Streamed, "ENCODEDBY=edcast Vorbis Streamer");
	vorbis_comment_add(&vc, Streamed);
	if(strlen(g->sourceDescription) > 0) {
		sprintf(Streamed, "TRANSCODEDFROM=%s", g->sourceDescription);
		vorbis_comment_add(&vc, Streamed);
	}

	/* Build the packets */
	memset(&header_main, '\000', sizeof(header_main));
	memset(&header_comments, '\000', sizeof(header_comments));
	memset(&header_codebooks, '\000', sizeof(header_codebooks));

	vorbis_analysis_headerout(&g->vd, &vc, &header_main, &header_comments, &header_codebooks);

	ogg_stream_packetin(&g->os, &header_main);
	ogg_stream_packetin(&g->os, &header_comments);
	ogg_stream_packetin(&g->os, &header_codebooks);

	g->in_header = 1;

	ogg_page	og;
	int			eos = 0;
	int			sentbytes = 0;

	while(!eos) {
		int result = ogg_stream_flush(&g->os, &og);

		if(result == 0) break;
		sentbytes += sendToServer(g, g->gSCSocket, (char *) og.header, og.header_len, CODEC_TYPE);
		sentbytes += sendToServer(g, g->gSCSocket, (char *) og.body, og.body_len, CODEC_TYPE);
	}

	vorbis_comment_clear(&vc);

	return 1;
}

int do_encoding(edcastGlobals *g, float *samples, int numsamples, int nch) {
	g->gCurrentlyEncoding = 1;

	int				s;
	int				count = 0;

	int				eos = 0;
	int				ret = 0;
	int				sentbytes = 0;
	char			buf[255] = "";

	g->gCurrentlyEncoding = 1;

	if(g->weareconnected) {
		s = numsamples * nch;

		long	leftMax = 0;
		long	rightMax = 0;
		int		samplecounter = 0;

		LogMessage(g,LOG_DEBUG, "determining left/right max...");
		for(int i = 0; i < numsamples * 2; i = i + 2) {
			leftMax += abs((int) ((float) samples[i] * 32767.f));
			rightMax += abs((int) ((float) samples[i + 1] * 32767.f));
		}

		if(numsamples > 0) {
			leftMax = leftMax / (numsamples * 2);
			rightMax = rightMax / (numsamples * 2);
			if(g->VUCallback) {
				g->VUCallback(leftMax, rightMax);
			}
		}

		/*
		 * If a song change was detected, close the stream and resend new ;
		 * vorbis headers (with new comments) - all done by icecast2SendMetadata();
		 */
		if(g->ice2songChange) {
			LogMessage(g,LOG_DEBUG, "Song change processing...");
			g->ice2songChange = false;

				//icecast2SendMetadata(g);
			vorbis_analysis_wrote(&g->vd, 0);
			ogg_encode_dataout(g);
			initializeencoder(g);
		}

		LogMessage(g,LOG_DEBUG, "vorbis_analysis_buffer...");

		float	**buffer = vorbis_analysis_buffer(&g->vd, numsamples);
		int		samplecount = 0;
		int		i;

		samplecounter = 0;

		for(i = 0; i < numsamples * 2; i = i + 2) {
			buffer[0][samplecounter] = samples[i];
			if(g->currentChannels == 2) {
				buffer[1][samplecounter] = samples[i + 1];
			}

			samplecounter++;
		}
		LogMessage(g,LOG_DEBUG, "vorbis_analysis_wrote...");

		ret = vorbis_analysis_wrote(&g->vd, numsamples);

//		mutex.enter();
		LogMessage(g,LOG_DEBUG, "ogg_encode_dataout...");
		/* Stream out what we just prepared for Vorbis... */
		sentbytes = ogg_encode_dataout(g);
		LogMessage(g,LOG_DEBUG, "done ogg_encode_dataout...");
//		mutex.leave();
	

		/*
		 * Generic error checking, if there are any socket problems, the trigger ;
		 * a disconnection handling->..
		 */
		if(sentbytes < 0) {
			char buf[2046] = "";

			disconnectFromServer(g);
			if(g->gForceStop) {
				g->gForceStop = 0;
				return 0;
			}

			sprintf(buf, "Disconnected from server");
			g->forcedDisconnect = true;
			g->forcedDisconnectSecs = time(&(g->forcedDisconnectSecs));
			if(g->serverStatusCallback)
				g->serverStatusCallback(g, (void *) buf);
		}
	}

	g->gCurrentlyEncoding = 0;
	return 1;
}

void config_read(edcastGlobals *g) {
	strcpy(g->gAppName, "edcast");

	char	buf[255] = "";
	char	desc[1024] = "";

	sprintf(desc, "The source URL for the broadcast. It must be in the form http://server:port/mountpoint.  For those servers without a mountpoint (Shoutcast) use http://server:port.");
	GetConfigVariable(g, g->gAppName, "SourceURL", "http://localhost/", g->gSourceURL, sizeof(g->gSourceURL), desc);

	if(g->sourceURLCallback) {
		g->sourceURLCallback(g, (char *) g->gSourceURL);
	}

	sprintf(desc, "Destination server details (to where you are encoding).  Valid server types : Shoutcast, Icecast, Icecast2");
	GetConfigVariable(g, g->gAppName, "ServerType", "Icecast2", g->gServerType, sizeof(g->gServerType), desc);
//	sprintf(desc, "The server to which the stream is sent. It can be a hostname  or IP (example: www.stream.com, 192.168.1.100)");
	GetConfigVariable(g, g->gAppName, "Server", "localhost", g->gServer, sizeof(g->gServer), NULL);
//	sprintf(desc, "The port to which the stream is sent. Must be a number (example: 8000)");
	GetConfigVariable(g, g->gAppName, "Port", "8000", g->gPort, sizeof(g->gPort), NULL);
//	sprintf(desc, "This is the encoder password for the destination server (example: hackme)");
	GetConfigVariable(g, g->gAppName, "ServerPassword", "changemenow", g->gPassword, sizeof(g->gPassword), NULL);
//	sprintf(desc,"Used for Icecast/Icecast2 servers, The mountpoint must end in .ogg for Vorbis streams and have NO extention for MP3 streams.  If you are sending to a Shoutcast server, this MUST be blank. (example: /mp3, /myvorbis.ogg)");
	GetConfigVariable(g, g->gAppName, "ServerMountpoint", "/stream.ogg", g->gMountpoint, sizeof(g->gMountpoint), NULL);
//	sprintf(desc,"This setting tells the destination server to list on any available YP listings. Not all servers support this (Shoutcast does, Icecast2 doesn't) (example: 1 for YES, 0 for NO)");
	sprintf(desc,"YP (Stream Directory) Settings");
	g->gPubServ = GetConfigVariableLong(g, g->gAppName, "ServerPublic", 1, desc);
//	sprintf(desc, "This is used in the YP listing, I think only Shoutcast supports this (example: #mystream)");
	GetConfigVariable(g, g->gAppName, "ServerIRC", "", g->gServIRC, sizeof(g->gServIRC), NULL);
//	sprintf(desc, "This is used in the YP listing, I think only Shoutcast supports this (example: myAIMaccount)");
	GetConfigVariable(g, g->gAppName, "ServerAIM", "", g->gServAIM, sizeof(g->gServAIM), NULL);
//	sprintf(desc, "This is used in the YP listing, I think only Shoutcast supports this (example: 332123132)");
	GetConfigVariable(g, g->gAppName, "ServerICQ", "", g->gServICQ, sizeof(g->gServICQ), NULL);
//	sprintf(desc, "The URL that is associated with your stream. (example: http://www.mystream.com)");
	GetConfigVariable(g, g->gAppName, "ServerStreamURL", "http://www.oddsock.org", g->gServURL, sizeof(g->gServURL), NULL);
//	sprintf(desc, "The Stream Name");
	GetConfigVariable(g, g->gAppName, "ServerName", "This is my server name", g->gServName, sizeof(g->gServName), NULL);
//	sprintf(desc, "A short description of the stream (example: Stream House on Fire!)");
	GetConfigVariable(g, g->gAppName, "ServerDescription", "This is my server description", g->gServDesc, sizeof(g->gServDesc), NULL);
//	sprintf(desc, "Genre of music, can be anything you want... (example: Rock)");
	GetConfigVariable(g, g->gAppName, "ServerGenre", "Rock", g->gServGenre, sizeof(g->gServGenre), NULL);
//	sprintf(desc,"Wether or not edcast will reconnect if it is disconnected from the destination server (example: 1 for YES, 0 for NO)");
	sprintf(desc,"Misc encoder properties");
	g->gAutoReconnect = GetConfigVariableLong(g, g->gAppName, "AutomaticReconnect", 1, desc);
//	sprintf(desc, "How long it will wait (in seconds) between reconnect attempts. (example: 10)");
	g->gReconnectSec = GetConfigVariableLong(g, g->gAppName, "AutomaticReconnectSecs", 10, NULL);


	//	sprintf(desc, "What format to encode to. Valid values are (OGG, LAME) (example: OGG, LAME)");
	sprintf(desc, "Output codec selection (Valid selections : MP3, OggVorbis, Ogg FLAC, AAC, AAC Plus)");
	GetConfigVariable(g, g->gAppName, "Encode", "OggVorbis", g->gEncodeType, sizeof(g->gEncodeType), desc);

	if(g->streamTypeCallback)
			g->streamTypeCallback(g, (void *) "OggVorbis");

	if(g->destURLCallback) {
		sprintf(buf, "http://%s:%s%s", g->gServer, g->gPort, g->gMountpoint);
		g->destURLCallback(g, (char *) buf);
	}

//	sprintf(desc, "Bitrate. This is the mean bitrate if using VBR.");
	sprintf(desc, "General settings (non-codec related).  Note : NumberChannels = 1 for MONO, 2 for STEREO");
	g->currentBitrate = GetConfigVariableLong(g, g->gAppName, "BitrateNominal", 128, desc);

//	sprintf(desc,"Minimum Bitrate. Used only if using Bitrate Management (not recommended) or LAME VBR(example: 64, 128)");
	g->currentBitrateMin = GetConfigVariableLong(g, g->gAppName, "BitrateMin", 128, NULL);

//	sprintf(desc,"Maximum Bitrate. Used only if using Bitrate Management (not recommended) or LAME VBR (example: 64, 128)");
	g->currentBitrateMax = GetConfigVariableLong(g, g->gAppName, "BitrateMax", 128, NULL);

//	sprintf(desc, "Number of channels. Valid values are (1, 2). 1 means Mono, 2 means Stereo (example: 2,1)");
	g->currentChannels = GetConfigVariableLong(g, g->gAppName, "NumberChannels", 2, NULL);

//	sprintf(desc, "Sample rate for the stream. Valid values depend on wether using lame or vorbis. Vorbis supports odd samplerates such as 32kHz and 48kHz, but lame appears to not.feel free to experiment (example: 44100, 22050, 11025)");
	g->currentSamplerate = GetConfigVariableLong(g, g->gAppName, "Samplerate", 44100, NULL);

//	sprintf(desc, "Vorbis Quality Level. Valid values are between -1 (lowest quality) and 10 (highest).  The lower the quality the lower the output bitrate. (example: -1, 3)");
	sprintf(desc, "Ogg Vorbis specific settings.  Note: Valid settings for BitrateQuality flag are (Quality, Bitrate Management)");
	GetConfigVariable(g, g->gAppName, "OggQuality", "0", g->gOggQuality, sizeof(g->gOggQuality), desc);


//	sprintf(desc,"This flag specifies if you want Vorbis Quality or Bitrate Management.  Quality is always recommended. Valid values are (Bitrate, Quality). (example: Quality, Bitrate Management)");
	GetConfigVariable(g, g->gAppName, "OggBitrateQualityFlag", "Quality", g->gOggBitQual, sizeof(g->gOggBitQual), NULL);
	g->gOggBitQualFlag = 0;
	if(!strncmp(g->gOggBitQual, "Q", 1)) {

		/* Quality */
		g->gOggBitQualFlag = 0;
	}

	if(!strncmp(g->gOggBitQual, "B", 1)) {

		/* Bitrate */
		g->gOggBitQualFlag = 1;
	}

	g->gAutoCountdown = atoi(g->gAutoStartSec);
	if(strlen(g->gMountpoint) > 0) {
		strcpy(g->gIceFlag, "1");
	}
	else {
		strcpy(g->gIceFlag, "0");
	}

	char	tempString[255] = "";

	memset(tempString, '\000', sizeof(tempString));
	ReplaceString(g->gServer, tempString, " ", "");
	strcpy(g->gServer, tempString);

	memset(tempString, '\000', sizeof(tempString));
	ReplaceString(g->gPort, tempString, " ", "");
	strcpy(g->gPort, tempString);


	if(!strcmp(g->gServerType, "KasterBlaster")) {
		g->gShoutcastFlag = 1;
		g->gIcecastFlag = 0;
		g->gIcecast2Flag = 0;
	}

	if(!strcmp(g->gServerType, "Shoutcast")) {
		g->gShoutcastFlag = 1;
		g->gIcecastFlag = 0;
		g->gIcecast2Flag = 0;
	}

	if(!strcmp(g->gServerType, "Icecast")) {
		g->gShoutcastFlag = 0;
		g->gIcecastFlag = 1;
		g->gIcecast2Flag = 0;
	}

	if(!strcmp(g->gServerType, "Icecast2")) {
		g->gShoutcastFlag = 0;
		g->gIcecastFlag = 0;
		g->gIcecast2Flag = 1;
	}

	if(g->serverTypeCallback) {
		g->serverTypeCallback(g, (void *) g->gServerType);
	}

	if(g->serverNameCallback) {
		char	*pdata = NULL;
		int		pdatalen = strlen(g->gServDesc) + strlen(g->gServName) + strlen(" () ") + 1;

		pdata = (char *) calloc(1, pdatalen);
		sprintf(pdata, "%s (%s)", g->gServName, g->gServDesc);
		g->serverNameCallback(g, (void *) pdata);
		free(pdata);
	}

	sprintf(desc, "Used for any window positions (X value)");
	g->lastX = GetConfigVariableLong(g, g->gAppName, "lastX", 0, desc);
	sprintf(desc, "Used for any window positions (Y value)");
	g->lastY = GetConfigVariableLong(g, g->gAppName, "lastY", 0, desc);
	sprintf(desc, "Used for dummy window positions (X value)");
	g->lastDummyX = GetConfigVariableLong(g, g->gAppName, "lastDummyX", 0, desc);
	sprintf(desc, "Used for dummy window positions (Y value)");
	g->lastDummyY = GetConfigVariableLong(g, g->gAppName, "lastDummyY", 0, desc);

	sprintf(desc, "Locked Metadata");
	GetConfigVariable(g, g->gAppName, "LockMetadata", "", g->gManualSongTitle, sizeof(g->gManualSongTitle), desc);
	sprintf(desc, "Flag which indicates if we are using locked metadata");
	g->gLockSongTitle = GetConfigVariableLong(g, g->gAppName, "LockMetadataFlag", 0, desc);


	/* Set some derived values */
	char	localBitrate[255] = "";
	char	mode[50] = "";

	if(g->currentChannels == 1)
		strcpy(mode, "Mono");
	if(g->currentChannels == 2)
		strcpy(mode, "Stereo");

	if(g->serverStatusCallback) {
		g->serverStatusCallback(g, (void *) "Disconnected");
	}

	sprintf(desc, "Number of encoders to use");
	g->gNumEncoders = GetConfigVariableLong(g, g->gAppName, "NumEncoders", 0, desc);

}

void config_write(edcastGlobals *g) {
	strcpy(g->gAppName, "edcast");

	char	buf[255] = "";
	char	desc[1024] = "";
	char	tempString[1024] = "";

	memset(tempString, '\000', sizeof(tempString));
	ReplaceString(g->gServer, tempString, " ", "");
	strcpy(g->gServer, tempString);

	memset(tempString, '\000', sizeof(tempString));
	ReplaceString(g->gPort, tempString, " ", "");
	strcpy(g->gPort, tempString);

	PutConfigVariable(g, g->gAppName, "SourceURL", g->gSourceURL);
	PutConfigVariable(g, g->gAppName, "ServerType", g->gServerType);
	PutConfigVariable(g, g->gAppName, "Server", g->gServer);
	PutConfigVariable(g, g->gAppName, "Port", g->gPort);
	PutConfigVariable(g, g->gAppName, "ServerMountpoint", g->gMountpoint);
	PutConfigVariable(g, g->gAppName, "ServerPassword", g->gPassword);
	PutConfigVariableLong(g, g->gAppName, "ServerPublic", g->gPubServ);
	PutConfigVariable(g, g->gAppName, "ServerIRC", g->gServIRC);
	PutConfigVariable(g, g->gAppName, "ServerAIM", g->gServAIM);
	PutConfigVariable(g, g->gAppName, "ServerICQ", g->gServICQ);
	PutConfigVariable(g, g->gAppName, "ServerStreamURL", g->gServURL);
	PutConfigVariable(g, g->gAppName, "ServerDescription", g->gServDesc);
	PutConfigVariable(g, g->gAppName, "ServerName", g->gServName);
	PutConfigVariable(g, g->gAppName, "ServerGenre", g->gServGenre);
	PutConfigVariableLong(g, g->gAppName, "AutomaticReconnect", g->gAutoReconnect);
	PutConfigVariableLong(g, g->gAppName, "AutomaticReconnectSecs", g->gReconnectSec);
	PutConfigVariable(g, g->gAppName, "Encode", g->gEncodeType);

	PutConfigVariableLong(g, g->gAppName, "BitrateNominal", g->currentBitrate);
	PutConfigVariableLong(g, g->gAppName, "BitrateMin", g->currentBitrateMin);
	PutConfigVariableLong(g, g->gAppName, "BitrateMax", g->currentBitrateMax);
	PutConfigVariableLong(g, g->gAppName, "NumberChannels", g->currentChannels);
	PutConfigVariableLong(g, g->gAppName, "Samplerate", g->currentSamplerate);
	PutConfigVariable(g, g->gAppName, "OggQuality", g->gOggQuality);
	if(g->gOggBitQualFlag) {
		strcpy(g->gOggBitQual, "Bitrate");
	}
	else {
		strcpy(g->gOggBitQual, "Quality");
	}

	PutConfigVariable(g, g->gAppName, "OggBitrateQualityFlag", g->gOggBitQual);

	PutConfigVariableLong(g, g->gAppName, "lastX", g->lastX);
	PutConfigVariableLong(g, g->gAppName, "lastY", g->lastY);
	PutConfigVariableLong(g, g->gAppName, "lastDummyX", g->lastDummyX);
	PutConfigVariableLong(g, g->gAppName, "lastDummyY", g->lastDummyY);

	PutConfigVariable(g, g->gAppName, "LockMetadata", g->gManualSongTitle);
	PutConfigVariableLong(g, g->gAppName, "LockMetadataFlag", g->gLockSongTitle);

	PutConfigVariableLong(g, g->gAppName, "NumEncoders", g->gNumEncoders);
}

/*
 =======================================================================================================================
    Input is in interleaved float samples
 =======================================================================================================================
 */
int handle_output(edcastGlobals *g, float *samples, int nsamples, int nchannels, int in_samplerate) {
	int			ret = 1;
	static int	current_insamplerate = 0;
	static int	current_nchannels = 0;
	long		out_samplerate = 0;
	long		out_nch = 0;
	int			samplecount = 0;
	float		*samplePtr = 0;
	int			in_nch = nchannels;

	nchannels = 2;
	
	float *samples_rechannel = NULL;
	float *samples_resampled = NULL;
	short *samples_resampled_int = NULL;

	if(g == NULL) {
		return 1;
	}

	if(g->weareconnected) {
		out_samplerate = getCurrentSamplerate(g);
		out_nch = getCurrentChannels(g);

		if(current_insamplerate != in_samplerate) {
			resetResampler(g);
			current_insamplerate = in_samplerate;
		}

		if(current_nchannels != nchannels) {
			resetResampler(g);
			current_nchannels = nchannels;
		}

		samples_rechannel = new float[sizeof(float) * nsamples * nchannels];
		memset(samples_rechannel, '\000', sizeof(float) * nsamples * nchannels);

		samplePtr = samples;

		int make_mono = 0;
		int make_stereo = 0;

		if((in_nch == 2) && (out_nch == 1)) {
			make_mono = 1;
		}

		if((in_nch == 1) && (out_nch == 2)) {
			make_stereo = 1;
		}

		if((in_nch == 1) && (out_nch == 1)) {
			make_stereo = 1;
		}

		int samplecounter = 0;

		if(make_mono) {
			for(int i = 0; i < nsamples * 2; i = i + 2) {
				samples_rechannel[i] = (samples[i] + samples[i + 1]) / 2;
				samples_rechannel[i + 1] = (samples[i] + samples[i + 1]) / 2;
			}
		}

		if(make_stereo) {
			for(int i = 0; i < nsamples; i = i + 1) {
				samples_rechannel[samplecounter] = (samples[i]);
				samplecounter++;
				samples_rechannel[samplecounter] = (samples[i]);
				samplecounter++;
			}
		}

		if(!(make_mono) && !(make_stereo)) {
			for(int i = 0; i < nsamples * 2; i = i + 1) {
				samples_rechannel[i] = (samples[i]);
			}
		}

		LogMessage(g,LOG_DEBUG, "In samplerate = %d, Out = %d", in_samplerate, out_samplerate);
		samplePtr = samples_rechannel;
		if(in_samplerate != out_samplerate) {
			nchannels = 2;

			/* Call the resampler */
			LogMessage(g,LOG_DEBUG, "Initializing resampler");

			initializeResampler(g, in_samplerate, nchannels);
			
			int buf_samples = (((hyper)nsamples * out_samplerate) / in_samplerate);
			samples_resampled = (float *) malloc(sizeof(float) * buf_samples * nchannels);
			memset(samples_resampled, '\000', sizeof(float) * buf_samples * nchannels);

			LogMessage(g,LOG_DEBUG, "calling ocConvertAudio");
			long	out_samples = ocConvertAudio(g,
												 (float *) samplePtr,
												 (float *) samples_resampled,
												 nsamples,
												 buf_samples);

			samples_resampled_int = (short *) malloc(sizeof(short) * out_samples * nchannels);
			memset(samples_resampled_int, '\000', sizeof(short) * out_samples * nchannels);

			LogMessage(g,LOG_DEBUG, "ready to do encoding");

			if(out_samples > 0) {
				samplecount = 0;

				/* Here is the call to actually do the encoding->... */
				LogMessage(g,LOG_DEBUG, "do_encoding start");
				ret = do_encoding(g, (float *) (samples_resampled), out_samples, out_nch);
				LogMessage(g,LOG_DEBUG, "do_encoding end (%d)", ret);
			}

			if(samples_resampled_int) {
				free(samples_resampled_int);
				samples_resampled_int = NULL;
			}

			if(samples_resampled) {
				free(samples_resampled);
				samples_resampled = NULL;
			}
		}
		else {
			LogMessage(g,LOG_DEBUG, "do_encoding start");
			ret = do_encoding(g, (float *) samples_rechannel, nsamples, nchannels);
			LogMessage(g,LOG_DEBUG, "do_encoding end (%d)", ret);
		}

		if(samples_rechannel) {
			free(samples_rechannel);
			samples_rechannel = NULL;
		}
		LogMessage(g,LOG_DEBUG, "%d Calling handle output - Ret = %d", g->encoderNumber, ret);
	}

	return ret;
}

void addUISettings(edcastGlobals *g) {
	addConfigVariable(g, "AutomaticReconnect");
	addConfigVariable(g, "AutomaticReconnectSecs");
	addConfigVariable(g, "lastX");
	addConfigVariable(g, "lastY");
	addConfigVariable(g, "lastDummyX");
	addConfigVariable(g, "lastDummyY");
	addConfigVariable(g, "NumEncoders");
}

void addBasicEncoderSettings(edcastGlobals *g) {
    addConfigVariable(g, "ServerType");
    addConfigVariable(g, "Server");
    addConfigVariable(g, "Port");
    addConfigVariable(g, "ServerMountpoint");
    addConfigVariable(g, "ServerPassword");
    addConfigVariable(g, "ServerPublic");
    addConfigVariable(g, "ServerIRC");
    addConfigVariable(g, "ServerAIM");
    addConfigVariable(g, "ServerICQ");
    addConfigVariable(g, "ServerStreamURL");
    addConfigVariable(g, "ServerDescription");
    addConfigVariable(g, "ServerName");
    addConfigVariable(g, "ServerGenre");
    addConfigVariable(g, "AutomaticReconnectSecs");
    addConfigVariable(g, "Encode");
    addConfigVariable(g, "BitrateNominal");
    addConfigVariable(g, "BitrateMin");
    addConfigVariable(g, "BitrateMax");
    addConfigVariable(g, "NumberChannels");
    addConfigVariable(g, "Samplerate");
    addConfigVariable(g, "OggQuality");
    addConfigVariable(g, "OggBitrateQualityFlag");
}

void LogMessage(edcastGlobals *g, int type, char *source, int line, char *fmt, ...) {
	va_list parms;
	char	errortype[25] = "";
	int	addNewline = 1;
	struct tm *tp;
	time_t t;
	int parseableOutput = 0;
	char    timeStamp[255];

	char	sourceLine[1024] = "";

	char *p1 = NULL;

	p1 = strrchr(source, '\\');

	if (p1) {
		strcpy(sourceLine, p1+1);
	}
	else {
		strcpy(sourceLine, source);
	}
	memset(timeStamp, '\000', sizeof(timeStamp));

	time(&t);
	tp = localtime(&t);
	strftime(timeStamp, sizeof(timeStamp), "%m/%d/%y %H:%M:%S", tp);
	
	switch (type) {
		case LM_ERROR:
			strcpy(errortype, "Error");
			break;
		case LM_INFO:
			strcpy(errortype, "Info");
			break;
		case LM_DEBUG:
			strcpy(errortype, "Debug");
			break;
		default:
			strcpy(errortype, "Unknown");
			break;
	}

	bool complain=((type<=LM_ERROR)&&acfg_logerror.get_static_instance().get_state())||
		((type<=LM_INFO)&&acfg_loginfo.get_static_instance().get_state())||
		((type<=LM_DEBUG)&&acfg_logdebug.get_static_instance().get_state());

	if (complain) {
		char_t	loc[1024] = "";
		char_t	buffer[1024] = "";
		sprintf(loc,  "%s %s(%s:%d): ", timeStamp, errortype, sourceLine, line);
		va_start(parms, fmt);
		vsprintf(buffer, fmt, parms);
		va_end(parms);
		strcat(loc,buffer);
		switch (type) {
			case LM_ERROR:
				console::error(loc);
				break;
			case LM_INFO:
				console::info(loc);
				break;
			case LM_DEBUG:
				console::print(loc);	
				break;
		}
	}
}

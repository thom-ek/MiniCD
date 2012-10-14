#include<exec/types.h>

#define STAT_NODISK 0
#define STAT_PLAYING 1
#define STAT_STOPPED 2
#define STAT_PAUSED 3
#define STAT_DATADISK 4

extern TEXT ActTime[];
extern UBYTE ActTimeType;
extern struct MsgPort     *TimerMP;

extern TEXT CONF_SongPath[];
extern TEXT CONF_SCSIDevice[];
extern UBYTE CONF_SCSIUnit;
extern UBYTE CONF_AutoPlay;
extern UBYTE CONF_AutoEject;

int TimerInit(void);
void TimerExit(void);
char SCSI_Init(void);
void SCSI_Exit(void);
void SCMD_PlayAudio(int);
void SCMD_PauseResume(void);
void SCMD_Resume(void);
void SCMD_Eject(void);
void SCMD_Load(void);
void SCMD_Stop(void);
void SCMD_SetVolume(int);
void SCMD_ReadTOC(void);
void SCMD_ReadTitleTime(int);
void SCMD_Jump(int);
int SCMD_GetStatus(void);

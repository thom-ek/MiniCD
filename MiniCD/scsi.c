#include<proto/exec.h>
#include<proto/dos.h>
#include<exec/memory.h>
#include<stdio.h>
#include<string.h>
#include<devices/scsidisk.h>
#include<gadtoolsbox/gadtoolsbox.h>
#include"scsi.h"

void SCMD_GetCDROMInfo(void);

#define min(x,y) (((x)>(y))?(y):(x))
#define max(x,y) (((x)>(y))?(x):(y))

#define BYTES_PER_LINE  16
#define SENSE_LEN 252
#define MAX_DATA_LEN 252
#define MAX_TOC_LEN 804    /* max TOC size = 100 TOC track descriptors */
#define PAD 0
#define LINE_BUF (128)
#define NUM_OF_CDDAFRAMES 75  /* 75 frames per second audio */
#define CDDALEN 2448    /* 1 frame has max. 2448 bytes (subcode 2) */

#define OFFS_KEY 2
#define OFFS_CODE 12

typedef struct MsgPort MSGPORT;
typedef struct IOStdReq IOSTDREQ;
typedef struct List LIST;
typedef struct Node NODE;
typedef struct SCSICmd SCSICMD;

typedef struct
{
	BYTE	code;
	UBYTE  *ptr;
} IDTOSTRING;

typedef struct
{
	UBYTE  opcode;
	UBYTE  b1;
	UBYTE  b2;
	UBYTE  b3;
	UBYTE  b4;
	UBYTE  control;
} SCSICMD6;

typedef struct
{
	UBYTE  opcode;
	UBYTE  b1;
	UBYTE  b2;
	UBYTE  b3;
	UBYTE  b4;
	UBYTE  b5;
	UBYTE  b6;
	UBYTE  b7;
	UBYTE  b8;
	UBYTE  control;
} SCSICMD10;

typedef struct
{
	UBYTE  opcode;
	UBYTE  b1;
	UBYTE  b2;
	UBYTE  b3;
	UBYTE  b4;
	UBYTE  b5;
	UBYTE  b6;
	UBYTE  b7;
	UBYTE  b8;
	UBYTE  b9;
	UBYTE  b10;
	UBYTE  control;
} SCSICMD12;

#define  SCSI_CMD_TUR	0x00
#define  SCSI_CMD_RZU	0x01
#define  SCSI_CMD_RQS	0x03
#define  SCSI_CMD_FMU	0x04
#define  SCSI_CMD_RAB	0x07
#define  SCSI_CMD_RD 0x08
#define  SCSI_CMD_WR 0x0A
#define  SCSI_CMD_SK 0x0B
#define  SCSI_CMD_INQ	0x12
#define  SCSI_CMD_MSL	0x15
#define  SCSI_CMD_RU 0x16
#define  SCSI_CMD_RLU	0x17
#define  SCSI_CMD_MSE	0x1A
#define  SCSI_CMD_SSU	0x1B
#define  SCSI_CMD_RDI	0x1C
#define  SCSI_CMD_SDI	0x1D
#define SCSI_CMD_PAMR	0x1E
#define  SCSI_CMD_RCP	0x25
#define  SCSI_CMD_RXT	0x28
#define  SCSI_CMD_WXT	0x2A
#define  SCSI_CMD_SKX	0x2B
#define  SCSI_CMD_WVF	0x2E
#define  SCSI_CMD_VF 0x2F
#define  SCSI_CMD_RDD	0x37
#define  SCSI_CMD_WDB	0x3B
#define  SCSI_CMD_RDB	0x3C

#define SCSI_CMD_COPY		0x18
#define SCSI_CMD_COMPARE	0x39
#define SCSI_CMD_COPYANDVERIFY	0x3A
#define SCSI_CMD_CHGEDEF	0x40
#define SCSI_CMD_READSUBCHANNEL  0x42
#define SCSI_CMD_READTOC	0x43
#define SCSI_CMD_READHEADER	0x44
#define SCSI_CMD_PLAYAUDIO12  0xA5
#define SCSI_CMD_PLAYAUDIOTRACKINDEX	0x48
#define SCSI_CMD_PAUSERESUME	 0x4B

UBYTE *scsi_data = NULL;
UBYTE *toc_buf = NULL;

UBYTE buffer[LINE_BUF];

int   ActStat = 0;   /* 0=No Disk; 1=Playing; 2=Stopped; 3=Paused; 4=Datadisk */
int   ActTitle = 0;

char  SetNextTitle = 0;

MSGPORT *mp_ptr;
IOSTDREQ *io_ptr;
SCSICMD scsi_cmd;
UBYTE *scsi_sense;

UBYTE TOC_NumTracks = 0;
UBYTE TOC_Flags[100]; /* 0=Musik, 1=Daten */
ULONG TOC_Addr[100];
STRPTR TOC_Title[100];
TEXT TOC_CDTitle[128];
TEXT TOC_CDInterpret[128];
TEXT TOC_TitleStrs[4000];
TEXT TOC_CDID[20];

TEXT CONF_SongPath[256];
TEXT CONF_SCSIDevice[256];
UBYTE CONF_SCSIUnit; 
UBYTE CONF_AutoPlay;
UBYTE CONF_AutoEject;

struct timerequest *TimerIO = NULL;
struct MsgPort     *TimerMP = NULL;

void SCSI_Exit( void )
{
	if (io_ptr)
	{
		CloseDevice ((struct IORequest *) io_ptr);
		DeleteStdIO (io_ptr);
	}
	if (mp_ptr)
		DeletePort (mp_ptr);
	if (scsi_sense)
		FreeVec(scsi_sense);
	if (toc_buf)
		FreeVec(toc_buf);
	if (scsi_data)
		FreeVec(scsi_data);
}

char SCSI_Init(void)
{
	if ((scsi_data = (UBYTE *) AllocVec (MAX_DATA_LEN, MEMF_CHIP | MEMF_CLEAR)) == NULL)
		return FALSE;
	if ((toc_buf = (UBYTE *) AllocVec (MAX_TOC_LEN, MEMF_CHIP)) == NULL)
		return FALSE;
	if ((scsi_sense = (UBYTE *) AllocVec (SENSE_LEN, MEMF_CHIP || MEMF_CLEAR)) == NULL)
		return FALSE;
	if ((mp_ptr = (MSGPORT *) CreatePort (NULL, 0)) == NULL)
		return FALSE;
	if ((io_ptr = (IOSTDREQ *) CreateStdIO (mp_ptr)) == NULL)
		return FALSE;
	if (OpenDevice (CONF_SCSIDevice, CONF_SCSIUnit, (struct IORequest *) io_ptr, 0) != 0)
		return FALSE;
	return TRUE;
}

int TimerInit(void)
{
	if (TimerMP = CreatePort(0,0))
		if (TimerIO = (struct timerequest *) CreateExtIO(TimerMP, sizeof(struct timerequest)))
			if (OpenDevice( TIMERNAME, UNIT_VBLANK, (struct IORequest *)TimerIO, 0L)==NULL)
				return( 1 );
	return( 0 );
}

void TimerExit(void)
{
	if (TimerIO)
	{
		CloseDevice((struct IORequest *)TimerIO);
		DeleteExtIO((struct IORequest *)TimerIO);
	}
	if (TimerMP)
	{
		DeletePort(TimerMP);
	}
}

#define CD_BLOCK_OFFSET 150
#define CD_SECS 60
#define CD_FRAMES 75

void lba2msf (ULONG lba, UBYTE *m, UBYTE *s, UBYTE *f)
{   
	ULONG tmp;

	tmp = lba + CD_BLOCK_OFFSET;	/* offset of first logical frame */
	tmp &= 0xffffff;		/* negative lbas use only 24 bits */
	*m = tmp / (CD_SECS * CD_FRAMES);
	tmp %= (CD_SECS * CD_FRAMES);
	*s = tmp / CD_FRAMES;
	*f = tmp % CD_FRAMES;
}

int DoScsiCmd(UBYTE * data, int datasize, UBYTE * cmd, int cmdsize, UBYTE flags)
{
	io_ptr->io_Length = sizeof (SCSICMD);
	io_ptr->io_Data = (APTR) & scsi_cmd;
	io_ptr->io_Command = HD_SCSICMD;

	scsi_cmd.scsi_Data = (APTR) data;
	scsi_cmd.scsi_Length = datasize;
	scsi_cmd.scsi_SenseActual = 0;
	scsi_cmd.scsi_SenseData = scsi_sense;
	scsi_cmd.scsi_SenseLength = SENSE_LEN;
	scsi_cmd.scsi_Command = cmd;
	scsi_cmd.scsi_CmdLength = cmdsize;
	scsi_cmd.scsi_Flags = flags;

	(void) DoIO ((struct IORequest *) io_ptr);

	return (io_ptr->io_Error);
}

int DoScsiCmdAsync(UBYTE * data, int datasize, UBYTE * cmd, int cmdsize, UBYTE flags)
{
	io_ptr->io_Length = sizeof (SCSICMD);
	io_ptr->io_Data = (APTR) & scsi_cmd;
	io_ptr->io_Command = HD_SCSICMD;

	scsi_cmd.scsi_Data = (APTR) data;
	scsi_cmd.scsi_Length = datasize;
	scsi_cmd.scsi_SenseActual = 0;
	scsi_cmd.scsi_SenseData = scsi_sense;
	scsi_cmd.scsi_SenseLength = SENSE_LEN;
	scsi_cmd.scsi_Command = cmd;
	scsi_cmd.scsi_CmdLength = cmdsize;
	scsi_cmd.scsi_Flags = flags;

	(void) SendIO ((struct IORequest *) io_ptr);

	return (io_ptr->io_Error);
}

char SCMD_Inquiry ( void )
{
	static SCSICMD6 command =
	{
		SCSI_CMD_INQ,
		PAD,
		PAD,
		PAD,
		0,
		PAD
	};
	static int err;

	command.b4 = MAX_DATA_LEN;

	if ((err = DoScsiCmd ((UBYTE *) scsi_data, MAX_DATA_LEN,
						  (UBYTE *) & command, sizeof (command),
						  (SCSIF_READ | SCSIF_AUTOSENSE))) == 0)
	{
		return ((char)((scsi_data[0] & 0x1F)==0x05));
	}
	return( FALSE );
}

void SCMD_PlayAudio(int starttrack)
{
	static SCSICMD12 command =
	{
		SCSI_CMD_PLAYAUDIO12,
		PAD,
		0, 0, 0, 0,
		0, 0, 0, 0,
		PAD,
		PAD
	};

	ULONG Addr;

// skip data tracks
	while( (TOC_Flags[starttrack-1]&1)&&(starttrack<=TOC_NumTracks) ) starttrack++;

	if(starttrack>TOC_NumTracks)
	{
		SCMD_Stop();
		return;
	}

	Addr = TOC_Addr[starttrack-1];

	command.b2 = (Addr&0xFF000000)>>24;
	command.b3 = (Addr&0x00FF0000)>>16;
	command.b4 = (Addr&0x0000FF00)>>8;
	command.b5 = (Addr&0x000000FF);

	Addr = TOC_Addr[TOC_NumTracks]-TOC_Addr[starttrack-1];

	command.b6 = (Addr&0xFF000000)>>24;
	command.b7 = (Addr&0x00FF0000)>>16;
	command.b8 = (Addr&0x0000FF00)>>8;
	command.b9 = (Addr&0x000000FF);

	DoScsiCmd ((UBYTE *) scsi_data, MAX_DATA_LEN,
			   (UBYTE *) & command, sizeof (command),
			   (SCSIF_READ | SCSIF_AUTOSENSE));
}

void SCMD_PauseResume( void )
{
	static SCSICMD10 command =
	{
		SCSI_CMD_PAUSERESUME,
		PAD,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		PAD
	};

	command.b8 = (ActStat==1)?0x00:0x01;

	DoScsiCmd (0, 0,
		  (UBYTE *) & command, sizeof (command),
		  (SCSIF_READ | SCSIF_AUTOSENSE));
//	DrawPrgmSymb( (ActStat==1)?3:1 );
}

void SCMD_Resume( void )
{
	static SCSICMD10 command =
	{
		SCSI_CMD_PAUSERESUME,
		PAD,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		PAD
	};

	command.b8 = 0x01;

	DoScsiCmd (0, 0,
		  (UBYTE *) & command, sizeof (command),
		  (SCSIF_READ | SCSIF_AUTOSENSE));
//	DrawPrgmSymb( (ActStat==1)?3:1 );
}

void SCMD_Eject( void )
{
	static SCSICMD6 command =
	{
		SCSI_CMD_SSU,
		0,
		PAD,
		PAD,
		0,
		PAD
	};

	int err;

	command.b4 = 2;

	if ((err = DoScsiCmd (0, 0,
		 (UBYTE *) & command, sizeof (command),
		 (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
	{
		return;
	}
}

void SCMD_Load( void )
{
	static SCSICMD6 command =
	{
		SCSI_CMD_SSU,
		0,
		PAD,
		PAD,
		0,
		PAD
	};

	int err;

	command.b4 = 3;

	if ((err = DoScsiCmd (0, 0,
		 (UBYTE *) & command, sizeof (command),
		 (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
	{
		return;
	}
}

void SCMD_Stop( void )
{
	static SCSICMD6 command =
	{
		SCSI_CMD_SSU,
		0 ,
		PAD ,
		PAD ,
		0 ,
		PAD
	};

	command.b4=0;

	ActTitle=0;

	DoScsiCmd (0, 0,
		  (UBYTE *) & command, sizeof (command),
		  (SCSIF_READ | SCSIF_AUTOSENSE));
}

/*
typedef u_int8_t UBYTE;

struct scsi_blk_desc {
	u_int8_t density;
	u_int8_t nblocks[3];
	u_int8_t reserved;
	u_int8_t blklen[3];
};

struct scsi_mode_header {
	u_int8_t data_length;
	u_int8_t medium_type;
	u_int8_t dev_spec;
	u_int8_t blk_desc_len;
};


union scsi_cd_pages {
	struct scsi_cd_write_params_page write_params;
	struct cd_audio_page audio;
};

struct cd_audio_page {
	u_int8_t page_code;
#define		CD_PAGE_CODE	0x3F
#define		AUDIO_PAGE	0x0e
#define		CD_PAGE_PS	0x80
	u_int8_t param_len;
	u_int8_t flags;
#define		CD_PA_SOTC	0x02
#define		CD_PA_IMMED	0x04
	u_int8_t unused[2];
	u_int8_t format_lba;
#define		CD_PA_FORMAT_LBA 0x0F
#define		CD_PA_APR_VALID	0x80
	u_int8_t lb_per_sec[2];
	struct port_control {
		u_int8_t channels;
#define	CHANNEL 0x0F
#define	CHANNEL_0 1
#define	CHANNEL_1 2
#define	CHANNEL_2 4
#define	CHANNEL_3 8
#define		LEFT_CHANNEL	CHANNEL_0
#define		RIGHT_CHANNEL	CHANNEL_1
#define		MUTE_CHANNEL	0x0
#define		BOTH_CHANNEL	LEFT_CHANNEL | RIGHT_CHANNEL
		u_int8_t volume;
	} port[4];
#define	LEFT_PORT	0
#define	RIGHT_PORT	1
};

struct scsi_cd_mode_data {
	struct scsi_mode_header header;
	struct scsi_blk_desc blk_desc;
	union scsi_cd_pages page;
};
*/

BOOL SCMD_IsDataDisk(void)
{
	static int err;
	static SCSICMD6 modecommand;
	static struct volmodedata
	{
		UBYTE data_length;
		UBYTE medium_type;
		UBYTE dev_spec;
		UBYTE blk_desc_len;
//		UBYTE density;
//		UBYTE nblocks[3];
//		UBYTE reserved;
//		UBYTE blklen[3];
		UBYTE page;  /* page code 0x0E */
		UBYTE plength;  /* page length */
		UBYTE flags; /* bit 2: Immed, bit 1: SOTC */
		UBYTE unused[2];
		UBYTE format_lba;	/* bit 7: APRVal, bit 3-0: format of LBAs / Sec. */
		UWORD bps;   /* logical blocks per second audio playback */
		UBYTE out0;  /* lower 4 bits: output port 0 channel selection */
		UBYTE vol0;  /* output port 0 volume */
		UBYTE out1;  /* lower 4 bits: output port 1 channel selection */
		UBYTE vol1;  /* output port 1 volume */
		UBYTE out2;  /* lower 4 bits: output port 2 channel selection */
		UBYTE vol2;  /* output port 2 volume */
		UBYTE out3;  /* lower 4 bits: output port 3 channel selection */
		UBYTE vol3;  /* output port 3 volume */
	} modedata;

	memset(&modedata,0,sizeof(struct volmodedata));
	memset(&modecommand,0,sizeof(SCSICMD6));

	modecommand.opcode  = SCSI_CMD_MSE;
	modecommand.b1   = 0;
	modecommand.b2   = 0x0E;
	modecommand.b3   = 0;
	modecommand.b4   = sizeof(struct volmodedata)&0xff;
	modecommand.control = 0;

	if ((err = DoScsiCmd ((UBYTE *) &modedata, sizeof(modedata),
						  (UBYTE *) &modecommand, sizeof (modecommand),
						  (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
		return FALSE;
	if((modedata.medium_type==0x01) || (modedata.medium_type==0x04) || (modedata.medium_type==0x05) || (modedata.medium_type==0x08))
		return TRUE;
	return FALSE;
}

/* volume 1 - 31 */
void SCMD_SetVolume (int volume)
{
	static int err, i, j;
	static SCSICMD6 modecommand;
	static struct volmodedata
	{
		UBYTE data_length;
		UBYTE medium_type;
		UBYTE dev_spec;
		UBYTE blk_desc_len;
//		UBYTE density;
//		UBYTE nblocks[3];
//		UBYTE reserved;
//		UBYTE blklen[3];
		UBYTE page;  /* page code 0x0E */
		UBYTE plength;  /* page length */
		UBYTE flags; /* bit 2: Immed, bit 1: SOTC */
		UBYTE unused[2];
		UBYTE format_lba;	/* bit 7: APRVal, bit 3-0: format of LBAs / Sec. */
		UWORD bps;   /* logical blocks per second audio playback */
		UBYTE out0;  /* lower 4 bits: output port 0 channel selection */
		UBYTE vol0;  /* output port 0 volume */
		UBYTE out1;  /* lower 4 bits: output port 1 channel selection */
		UBYTE vol1;  /* output port 1 volume */
		UBYTE out2;  /* lower 4 bits: output port 2 channel selection */
		UBYTE vol2;  /* output port 2 volume */
		UBYTE out3;  /* lower 4 bits: output port 3 channel selection */
		UBYTE vol3;  /* output port 3 volume */
	} modedata,modemaskdata;

	SCMD_GetCDROMInfo();

	memset(&modedata,0,sizeof(struct volmodedata));
	memset(&modemaskdata,0,sizeof(struct volmodedata));
	memset(&modecommand,0,sizeof(SCSICMD6));

	modecommand.opcode  = SCSI_CMD_MSE;
	modecommand.b1   = 0;
	modecommand.b2   = 0x0E;
	modecommand.b3   = 0;
	modecommand.b4   = sizeof(struct volmodedata)&0xff; //MAX_DATA_LEN;
	modecommand.control = 0;

	if ((err = DoScsiCmd ((UBYTE *) &modedata, sizeof(modedata),
						  (UBYTE *) &modecommand, sizeof (modecommand),
						  (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
		return;
	modecommand.b2  = 0x4e;
	if ((err = DoScsiCmd ((UBYTE *) &modemaskdata, sizeof(modedata),
						  (UBYTE *) &modecommand, sizeof (modecommand),
						  (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
		return;

/*	for (j = (scsi_data[0]+1), i = scsi_data[3] + 4; i < j; i += scsi_data[i+1] + 2)
		memcpy (&modedata.page, &scsi_data[i], 16);

	modedata.page = 0x0e;
	modedata.plength = 0x0e;

	if(volume==-1)
	{
		volume=(modedata.vol0+modedata.vol1-2)/16;
		volume=max(1,min(31,volume));
	}
*/
//	printf("medium: %d\n",modedata.medium_type);
//	printf("out0: %d %d %d %d, vol: %d %d %d %d\n",modedata.out0,modedata.out1,modedata.out2,modedata.out3,modedata.vol0,modedata.vol1,modedata.vol2,modedata.vol3);
//	printf("out0: %d %d %d %d, vol: %d %d %d %d\n",modemaskdata.out0,modemaskdata.out1,modemaskdata.out2,modemaskdata.out3,modemaskdata.vol0,modemaskdata.vol1,modemaskdata.vol2,modemaskdata.vol3);

	modedata.out0 = 1;
	modedata.vol0 = 0;
	modedata.out1 = 2;
	modedata.vol1 = 0;//volume*8+1;
	modedata.out2 = 0;
	modedata.vol2 = 0;//volume*8+1;
	modedata.out3 = 0;
	modedata.vol3 = 0;//volume*8+1;

//	printf("out0: %d %d %d %d, vol: %d %d %d %d\n",modedata.out0,modedata.out1,modedata.out2,modedata.out3,modedata.vol0,modedata.vol1,modedata.vol2,modedata.vol3);

	modecommand.opcode = SCSI_CMD_MSL;
	modecommand.b1	 = 0x10;
	modecommand.b2	 = 0;
	modecommand.b3	 = 0;
	modecommand.b4	 = sizeof (struct volmodedata)&0xff;
	modecommand.control   = 0;
	modedata.data_length=0;

	if ((err = DoScsiCmd ((UBYTE *) &modedata, sizeof(modedata),
						  (UBYTE *) &modecommand, sizeof (modecommand),
						  (SCSIF_WRITE | SCSIF_AUTOSENSE))) != 0)
		return;
}

char *GetNextStr( char *a )
{
	char *b;

	b = a;
	while( *b!='\n' ) b++;
	*b = '\0';
	return( b+1 );
	}

void ShowCDTitle( void )
{
	int a;

	printf("Interpret: %s\n",TOC_CDInterpret);
	printf("Title: %s\n",TOC_CDTitle);
	for(a=0;a<TOC_NumTracks;a++)
		printf("T: %d. %s\n",a,TOC_Title[a]);
	printf("Tracks: %d\n",TOC_NumTracks);
	ActTitle = 0;
}

void SCMD_ReadTOC ( void )
{
	static SCSICMD10 command =
	{
		SCSI_CMD_READTOC,
		0,
		PAD, PAD, PAD, PAD,
		0,
		0x03, 0x24,
		PAD
	};

	int err, tocsize;
	int i,s;
	UBYTE *tocptr;
	char Buffer[256];
	BPTR FH;
	BOOL DataDisk=TRUE;

	if ((err = DoScsiCmd ((UBYTE *) toc_buf, MAX_TOC_LEN,
		   (UBYTE *) & command, sizeof (command),
		   (SCSIF_READ | SCSIF_AUTOSENSE))) == 0)
	{
		tocsize = (toc_buf[0] << 8) | toc_buf[1];	 /* first word encodes length */

		/* TOC_NumTracks = toc_buf[3]; */
		TOC_Addr[2] = 0;
		TOC_NumTracks = 0;
		if (tocsize >= 2)  /* TOC Data Length - FTN - LTN */
	   tocsize -= 2;
		for (tocptr = &toc_buf[4]; tocptr < (&toc_buf[4] + tocsize); tocptr += 8)
		{
			TOC_Addr[TOC_NumTracks] = (tocptr[4] << 24) | (tocptr[5] << 16) | (tocptr[6] << 8) | (tocptr[7]);
	   	// printf ("Track number: %d ", tocptr[2]);
			TOC_Flags[TOC_NumTracks] = (tocptr[1] & 0x04) ? 1 : 0;
			TOC_NumTracks++;
		}
		TOC_NumTracks--;

//		for(s=0;s<TOC_NumTracks;s++) if(TOC_Flags[s]!=1) DataDisk=FALSE;
//		if(DataDisk) ActStat=STAT_DATADISK;
		if(SCMD_IsDataDisk()) ActStat=STAT_DATADISK;

//		printf("TotalDisc: %2d:%2d\n", TOC_Addr[TOC_NumTracks]/75/60,(TOC_Addr[TOC_NumTracks]/75)%60 );
		sprintf(TOC_CDID,"ID%02d%06X%06X", TOC_NumTracks, TOC_Addr[2], TOC_Addr[TOC_NumTracks] );

		sprintf( Buffer, CONF_SongPath );
		AddPart( Buffer, TOC_CDID, 256 );

		if (FH = Open( Buffer, MODE_OLDFILE ))
		{
			FGets(FH,Buffer,4000);
			Buffer[strlen(Buffer)-1]='\0';
			strcpy(TOC_CDInterpret,Buffer);
			FGets(FH,Buffer,4000);
			Buffer[strlen(Buffer)-1]='\0';
			strcpy(TOC_CDTitle, Buffer);
			for (i=0; i<TOC_NumTracks; i++)
			{
				FGets(FH,Buffer,4000);
				Buffer[strlen(Buffer)-1]='\0';
				sprintf(TOC_Title[i],"%2d. %s\0",i+1,Buffer);
			}
			Close( FH );
		}
		else
		{
			strcpy( TOC_CDTitle, "<Unknown CD>" );
			strcpy( TOC_CDInterpret, TOC_CDID );
			for (i=0; i<TOC_NumTracks; i++)
				sprintf( TOC_Title[i],"%2d. <Unknown Title>",i+1);
		}
	}
}

void SCMD_Jump( int blocks )
{
	static SCSICMD10 command1 =
	{
		SCSI_CMD_READSUBCHANNEL,
		0,
		0x40,
		0,
		PAD,
		PAD,
		0,
		0,0,
		PAD
	};

	static SCSICMD12 command2 =
	{
		SCSI_CMD_PLAYAUDIO12,
		PAD,
		0, 0, 0, 0,
		0, 0, 0, 0,
		PAD,
		PAD
	};

	int err;
	int addr;

	command1.b2 = 0x40;
	command1.b3 = 1;
	command1.b6 = 0;

	command1.b7 = 255;
	command1.b8 = 255;

	if ((err = DoScsiCmd ((UBYTE *) scsi_data, MAX_DATA_LEN,
						  (UBYTE *) &command1, sizeof (command1),
						  (SCSIF_READ | SCSIF_AUTOSENSE))) == 0)
	{
		addr = ((scsi_data[8] << 16) | (scsi_data[9] << 16) | (scsi_data[10] << 8) | (scsi_data[11]) );
		addr += blocks;

		if ((addr>=TOC_Addr[ActTitle-1])&&(addr<TOC_Addr[ActTitle]))
		{
			command2.b2 = (addr&0xFF000000)>>24;
			command2.b3 = (addr&0x00FF0000)>>16;
			command2.b4 = (addr&0x0000FF00)>>8;
			command2.b5 = (addr&0x000000FF);

			addr = TOC_Addr[ActTitle]-TOC_Addr[ActTitle-1]-1;

			command2.b6 = (addr&0xFF000000)>>24;
			command2.b7 = (addr&0x00FF0000)>>16;
			command2.b8 = (addr&0x0000FF00)>>8;
			command2.b9 = (addr&0x000000FF);

			DoScsiCmd((UBYTE *) scsi_data, MAX_DATA_LEN,
					   (UBYTE *) & command2, sizeof (command2),
					   (SCSIF_READ | SCSIF_AUTOSENSE));
		}
	}
}

ULONG sign=TRUE,prevaddr=0;

void SCMD_ReadTitleTime( int settimer )
{
	static SCSICMD10 command =
	{
		SCSI_CMD_READSUBCHANNEL,
		0,
		0x40,
		0,
		PAD,
		PAD,
		0,
		0,0,
		PAD
	};

	int err;
	int microsleft = 1000000;
	ULONG addr=0L,tmp;
	UBYTE mins,secs,frames;

	command.b2 = 0x40;
	command.b3 = 1;
	command.b6 = 0;

	command.b7 = 255;
	command.b8 = 255;

	if ((err = DoScsiCmd ((UBYTE *) scsi_data, MAX_DATA_LEN,
						  (UBYTE *) &command, sizeof (command),
						  (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
	{
		ActStat=STAT_NODISK;
		ActTitle=0;
		sprintf(ActTime,"NoDisk");
	}
	else if ((scsi_data[1] == 0x11) || (scsi_data[1] == 0x12))
	{
		switch(scsi_data[1])
		{
			case 0x11: ActStat=STAT_PLAYING; break;
			case 0x12: ActStat=STAT_PAUSED; break;
			default: ActStat=STAT_STOPPED; break;
		}
		if(ActStat==STAT_NODISK) SCMD_ReadTOC();

		if(ActStat!=STAT_STOPPED)
		{
			ActTitle = scsi_data[6];
// Actual Track
			addr = ((scsi_data[12] << 24) | (scsi_data[13] << 16) | (scsi_data[14] << 8) | (scsi_data[15]) );
			lba2msf(addr,&mins,&secs,&frames);
			tmp=((ULONG)((ULONG)mins<<16)|((ULONG)secs<<8));
//			printf("Tmp: %ld; %d:%d:%d\n",tmp,mins,secs,frames);
			if(tmp>=prevaddr) sign=TRUE; else sign=FALSE;
			prevaddr=tmp;
			microsleft = (ActStat==STAT_PLAYING)?((75 - (addr%75))*13333 + 25):1000000;
			if(ActTimeType==0)
			if(sign || ActStat==STAT_PAUSED) sprintf(ActTime," %02.2d  %02.2d:%02.2d",ActTitle,mins,secs);
			else sprintf(ActTime," %02.2d -%02.2d:%02.2d",ActTitle,mins,secs);

// Actual CD
			addr = addr + TOC_Addr[scsi_data[6]-1];
			lba2msf(addr,&mins,&secs,&frames);
			if(ActTimeType==1) sprintf(ActTime," %02.2d  %02.2d:%02.2d",ActTitle,mins,secs);

// Remain Track
			addr = TOC_Addr[scsi_data[6]]-addr;
			lba2msf(addr,&mins,&secs,&frames);
			if(ActTimeType==2) sprintf(ActTime," %02.2d  %02.2d:%02.2d",ActTitle,mins,secs);

// Remain CD
			addr = ((scsi_data[12] << 24) | (scsi_data[13] << 16) | (scsi_data[14] << 8) | (scsi_data[15]) );
			addr = addr + TOC_Addr[scsi_data[6]-1]; addr=TOC_Addr[TOC_NumTracks]-addr;
			lba2msf(addr,&mins,&secs,&frames);
			if(ActTimeType==3) sprintf(ActTime," %02.2d  %02.2d:%02.2d",ActTitle,mins,secs);
		}
		else
		{
			addr=TOC_Addr[TOC_NumTracks];
			sprintf(ActTime," %02.2d  %02.2d:%02.2d",TOC_NumTracks,addr/(75*60),(addr/75)%60);
		}
	}
	else if(ActStat!=STAT_STOPPED)
	{
		// do init
		if(ActStat==STAT_NODISK)
		{
			SCMD_ReadTOC();
		}
		if(ActStat==STAT_PLAYING && CONF_AutoEject && ActTitle==TOC_NumTracks) SCMD_Eject();
		ActStat=STAT_STOPPED;
		if(SCMD_IsDataDisk()) ActStat=STAT_DATADISK;
		addr=TOC_Addr[TOC_NumTracks];
		lba2msf(addr,&mins,&secs,&frames);
		sprintf(ActTime," %02.2d  %02.2d:%02.2d",TOC_NumTracks,mins,secs);
	}
	if(settimer)// && ActStat!=STAT_STOPPED)
	{
		TimerIO->tr_node.io_Command = TR_ADDREQUEST;
		TimerIO->tr_time.tv_secs	= microsleft/1000000;
		TimerIO->tr_time.tv_micro   = microsleft%1000000;
		SendIO((struct IORequest *)TimerIO);
	}
}

int SCMD_GetStatus()
{
	static SCSICMD10 command =
	{
		SCSI_CMD_READSUBCHANNEL,
		0,
		0x40,
		0,
		PAD,
		PAD,
		0,
		0,0,
		PAD
	};

	command.b2 = 0x40;
	command.b3 = 1;
	command.b6 = 0;

	command.b7 = 255;
	command.b8 = 255;

	if (DoScsiCmd ((UBYTE *) scsi_data, MAX_DATA_LEN,
				  (UBYTE *) &command, sizeof (command),
				  (SCSIF_READ | SCSIF_AUTOSENSE)) != 0)
	{
		return STAT_NODISK;
	}
	else
	{
		if(SCMD_IsDataDisk()) return STAT_DATADISK;
		switch(scsi_data[1])
		{
			case 0x11: return STAT_PLAYING;
			case 0x12: return STAT_PAUSED;
			default: return STAT_STOPPED;
		}
	}
}

#define swapw(x) ((x<<8)| (x>>8))

void SCMD_GetCDROMInfo()
{
	static int err, i, j;
	static SCSICMD6 modecommand;
struct atapi_cap_page {
	/* Capabilities page */
	UBYTE page_code;
	UBYTE param_len;  
	UBYTE reserved1[2];

	UBYTE cap1;
#define AUDIO_PLAY 0x01		/* audio play supported */
#define AV_COMPOSITE		/* composite audio/video supported */
#define DA_PORT1		/* digital audio on port 1 */
#define DA_PORT2		/* digital audio on port 2 */
#define M2F1			/* mode 2 form 1 (XA) read */
#define M2F2			/* mode 2 form 2 format */
#define CD_MULTISESSION		/* multi-session photo-CD */
	UBYTE cap2;
#define CD_DA		0x01	/* audio-CD read supported */
#define CD_DA_STREAM	0x02	/* CD-DA streaming */
#define RW_SUB		0x04	/* combined R-W subchannels */
#define RW_SUB_CORR	0x08	/* R-W subchannel data corrected */
#define C2_ERRP		0x10	/* C2 error pointers supported */
#define ISRC		0x20	/* can return the ISRC */
#define UPC		0x40	/* can return the catalog number UPC */
	UBYTE m_status;
#define CANLOCK		0x01	/* could be locked */
#define LOCK_STATE	0x02	/* current lock state */
#define PREVENT_JUMP	0x04	/* prevent jumper installed */
#define CANEJECT	0x08	/* can eject */
#define MECH_MASK	0xe0	/* loading mechanism type */
#define MECH_CADDY		0x00
#define MECH_TRAY		0x20
#define MECH_POPUP		0x40
#define MECH_CHANGER_INDIV	0x80
#define MECH_CHANGER_CARTRIDGE	0xa0
	UBYTE cap3;
#define SEPARATE_VOL	0x01	/* independent volume of channels */
#define SEPARATE_MUTE	0x02	/* independent mute of channels */
#define SUPP_DISK_PRESENT 0x04	/* changer can report contents of slots */
#define SSS		0x08	/* software slot selection */
	UWORD max_speed;	/* max raw data rate in bytes/1000 */
	UWORD max_vol_levels; /* number of discrete volume levels */
	UWORD buf_size;	/* internal buffer size in bytes/1024 */
	UWORD cur_speed;	/* current data rate in bytes/1000  */
	/* Digital drive output format description (optional?) */
	UBYTE reserved2;
	UBYTE dig_output; /* Digital drive output format description */
	UBYTE reserved3[2];
} capdata;

	memset(&capdata,0,sizeof(struct atapi_cap_page));
	memset(&modecommand,0,sizeof(SCSICMD6));

	modecommand.opcode  = SCSI_CMD_MSE;
	modecommand.b1   = 0;
	modecommand.b2   = 0x0E;
	modecommand.b3   = 0;
	modecommand.b4   = sizeof(struct atapi_cap_page)&0xff; //MAX_DATA_LEN;
	modecommand.control = 0;

	if ((err = DoScsiCmd ((UBYTE *) &capdata, sizeof(capdata),
						  (UBYTE *) &modecommand, sizeof (modecommand),
						  (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
		return;
//	printf("cap1: %d, cap2: %d, cap3: %d\n",capdata.cap1,capdata.cap2,capdata.cap3);
//	printf("status: %d, maxspeed: %d, maxvollev: %d, bufsize: %d, curspeed: %d\n",capdata.m_status,swapw(capdata.max_speed),swapw(capdata.max_vol_levels),swapw(capdata.buf_size),swapw(capdata.cur_speed));
}

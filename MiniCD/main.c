#include<stdio.h>
#include<string.h>
#include<proto/pm.h>
#include<proto/dos.h>
#include<proto/icon.h>
#include<proto/exec.h>
#include<proto/diskfont.h>
#include<proto/gadtools.h>
#include<proto/graphics.h>
#include<proto/listview.h>
#include<proto/intuition.h>
#include<proto/amigaguide.h>
#include<proto/commodities.h>
#include<rexx/errors.h>
#include<exec/memory.h>
#include<intuition/icclass.h>
#include<intuition/gadgetclass.h>
#include<intuition/imageclass.h>
#include<gadtoolsbox/gadtoolsbox.h>
#include"scsi.h"
#include"minicd_rev.h"

#define unless(x) if(!(x))
#define Leave(x) { if(x) printf("%s\n",x); goto end; }

TEXT VERSTR[]=VERSTAG;

#define APPNAME	"MiniCD"
#define APPTITLE VERS
#define APPDESCRIPTION "CD Audio Player"
#define APPPUBNAME "MINICD.1"
#define APPEXTENSION "minicd"
#define APPGUIDE "MiniCD.guide"

#define STAT_NODISK 0
#define STAT_PLAYING 1
#define STAT_STOPPED 2
#define STAT_PAUSED 3
#define STAT_DATADISK 4

struct gtbApplication *appl=NULL;

Object *EditorWnd=NULL;
Object *MainWnd=NULL;
Object *Timer=NULL;
Object *Cdity=NULL;
Object *CtrlC=NULL;
Object *Scrn=NULL;
Object *ARexx=NULL;
Object *AmigaGuide=NULL;
Object *IMPlay=NULL;
Object *IMStop=NULL;
Object *IMPause=NULL;
Object *IMEject=NULL;
Class *ImageClass=NULL;
Object *GadTimerDisp=NULL;
Object *GadEditor=NULL;

struct PopupMenuBase *PopupMenuBase=NULL;
struct PopupMenu *MainMenu=NULL,*TrackMenu=NULL,*TimeMenu=NULL;
struct Library *ListViewBase=NULL;
Class *ListViewClass=NULL;
struct Gadget *MainGList=NULL;
struct LVList EditorList;

struct Screen *Scr;
struct DrawInfo *DRI;

BOOL WIND_HIDDEN=FALSE;

ULONG CONF_XPos,CONF_YPos,CONF_HeightAdd;
TEXT CONF_FontName[32];
TEXT ActTime[256]="00  00:00";
UBYTE ActTimeType=0;	// 0 - AT, 1 - ACD, 2 - RT, 3 - RCD 
UBYTE CONF_Window;
extern UBYTE TOC_NumTracks;
extern UBYTE TOC_Flags[]; /* 0=Musik, 1=Daten */
extern ULONG TOC_Addr[];
extern STRPTR TOC_Title[];
extern TEXT TOC_CDTitle[];
extern TEXT TOC_CDInterpret[];
extern TEXT TOC_TitleStrs[];
extern TEXT TOC_CDID[];
extern int ActStat;   /* 0=No Disk; 1=Playing; 2=Stopped; 3=Paused; 4=Datadisk */
extern int ActTitle;

/************************************************************************************/

void ShowStatus(int);
Object *OpenWindow_Main(void);
void CloseWindow_Main(void);
Object *OpenWindow_Editor(void);
void CloseWindow_Editor(void);
void SaveTitles(void);

/************************************************************************************/

#define SAVEDS 
#define ASMFUNC __saveds __asm
#define REG(x) register __## x
typedef ULONG (*HookFunction)(void);

#define IM_PLAY 0
#define IM_STOP 1
#define IM_PAUSE 2
#define IM_EJECT 3
#define IM_NEXTTRACK 4
#define IM_PREVTRACK 5
#define IM_FREVIND 6
#define IM_FFORWARD 7

#define IMAGE_ImageNum (TAG_USER+1)

struct IMAGEData
{
	UBYTE id_ImageNum;
	UWORD id_Width,id_Height;
};

ULONG IMAGE_NEW(Class *cl,Object *o,struct opSet *ops )
{
	Object *object;
	struct IMAGEData *ID;

	object = (Object *)DoSuperMethodA( cl, o, (Msg)ops );
	if(object)
	{
		ID = INST_DATA( cl, object );

		ID->id_ImageNum = GetTagData( IMAGE_ImageNum, FALSE, ops->ops_AttrList );
		ID->id_Width = GetTagData( IA_Width, FALSE, ops->ops_AttrList );
		ID->id_Height = GetTagData( IA_Height, FALSE, ops->ops_AttrList );
	}
	return( (ULONG)object );
}

UBYTE __chip AreaBuffer[1024];
UBYTE __chip TmpRasBuffer[1024];

ULONG IMAGE_DRAW(Class *cl,Object *o,struct impDraw *imp )
{
	ULONG retval;
	struct DrawInfo *dri;
	struct RastPort *RP = imp->imp_RPort;
	struct IMAGEData *ID = INST_DATA( cl, o );
	struct Image *im;
	UBYTE DrawPen;
	struct AreaInfo AreaInfo;
	struct TmpRas TmpRas;

	im=(struct Image *)o;

	retval = DoSuperMethodA(cl, o, (Msg) imp);

	dri = imp->imp_DrInfo;

	if(imp->imp_State==IDS_NORMAL)
		DrawPen=dri->dri_Pens[BARDETAILPEN];
	else
		DrawPen=dri->dri_Pens[FILLPEN];

	SetAPen(RP,dri->dri_Pens[BARBLOCKPEN]);
	RectFill(RP,im->LeftEdge+imp->imp_Offset.X,im->TopEdge+imp->imp_Offset.Y,im->LeftEdge+imp->imp_Offset.X+ID->id_Width,im->TopEdge+imp->imp_Offset.Y+ID->id_Height);

	SetAPen(RP,DrawPen);
	switch(ID->id_ImageNum)
	{
		case IM_PLAY:
			if(ActStat==STAT_PLAYING) SetAPen(RP,dri->dri_Pens[FILLPEN]);
			InitArea(&AreaInfo,AreaBuffer,20); RP->AreaInfo=&AreaInfo;
			InitTmpRas(&TmpRas,TmpRasBuffer,1024); RP->TmpRas=&TmpRas;
			AreaMove(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 );
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			AreaEnd(RP);
			RP->AreaInfo=NULL; RP->TmpRas=NULL;
			break;
		case IM_STOP:
			if(ActStat==STAT_STOPPED || ActStat==STAT_NODISK) SetAPen(RP,dri->dri_Pens[FILLPEN]);
			InitArea(&AreaInfo,AreaBuffer,20); RP->AreaInfo=&AreaInfo;
			InitTmpRas(&TmpRas,TmpRasBuffer,1024); RP->TmpRas=&TmpRas;
			AreaMove(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			AreaEnd(RP);
			RP->AreaInfo=NULL; RP->TmpRas=NULL;
			break;
		case IM_PAUSE:
			if(ActStat==STAT_PAUSED) SetAPen(RP,dri->dri_Pens[FILLPEN]);
			Move(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			Draw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			Move(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 2, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			Draw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 2, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			Move(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			Draw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			Move(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 2, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 3);
			Draw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 2, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			break;
		case IM_EJECT:
			InitArea(&AreaInfo,AreaBuffer,20); RP->AreaInfo=&AreaInfo;
			InitTmpRas(&TmpRas,TmpRasBuffer,1024); RP->TmpRas=&TmpRas;
			AreaMove(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 0);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2    , im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 - 4);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 0);
			AreaDraw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 0);
			AreaEnd(RP);
			RP->AreaInfo=NULL; RP->TmpRas=NULL;
			Move(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 1);
			Draw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 1);
			Move(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 - 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			Draw(RP,im->LeftEdge+imp->imp_Offset.X + ID->id_Width/2 + 3, im->TopEdge+imp->imp_Offset.Y + ID->id_Height/2 + 3);
			break;
		case IM_NEXTTRACK:
		case IM_PREVTRACK:
		case IM_FREVIND:
		case IM_FFORWARD:
			break;
	}
	return( retval );
}

ULONG ASMFUNC IMAGE_Dispatcher( REG(a0) Class *cl, REG(a2) Object *o, REG(a1) Msg msg)
{
	ULONG retval;

	switch( msg->MethodID )
	{
		case OM_NEW:
			retval = IMAGE_NEW(cl, o, (struct opSet *)msg );
			break;
		case IM_DRAW:
			retval = IMAGE_DRAW(cl, o, (struct impDraw *)msg );
			break;
		default:
			retval = DoSuperMethodA(cl, o, msg);
			break;
	}
	return(retval);
}

/************************************************************************************/

int TemplateLen(struct RastPort *rp)
{
	int width,a,w;
	TEXT chars[]="0123456789",c[2]="\0";
	TEXT chars2[]=" -";
	TEXT TEMPLATE[11];
	char mw=0,mw2=0;

	for(a=0,width=0;a<10;a++)
	{
		c[0]=chars[a];
		w=TextLength(rp,c,1);
		if(w>width) { width=w; mw=chars[a]; }
	}

	for(a=0,width=0;a<2;a++)
	{
		c[0]=chars2[a];
		w=TextLength(rp,c,1);
		if(w>width) { width=w; mw2=chars2[a]; }
	}

	TEMPLATE[0]=' ';
	TEMPLATE[1]=mw;
	TEMPLATE[2]=mw;
	TEMPLATE[3]=' ';
	TEMPLATE[4]=mw2;
	TEMPLATE[5]=mw;
	TEMPLATE[6]=mw;
	TEMPLATE[7]=':';
	TEMPLATE[8]=mw;
	TEMPLATE[9]=mw;
	TEMPLATE[10]='\0';
	return TextLength(rp,TEMPLATE,strlen(TEMPLATE));
}

/************************************************************************************/

ULONG ASMFUNC PM_Dispatcher(register __a0 struct Hook *hook, register __a2 APTR object, register __a1 APTR msg)
{
	struct PopupMenu *pm=(struct PopupMenu *)object;
	ULONG (*md_Menu)(Object *,ULONG,struct PopupMenu *);

	if(pm->UserData)
	{
		md_Menu=(ULONG (*)(Object *,ULONG,struct PopupMenu *))pm->UserData;
		hook->h_Data=(APTR)md_Menu(object,NULL,pm);
	}
	return TRUE;
}

ULONG gtbOpenPopupMenu(Object *WndObj,struct PopupMenu *pm,ULONG code)
{
	ULONG retval=TRUE;
	struct Hook hook={NULL,NULL,(HOOKFUNC)PM_Dispatcher,NULL,NULL};
	struct Window *wnd;

	hook.h_Data=(APTR)&retval;
	GetAttr(WIND_Window,WndObj,(ULONG *)&wnd);
	PM_OpenPopupMenu(wnd,PM_Menu,pm,PM_Code,code,PM_MenuHandler,&hook,TAG_DONE);
	return (ULONG)hook.h_Data;
}

/************************************************************************************/

ULONG CtrlCHandler(Object *Obj,ULONG type,APTR msg)
{
	return FALSE;
}

ULONG TimerHandler(Object *Obj,ULONG type,APTR msg)
{
	GetMsg(TimerMP);
	ShowStatus(1);
	if(ActStat!=STAT_NODISK && ActStat!=STAT_DATADISK && !WIND_HIDDEN && !MainWnd)
	{
		OpenWindow_Main();
		if(CONF_AutoPlay>0)
		{
			Delay(CONF_AutoPlay*50);
			if(ActTitle==0) ActTitle=1;
			SCMD_PlayAudio(ActTitle);
		}
	}
	else
		if((ActStat==STAT_NODISK || ActStat==STAT_DATADISK) && MainWnd) CloseWindow_Main();
	return TRUE;
}

ULONG CdityHandler(Object *Obj,ULONG type,struct CxMessage *cxmsg)
{
	struct Window *Wnd;
	CxObj *broker;

	switch(type)
	{
		case CXM_IEVENT:
			if(!MainWnd)
			{
				OpenWindow_Main();
				WIND_HIDDEN=FALSE;
			}
			else
			{
				GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
				WindowToFront(Wnd);
				ScreenToFront(Wnd->WScreen);
				ActivateWindow(Wnd);
			}
			break;
		case CXM_COMMAND:
			switch(cxmsg->cx_ID)
			{
				case CXCMD_DISABLE:
					GetAttr(COMM_Broker,Obj,(ULONG *)&broker);
					ActivateCxObj(broker,0L);
					break;
				case CXCMD_ENABLE:
					GetAttr(COMM_Broker,Obj,(ULONG *)&broker);
					ActivateCxObj(broker,1L);
					break;
				case CXCMD_APPEAR:
					if(!MainWnd)
					{
						OpenWindow_Main();
						WIND_HIDDEN=FALSE;
					}
					else
					{
						GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
						WindowToFront(Wnd);
						ScreenToFront(Wnd->WScreen);
						ActivateWindow(Wnd);
					}
					break;
				case CXCMD_DISAPPEAR:
					if(MainWnd) CloseWindow_Main();
					WIND_HIDDEN=TRUE;
					break;
				case CXCMD_KILL:
				case CXCMD_UNIQUE:
					return FALSE;
			}
			break;
	}
	return TRUE;
}

/************************************************************************************/

ULONG GadPlayHandler(Object *GadgetObj,ULONG type,struct IntuiMessage *message)
{
	struct Window *Wnd;

	if(SCMD_GetStatus()==STAT_PLAYING)
	{
		if(ActTitle<TOC_NumTracks)
		{
			ActTitle++;
			SCMD_PlayAudio(ActTitle);
		}
	}
	else
	{
		if(ActTitle==0) ActTitle=1;
		SCMD_PlayAudio(ActTitle);
	}
	ShowStatus(1);

	GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
	RefreshGList(MainGList,Wnd,NULL,-1);
	return TRUE;
}

ULONG GadStopHandler(Object *GadgetObj,ULONG type,struct IntuiMessage *message)
{
	struct Window *Wnd;

	SCMD_Stop();
	ActStat=STAT_STOPPED;
	ActTitle=0;
	ShowStatus(0);

	GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
	RefreshGList(MainGList,Wnd,NULL,-1);
	return TRUE;
}

ULONG GadPauseHandler(Object *GadgetObj,ULONG type,struct IntuiMessage *message)
{
	struct Window *Wnd;

	SCMD_PauseResume();
	ShowStatus(0);

	GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
	RefreshGList(MainGList,Wnd,NULL,-1);
	return TRUE;
}

ULONG GadEjectHandler(Object *GadgetObj,ULONG type,struct IntuiMessage *message)
{
	struct Window *Wnd;

	SCMD_Eject();
	ActStat=STAT_NODISK;
	ActTitle=0;
	ShowStatus(1);

	GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
	RefreshGList(MainGList,Wnd,NULL,-1);
	return TRUE;
}

ULONG MenuHideHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	return 2;
}

ULONG MenuQuitHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	return FALSE;
}

ULONG MenuAboutHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	printf("About\n");
	return TRUE;
}

ULONG MenuNextTrackHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	if(ActTitle<TOC_NumTracks)
	{
		ActTitle++;
		if(ActStat==STAT_PLAYING) SCMD_PlayAudio(ActTitle);
	}
	ShowStatus(1);
	return TRUE;
}

ULONG MenuPrevTrackHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	if(ActTitle>1)
	{
		ActTitle--;
		if(ActStat==STAT_PLAYING) SCMD_PlayAudio(ActTitle);
	}
	ShowStatus(1);
	return TRUE;
}

ULONG MenuTrackNumHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	SCMD_PlayAudio(pm->ID+1);
	ShowStatus(1);
	return TRUE;
}

ULONG MenuTimeActualTrackHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	ActTimeType=0;
	return TRUE;
}

ULONG MenuTimeActualCDHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	ActTimeType=1;
	return TRUE;
}

ULONG MenuTimeRemainTrackHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	ActTimeType=2;
	return TRUE;
}

ULONG MenuTimeRemainCDHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	ActTimeType=3;
	return TRUE;
}

ULONG GadArtistHandler(Object *GadgetObj,ULONG type,struct IntuiMessage *message)
{
	STRPTR buf;
	gtbGetGadgetAttr(GTST_String,GadgetObj,(ULONG *)&buf);
	strncpy(TOC_CDInterpret,buf,128);
	TOC_CDInterpret[127]='\0';
	return TRUE;
}

ULONG GadTitleHandler(Object *GadgetObj,ULONG type,struct IntuiMessage *message)
{
	STRPTR buf;
	gtbGetGadgetAttr(GTST_String,GadgetObj,(ULONG *)&buf);
	strncpy(TOC_CDTitle,buf,128);
	TOC_CDTitle[127]='\0';
	return TRUE;
}

ULONG MenuEditorHandler(Object *MenuObj,ULONG type,struct PopupMenu *pm)
{
	if(OpenWindow_Editor())
	{
		while(gtbHandleObject(Timer,EditorWnd,CtrlC,TAG_DONE));
		CloseWindow_Editor();
	}
	return TRUE;
}

/************************************************************************************/

ULONG tl=0;

ULONG MainWndHandler(Object *WindowObj,ULONG type,APTR message)
{
	struct IntuiMessage *im;
	struct Window *Wnd;
	struct TagItem Tags[256];
	ULONG a;

	switch(type)
	{
		case TYPE_INTUIMESSAGE:
			im=(struct IntuiMessage *)message;
			switch(im->Class)
			{
				case IDCMP_MOUSEBUTTONS:
					if(im->Code==MENUDOWN)
					{
						GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
						if((im->MouseX>(Wnd->BorderLeft+(Scr->BarHeight)*4)) && (im->MouseX<((Wnd->BorderLeft+(Scr->BarHeight)*4)+tl)) && (im->MouseY<Wnd->Height))
						{
							if(TrackMenu) PM_FreePopupMenu(TrackMenu); TrackMenu=NULL;
							for(a=0;a<TOC_NumTracks;a++)
							{
								Tags[a].ti_Tag=PM_Item;
								Tags[a].ti_Data=(ULONG)PM_MakeItem(PM_Title,TOC_Title[a],PM_UserData,MenuTrackNumHandler,PM_ID,a,PM_Bold,ActTitle==a+1?TRUE:FALSE,TAG_DONE);
							}
							Tags[a].ti_Tag=TAG_DONE;
							TrackMenu=PM_MakeMenu(
								PMMenuTitle("Track list"), 
								PMItem("Next track"), PM_UserData, MenuNextTrackHandler, PM_CommKey,".", PM_Disabled, (ActTitle==TOC_NumTracks || ActStat==STAT_NODISK || ActStat==STAT_STOPPED)?TRUE:FALSE, End,
								PMItem("Previous track"), PM_UserData, MenuPrevTrackHandler, PM_CommKey,",", PM_Disabled, (ActTitle==1 || ActStat==STAT_NODISK || ActStat==STAT_STOPPED)?TRUE:FALSE, End,
								PMItem("Edit titles"), PM_UserData, MenuEditorHandler, PM_CommKey,"D", PM_Disabled, (ActStat==STAT_NODISK)?TRUE:FALSE, End,
								PMBar, PMEnd,
								TAG_MORE, (ULONG)&Tags);
							if(TrackMenu) return gtbOpenPopupMenu(MainWnd,TrackMenu,im->Code);
						}
						else
						if((im->MouseX>0) && (im->MouseX>((Wnd->BorderLeft+(Scr->BarHeight)*4)+tl)) && (im->MouseX<Wnd->Width) && (im->MouseY<Wnd->Height))
							return gtbOpenPopupMenu(MainWnd,TimeMenu,im->Code);
						else
						if((im->MouseX>0) && (im->MouseX<Wnd->Width) && (im->MouseY<Wnd->Height))
							return gtbOpenPopupMenu(MainWnd,MainMenu,im->Code);
					}
					break;
				case IDCMP_RAWKEY:
					switch(im->Code)
					{
						case 0x38:  // < key down
							SCMD_Jump(-750);
							break;
						case 0x39:  // > key down
							SCMD_Jump(750);
							break;
						case 0xb8:  // < key up
							break;
						case 0xb9:  // > key up
							break;
						case 0x5f:	// Help key up
							gtbSendAmigaGuideCmd(AmigaGuide,"ALINK main");
							break;
					}
					break;
			}
			break;
	}
	return TRUE;
}

int OldActStat=0;

void ShowStatus(int n)
{
	struct Window *Wnd;

	SCMD_ReadTitleTime(n);
	if(MainWnd)
	{
		GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
		SetAPen(Wnd->RPort,DRI->dri_Pens[BARBLOCKPEN]);
		RectFill(Wnd->RPort,Wnd->BorderLeft+(Scr->BarHeight)*4,Wnd->BorderTop,Wnd->Width-Wnd->BorderRight-(CONF_Window?1:0),Wnd->Height-Wnd->BorderBottom-(CONF_Window?1:0));
		gtbSetGadgetAttrs(GadTimerDisp,MainWnd,GTTX_Text,ActTime,TAG_DONE);
		if(OldActStat!=ActStat)
		{
			RefreshGList(MainGList,Wnd,NULL,-1);
			OldActStat=ActStat;
		}
	}
}

/************************************************************************************/

struct gtbRexxCommand commands[]=
{
	{"mcd_quit",0,NULL},
	{"mcd_hide",1,NULL},
	{"mcd_about",2,NULL},
	{"mcd_help",3,NULL},
	{"mcd_play",4,"TRACK/N"},
	{"mcd_stop",5,NULL},
	{"mcd_pause",6,NULL},
	{"mcd_eject",7,NULL},
	NULL
};

ULONG ARexxHandler(Object *Obj,struct gtbRexxCommand *rxcmd,struct gtbRexxMessage *rxmsg)
{
	if(rxcmd)
	switch(rxcmd->rc_ID)
	{
		case 0:
			return FALSE;
		case 1:
			return 2;
		case 2:
			rxmsg->rm_Result="MiniCD v1.3 by Tomasz Muszynski";
			break;
		case 3:
			gtbSendAmigaGuideCmd(AmigaGuide,"ALINK main");
			break;
		case 4:
			ActTitle=*((long *)rxmsg->rm_ArgList[0]);
			GadPlayHandler(NULL,NULL,NULL);
			break;
		case 5:
			GadStopHandler(NULL,NULL,NULL);
			break;
		case 6:
			GadPauseHandler(NULL,NULL,NULL);
			break;
		case 7:
			GadEjectHandler(NULL,NULL,NULL);
			break;
	}
	return TRUE;
}

/************************************************************************************/

struct TextAttr TextAttr={NULL,0,FS_NORMAL,FPF_DISKFONT};
struct TextFont *TFont=NULL;

Object *OpenWindow_Main(void)
{
	struct Window *Wnd;
	Object *prev,*DepthGad;
	int tlen;
	struct RastPort rp;

	if((TextAttr.ta_YSize>0 && TextAttr.ta_Name[0]!='\0'))
	{
		InitRastPort(&rp);
		SetFont(&rp,TFont);
		tlen=TemplateLen(&rp);
	}
	else
		tlen=TemplateLen(&Scr->RastPort);

	DepthGad=NewObject(NULL,SYSICLASS,SYSIA_DrawInfo,DRI,SYSIA_Which,SDEPTHIMAGE,SYSIA_Size,SYSISIZE_MEDRES,TAG_DONE);
	unless(MainWnd=NewObject(appl->ga_WndClass,NULL,
		(CONF_XPos==-1)?WIND_RelRight:WIND_Left,(CONF_XPos==-1)?(((struct Image *)DepthGad)->Width):CONF_XPos,
		WIND_Top,(CONF_YPos==-1)?0:CONF_YPos,
		WIND_Width,(Scr->BarHeight*4+tlen+4)+2+(CONF_Window?3:0),
		WIND_Height,Scr->BarHeight+CONF_HeightAdd+(CONF_Window?5:0),
		WIND_ScreenObject,Scrn,
		WIND_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_GADGETDOWN|IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY,
		WIND_Handler,(ULONG)MainWndHandler,
		WIND_Activate,FALSE,
		CONF_Window?TAG_IGNORE:WIND_Borderless,TRUE,
		CONF_Window?WIND_ToolDragBar:TAG_IGNORE,TOOL_VERT,
		WIND_RMBTrap,TRUE,
		TAG_DONE)) goto err;
	if(DepthGad) DisposeObject(DepthGad);

	GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
	tl=TextLength(Wnd->RPort," 00 ",4);
	SetFont(Wnd->RPort,TFont);
	SetAPen(Wnd->RPort,DRI->dri_Pens[BARBLOCKPEN]);
	RectFill(Wnd->RPort,Wnd->BorderLeft,Wnd->BorderTop,Wnd->Width-Wnd->BorderRight-(CONF_Window?1:0),Wnd->Height-Wnd->BorderBottom-(CONF_Window?1:0));

	unless(prev=(Object *)CreateContext(&MainGList)) goto err;
	unless(prev=gtbCreateGadget(GENERIC_KIND,NULL,NULL,
		GA_Previous,prev,
		GA_Left,Wnd->BorderLeft,
		GA_Top,Wnd->BorderTop,
		GA_Width,Scr->BarHeight,
		GA_Height,Scr->BarHeight,
		GA_LabelImage,IMPlay,
		GT_ScreenObj,Scrn,
		GA_RelVerify,TRUE,
		GT_Handler,(ULONG)GadPlayHandler,
		TAG_DONE)) goto err;
	unless(prev=gtbCreateGadget(GENERIC_KIND,NULL,NULL,
		GA_Previous,prev,
		GA_Left,Wnd->BorderLeft+(Scr->BarHeight)*1,
		GA_Top,Wnd->BorderTop,
		GA_Width,Scr->BarHeight,
		GA_Height,Scr->BarHeight,
		GA_LabelImage,IMStop,
		GT_ScreenObj,Scrn,
		GA_RelVerify,TRUE,
		GT_Handler,(ULONG)GadStopHandler,
		TAG_DONE)) goto err;
	unless(prev=gtbCreateGadget(GENERIC_KIND,NULL,NULL,
		GA_Previous,prev,
		GA_Left,Wnd->BorderLeft+(Scr->BarHeight)*2,
		GA_Top,Wnd->BorderTop,
		GA_Width,Scr->BarHeight,
		GA_Height,Scr->BarHeight,
		GA_LabelImage,IMPause,
		GT_ScreenObj,Scrn,
		GA_RelVerify,TRUE,
		GT_Handler,(ULONG)GadPauseHandler,
		TAG_DONE)) goto err;
	unless(prev=gtbCreateGadget(GENERIC_KIND,NULL,NULL,
		GA_Previous,prev,
		GA_Left,Wnd->BorderLeft+(Scr->BarHeight)*3,
		GA_Top,Wnd->BorderTop,
		GA_Width,Scr->BarHeight,
		GA_Height,Scr->BarHeight,
		GA_LabelImage,IMEject,
		GT_ScreenObj,Scrn,
		GA_RelVerify,TRUE,
		GT_Handler,(ULONG)GadEjectHandler,
		TAG_DONE)) goto err;
	unless(GadTimerDisp=prev=gtbCreateGadget(TEXT_KIND,NULL,NULL,
		GA_Previous,prev,
		GA_Left,Wnd->BorderLeft+(Scr->BarHeight)*4,
		GA_Top,Wnd->BorderTop,
		GA_Width,tlen+2,
		GA_Height,Scr->BarHeight,
		GT_ScreenObj,Scrn,
		(TextAttr.ta_YSize>0 && TextAttr.ta_Name[0]!='\0')?GA_TextAttr:TAG_IGNORE,&TextAttr,
		GTTX_FrontPen,DRI->dri_Pens[BARDETAILPEN],
		GTTX_BackPen,DRI->dri_Pens[BARBLOCKPEN],
		TAG_DONE)) goto err;

	DoMethod(MainWnd,OM_ADDMEMBER,MainGList);
	RefreshGList(MainGList,Wnd,NULL,-1);
	GT_RefreshWindow(Wnd,NULL);

	ShowStatus(0);
	return MainWnd;
err:
	if(MainGList) gtbFreeGadgets(MainGList);
	if(MainWnd) DisposeObject(MainWnd);
	return NULL;
}

void CloseWindow_Main(void)
{
	struct Window *Wnd;

	if(MainWnd)
	{
		GetAttr(WIND_Window,MainWnd,(ULONG *)&Wnd);
		RemoveGList(Wnd,MainGList,-1);
		gtbFreeGadgets(MainGList);
		DisposeObject(MainWnd);
		MainWnd=NULL;
	}
}

#define ARG_TEMPLATE "CX_PRIORITY/N/K,CX_POPKEY/K,CX_POPUP/K,DEVICE/K,UNIT/N/K,SONGPATH/K,AUTOPLAY/N/K,AUTOEJECT/S,XPOS/N/K,YPOS/N/K,HEIGHTADD/N/K,FONTNAME/K,FONTSIZE/N/K,WINDOW/S"
#define ARG_PRIORITY 0
#define ARG_POPKEY 1
#define ARG_POPUP 2
#define ARG_DEVICE 3
#define ARG_UNIT 4
#define ARG_SONGPATH 5
#define ARG_AUTOPLAY 6
#define ARG_AUTOEJECT 7
#define ARG_XPOS 8
#define ARG_YPOS 9
#define ARG_HEIGHTADD 10
#define ARG_FONTNAME 11
#define ARG_FONTSIZE 12
#define ARG_WINDOW 13
#define ARG_COUNT 14

ULONG DEF_PRIORITY=1;
ULONG DEF_UNIT=1;
ULONG DEF_AUTOPLAY=0;
ULONG DEF_AUTOEJECT=0;
ULONG DEF_XPOS=-1;
ULONG DEF_YPOS=-1;
ULONG DEF_HEIGHTADD=0;
ULONG DEF_FONTSIZE=0;
ULONG DEF_WINDOW=0;

BOOL args_Read(STRPTR template,LONG *opts,int argc)
{
	struct RDArgs *rdargs=NULL;
	struct DiskObject *keyobj=NULL;
	int a;

	if(argc==0)
	{
		if(keyobj=GetDiskObject(WBenchMsg->sm_ArgList->wa_Name))
		{
			if(rdargs=AllocDosObject(DOS_RDARGS,TAG_DONE))
			{
				for(rdargs->RDA_Source.CS_Length=0,a=0;keyobj->do_ToolTypes[a];a++) if(stricmp(keyobj->do_ToolTypes[a],"DONOTWAIT") && (keyobj->do_ToolTypes[a][0]!='(')) rdargs->RDA_Source.CS_Length+=strlen(keyobj->do_ToolTypes[a])+1;
				if(rdargs->RDA_Source.CS_Buffer=AllocVec(rdargs->RDA_Source.CS_Length+1,MEMF_ANY|MEMF_CLEAR))
				{
					for(a=0;keyobj->do_ToolTypes[a];a++) if(stricmp(keyobj->do_ToolTypes[a],"DONOTWAIT") && (keyobj->do_ToolTypes[a][0]!='(')) { strcat(rdargs->RDA_Source.CS_Buffer,keyobj->do_ToolTypes[a]); strcat(rdargs->RDA_Source.CS_Buffer," "); }
					rdargs->RDA_Source.CS_Buffer[strlen(rdargs->RDA_Source.CS_Buffer)]='\n';
//			printf("%s\n",rdargs->RDA_Source.CS_Buffer);
					rdargs->RDA_Source.CS_CurChr=0;
					ReadArgs(template,opts,rdargs);
					FreeArgs(rdargs);
					FreeVec(rdargs->RDA_Source.CS_Buffer);
				}
				FreeDosObject(DOS_RDARGS,rdargs);
			}
			FreeDiskObject(keyobj);
		}
	}
	else
	{
		if(rdargs=ReadArgs(template,opts,NULL))
			FreeArgs(rdargs);
		else
			return FALSE;
	}
	return TRUE;
}

BOOL args_StrToBOOL(long *s)
{
	BOOL retval=((stricmp((char *)s, "YES") == 0) ||
		(stricmp((char *)s, "TRUE") == 0) ||
		(stricmp((char *)s, "ON") == 0)) ? TRUE : FALSE;

	s=(long *)retval;
	return retval;
}

void main(int argc, char *argv[])
{
	ULONG retval,a,oldstat;
	long *(opts[ARG_COUNT]);

	for(a=0;a<100;a++) TOC_Title[a]=NULL;

	opts[ARG_PRIORITY]=&DEF_PRIORITY;
	opts[ARG_POPKEY]=(long *)"control p";
	opts[ARG_POPUP]=(long *)"YES";
	opts[ARG_DEVICE]=(long *)"scsi.device";
	opts[ARG_UNIT]=&DEF_UNIT;
	opts[ARG_SONGPATH]=(long *)"Songs";
	opts[ARG_AUTOPLAY]=&DEF_AUTOPLAY;
	opts[ARG_AUTOEJECT]=(long *)DEF_AUTOEJECT;
	opts[ARG_XPOS]=&DEF_XPOS;
	opts[ARG_YPOS]=&DEF_YPOS;
	opts[ARG_HEIGHTADD]=&DEF_HEIGHTADD;
	opts[ARG_FONTNAME]=(long *)"";
	opts[ARG_FONTSIZE]=&DEF_FONTSIZE;
	opts[ARG_WINDOW]=(long *)DEF_WINDOW;

	if(!args_Read(ARG_TEMPLATE,(long *)opts,argc))
	{
		PrintFault(IoErr(),argv[0]);
		Leave(NULL);
	}
	args_StrToBOOL(opts[ARG_POPUP]);
	CONF_AutoPlay=*opts[ARG_AUTOPLAY];
	CONF_AutoEject=(UBYTE)opts[ARG_AUTOEJECT];
	CONF_SCSIUnit=*opts[ARG_UNIT];
	CONF_XPos=*opts[ARG_XPOS];
	CONF_YPos=*opts[ARG_YPOS];
	CONF_HeightAdd=*opts[ARG_HEIGHTADD];
	CONF_Window=(UBYTE)opts[ARG_WINDOW];

	TextAttr.ta_YSize=(UBYTE)*opts[ARG_FONTSIZE];
	TextAttr.ta_Name=CONF_FontName;
	strcpy(CONF_SCSIDevice,(char *)opts[ARG_DEVICE]);
	strcpy(CONF_SongPath,(char *)opts[ARG_SONGPATH]);
	strcpy(CONF_FontName,(char *)opts[ARG_FONTNAME]);

	unless(SCSI_Init())
	{
		printf("Can't open %s unit %d\n",CONF_SCSIDevice,CONF_SCSIUnit);
		Leave(NULL);
	}
	unless(TimerInit()) Leave("Can't open timer.device");
	unless(PopupMenuBase=(struct PopupMenuBase *)OpenLibrary(POPUPMENU_NAME,POPUPMENU_VERSION)) Leave("Can't open popupmenu.library");
	unless(ListViewBase=OpenLibrary(LISTVIEWNAME,LISTVIEWVERSION)) Leave("Can't open listview.gadget");
	ListViewClass=GetListViewClass();
	MainMenu=PM_MakeMenu(
			PMMenuTitle("MiniCD v1.3"), 
			PMItem("Play"), PM_UserData, GadPlayHandler, PM_CommKey,"P", End,
			PMItem("Stop"), PM_UserData, GadStopHandler, PM_CommKey,"S", End,
			PMItem("Pause"), PM_UserData, GadPauseHandler, PM_CommKey,"A", End,
			PMItem("Eject"), PM_UserData, GadEjectHandler, PM_CommKey,"E", End,
			PMBar, PMEnd,
//			PMItem("About"), PM_UserData, MenuAboutHandler, PM_CommKey,"?", End,
//			PMBar, PMEnd,
			PMItem("Hide"), PM_UserData, MenuHideHandler, PM_CommKey,"H", End,
			PMItem("Quit"), PM_UserData, MenuQuitHandler, PM_CommKey,"Q", End,
			End;
	unless(MainMenu) Leave("Can't create PopUpMenu");

	TimeMenu=PM_MakeMenu(
			PMMenuTitle("Time"), 
			PMItem("Title time"), PM_UserData, MenuTimeActualTrackHandler, End,
			PMItem("Total time"), PM_UserData, MenuTimeActualCDHandler, End,
			PMBar, End,
			PMItem("Remain title"), PM_UserData, MenuTimeRemainTrackHandler, End,
			PMItem("Remain disk"), PM_UserData, MenuTimeRemainCDHandler, End,
			End;
	unless(TimeMenu) Leave("Can't create PopUpMenu");

	if(!(ImageClass = MakeClass (NULL, IMAGECLASS, NULL, sizeof(struct IMAGEData), 0L))) Leave("Can't make class");
	ImageClass->cl_Dispatcher.h_Entry = (HookFunction)  IMAGE_Dispatcher;

	unless(appl=gtbNewApplication(TAG_DONE)) Leave("Can't create application");
	for(a=0;a<100;a++) TOC_Title[a]=AllocVec(128,MEMF_ANY);

	unless(Scrn=NewObject(appl->ga_ScrClass,NULL,
		SCRN_LockPubName,"Workbench",
		TAG_DONE)) Leave("Can't create screen");
	unless(Cdity=NewObject(appl->ga_CxClass,NULL,
		COMM_Name,APPNAME,
		COMM_Title,APPTITLE,
		COMM_Description,APPDESCRIPTION,
		COMM_Unique,TRUE,
		COMM_Notify,TRUE,
		COMM_ShowHide,TRUE,
		COMM_Handler,(ULONG)CdityHandler,
		COMM_AddHotKeyStr,opts[ARG_POPKEY],COMM_AddHotKeyID,1,
		COMM_Priority,*opts[ARG_PRIORITY],
		TAG_DONE)) Leave("Can't create commodity");
	unless(CtrlC=NewObject(appl->ga_MsgClass,NULL,
		MESG_SigBit,SIGBREAKF_CTRL_C,
		MESG_Handler,(ULONG)CtrlCHandler,
		TAG_DONE)) Leave("Can't create message handler");
	unless(Timer=NewObject(appl->ga_MsgClass,NULL,
		MESG_SigBit,(1L<<TimerMP->mp_SigBit),
		MESG_Handler,(ULONG)TimerHandler,
		TAG_DONE)) Leave("Can't create message handler");
	unless(ARexx=NewObject(appl->ga_ARexxClass,NULL,
		REXX_Name,APPPUBNAME,
		REXX_Extension,APPEXTENSION,
		REXX_CommandTable,commands,
		REXX_Handler,(ULONG)ARexxHandler,
		TAG_DONE)) Leave("Can't create arexx");
	AmigaGuide=NewObject(appl->ga_AGClass,NULL,
		AGUI_Name,APPGUIDE,
		AGUI_ScreenObject,Scrn,
		AGUI_ARexxObject,ARexx,
		AGUI_Activate,TRUE,
		AGUI_BaseName,APPNAME,
		TAG_DONE);

	if(TextAttr.ta_YSize>0 && TextAttr.ta_Name[0]!='\0') TFont=OpenDiskFont(&TextAttr);

	GetAttr(SCRN_Screen,Scrn,(ULONG *)&Scr);
	GetAttr(SCRN_DrawInfo,Scrn,(ULONG *)&DRI);

	unless(IMPlay=NewObject(ImageClass,NULL,IA_Width,Scr->BarHeight,IA_Height,Scr->BarHeight,IMAGE_ImageNum,IM_PLAY,TAG_DONE)) Leave("Can't create image");
	unless(IMStop=NewObject(ImageClass,NULL,IA_Width,Scr->BarHeight,IA_Height,Scr->BarHeight,IMAGE_ImageNum,IM_STOP,TAG_DONE)) Leave("Can't create image");
	unless(IMPause=NewObject(ImageClass,NULL,IA_Width,Scr->BarHeight,IA_Height,Scr->BarHeight,IMAGE_ImageNum,IM_PAUSE,TAG_DONE)) Leave("Can't create image");
	unless(IMEject=NewObject(ImageClass,NULL,IA_Width,Scr->BarHeight,IA_Height,Scr->BarHeight,IMAGE_ImageNum,IM_EJECT,TAG_DONE)) Leave("Can't create image");

	oldstat=SCMD_GetStatus();
	SCMD_ReadTitleTime(1);
	if(ActTitle>0 && TOC_NumTracks>0) if(oldstat==STAT_PLAYING) SCMD_Resume();
	SCMD_SetVolume(0);

	if(opts[ARG_POPUP]==FALSE) WIND_HIDDEN=TRUE;
	if((ActStat!=STAT_NODISK) && (ActStat!=STAT_DATADISK)) if(!WIND_HIDDEN) unless(OpenWindow_Main()) Leave("Can't create window");

	while(retval=gtbHandleObject(Timer,Cdity,CtrlC,MainWnd,ARexx,TAG_DONE))
	{
		if(retval==2)
		{
			CloseWindow_Main();
			MainWnd=TAG_DONE;
			WIND_HIDDEN=TRUE;
		}
	}
	Wait(1<<TimerMP->mp_SigBit);
	GetMsg(TimerMP);
end:
	if(IMEject) DisposeObject(IMEject);
	if(IMPause) DisposeObject(IMPause);
	if(IMStop) DisposeObject(IMStop);
	if(IMPlay) DisposeObject(IMPlay);
	if(TFont) CloseFont(TFont);
	if(AmigaGuide) DisposeObject(AmigaGuide);
	if(ARexx) DisposeObject(ARexx);
	if(Timer) DisposeObject(Timer);
	if(CtrlC) DisposeObject(CtrlC);
	if(Cdity) DisposeObject(Cdity);
	if(MainWnd) CloseWindow_Main();
	if(Scrn) DisposeObject(Scrn);
	for(a=0;a<100;a++) if(TOC_Title[a]) FreeVec(TOC_Title[a]);
	if(appl) gtbDisposeApplication(appl);
	if(ImageClass) FreeClass(ImageClass);
	if(MainMenu) PM_FreePopupMenu(MainMenu);
	if(TrackMenu) PM_FreePopupMenu(TrackMenu);
	if(TimeMenu) PM_FreePopupMenu(TimeMenu);
	if(ListViewBase) CloseLibrary(ListViewBase);
	if(PopupMenuBase) CloseLibrary((struct Library *)PopupMenuBase);
	TimerExit();
	SCSI_Exit();
}

ULONG EditorWndHandler(Object *WindowObj,ULONG type,APTR message)
{
	struct IntuiMessage *im;
	ULONG visible,var;

	switch(type)
	{
		case TYPE_INTUIMESSAGE:
			im=(struct IntuiMessage *)message;
			switch(im->Class)
			{
				case IDCMP_CLOSEWINDOW:
					return FALSE;
					break;
				case IDCMP_NEWSIZE:
					gtbGetGadgetAttr(LIST_Visible,GadEditor,&visible);
					SetAttrs(WindowObj,WIND_VertVisible,visible,TAG_DONE);
					break;
				case IDCMP_GADGETDOWN:
				case IDCMP_GADGETUP:
					printf("Unknown gadget\n");
					break;
			}
			break;
		case TYPE_VERTSCROLL:
			GetAttr(WIND_VertTop,WindowObj,&var);
			gtbSetGadgetAttrs(GadEditor,WindowObj,LIST_Top,var,TAG_DONE);
			break;
	}
	return TRUE;
}

ULONG __saveds __asm ListFunc(register __a0 struct Hook *hook,register __a2 Object *object,register __a1 APTR msg)
{
	struct TLVEditMsg *tlvem=msg;

	switch(tlvem->tlvm_MethodID)
	{
		case TLV_EDIT:
			sprintf(tlvem->tlvm_Node->ln_Name,"%.123s",tlvem->tlvm_String);
			return TLVCB_OK;
	}
	return TLVCB_UNKNOWN;
}

struct Hook ListHook={NULL,NULL,(HOOKFUNC)ListFunc,NULL,NULL};

struct Gadget *EditorGList=NULL;
Object *OpenWindow_Editor(void)
{
	struct Window *Wnd;
	Object *prev;
	struct LVNode *node;
	ULONG visible,a;

	Tree_NewList(&EditorList);

	for(a=0;a<TOC_NumTracks;a++)
	{
		if(node=AllocVec(sizeof(struct LVNode),MEMF_ANY|MEMF_CLEAR))
		{
			node->ln_Name=TOC_Title[a]+4;
			Tree_AddTail(&EditorList,node);
		}
	}

	SetAttrs(MainWnd,WIND_LockWindow,TRUE,WIND_BusyPointer,TRUE,TAG_DONE);
	unless(EditorWnd=NewObject(appl->ga_WndClass,NULL,
		WIND_CenterX,TRUE,
		WIND_CenterY,TRUE,
		WIND_Width,300,
		WIND_Height,150,
		WIND_MaxWidth,-1,
		WIND_MaxHeight,-1,
		WIND_ScreenObject,Scrn,
		WIND_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_GADGETDOWN|IDCMP_MOUSEBUTTONS|IDCMP_NEWSIZE,
		WIND_Handler,(ULONG)EditorWndHandler,
		WIND_Activate,TRUE,
		WIND_RMBTrap,TRUE,
		WIND_DragBar,TRUE,
		WIND_DepthGadget,TRUE,
		WIND_CloseGadget,TRUE,
		WIND_SizeGadget,TRUE,
		WIND_VertScroll,TRUE,
		WIND_VertTop,0,
		WIND_VertTotal,TOC_NumTracks,
		WIND_VertVisible,TOC_NumTracks,
		WIND_Title,"Edit titles",
		TAG_DONE)) goto err;

	GetAttr(WIND_Window,EditorWnd,(ULONG *)&Wnd);

	unless(prev=(Object *)CreateContext(&EditorGList)) goto err;
	unless(prev=gtbCreateGadget(STRING_KIND,NULL,NULL,
		GA_Previous,prev,
		GA_Left,Wnd->BorderLeft+TextLength(Wnd->RPort,"Artist:",7)+8,
		GA_Top,Wnd->BorderTop,
		GA_Width,Wnd->Width-Wnd->BorderLeft-Wnd->BorderRight-8-TextLength(Wnd->RPort,"Artist:",7),
		GA_Height,(Wnd->RPort->Font->tf_YSize+Wnd->RPort->Font->tf_Baseline+2),
		GA_Text,"Artist:",
//		GA_TabCycle,TRUE,
		GT_ScreenObj,Scrn,
//		GT_PlaceText,PLACETEXT_LEFT,
		GTST_String,TOC_CDInterpret,
		GTST_MaxChars,128,
//		GTTX_Border,TRUE,
//		GTTX_Text,TOC_CDInterpret,
		GT_Handler,(ULONG)GadArtistHandler,
		TAG_DONE))
	{
		printf("error\n");
		 goto err;
	}
	unless(prev=gtbCreateGadget(STRING_KIND,NULL,NULL,
		GA_Previous,prev,
		GA_Left,Wnd->BorderLeft+TextLength(Wnd->RPort,"Title:",6)+8,
		GA_Top,Wnd->BorderTop+(Wnd->RPort->Font->tf_YSize+Wnd->RPort->Font->tf_Baseline+2),
		GA_Width,Wnd->Width-Wnd->BorderLeft-Wnd->BorderRight-8-TextLength(Wnd->RPort,"Title:",6),
		GA_Height,(Wnd->RPort->Font->tf_YSize+Wnd->RPort->Font->tf_Baseline+2),
		GA_Text,"Title:",
//		GA_TabCycle,TRUE,
		GT_ScreenObj,Scrn,
//		GT_PlaceText,PLACETEXT_LEFT,
		GTST_String,TOC_CDTitle,
		GTST_MaxChars,128,
//		GTTX_Border,TRUE,
//		GTTX_Text,TOC_CDTitle,
		GT_Handler,(ULONG)GadTitleHandler,
		TAG_DONE)) goto err;
	unless(GadEditor=prev=gtbCreateGadget(BOOPSI_KIND,ListViewClass,NULL,
		ICA_TARGET,ICTARGET_IDCMP,
		GA_Previous,prev,
		GA_Left,Wnd->BorderLeft,
		GA_Top,Wnd->BorderTop+(Wnd->RPort->Font->tf_YSize+Wnd->RPort->Font->tf_Baseline+2)*2,
		GA_RelWidth,-Wnd->BorderRight-Wnd->BorderLeft,
		GA_RelHeight,-Wnd->BorderBottom-Wnd->BorderTop-((Wnd->RPort->Font->tf_YSize+Wnd->RPort->Font->tf_Baseline+2)*2),
		GT_ScreenObj,Scrn,
		LIST_Border,TRUE,
		LIST_Labels,&EditorList,
		LIST_HookEdit,&ListHook,
		TAG_DONE)) goto err;

	DoMethod(EditorWnd,OM_ADDMEMBER,EditorGList);
	RefreshGList(EditorGList,Wnd,NULL,-1);
	GT_RefreshWindow(Wnd,NULL);
	gtbGetGadgetAttr(LIST_Visible,GadEditor,&visible);
	SetAttrs(EditorWnd,WIND_VertVisible,visible,TAG_DONE);

	return EditorWnd;
err:
	if(EditorGList) gtbFreeGadgets(EditorGList);
	if(EditorWnd) DisposeObject(EditorWnd);
	while(node=Tree_RemTail(&EditorList)) FreeVec(node);
	SetAttrs(MainWnd,WIND_LockWindow,FALSE,WIND_BusyPointer,FALSE,TAG_DONE);
	return NULL;
}

void CloseWindow_Editor(void)
{
	struct Window *Wnd;
	struct LVNode *node;

	if(EditorWnd)
	{
		GetAttr(WIND_Window,EditorWnd,(ULONG *)&Wnd);
		RemoveGList(Wnd,EditorGList,-1);
		gtbFreeGadgets(EditorGList);
		DisposeObject(EditorWnd);
		EditorWnd=NULL;
		while(node=Tree_RemTail(&EditorList)) FreeVec(node);
		SetAttrs(MainWnd,WIND_LockWindow,FALSE,WIND_BusyPointer,FALSE,TAG_DONE);
		SaveTitles();
	}
}

void SaveTitles()
{
	int a;
	BPTR file;
	TEXT fname[256];

	strcpy(fname,CONF_SongPath);
	AddPart(fname,TOC_CDID,256);
	if(file=Open(fname,MODE_NEWFILE))
	{
		FPuts(file,TOC_CDInterpret);
		FPuts(file,"\n");
		FPuts(file,TOC_CDTitle);
		FPuts(file,"\n");
		for(a=0;a<TOC_NumTracks;a++)
		{
			FPuts(file,TOC_Title[a]+4);
			FPuts(file,"\n");
		}
		Close(file);
	}
}

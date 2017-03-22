#pragma once

#include "winsock2.h"
#include "MXml.h"
#include "MServer.h"
#include "MMatchObject.h"
#include "MAgentObject.h"
#include "MMatchChannel.h"
#include "MMatchStage.h"
#include "MMatchClan.h"
#include "MSafeUDP.h"
#include "MMatchTransDataType.h"
#include "MMatchAdmin.h"
#include "MAsyncProxy.h"
#include "MMatchGlobal.h"
#include "MMatchShutdown.h"
#include "MMatchChatRoom.h"
#include "MLadderMgr.h"
#include "MMatchQuest.h"
#include "MTypes.h"
#include "MMatchDebug.h"
#include "MStringRes.h"
#include "MMatchStringResManager.h"
#include "MMatchEventManager.h"
#include "GlobalTypes.h"
#include <queue>
#include <unordered_map>
#include "LagCompensation.h"
#include "SQLiteDatabase.h"
#include "MSSQLDatabase.h"

class MMatchAuthBuilder;
class MMatchScheduleMgr;
class MNJ_DBAgentClient;

#define MATCHSERVER_UID		MUID(0, 2)
#define CHECKMEMORYNUMBER	888888

enum CUSTOM_IP_STATUS
{
	CIS_INVALID = 0,
	CIS_NONBLOCK,
	CIS_BLOCK,
	CIS_NON,
};

enum COUNT_CODE_STATUS
{
	CCS_INVALID = 0,
	CCS_NONBLOCK,
	CCS_BLOCK,
	CCS_NON,
};

struct DBVariant
{
	union U
	{
		U() {}
		~U() {}
		SQLiteDatabase sqlite;
#ifdef MSSQL_ENABLED
		MSSQLDatabase mssql;
#endif
	};

	DatabaseType Type;
	U u;

	// TODO: Fixfix
	/*DBVariant(DatabaseType Type)
		: Type(Type)*/
	DBVariant() {}
	void Create(DatabaseType Type)
	{
		this->Type = Type;

		if (Type == DatabaseType::SQLite)
			new (&u) SQLiteDatabase;
#ifdef MSSQL_ENABLED
		else
			new (&u) MSSQLDatabase;
#endif
	}
	DBVariant(const DBVariant&) = delete;
	DBVariant(DBVariant&&) = delete;
	~DBVariant()
	{
		if (Type == DatabaseType::SQLite)
			u.sqlite.~SQLiteDatabase();
#ifdef MSSQL
		else
			u.mssql.~MSSQLDatabase();
#endif
	}
};

class MMatchServer : public MServer {
public:
	MMatchServer();
	virtual ~MMatchServer();

	static MMatchServer* GetInstance();

	bool Create(int nPort);
	void Destroy();
	virtual void Shutdown();
	virtual MUID UseUID() override;

	MMatchChatRoomMgr* GetChatRoomMgr() { return &m_ChatRoomMgr; }

	void ChannelResponsePlayerList(const MUID& uidPlayer, const MUID& uidChannel, int nPage);
	void ChannelResponseAllPlayerList(const MUID& uidPlayer, const MUID& uidChannel,
		unsigned long int nPlaceFilter, unsigned long int nOptions);

	MMatchStage* FindStage(const MUID& uidStage);

	void OnGameKill(const MUID& uidAttacker, const MUID& uidVictim);

	bool CheckUpdateItemXML();
	IPtoCountryList& GetTmpIPtoCountryList() { return m_TmpIPtoCountryList; }
	BlockCountryCodeList& GetTmpBlockCountryCodeList() { return m_TmpBlockCountryCodeList; }
	CustomIPList& GetTmpCustomIPList() { return m_TmpCustomIPList; }
	void SetUseCountryFilter();
	void SetAccetpInvalidIP();
	void UpdateIPtoCountryList();
	void UpdateBlockCountryCodeLsit();
	void UpdateCustomIPList();
	void ResetBlockCount() { m_dwBlockCount = 0; }
	void ResetNonBlockCount() { m_dwNonBlockCount = 0; }
	void IncreaseBlockCount() { ++m_dwBlockCount = 0; }
	void IncreaseNonBlockCount() { ++m_dwNonBlockCount = 0; }
	DWORD GetBlockCount() { return m_dwBlockCount; }
	DWORD GetNonBlockCount() { return m_dwNonBlockCount; }
	bool CheckIsValidIP(const MUID& CommUID, const std::string& strIP, std::string& strCountryCode3,
		bool bUseFilter);

	void CustomCheckEventObj(const DWORD dwEventID, MMatchObject* pObj, void* pContext);

	MLadderMgr*	GetLadderMgr() { return &m_LadderMgr; }
	MMatchObjectList*	GetObjects() { return &m_Objects; }
	MMatchStageMap*		GetStageMap() { return &m_StageMap; }
	MMatchChannelMap*	GetChannelMap() { return &m_ChannelMap; }
	MMatchClanMap*		GetClanMap() { return &m_ClanMap; }
	IDatabase*			GetDBMgr() { return reinterpret_cast<IDatabase*>(&Database.u); }
	MMatchQuest*		GetQuest() { return &m_Quest; }
	int GetClientCount() const { return (int)m_Objects.size(); }
	int GetAgentCount() const { return (int)m_AgentMap.size(); }

	void PostDeath(const MMatchObject& Victim, const MMatchObject& Attacker);
	void PostDamage(const MUID& Target, const MUID& Sender,
		ZDAMAGETYPE DamageType, MMatchWeaponType WeaponType,
		int Damage, float PiercingRatio);
	void PostHPAPInfo(const MMatchObject& Object, int HP, int AP);

	void PostAsyncJob(MAsyncJob* pJob);

	MMatchClan* FindClan(const int nCLID);
	void ResponseClanMemberList(const MUID& uidChar);

	int GetLadderTeamIDFromDB(int nTeamTableIndex, const int* pnMemberCIDArray, int nMemberCount);
	void SaveLadderTeamPointToDB(int nTeamTableIndex, int nWinnerTeamID,
		int nLoserTeamID, bool bIsDrawGame);
	void SaveClanPoint(MMatchClan* pWinnerClan, MMatchClan* pLoserClan, bool bIsDrawGame,
		int nRoundWins, int nRoundLosses, int nMapID, int nGameType,
		int nOneTeamMemberCount, list<MUID>& WinnerObjUIDs,
		const char* szWinnerMemberNames, const char* szLoserMemberNames, float fPointRatio);
	void BroadCastClanRenewVictories(const char* szWinnerClanName,
		const char* szLoserClanName, const int nVictories);
	void BroadCastClanInterruptVictories(const char* szWinnerClanName,
		const char* szLoserClanName, const int nVictories);
	void BroadCastDuelRenewVictories(const MUID& chanID, const char* szChampionName,
		const char* szChannelName, int nRoomNumber, const int nVictories);
	void BroadCastDuelInterruptVictories(const MUID& chanID, const char* szChampionName,
		const char* szInterrupterName, const int nVictories);

	void OnVoteCallVote(const MUID& uidPlayer, const char* pszDiscuss, const char* pszArg);
	void OnVoteYes(const MUID& uidPlayer);
	void OnVoteNo(const MUID& uidPlayer);
	void VoteAbort(const MUID& uidPlayer);

	void OnAdminServerHalt();
	void AdminTerminalOutput(const MUID& uidAdmin, const char* szText);
	template<size_t size> bool OnAdminExecute(MAdminArgvInfo *pAI, char(&szOut)[size]) {
		return OnAdminExecute(pAI, szOut, size);
	}
	bool OnAdminExecute(MAdminArgvInfo* pAI, char* szOut, int maxlen);
	void ApplyObjectTeamBonus(MMatchObject* pObject, int nAddedExp);
	void ProcessPlayerXPBP(MMatchStage* pStage, MMatchObject* pPlayer, int nAddedXP, int nAddedBP);
	bool InsertCharItem(const MUID& uidPlayer, const unsigned long int nItemID, bool bRentItem, int nRentPeriodHour);

	void OnDuelSetObserver(const MUID& uidChar);
	void OnDuelQueueInfo(const MUID& uidStage, const MTD_DuelQueueInfo& QueueInfo);
	void OnQuestSendPing(const MUID& uidStage, unsigned long int t);

	void OnRequestCharQuestItemList(const MUID& uidSender);

	bool IsCreated() const { return m_bCreated; }
	inline u64 GetTickTime();

	// Get player
	MMatchObject* GetObject(const MUID& uid);
	MMatchObject* GetPlayerByCommUID(const MUID& uid);
	MMatchObject* GetPlayerByName(const char* pszName);
	MMatchObject* GetPlayerByAID(unsigned long int nAID);

	// Get channel
	MMatchChannel* FindChannel(const MUID& uidChannel);
	MMatchChannel* FindChannel(const MCHANNEL_TYPE nChannelType, const char* pszChannelName);

	// Packet routing
	void RouteToListener(MObject* pObject, MCommand* pCommand);
	void RouteToAllConnection(MCommand* pCommand);
	void RouteToAllClient(MCommand* pCommand);
	template <typename T>
	void RouteToAllClientIf(MCommand* pCommand, T& pred)
	{
		for (MMatchObjectList::iterator i = m_Objects.begin(); i != m_Objects.end(); i++) {
			MMatchObject* pObj = (MMatchObject*)((*i).second);
			if (pObj->GetUID() < MUID(0, 3)) continue;
			if (!pred(*pObj))
				continue;

			MCommand* pSendCmd = pCommand->Clone();
			pSendCmd->m_Receiver = pObj->GetUID();
			Post(pSendCmd);
		}
		delete pCommand;
	}
	void RouteToChannel(const MUID& uidChannel, MCommand* pCommand);
	void RouteToChannelLobby(const MUID& uidChannel, MCommand* pCommand);
	void RouteToStage(const MUID& uidStage, MCommand* pCommand);
	void RouteToStageWaitRoom(const MUID& uidStage, MCommand* pCommand);
	void RouteToBattle(const MUID& uidStage, MCommand* pCommand);
	void RouteToBattleExcept(const MUID& uidStage, MCommand* pCommand, const MUID& uidExceptedPlayer);
	void RouteToClan(const int nCLID, MCommand* pCommand);
	void RouteResponseToListener(MObject* pObject, const int nCmdID, int nResult);

	unsigned long int GetStageListChecksum(MUID& uidChannel, int nStageCursor, int nStageCount);
	void StageList(const MUID& uidPlayer, int nStageStartIndex, bool bCacheUpdate);
	void StageLaunch(const MUID& uidStage);
	void StageFinishGame(const MUID& uidStage);

	void StandbyClanList(const MUID& uidPlayer, int nClanListStartIndex, bool bCacheUpdate);

	unsigned long GetChannelListChecksum() const { return m_ChannelMap.GetChannelListChecksum(); }
	void ChannelList(const MUID& uidPlayer, MCHANNEL_TYPE nChannelType);

	u64 GetGlobalClockCount();

	void ResponseBridgePeer(const MUID& uidChar, int nCode);
	void ResponseRoundState(const MUID& uidStage);
	void ResponseRoundState(MMatchObject* pObj, const MUID& uidStage);
	void ResponsePeerList(const MUID& uidChar, const MUID& uidStage);
	void ResponseGameInfo(const MUID& uidChar, const MUID& uidStage);

	// Announce
	void Announce(const MUID& CommUID, char* pszMsg);
	void Announce(MObject* pObj, char* pszMsg);
	void AnnounceF(const MUID& CommUID, char* pszMsg, ...)
	{
		char buf[512];

		va_list args;

		va_start(args, pszMsg);
		vsprintf_safe(buf, pszMsg, args);
		va_end(args);

		Announce(CommUID, buf);
	}
	void AnnounceErrorMsg(const MUID& CommUID, const int nErrorCode);
	void AnnounceErrorMsg(MObject* pObj, const int nErrorCode);

	LagCompManager LagComp;

protected:
	friend MVoteDiscuss;
	friend MMatchStage;
	friend MNJ_DBAgentClient;
	friend MLadderMgr;
	friend bool StageKick(MMatchServer* pServer, const MUID& uidPlayer, const MUID& uidStage,
		char* pszChat);

	virtual bool OnCreate();
	virtual void OnDestroy();
	virtual void OnRegisterCommand(MCommandManager* pCommandManager) override final;
	virtual bool OnCommand(MCommand* pCommand) override;
	virtual void OnRun() override;
	virtual void OnPrepareRun() override;

	virtual void OnNetClear(const MUID& CommUID) override;
	virtual void OnNetPong(const MUID& CommUID, unsigned int nTimeStamp) override;

	bool CheckOnLoginPre(const MUID& CommUID, int nCmdVersion, bool& outbFreeIP,
		std::string& strCountryCode3);
	void OnMatchLogin(const MUID& CommUID, const char* UserID, const unsigned char *HashedPassword,
		int HashLength, int CommandVersion, unsigned long ChecksumPack,
		u32 Major, u32 Minor, u32 Patch, u32 Revision);
	void OnMatchLoginFromDBAgent(const MUID& CommUID, const char* szLoginID,
		const char* szName, int nSex, bool bFreeLoginIP, unsigned long nChecksumPack);
	void OnMatchLoginFailedFromDBAgent(const MUID& CommUID, int nResult);
	void OnBridgePeer(const MUID& uidChar, DWORD dwIP, DWORD nPort);
	bool AddObjectOnMatchLogin(const MUID& uidComm,
		MMatchAccountInfo* pSrcAccountInfo,
		bool bFreeLoginIP,
		std::string strCountryCode3,
		unsigned long nChecksumPack);

	void NotifyFailedLogin(const MUID &uidComm, const char *szReason);
	void CreateAccount(const MUID &uidComm, const char *szUsername,
		const unsigned char *HashedPassword, int HashLength, const char *szEmail);
	void CreateAccountResponse(const MUID &uidComm, const char *szMessage);

	void LockUIDGenerate() { m_csUIDGenerateLock.Lock(); }
	void UnlockUIDGenerate() { m_csUIDGenerateLock.Unlock(); }

	int ObjectAdd(const MUID& uidComm);
	int ObjectRemove(const MUID& uid, MMatchObjectList::iterator* pNextItor);

	int MessageSay(MUID& uid, char* pszSay);

	// UDP
	MSafeUDP* GetSafeUDP() { return &m_SafeUDP; }
	void SendCommandByUDP(MCommand* pCommand, char* szIP, int nPort);
	void ParsePacket(char* pData, MPacketHeader* pPacketHeader, DWORD dwIP, WORD wRawPort);
	static bool UDPSocketRecvEvent(DWORD dwIP, WORD wRawPort, char* pPacket, DWORD dwSize);
	void ParseUDPPacket(char* pData, MPacketHeader* pPacketHeader, DWORD dwIP, WORD wRawPort);

	// Async DB
	void ProcessAsyncJob();
	void OnAsyncGetAccountCharList(MAsyncJob* pJobResult);
	void OnAsyncGetAccountCharInfo(MAsyncJob* pJobResult);
	void OnAsyncGetCharInfo(MAsyncJob* pJobResult);
	void OnAsyncCreateChar(MAsyncJob* pJobResult);
	void OnAsyncDeleteChar(MAsyncJob* pJobResult);
	void OnAsyncGetFriendList(MAsyncJob* pJobInput);
	void OnAsyncGetLoginInfo(MAsyncJob* pJobInput);
	void OnAsyncWinTheClanGame(MAsyncJob* pJobInput);
	void OnAsyncUpdateCharInfoData(MAsyncJob* pJobInput);
	void OnAsyncCharFinalize(MAsyncJob* pJobInput);
	void OnAsyncBringAccountItem(MAsyncJob* pJobResult);
	void OnAsyncInsertConnLog(MAsyncJob* pJobResult);
	void OnAsyncInsertGameLog(MAsyncJob* pJobResult);
	void OnAsyncCreateClan(MAsyncJob* pJobResult);
	void OnAsyncExpelClanMember(MAsyncJob* pJobResult);
	void OnAsyncInsertEvent(MAsyncJob* pJobResult);
	void OnAsyncUpdateIPtoCoutryList(MAsyncJob* pJobResult);
	void OnAsyncUpdateBlockCountryCodeList(MAsyncJob* pJobResult);
	void OnAsyncUpdateCustomIPList(MAsyncJob* pJobResult);

	bool InitScheduler();
	bool InitLocale();
	bool InitEvent();

	virtual bool InitSubTaskSchedule() { return true; }

	void DisconnectObject(const MUID& uidObject);
	void DebugTest();

	const char* GetDefaultChannelName() { return m_szDefaultChannelName; }
	void SetDefaultChannelName(const char* pszName) { strcpy_safe(m_szDefaultChannelName, pszName); }
	const char* GetDefaultChannelRuleName() { return m_szDefaultChannelRuleName; }
	void SetDefaultChannelRuleName(const char* pszName) { strcpy_safe(m_szDefaultChannelRuleName, pszName); }

	bool ChannelAdd(const char* pszChannelName, const char* pszRuleName,
		MUID* pAllocUID, MCHANNEL_TYPE nType = MCHANNEL_TYPE_PRESET,
		int nMaxPlayers = DEFAULT_CHANNEL_MAXPLAYERS, int nLevelMin = -1, int nLevelMax = -1);
	bool ChannelJoin(const MUID& uidPlayer, const MUID& uidChannel);
	bool ChannelJoin(const MUID& uidPlayer, const MCHANNEL_TYPE nChannelType, const char* pszChannelName);
	bool ChannelLeave(const MUID& uidPlayer, const MUID& uidChannel);
	bool ChannelChat(const MUID& uidPlayer, const MUID& uidChannel, char* pszChat);

	void ResponseChannelRule(const MUID& uidPlayer, const MUID& uidChannel);

	void OnRequestRecommendedChannel(const MUID& uidComm);
	void OnRequestChannelJoin(const MUID& uidPlayer, const MUID& uidChannel);
	void OnRequestChannelJoin(const MUID& uidPlayer, const MCHANNEL_TYPE nChannelType, const char* pszChannelName);
	void OnChannelChat(const MUID& uidPlayer, const MUID& uidChannel, char* pszChat);
	void OnStartChannelList(const MUID& uidPlayer, const int nChannelType);
	void OnStopChannelList(const MUID& uidPlayer);

	void OnChannelRequestPlayerList(const MUID& uidPlayer, const MUID& uidChannel, int nPage);
	void OnChannelRequestAllPlayerList(const MUID& uidPlayer, const MUID& uidChannel, unsigned long int nPlaceFilter, unsigned long int nOptions);

	bool StageAdd(MMatchChannel* pChannel, const char* pszStageName, bool bPrivate, const char* pszStagePassword, MUID* pAllocUID);
	bool StageRemove(const MUID& uidStage, MMatchStageMap::iterator* pNextItor);
	bool StageJoin(const MUID& uidPlayer, const MUID& uidStage);
	bool StageLeave(const MUID& uidPlayer, const MUID& uidStage);
	bool StageEnterBattle(const MUID& uidPlayer, const MUID& uidStage);
	bool StageLeaveBattle(const MUID& uidPlayer, const MUID& uidStage);
	bool StageChat(const MUID& uidPlayer, const MUID& uidStage, char* pszChat);
	bool StageTeam(const MUID& uidPlayer, const MUID& uidStage, MMatchTeam nTeam);
	bool StagePlayerState(const MUID& uidPlayer, const MUID& uidStage, MMatchObjectStageState nStageState);
	bool StageMaster(const MUID& uidStage);

	MCommand* CreateCmdResponseStageSetting(const MUID& uidStage);
	MCommand* CreateCmdMatchResponseLoginOK(const MUID& uidComm,
		MUID& uidPlayer,
		const char* szUserID,
		MMatchUserGradeID nUGradeID,
		MMatchPremiumGradeID nPGradeID,
		const char* szRandomValue);
	MCommand* CreateCmdMatchResponseLoginFailed(const MUID& uidComm, const int nResult);


	float GetDuelVictoryMultiflier(int nVictorty);
	float GetDuelPlayersMultiflier(int nPlayerCount);
	void CalcExpOnGameKill(MMatchStage* pStage, MMatchObject* pAttacker, MMatchObject* pVictim,
		int* poutAttackerExp, int* poutVictimExp);
	const int CalcBPonGameKill(MMatchStage* pStage, MMatchObject* pAttacker, const int nAttackerLevel, const int nVictimLevel);
	void ProcessOnGameKill(MMatchStage* pStage, MMatchObject* pAttacker, MMatchObject* pVictim);
	void PostGameDeadOnGameKill(MUID& uidStage, MMatchObject* pAttacker, MMatchObject* pVictim,
		int nAddedAttackerExp, int nSubedVictimExp);

	// Stage
	void OnStageCreate(const MUID& uidChar, char* pszStageName, bool bPrivate, char* pszStagePassword);
	void OnStageJoin(const MUID& uidPlayer, const MUID& uidStage);
	void OnPrivateStageJoin(const MUID& uidPlayer, const MUID& uidStage, char* pszPassword);
	void OnStageFollow(const MUID& uidPlayer, const char* pszTargetName);
	void OnStageLeave(const MUID& uidPlayer, const MUID& uidStage);
	void OnStageRequestPlayerList(const MUID& uidPlayer, const MUID& uidStage);
	void OnStageEnterBattle(const MUID& uidPlayer, const MUID& uidStage);
	void OnStageLeaveBattle(const MUID& uidPlayer, const MUID& uidStage);
	void OnStageChat(const MUID& uidPlayer, const MUID& uidStage, char* pszChat);
	void OnRequestQuickJoin(const MUID& uidPlayer, void* pQuickJoinBlob);
	void ResponseQuickJoin(const MUID& uidPlayer, MTD_QuickJoinParam* pQuickJoinParam);
	void OnStageGo(const MUID& uidPlayer, unsigned int nRoomNo);
	void OnStageTeam(const MUID& uidPlayer, const MUID& uidStage, MMatchTeam nTeam);
	void OnStagePlayerState(const MUID& uidPlayer, const MUID& uidStage, MMatchObjectStageState nStageState);
	void OnStageStart(const MUID& uidPlayer, const MUID& uidStage, int nCountdown);
	void OnStartStageList(const MUID& uidComm);
	void OnStopStageList(const MUID& uidComm);
	void OnStageRequestStageList(const MUID& uidPlayer, const MUID& uidChannel, const int nStageCursor);
	void OnStageMap(const MUID& uidStage, const char* pszMapName);
	void OnStageSetting(const MUID& uidPlayer, const MUID& uidStage, void* pStageBlob, int nStageCount);
	void OnRequestStageSetting(const MUID& uidComm, const MUID& uidStage);
	void OnRequestPeerList(const MUID& uidChar, const MUID& uidStage);
	void OnRequestGameInfo(const MUID& uidChar, const MUID& uidStage);
	void OnMatchLoadingComplete(const MUID& uidPlayer, int nPercent);
	void OnRequestRelayPeer(const MUID& uidChar, const MUID& uidPeer);
	void OnPeerReady(const MUID& uidChar, const MUID& uidPeer);
	void OnGameRoundState(const MUID& uidStage, int nState, int nRound);

	// Ingame stuff
	void OnRequestSpawn(const MUID& uidChar, const MVector& pos, const MVector& dir);
	void OnGameRequestTimeSync(const MUID& uidComm, unsigned long nLocalTimeStamp);
	void OnGameReportTimeSync(const MUID& uidComm, unsigned long nLocalTimeStamp,
		unsigned int nDataChecksum);
	void OnUpdateFinishedRound(const MUID& uidStage, const MUID& uidChar,
		void* pPeerInfo, void* pKillInfo);
	void OnRequestForcedEntry(const MUID& uidStage, const MUID& uidChar);
	void OnRequestSuicide(const MUID& uidPlayer);
	void OnRequestObtainWorldItem(const MUID& uidPlayer, int nItemUID);
	void OnRequestSpawnWorldItem(const MUID& uidPlayer, int nItemID,
		float x, float y, float z);

	// User actions
	void OnUserWhisper(const MUID& uidComm, char* pszSenderName, char* pszTargetName, char* pszMessage);
	void OnUserWhere(const MUID& uidComm, char* pszTargetName);
	void OnUserOption(const MUID& uidComm, unsigned long nOptionFlags);

	// Chatrooms
	void OnChatRoomCreate(const MUID& uidPlayer, const char* pszChatRoomName);
	void OnChatRoomJoin(const MUID& uidComm, char* pszPlayerName, char* pszChatRoomName);
	void OnChatRoomLeave(const MUID& uidComm, char* pszPlayerName, char* pszChatRoomName);
	void OnChatRoomSelectWrite(const MUID& uidComm, const char* szChatRoomName);
	void OnChatRoomInvite(const MUID& uidComm, const char* pszTargetName);
	void OnChatRoomChat(const MUID& uidComm, const char* pszMessage);

	// Ladder
	bool LadderJoin(const MUID& uidPlayer, const MUID& uidStage, MMatchTeam nTeam);
	void LadderGameLaunch(MLadderGroup* pGroupA, MLadderGroup* pGroupB);

	void OnLadderRequestInvite(const MUID& uidPlayer, void* pGroupBlob);
	void OnLadderInviteAgree(const MUID& uidPlayer);
	void OnLadderInviteCancel(const MUID& uidPlayer);
	void OnLadderRequestChallenge(const MUID& uidPlayer, void* pGroupBlob,
		unsigned long int nOptions);
	void OnLadderRequestCancelChallenge(const MUID& uidPlayer);

	void OnRequestProposal(const MUID& uidProposer, int nProposalMode, int nRequestID,
		const int nReplierCount, void* pReplierNamesBlob);
	void OnReplyAgreement(MUID& uidProposer, MUID& uidReplier, const char* szReplierName,
		const int nProposalMode, int nRequestID, bool bAgreement);

	// Test server
	void OnRequestCopyToTestServer(const MUID& uidPlayer);
	void ResponseCopyToTestServer(const MUID& uidPlayer, int nResult);

	// Characters
	void OnRequestMySimpleCharInfo(const MUID& uidPlayer);
	void ResponseMySimpleCharInfo(const MUID& uidPlayer);
	void OnRequestCharInfoDetail(const MUID& uidChar, const char* szCharName);
	void ResponseCharInfoDetail(const MUID& uidChar, const char* szCharName);
	void OnRequestAccountCharList(const MUID& uidPlayer);
	void OnRequestAccountCharInfo(const MUID& uidPlayer, int nCharNum);
	void OnRequestSelectChar(const MUID& uidPlayer, int nCharIndex);
	void OnRequestDeleteChar(const MUID& uidPlayer, int nCharIndex, const char* szCharName);
	bool ResponseDeleteChar(const MUID& uidPlayer, int nCharIndex, const char* szCharName);
	void OnRequestCreateChar(const MUID& uidPlayer, int nCharIndex, const char* szCharName,
		unsigned int nSex, unsigned int nHair, unsigned int nFace,
		unsigned int nCostume);
	bool ResponseCreateChar(const MUID& uidPlayer, int nCharIndex, const char* szCharName,
		MMatchSex nSex, unsigned int nHair, unsigned int nFace,
		unsigned int nCostume);
	void OnCharClear(const MUID& uidPlayer);
	bool CharInitialize(const MUID& uidPlayer);
	bool CharFinalize(const MUID& uidPlayer);
	bool CorrectEquipmentByLevel(MMatchObject* pPlayer, MMatchCharItemParts nPart,
		int nLegalItemLevelDiff = 0);
	bool RemoveCharItem(MMatchObject* pObject, MUID& uidItem);

	// Friends
	void OnFriendAdd(const MUID& uidPlayer, const char* pszName);
	void OnFriendRemove(const MUID& uidPlayer, const char* pszName);
	void OnFriendList(const MUID& uidPlayer);
	void OnFriendMsg(const MUID& uidPlayer, const char* szMsg);
	void FriendList(const MUID& uidPlayer);

	int ValidateCreateClan(const char* szClanName, MMatchObject* pMasterObject,
		MMatchObject** ppSponsorObject);
	void UpdateCharClanInfo(MMatchObject* pObject, int nCLID, const char* szClanName,
		MMatchClanGrade nGrade);

	// Clans
	void OnClanRequestCreateClan(const MUID& uidPlayer, int nRequestID, const char* szClanName,
		char** szSponsorNames);
	void OnClanAnswerSponsorAgreement(int nRequestID, const MUID& uidClanMaster,
		char* szSponsorCharName, bool bAnswer);
	void OnClanRequestAgreedCreateClan(const MUID& uidPlayer, const char* szClanName,
		char** szSponsorNames);
	void OnClanRequestCloseClan(const MUID& uidClanMaster, const char* szClanName);
	void ResponseCloseClan(const MUID& uidClanMaster, const char* szClanName);
	void OnClanRequestJoinClan(const MUID& uidClanAdmin, const char* szClanName,
		const char* szJoiner);
	void ResponseJoinClan(const MUID& uidClanAdmin, const char* szClanName,
		const char* szJoiner);
	void OnClanAnswerJoinAgreement(const MUID& uidClanAdmin, const char* szJoiner, bool bAnswer);
	void OnClanRequestAgreedJoinClan(const MUID& uidClanAdmin, const char* szClanName,
		const char* szJoiner);
	void ResponseAgreedJoinClan(const MUID& uidClanAdmin, const char* szClanName,
		const char* szJoiner);

	void OnClanRequestLeaveClan(const MUID& uidPlayer);
	void ResponseLeaveClan(const MUID& uidPlayer);
	void OnClanRequestChangeClanGrade(const MUID& uidClanMaster, const char* szMember, int nClanGrade);
	void ResponseChangeClanGrade(const MUID& uidClanMaster, const char* szMember, int nClanGrade);
	void OnClanRequestExpelMember(const MUID& uidClanAdmin, const char* szMember);
	void ResponseExpelMember(const MUID& uidClanAdmin, const char* szMember);
	void OnClanRequestMsg(const MUID& uidSender, const char* szMsg);
	void OnClanRequestMemberList(const MUID& uidChar);
	void OnClanRequestClanInfo(const MUID& uidChar, const char* szClanName);

	void OnClanRequestEmblemURL(const MUID& uidChar, int nCLID);

	// Admin
	void OnAdminTerminal(const MUID& uidAdmin, const char* szText);
	void OnAdminAnnounce(const MUID& uidAdmin, const char* szChat, unsigned long int nType);
	void OnAdminRequestServerInfo(const MUID& uidAdmin);
	void OnAdminServerHalt(const MUID& uidAdmin);

	void OnAdminRequestBanPlayer(const MUID& uidAdmin, const char* szPlayer);
	void OnAdminRequestUpdateAccountUGrade(const MUID& uidAdmin, const char* szPlayer);
	void OnAdminPingToAll(const MUID& uidAdmin);
	void OnAdminRequestSwitchLadderGame(const MUID& uidAdmin, const bool bEnabled);
	void OnAdminHide(const MUID& uidAdmin);
	void OnAdminResetAllHackingBlock(const MUID& uidAdmin);

	// Event
	void OnEventChangeMaster(const MUID& uidAdmin);
	void OnEventChangePassword(const MUID& uidAdmin, const char* szPassword);
	void OnEventRequestJjang(const MUID& uidAdmin, const char* pszTargetName);
	void OnEventRemoveJjang(const MUID& uidAdmin, const char* pszTargetName);

	// Items
	bool BuyItem(MMatchObject* pObject, unsigned int nItemID, bool bRentItem = false, int nRentPeriodHour = 0);
	void OnRequestBuyItem(const MUID& uidPlayer, const unsigned long int nItemID);
	bool ResponseBuyItem(const MUID& uidPlayer, const unsigned long int nItemID);
	void OnRequestSellItem(const MUID& uidPlayer, const MUID& uidItem);
	bool ResponseSellItem(const MUID& uidPlayer, const MUID& uidItem);
	void OnRequestShopItemList(const MUID& uidPlayer, const int nFirstItemIndex, const int nItemCount);
	void ResponseShopItemList(const MUID& uidPlayer, const int nFirstItemIndex, const int nItemCount);
	void OnRequestCharacterItemList(const MUID& uidPlayer);
	void ResponseCharacterItemList(const MUID& uidPlayer);
	void OnRequestAccountItemList(const MUID& uidPlayer);
	void ResponseAccountItemList(const MUID& uidPlayer);
	void OnRequestEquipItem(const MUID& uidPlayer, const MUID& uidItem, const long int nEquipmentSlot);
	void ResponseEquipItem(const MUID& uidPlayer, const MUID& uidItem, const MMatchCharItemParts parts);
	void OnRequestTakeoffItem(const MUID& uidPlayer, const unsigned long int nEquipmentSlot);
	void ResponseTakeoffItem(const MUID& uidPlayer, const MMatchCharItemParts parts);
	void OnRequestBringAccountItem(const MUID& uidPlayer, const int nAIID);
	void ResponseBringAccountItem(const MUID& uidPlayer, const int nAIID);
	void OnRequestBringBackAccountItem(const MUID& uidPlayer, const MUID& uidItem);
	void ResponseBringBackAccountItem(const MUID& uidPlayer, const MUID& uidItem);

	// Locator and keeper stuff
	void OnResponseServerStatus(const MUID& uidSender);
	void OnRequestServerHearbeat(const MUID& uidSender);
	void OnResponseServerHeartbeat(const MUID& uidSender);
	void OnRequestConnectMatchServer(const MUID& uidSender);
	void OnResponseConnectMatchServer(const MUID& uidSender);
	void OnRequestKeeperAnnounce(const MUID& uidSender, const char* pszAnnounce);
	void OnRequestStopServerWithAnnounce(const MUID& uidSender);
	void OnResponseStopServerWithAnnounce(const MUID& uidSender);
	void OnRequestSchedule(const MUID& uidSender,
		int nType,
		int nYear,
		int nMonth,
		int nDay,
		int nHour,
		int nMin,
		int nCount,
		int nCommand,
		const char* pszAnnounce);
	void OnResponseSchedule(const MUID& uidSender,
		int nType,
		int nYear,
		int nMonth,
		int nDay,
		int nHour,
		int nMin,
		int nCount,
		int nCommand,
		const char* pszAnnounce);

	// Quest
	void OnRequestNPCDead(const MUID& uidSender, const MUID& uidKiller, MUID& uidNPC, MVector& pos);
	void OnQuestRequestDead(const MUID& uidVictim);
	void OnQuestTestRequestNPCSpawn(const MUID& uidPlayer, int nNPCType, int nNPCCount);
	void OnQuestTestRequestClearNPC(const MUID& uidPlayer);
	void OnQuestTestRequestSectorClear(const MUID& uidPlayer);
	void OnQuestTestRequestQuestFinish(const MUID& uidPlayer);
	void OnQuestRequestMovetoPortal(const MUID& uidPlayer);
	void OnQuestReadyToNewSector(const MUID& uidPlayer);
	void OnQuestStageMapset(const MUID& uidStage, int nMapsetID);

	// Quest
	void OnResponseCharQuestItemList(const MUID& uidSender);
	void OnRequestBuyQuestItem(const MUID& uidSender, unsigned long int nItemID);
	void OnResponseBuyQeustItem(const MUID& uidSender, unsigned long int nItemID);
	void OnRequestSellQuestItem(const MUID& uidSender, unsigned long int nItemID, int nCount);
	void OnResponseSellQuestItem(const MUID& uidSender, unsigned long int nItemID, int nCount);
	void OnRequestDropSacrificeItemOnSlot(const MUID& uidSender, int nSlotIndex,
		unsigned long int nItemID);
	void OnRequestCallbackSacrificeItem(const MUID& uidSender, int nSlotIndex,
		unsigned long int nItemID);
	void OnRequestQL(const MUID& uidSender);
	void OnRequestSacrificeSlotInfo(const MUID& uidSender);
	void OnRequestMonsterBibleInfo(const MUID& uidSender);
	void OnResponseMonsterBibleInfo(const MUID& uidSender);

	void OnQuestPong(const MUID& uidSender);

	// Agent
	int AgentAdd(const MUID& uidComm);
	int AgentRemove(const MUID& uidAgent, MAgentObjectMap::iterator* pNextItor);
	MAgentObject* GetAgent(const MUID& uidAgent);
	MAgentObject* GetAgentByCommUID(const MUID& uidComm);
	bool CheckBridgeFault();
	MAgentObject* FindFreeAgent();
	void ReserveAgent(MMatchStage* pStage);
	void LocateAgentToClient(const MUID& uidPlayer, const MUID& uidAgent);

	void OnRegisterAgent(const MUID& uidComm, char* szIP, int nTCPPort, int nUDPPort);
	void OnUnRegisterAgent(const MUID& uidComm);
	void OnAgentStageReady(const MUID& uidCommAgent, const MUID& uidStage);
	void OnRequestLiveCheck(const MUID& uidComm, unsigned long nTimeStamp,
		unsigned long nStageCount, unsigned long nUserCount);

	void OnVoiceChat(const MUID& Player, unsigned char* EncodedFrame, int Length);

	void OnTunnelledP2PCommand(const MUID& Sender, const MUID& Receiver,
		const char* Blob, size_t BlobSize);

	void OnPeerShot(MMatchObject& SenderObj, MMatchStage& Stage, const struct ZPACKEDSHOTINFO& psi);

	void NotifyMessage(const MUID& uidChar, int nMsgID);

	void SetClientClockSynchronize(const MUID& CommUID);
	static unsigned long int ConvertLocalClockToGlobalClock(
		unsigned long int nLocalClock,
		unsigned long int nLocalClockDistance);
	static unsigned long int ConvertGlobalClockToLocalClock(
		unsigned long int nGlobalClock,
		unsigned long int nLocalClockDistance);

	void InsertChatDBLog(const MUID& uidPlayer, const char* szMsg);
	int ValidateMakingName(const char* szCharName, int nMinLength, int nMaxLength);

	int ValidateStageJoin(const MUID& uidPlayer, const MUID& uidStage);
	int ValidateChannelJoin(const MUID& uidPlayer, const MUID& uidChannel);
	int ValidateEquipItem(MMatchObject* pObj, MMatchItem* pItem, const MMatchCharItemParts parts);
	int ValidateChallengeLadderGame(MMatchObject** ppMemberObject, int nMemberCount);
	void CheckExpiredItems(MMatchObject* pObj);
	void ResponseExpiredItemIDList(MMatchObject* pObj,
		std::vector<unsigned long int>& vecExpiredItemIDList);

	bool LoadInitFile();
	bool LoadChannelPreset();
	bool InitDB();
	void UpdateServerLog();
	void UpdateServerStatusDB();

	void UpdateCharDBCachingData(MMatchObject* pObject);

	unsigned long GetItemFileChecksum() const { return m_nItemFileChecksum; }
	void SetItemFileChecksum(unsigned long nChecksum) { m_nItemFileChecksum = nChecksum; }

	bool CheckItemXML();

	MCountryFilter& GetCountryFilter() { return m_CountryFilter; }
	bool InitCountryFilterDB();
	CUSTOM_IP_STATUS CheckIsValidCustomIP(const MUID& CommUID,
		const std::string& strIP, std::string& strCountryCode3, const bool bUseFilter);
	COUNT_CODE_STATUS CheckIsNonBlockCountry(const MUID& CommUID,
		const std::string& strIP, std::string& strCountryCode3, const bool bUseFilter);

	inline void SetTickTime(u64 nTickTime);

	static MMatchServer*	m_pInstance;
	u64						m_nTickTime;

	u32					m_HSCheckCounter;

	u32					m_nItemFileChecksum;

	MUID				m_NextUseUID;
	MCriticalSection	m_csUIDGenerateLock;
	MCriticalSection	m_csTickTimeLock;

	MMatchObjectList	m_Objects;

	MMatchChannelMap	m_ChannelMap;

	char				m_szDefaultChannelName[CHANNELNAME_LEN];
	char				m_szDefaultChannelRuleName[CHANNELRULE_LEN];

	MMatchStageMap		m_StageMap;
	MMatchClanMap		m_ClanMap;
	MAgentObjectMap		m_AgentMap;

	MSafeUDP			m_SafeUDP;
	DBVariant			Database;

	MAsyncProxy			m_AsyncProxy;
	MMatchAdmin			m_Admin;
	MMatchShutdown		m_MatchShutdown;
	MMatchChatRoomMgr	m_ChatRoomMgr;
	MLadderMgr			m_LadderMgr;

	bool				m_bCreated{};

	MMatchScheduleMgr*		m_pScheduler;
	MMatchQuest				m_Quest;

	MCountryFilter			m_CountryFilter;
	IPtoCountryList			m_TmpIPtoCountryList;
	BlockCountryCodeList	m_TmpBlockCountryCodeList;
	CustomIPList			m_TmpCustomIPList;
	u32						m_dwBlockCount;
	u32						m_dwNonBlockCount;

	MMatchEventManager		m_CustomEventManager;

	u64 LastPingTime{};
};

void CopyCharInfoForTrans(MTD_CharInfo* pDest, MMatchCharInfo* pSrc, MMatchObject* pSrcObject);
void CopyCharInfoDetailForTrans(MTD_CharInfo_Detail* pDest, MMatchCharInfo* pSrcCharInfo, MMatchObject* pSrcObject);

inline MMatchServer* MGetMatchServer()
{
	return MMatchServer::GetInstance();
}

inline u64 MMatchServer::GetTickTime()
{
	m_csTickTimeLock.Lock();
	auto ret = m_nTickTime;
	m_csTickTimeLock.Unlock();
	return ret;
}

inline void MMatchServer::SetTickTime(u64 nTickTime)
{
	m_csTickTimeLock.Lock();
	m_nTickTime = nTickTime;
	m_csTickTimeLock.Unlock();
}

inline const char* MErrStr(const int nID)
{
	return MGetStringResManager()->GetErrorStr(nID);
}

bool IsExpiredBlockEndTime(const SYSTEMTIME& st);

void _CheckValidPointer(void* pPointer1, void* pPointer2, void* pPointer3, int nState, int nValue);
#define CheckValidPointer(A, B)		_CheckValidPointer(m_pMessengerManager, m_pScheduler, m_pAuthBuilder, A, B);CheckMemoryTest(A, B);
#define CheckValidPointer2(A, B)	MMatchServer::GetInstance()->CheckMemoryTest(A, B);
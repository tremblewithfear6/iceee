#ifndef PARTYMANAGER_H
#define PARTYMANAGER_H 

#include <string>
#include <vector>
#include "Creature.h"

struct PartyMember
{
	int mCreatureDefID;
	int mCreatureID;
	std::string mDisplayName;
	CreatureInstance *mCreaturePtr;
	int mSocket;
};

class ActiveParty
{
public:
	int mPartyID;
	int mLeaderDefID;
	int mLeaderID;
	std::string mLeaderName;
	std::vector<PartyMember> mMemberList;
	void AddMember(CreatureInstance* member);
	bool HasMember(int memberDefID);
	void RemoveMember(int memberDefID);
	bool UpdateLeaderDropped(int memberID);
	bool SetLeader(int newLeaderDefID);
	PartyMember* GetMemberByID(int memberID);
	PartyMember* GetMemberByDefID(int memberDefID);
	bool UpdatePlayerReferences(CreatureInstance* member);
	bool RemovePlayerReferences(int memberDefID, bool disconnect);
	void RebroadCastMemberList(char *buffer);
	void DebugDestroyParty(const char *buffer, int length);
	void Disband(char *buffer);
	int GetMaxPlayerLevel(void);

	void BroadCast(const char *buffer, int length);
	void BroadCastExcept(const char *buffer, int length, int excludeDefID);
	void BroadCastTo(const char *buffer, int length, int creatureDefID);
	void AttemptSend(int socket, const char *buffer, int length);
};

struct PartyUpdateOpTypes
{
	enum
	{
		INVITE					= 0,
		PROPOSE_INVITE			= 1,
		INVITE_REJECTED			= 2,
		ADD_MEMBER				= 3,
		REMOVE_MEMBER			= 4,
		JOINED_PARTY			= 5,
		LEFT_PARTY				= 6,
		IN_CHARGE				= 7,
		STRATEGY_CHANGE			= 8,
		STRATEGYFLAGS_CHANGE	= 9,
		OFFER_LOOT				= 10,
		LOOT_ROLL 				= 11,
		LOOT_WIN 				= 12,
		QUEST_INVITE			= 13,
	};
};

struct PartyManager
{
	std::vector<ActiveParty> mPartyList;
	int nextPartyID;

	PartyManager();
	ActiveParty* GetPartyByLeader(int leaderDefID);
	ActiveParty* GetPartyByID(int partyID);
	ActiveParty* GetPartyWithMember(int memberDefID);
	ActiveParty* CreateParty(CreatureInstance* leader);
	int GetNextPartyID(void);

	int AcceptInvite(CreatureInstance* member, CreatureInstance* leader);

	void BroadcastAddMember(CreatureInstance* member);
	void DoQuit(CreatureInstance* member);
	void DoRejectInvite(int leaderDefID, const char* nameDenied);
	void DoSetLeader(CreatureInstance *callMember, int newLeaderID);
	void DoKick(CreatureInstance *caller, int memberID);
	void DoQuestInvite(CreatureInstance *caller, const char *questName, int questID);
	void DeletePartyByID(int partyID);
	void UpdatePlayerReferences(CreatureInstance* member);
	void RemovePlayerReferences(int memberDefID, bool disconnect);
	void BroadCastPacket(int partyID, int callDefID, const char *buffer, int buflen);
	void CheckMemberLogin(CreatureInstance* member);
	int PrepMemberList(char *outbuf, int partyID, int memberID);
	void DebugDestroyParties(void);
	void DebugForceRemove(CreatureInstance *caller);

	static int WriteInvite(char *outbuf, int leaderId, const char *leaderName);
	static int WriteProposeInvite(char *outbuf, int proposeeId, const char *proposeeName, int proposerId, const char *proposerName);
	static int WriteMemberList(char *outbuf, ActiveParty *party, int memberID);
	static int WriteLeftParty(char *outbuf);
	static int WriteRemoveMember(char *outbuf, int memberID);
	static int WriteInCharge(char *outbuf, ActiveParty *party);
	static int WriteQuestInvite(char *outbuf, const char* questName, int questID);
	static int WriteRejectInvite(char *outbuf, const char *memberDenied);

	char WriteBuf[512];
};

extern PartyManager g_PartyManager;

#endif //#ifndef PARTYMANAGER_H
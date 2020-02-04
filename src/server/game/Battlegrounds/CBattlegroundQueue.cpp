#include "CBattlegroundQueue.h"

#include "BattlegroundMgr.h"
#include "World.h"

class GroupList : public std::list<GroupQueueInfo*>
{
public:
    void AddGroups(std::list<GroupQueueInfo*> list)
    {
        insert(end(), list.begin(), list.end());
    }

    void SortByJoinTime()
    {
        sort([](GroupQueueInfo* a, GroupQueueInfo* b) { return a->JoinTime < b->JoinTime; });
    }
};

bool CBattlegroundQueue::CheckMixedMatch(Battleground* bg_template, BattlegroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers)
{
    return CFBGGroupInserter(bg_template, bracket_id, maxPlayers, maxPlayers, minPlayers);
}

bool CBattlegroundQueue::MixPlayersToBG(Battleground* bg, BattlegroundBracketId bracket_id)
{
    return CFBGGroupInserter(bg, bracket_id, bg->GetFreeSlotsForTeam(ALLIANCE), bg->GetFreeSlotsForTeam(HORDE), 0);
}

bool CBattlegroundQueue::CFBGGroupInserter(Battleground* bg, BattlegroundBracketId bracket_id, uint32 AllyFree, uint32 HordeFree, uint32 MinPlayers)
{
    if (!sWorld->getBoolConfig(CONFIG_CFBG_ENABLED) || !bg->isBattleground())
        return false;

    // MinPlayers is only 0 when we're filling an existing BG.
    bool Filling = MinPlayers == 0;

    uint32 MaxAlly = AllyFree;
    uint32 MaxHorde = HordeFree;

    m_SelectionPools[TEAM_ALLIANCE].Init();
    m_SelectionPools[TEAM_HORDE].Init();

    // If players on different factions queue at the same second it'll be random who gets added first
    bool AllyFirst = urand(0, 1);
    auto Groups = GroupList();

    Groups.AddGroups(m_QueuedGroups[bracket_id][AllyFirst ? BG_QUEUE_NORMAL_ALLIANCE : BG_QUEUE_NORMAL_HORDE]);
    Groups.AddGroups(m_QueuedGroups[bracket_id][AllyFirst ? BG_QUEUE_NORMAL_HORDE : BG_QUEUE_NORMAL_ALLIANCE]);
    Groups.SortByJoinTime();

    for (auto& ginfo : Groups)
    {
        if (!ginfo->IsInvitedToBGInstanceGUID)
        {
            bool AddAsAlly = AllyFree == HordeFree ? ginfo->OTeam == ALLIANCE : AllyFree > HordeFree;

            ginfo->Team = AddAsAlly ? ALLIANCE : HORDE;

            if (m_SelectionPools[AddAsAlly ? TEAM_ALLIANCE : TEAM_HORDE].AddGroup(ginfo, AddAsAlly ? MaxAlly : MaxHorde))
                AddAsAlly ? AllyFree -= ginfo->Players.size() : HordeFree -= ginfo->Players.size();
            else if (!Filling)
                break;

            // Return when we're ready to start a BG, if we're in startup process
            if (m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() >= MinPlayers &&
                m_SelectionPools[TEAM_HORDE].GetPlayerCount() >= MinPlayers &&
                !Filling)
                return true;
        }
    }

    // If we're in BG testing one player is enough
    if (sBattlegroundMgr->isTesting() && m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() + m_SelectionPools[TEAM_HORDE].GetPlayerCount() > 0)
        return true;

    // Filling always returns true unless we're not actually having CFBG enabled
    if (Filling)
        return true;

    // Return false when we didn't manage to fill the BattleGround in Filling "mode".
    // reset selectionpool for further attempts
    m_SelectionPools[TEAM_ALLIANCE].Init();
    m_SelectionPools[TEAM_HORDE].Init();
    return false;
}

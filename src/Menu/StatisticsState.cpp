/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "StatisticsState.h"
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Engine/Options.h"
#include "../Savegame/SavedGame.h"
#include "MainMenuState.h"
#include "../Savegame/MissionStatistics.h"
#include "../Savegame/Base.h"
#include "../Savegame/SoldierDiary.h"
#include "../Savegame/SoldierDeath.h"
#include "../Savegame/BattleUnitStatistics.h"
#include "../Savegame/Country.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Statistics window.
 * @param game Pointer to the core game.
 */
StatisticsState::StatisticsState()
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0, POPUP_BOTH);
	_btnOk = new TextButton(50, 12, 135, 180);
	_txtTitle = new Text(310, 25, 5, 8);
	_lstStats = new TextList(280, 136, 12, 36);

	// Set palette
	setInterface("newGameMenu");

	add(_window, "window", "saveMenus");
	add(_btnOk, "button", "saveMenus");
	add(_txtTitle, "text", "saveMenus");
	add(_lstStats, "list", "saveMenus");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&StatisticsState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&StatisticsState::btnOkClick, Options::keyOk);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);

	_lstStats->setColumns(2, 200, 80);
	_lstStats->setDot(true);

	listStats();
}

/**
 *
 */
StatisticsState::~StatisticsState()
{

}

template <typename T>
T StatisticsState::sumVector(const std::vector<T> &vec) const
{
	T total = 0;
	for (typename std::vector<T>::const_iterator i = vec.begin(); i != vec.end(); ++i)
	{
		total += *i;
	}
	return total;
}

void StatisticsState::listStats()
{
	SavedGame *save = _game->getSavedGame();

	std::wostringstream ss;
	GameTime *time = save->getTime();
	if (save->getEnding() == END_WIN)
	{
		ss << tr("STR_MISSION_WIN");
	}
	else if (save->getEnding() == END_LOSE)
	{
		ss << tr("STR_MISSION_LOSS");
	}
	ss << L'\x02' << time->getDayString(_game->getLanguage()) << L" " << tr(time->getMonthString()) << L" " << time->getYear();
	_txtTitle->setText(ss.str());

	int monthlyScore = sumVector(save->getResearchScores()) / save->getResearchScores().size();
	int64_t totalIncome = sumVector(save->getIncomes());
	int64_t totalExpenses = sumVector(save->getExpenditures());

	int missionsWin = 0, missionsLoss = 0, nightMissions = 0;
	int bestScore = -9999, worstScore = 9999;
	for (std::vector<MissionStatistics*>::const_iterator i = save->getMissionStatistics()->begin(); i != save->getMissionStatistics()->end(); ++i)
	{
		if ((*i)->success)
		{
			missionsWin++;
		}
		else
		{
			missionsLoss++;
		}
		bestScore = std::max(bestScore, (*i)->score);
		worstScore = std::min(worstScore, (*i)->score);
		if ((*i)->daylight > 5)
		{
			nightMissions++;
		}
	}
	// Make sure dummy values aren't left in
	bestScore = (bestScore == -9999) ? 0 : bestScore;
	worstScore = (worstScore == 9999) ? 0 : worstScore;

	std::vector<Soldier*> allSoldiers;
	for (std::vector<Base*>::const_iterator i = save->getBases()->begin(); i != save->getBases()->end(); ++i)
	{
		allSoldiers.insert(allSoldiers.end(), (*i)->getSoldiers()->begin(), (*i)->getSoldiers()->end());
	}
	allSoldiers.insert(allSoldiers.end(), save->getDeadSoldiers()->begin(), save->getDeadSoldiers()->end());
	int soldiersRecruited = allSoldiers.size();
	int soldiersLost = save->getDeadSoldiers()->size();

	int aliensKilled = 0, aliensCaptured = 0, friendlyKills = 0;
	int daysWounded = 0, longestMonths = 0;
	std::map<std::string, int> weaponKills;
	for (std::vector<Soldier*>::iterator i = allSoldiers.begin(); i != allSoldiers.end(); ++i)
	{
		SoldierDiary *diary = (*i)->getDiary();
		aliensKilled += diary->getKillTotal();
		aliensCaptured += diary->getStunTotal();
		daysWounded += diary->getDaysWoundedTotal();
		longestMonths = std::max(longestMonths, diary->getMonthsService());
		std::map<std::string, int> weaponTotal = diary->getWeaponTotal();
		for (std::map<std::string, int>::const_iterator j = weaponTotal.begin(); j != weaponTotal.end(); ++j)
		{
			if (weaponKills.find(j->first) == weaponKills.end())
			{
				weaponKills[j->first] = j->second;
			}
			else
			{
				weaponKills[j->first] += j->second;
			}
		}

		if ((*i)->getDeath() != 0 && (*i)->getDeath()->getCause() != 0)
		{
			const BattleUnitKills *kills = (*i)->getDeath()->getCause();
			if (kills->faction == FACTION_PLAYER)
			{
				friendlyKills++;
			}
		}
	}

	int maxWeapon = 0;
	std::string highestWeapon = "STR_NONE";
	for (std::map<std::string, int>::const_iterator i = weaponKills.begin(); i != weaponKills.end(); ++i)
	{
		if (i->second > maxWeapon)
		{
			maxWeapon = i->second;
			highestWeapon = i->first;
		}
	}

	int ufosDetected = save->getId("STR_UFO") - 1;
	int alienBases = save->getId("STR_ALIEN_BASE") - 1;
	int terrorSites = save->getId("STR_TERROR_SITE") - 1;
	int totalCrafts = 0;
	for (std::vector<std::string>::const_iterator i = _game->getMod()->getCraftsList().begin(); i != _game->getMod()->getCraftsList().end(); ++i)
	{
		totalCrafts += save->getId(*i) - 1;
	}

	int currentBases = save->getBases()->size();
	int currentScientists = 0, currentEngineers = 0;
	for (std::vector<Base*>::const_iterator i = save->getBases()->begin(); i != save->getBases()->end(); ++i)
	{
		currentScientists += (*i)->getScientists();
		currentEngineers += (*i)->getEngineers();
	}

	int countriesLost = 0;
	for (std::vector<Country*>::const_iterator i = save->getCountries()->begin(); i != save->getCountries()->end(); ++i)
	{
		if ((*i)->getPact())
		{
			countriesLost++;
		}
	}

	int researchDone = save->getDiscoveredResearch().size();
	
	std::string difficulty[] = { "STR_1_BEGINNER", "STR_2_EXPERIENCED", "STR_3_VETERAN", "STR_4_GENIUS", "STR_5_SUPERHUMAN" };

	// TODO: Translate
	_lstStats->addRow(2, L"Difficulty", tr(difficulty[save->getDifficulty()]).c_str());
	_lstStats->addRow(2, L"Average monthly score", Text::formatNumber(monthlyScore).c_str());
	_lstStats->addRow(2, L"Total income", Text::formatFunding(totalIncome).c_str());
	_lstStats->addRow(2, L"Total expenditure", Text::formatFunding(totalExpenses).c_str());
	_lstStats->addRow(2, L"Missions accomplished", Text::formatNumber(missionsWin).c_str());
	_lstStats->addRow(2, L"Missions failed", Text::formatNumber(missionsLoss).c_str());
	_lstStats->addRow(2, L"Night missions", Text::formatNumber(nightMissions).c_str());
	_lstStats->addRow(2, L"Best rating", Text::formatNumber(bestScore).c_str());
	_lstStats->addRow(2, L"Worst rating", Text::formatNumber(worstScore).c_str());
	_lstStats->addRow(2, L"Soldiers recruited", Text::formatNumber(soldiersRecruited).c_str());
	_lstStats->addRow(2, L"Soldiers lost", Text::formatNumber(soldiersLost).c_str());
	_lstStats->addRow(2, L"Aliens killed", Text::formatNumber(aliensKilled).c_str());
	_lstStats->addRow(2, L"Aliens captured", Text::formatNumber(aliensCaptured).c_str());
	_lstStats->addRow(2, L"Friendly fire incidents", Text::formatNumber(friendlyKills).c_str());
	_lstStats->addRow(2, L"Most effective weapon", tr(highestWeapon).c_str());
	_lstStats->addRow(2, L"Longest months in service", Text::formatNumber(longestMonths).c_str());
	_lstStats->addRow(2, L"Sick days", Text::formatNumber(daysWounded).c_str());
	_lstStats->addRow(2, L"UFOs detected", Text::formatNumber(ufosDetected).c_str());
	_lstStats->addRow(2, L"Alien bases discovered", Text::formatNumber(alienBases).c_str());
	_lstStats->addRow(2, L"Countries infiltrated", Text::formatNumber(countriesLost).c_str());
	_lstStats->addRow(2, L"Cities terrorized", Text::formatNumber(terrorSites).c_str());
	_lstStats->addRow(2, L"Bases built", Text::formatNumber(currentBases).c_str());
	_lstStats->addRow(2, L"Craft owned", Text::formatNumber(totalCrafts).c_str());
	_lstStats->addRow(2, L"Scientists hired", Text::formatNumber(currentScientists).c_str());
	_lstStats->addRow(2, L"Engineers hired", Text::formatNumber(currentEngineers).c_str());
	_lstStats->addRow(2, L"Research completed", Text::formatNumber(researchDone).c_str());
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void StatisticsState::btnOkClick(Action *)
{
	_game->setSavedGame(0);
	_game->setState(new MainMenuState);
}

}